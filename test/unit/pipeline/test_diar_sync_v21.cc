// Numerical gate for the native v2.1 synchronous Sortformer profile.
//
// The 502-frame NeMo fixture spans three 188-frame chunks. The third chunk
// therefore observes the speaker cache after its first compression, covering
// more than a single forward or cache append.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "core/stages.h"
#include "model/streaming_sortformer.h"

namespace {

std::vector<float> ReadF32(const std::string& path) {
  std::ifstream input(path, std::ios::binary | std::ios::ate);
  if (!input) throw std::runtime_error("cannot open " + path);
  const std::streamsize bytes = input.tellg();
  input.seekg(0);
  std::vector<float> values(static_cast<size_t>(bytes) / sizeof(float));
  input.read(reinterpret_cast<char*>(values.data()), bytes);
  return values;
}

}  // namespace

int main() {
  constexpr char kReferenceDir[] = "models/reference";
  constexpr int kSpeakerCount = 4;
  constexpr double kTolerance = 1e-5;

  try {
    const std::vector<float> processed =
        ReadF32(std::string(kReferenceDir) + "/ref_stream_proc.f32");
    const std::vector<float> reference =
        ReadF32(std::string(kReferenceDir) + "/ref_stream_sync_v21.f32");
    int32_t metadata_values[3] = {0, 0, 0};
    std::ifstream metadata(std::string(kReferenceDir) + "/ref_stream_meta.i32",
                           std::ios::binary);
    if (!metadata) throw std::runtime_error("cannot open stream metadata");
    metadata.read(reinterpret_cast<char*>(metadata_values),
                  sizeof(metadata_values));

    const int mel_frames = metadata_values[0];
    const int valid_mel_frames = metadata_values[1];
    if (processed.size() != static_cast<size_t>(128 * mel_frames)) {
      throw std::runtime_error("processed-signal shape mismatch");
    }
    if (reference.size() % kSpeakerCount != 0) {
      throw std::runtime_error("oracle probability shape mismatch");
    }

    orator::model::SortformerDiarizer diarizer;
    orator::model::SortformerTuning tuning;
    tuning.spkcache_len = 188;
    tuning.fifo_len = 0;
    tuning.chunk_len = 188;
    tuning.spkcache_update_period = 188;
    tuning.chunk_left_context = 1;
    tuning.chunk_right_context = 1;
    tuning.spkcache_sil_frames = 3;
    diarizer.ApplyStreamingTuning(tuning);

    orator::core::DiarizationConfig config;
    config.sample_rate = 16000;
    config.max_speakers = kSpeakerCount;
    diarizer.Initialize(config);
    diarizer.LoadWeights("models/sortformer_4spk_v2.1.safetensors");

    const orator::core::DiarizationFrames output = diarizer.RunStreaming(
        processed.data(), 128, mel_frames, valid_mel_frames, 0.0);
    const int reference_frames =
        static_cast<int>(reference.size() / kSpeakerCount);
    if (output.num_frames != reference_frames) {
      std::printf("FAIL: sync v2.1 frame count ours=%d NeMo=%d\n",
                  output.num_frames, reference_frames);
      return 1;
    }

    double max_abs = 0.0;
    double sum_abs = 0.0;
    int argmax_matches = 0;
    for (int frame = 0; frame < reference_frames; ++frame) {
      int output_top = 0;
      int reference_top = 0;
      for (int speaker = 0; speaker < kSpeakerCount; ++speaker) {
        const size_t index =
            static_cast<size_t>(frame) * kSpeakerCount + speaker;
        const double difference =
            std::fabs(static_cast<double>(output.probs[index]) -
                      static_cast<double>(reference[index]));
        max_abs = std::max(max_abs, difference);
        sum_abs += difference;
        if (output.probs[index] >
            output.probs[static_cast<size_t>(frame) * kSpeakerCount +
                         output_top]) {
          output_top = speaker;
        }
        if (reference[index] >
            reference[static_cast<size_t>(frame) * kSpeakerCount +
                      reference_top]) {
          reference_top = speaker;
        }
      }
      if (output_top == reference_top) ++argmax_matches;
    }

    const double mean_abs = sum_abs / static_cast<double>(reference.size());
    std::printf(
        "sync v2.1 Sortformer vs NeMo: frames=%d max_abs=%.6g "
        "mean_abs=%.6g argmax=%d/%d (tol %.0e)\n",
        reference_frames, max_abs, mean_abs, argmax_matches, reference_frames,
        kTolerance);
    if (max_abs < kTolerance) {
      std::printf("test_diar_sync_v21 PASSED\n");
      return 0;
    }
    std::printf("test_diar_sync_v21 FAILED\n");
    return 1;
  } catch (const std::exception& error) {
    std::printf("FAIL: %s\n", error.what());
    return 1;
  }
}
