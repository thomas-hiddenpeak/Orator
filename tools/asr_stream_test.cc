// asr_stream_test: drive the AuditoryStream the EXACT way the WebSocket server
// does -- frame by frame, as if audio were arriving live -- instead of handing
// the whole clip to one offline Transcribe() call. This is the real streaming
// path: diarization runs incrementally and ASR endpoints + transcribes
// utterances as they complete, both on one shared clock.
//
// Usage: asr_stream_test <audio> <asr_model_dir> [diar_weights] [start_sec]
//                        [dur_sec] [frame_ms]
//
// It prints each {"type":"asr",...} event as it is produced during streaming,
// then the final {"type":"timeline",...} fusion, plus per-pipeline real-time
// factors measured over the streamed audio.

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "io/audio_file.h"
#include "pipeline/auditory_stream.h"

using namespace orator;

int main(int argc, char** argv) {
  if (argc < 3) {
    std::fprintf(stderr,
                 "usage: %s <audio> <asr_model_dir> [diar_weights] "
                 "[start_sec] [dur_sec] [frame_ms]\n",
                 argv[0]);
    return 2;
  }
  const std::string audio_path = argv[1];
  pipeline::AuditoryStream::Config cfg;
  cfg.asr_model_dir = argv[2];
  if (argc > 3) cfg.diarizer_weights = argv[3];
  const double start_sec = argc > 4 ? std::atof(argv[4]) : 0.0;
  const double dur_sec = argc > 5 ? std::atof(argv[5]) : 0.0;  // 0 => to end
  const int frame_ms = argc > 6 ? std::atoi(argv[6]) : 100;    // stream granularity

  std::printf(">> decoding %s to mono 16k PCM ...\n", audio_path.c_str());
  io::AudioData a = io::LoadAudioMono(audio_path, cfg.sample_rate);
  const int sr = a.sample_rate;
  long begin = static_cast<long>(start_sec * sr);
  long end = dur_sec > 0 ? begin + static_cast<long>(dur_sec * sr)
                         : static_cast<long>(a.samples.size());
  begin = std::max(0L, std::min(begin, static_cast<long>(a.samples.size())));
  end = std::max(begin, std::min(end, static_cast<long>(a.samples.size())));
  const float* pcm = a.samples.data() + begin;
  const long total = end - begin;
  const double clip_sec = static_cast<double>(total) / sr;
  std::printf("   streaming [%.1f, %.1f) = %ld samples (%.2fs) in %dms frames\n",
              start_sec, start_sec + clip_sec, total, clip_sec, frame_ms);

  std::printf(">> loading pipelines (diar + asr) ...\n");
  pipeline::AuditoryStream stream(cfg, [](const std::string& json) {
    // Print incremental ASR events as they stream; the final timeline too.
    if (json.rfind("{\"type\":\"asr\"", 0) == 0)
      std::printf("   [stream] %s\n", json.c_str());
  });
  stream.Start();

  std::printf(">> streaming ...\n");
  const int frame = std::max(1, sr * frame_ms / 1000);
  auto t0 = std::chrono::steady_clock::now();
  for (long off = 0; off < total; off += frame) {
    const int n = static_cast<int>(std::min<long>(frame, total - off));
    stream.PushAudio(pcm + off, n);
  }
  stream.EmitTimeline(/*finalize=*/true);
  auto t1 = std::chrono::steady_clock::now();
  const double wall = std::chrono::duration<double>(t1 - t0).count();

  // Final unified timeline (read back from the stream's accumulated results).
  std::printf("\n===== UNIFIED TIMELINE =====\n");
  std::printf("-- diarization (%zu segments) --\n", stream.diar_segments().size());
  for (const auto& s : stream.diar_segments()) {
    std::printf("  [%6.2f-%6.2f] spk%d (conf %.2f)\n", s.start_sec, s.end_sec,
                s.local_speaker, s.confidence);
  }
  std::printf("-- transcript (%zu utterances) --\n", stream.transcript().tokens.size());
  for (const auto& t : stream.transcript().tokens) {
    const int m = static_cast<int>(t.start_sec) / 60, s = static_cast<int>(t.start_sec) % 60;
    std::printf("  [%02d:%02d] %s\n", m, s, t.text.c_str());
  }
  std::printf("============================\n");

  const double diar_c = stream.diar_compute_sec();
  const double asr_c = stream.asr_compute_sec();
  std::printf("audio=%.2fs  wall=%.2fs (%.2fx)\n", clip_sec, wall,
              wall > 0 ? clip_sec / wall : 0.0);
  std::printf("  diarization compute=%.2fs (%.2fx)   asr compute=%.2fs (%.2fx)\n",
              diar_c, diar_c > 0 ? clip_sec / diar_c : 0.0, asr_c,
              asr_c > 0 ? clip_sec / asr_c : 0.0);
  std::printf("  (the two pipelines are independent; wall reflects running them "
              "sequentially in one thread)\n");
  return 0;
}
