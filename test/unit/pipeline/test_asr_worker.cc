// Verifies stable ASR text IDs across partial events, final events, and the
// typed text sink without loading a model.

#include <cstdio>
#include <string>
#include <vector>

#include "core/stages.h"
#include "core/time_base.h"
#include "pipeline/asr_worker.h"
#include "pipeline/comprehensive_timeline.h"

namespace {

class FakeAsr final : public orator::core::IAsr {
 public:
  void Initialize(const orator::core::AsrConfig&) override {}
  void LoadWeights(const std::string&) override {}
  void Reset() override {}
  orator::core::Transcript Transcribe(
      const orator::core::AudioChunk&) override {
    return {};
  }
  void set_max_new_tokens(int) override {}
  void StreamReset(long) override { current_ = "speech"; }
  std::string StreamChunk(const float*, int, cudaStream_t) override {
    return current_;
  }
  std::string StreamFinalize(cudaStream_t) override { return current_; }
  int stream_audio_tokens() const override { return 1; }
  std::string name() const override { return "fake_asr"; }

 private:
  std::string current_;
};

}  // namespace

int main() {
  int failures = 0;
#define CHECK(condition, message)         \
  do {                                    \
    if (!(condition)) {                   \
      std::printf("FAIL: %s\n", message); \
      ++failures;                         \
    }                                     \
  } while (0)

  FakeAsr asr;
  orator::pipeline::AsrWorker::Params params;
  params.sample_rate = 100;
  params.segment_sec = 0.0;
  params.asr_vad_gate = false;

  std::vector<std::string> events;
  std::vector<long> final_sink_ids;
  orator::pipeline::AsrWorker worker(
      &asr, params,
      [&events](const std::string& event) { events.push_back(event); },
      orator::core::TimeBase(100));
  worker.set_text_sink([&final_sink_ids](long id, double, double,
                                         const std::string&, bool final) {
    if (final) final_sink_ids.push_back(id);
  });

  const std::vector<float> audio(10, 0.25f);
  worker.ProcessSpan(audio.data(), static_cast<int>(audio.size()));
  worker.Finalize();
  worker.ProcessSpan(audio.data(), static_cast<int>(audio.size()));
  worker.Finalize();

  CHECK(final_sink_ids.size() == 2, "two final sink records emitted");
  if (final_sink_ids.size() == 2) {
    CHECK(final_sink_ids[0] == 0, "first final sink ID is zero");
    CHECK(final_sink_ids[1] == 1, "second final sink ID is one");
  }

  bool final_zero = false;
  bool final_one = false;
  for (const auto& event : events) {
    if (event.find("\"type\":\"asr\"") == std::string::npos) continue;
    if (event.find("\"text_id\":0") != std::string::npos) final_zero = true;
    if (event.find("\"text_id\":1") != std::string::npos) final_one = true;
  }
  CHECK(final_zero, "first final event reuses sink ID zero");
  CHECK(final_one, "second final event reuses sink ID one");

  {
    FakeAsr silent_asr;
    orator::pipeline::ComprehensiveTimeline evidence;
    evidence.AdvanceVadHorizon(1.0);
    params.asr_vad_gate = true;
    std::vector<std::string> silent_events;
    int silent_finals = 0;
    orator::pipeline::AsrWorker silent_worker(
        &silent_asr, params,
        [&silent_events](const std::string& event) {
          silent_events.push_back(event);
        },
        orator::core::TimeBase(100), /*stream=*/0, &evidence);
    silent_worker.set_text_sink(
        [&silent_finals](long, double, double, const std::string&, bool final) {
          if (final) ++silent_finals;
        });
    silent_worker.ProcessSpan(audio.data(), static_cast<int>(audio.size()));
    silent_worker.Finalize();

    CHECK(silent_worker.processed_samples() == static_cast<long>(audio.size()),
          "ASR accounts for silence skipped from typed VAD evidence");
    CHECK(silent_events.empty(),
          "confirmed silence from typed VAD evidence emits no ASR event");
    CHECK(silent_finals == 0,
          "confirmed silence from typed VAD evidence deposits no ASR final");
  }

  {
    FakeAsr retract_asr;
    orator::pipeline::ComprehensiveTimeline evidence;
    params.asr_vad_gate = true;
    std::vector<std::string> retract_events;
    evidence.UpdateVadState(true, 0.1);
    orator::pipeline::AsrWorker retract_worker(
        &retract_asr, params,
        [&retract_events](const std::string& event) {
          retract_events.push_back(event);
        },
        orator::core::TimeBase(100), /*stream=*/0, &evidence);
    retract_worker.set_text_sink(
        [](long, double, double, const std::string&, bool) {});
    retract_worker.ProcessSpan(audio.data(), static_cast<int>(audio.size()));
    evidence.UpdateVadState(false, 0.2);
    evidence.AdvanceVadHorizon(2.0);
    retract_worker.Finalize();

    CHECK(retract_events.size() == 2,
          "VAD-rejected provisional transcript emits partial and retract");
    if (retract_events.size() == 2) {
      CHECK(retract_events[0].find("\"type\":\"asr_partial\"") !=
                std::string::npos,
            "provisional transcript emits a partial event");
      CHECK(retract_events[1].find("\"type\":\"asr_retract\"") !=
                    std::string::npos &&
                retract_events[1].find("\"text_id\":0") != std::string::npos,
            "retract event reuses the provisional text ID");
    }
  }

  if (failures == 0) {
    std::printf("test_asr_worker PASSED\n");
    return 0;
  }
  std::printf("test_asr_worker FAILED (%d checks)\n", failures);
  return 1;
}
