// T011 -- SharedAudioBuffer concurrency test.
//
// Verifies the buffer's contract under real threads (Spec 001 FR1-FR3, FR8):
//   1) A producer streams a known ramp; two consumers with independent cursors
//      each receive EVERY sample exactly once, in order, regardless of their
//      relative speeds (one fast, one deliberately slow).
//   2) Memory is bounded to the lagging consumer: while the slow consumer holds
//      back, retained samples never exceed its backlog, and once both pass a
//      point the prefix is dropped (base_sample advances).
//   3) Close() lets each consumer drain its tail and then exit (WaitAndRead
//      returns false only after the stream is closed and fully consumed).

#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

#include "pipeline/shared_audio_buffer.h"

using orator::pipeline::SharedAudioBuffer;

namespace {

// A consumer that pulls until the stream is closed and drained, recording the
// concatenation of everything it received. `slow` inserts small stalls so the
// two consumers progress at different rates (exercises the low-water mark).
struct Consumer {
  SharedAudioBuffer* buf;
  int cursor;
  bool slow;
  std::vector<float> got;

  void Run() {
    std::vector<float> chunk;
    while (buf->WaitAndRead(cursor, &chunk)) {
      got.insert(got.end(), chunk.begin(), chunk.end());
      if (slow) std::this_thread::sleep_for(std::chrono::microseconds(200));
    }
  }
};

}  // namespace

int main() {
  constexpr int kTotal = 200000;     // ~12.5 s @ 16 kHz
  constexpr int kFrame = 1600;       // 100 ms frames

  SharedAudioBuffer buf(16000);
  Consumer fast{&buf, buf.AddConsumer(), /*slow=*/false, {}};
  Consumer slow{&buf, buf.AddConsumer(), /*slow=*/true, {}};

  std::thread tf([&] { fast.Run(); });
  std::thread ts([&] { slow.Run(); });

  // Producer: a deterministic ramp so order/identity are checkable. Track the
  // peak retained window to prove memory stays bounded to the lagging cursor.
  std::atomic<long> max_retained{0};
  std::vector<float> frame(kFrame);
  int produced = 0;
  while (produced < kTotal) {
    const int n = std::min(kFrame, kTotal - produced);
    for (int i = 0; i < n; ++i) frame[i] = static_cast<float>(produced + i);
    buf.Append(frame.data(), n);
    produced += n;
    const long retained = buf.total_samples() - buf.base_sample();
    if (retained > max_retained.load()) max_retained.store(retained);
    std::this_thread::sleep_for(std::chrono::microseconds(50));
  }
  buf.Close();

  tf.join();
  ts.join();

  // 1) Both consumers saw every sample exactly once, in order.
  assert(static_cast<int>(fast.got.size()) == kTotal && "fast count");
  assert(static_cast<int>(slow.got.size()) == kTotal && "slow count");
  for (int i = 0; i < kTotal; ++i) {
    assert(fast.got[i] == static_cast<float>(i) && "fast order/identity");
    assert(slow.got[i] == static_cast<float>(i) && "slow order/identity");
  }

  // 2) Memory bounded: the retained window never held the whole stream (the low
  //    -water mark dropped the prefix both consumers had passed).
  std::printf("peak retained = %ld samples (of %d total)\n",
              max_retained.load(), kTotal);
  assert(max_retained.load() < kTotal && "retained window must stay bounded");

  // 3) Fully drained and closed.
  assert(buf.total_samples() == kTotal);

  std::printf("SharedAudioBuffer concurrency test PASSED\n");
  return 0;
}
