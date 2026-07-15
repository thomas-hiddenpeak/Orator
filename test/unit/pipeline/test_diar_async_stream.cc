// Numerical gate for parameterized asynchronous Sortformer FIFO profiles.
// CTest runs the inherited Orator profile and both NVIDIA v2.1 published
// profiles. Each fixture repeats NeMo's valid processed signal three times and
// crosses repeated output/cache updates. The fixtures were generated from
// NVIDIA's local v2.1 checkpoint; the source .nemo SHA-256 is
// 8abd32832159c6ac1148c926b7276f35ba34582c444e559dce1f1253fea42ef8,
// the runtime safetensors SHA-256 is
// d036020b6b93977098929d417b1b106a952ec02cc38cafc9d3315ae0ec4d90b8,
// and the default inherited-profile raw oracle fixture SHA-256 is
// 2635b09033153aba85a413e514899238f629665bc5d08e3ec6b5b72ce9a4699e.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "core/stages.h"
#include "io/config_reader.h"
#include "model/streaming_sortformer.h"
#include "pipeline/auditory_stream.h"

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

std::vector<float> RepeatValidFrames(const std::vector<float>& input,
                                     int channels, int mel_frames,
                                     int valid_frames, int repetitions,
                                     int* output_mel_frames,
                                     int* output_valid_frames) {
  if (channels <= 0 || valid_frames <= 0 || mel_frames < valid_frames ||
      repetitions < 1) {
    throw std::runtime_error("invalid processed-signal repeat shape");
  }
  const int padding_frames = mel_frames - valid_frames;
  *output_valid_frames = valid_frames * repetitions;
  *output_mel_frames = *output_valid_frames + padding_frames;
  std::vector<float> output(static_cast<size_t>(channels) * *output_mel_frames);
  for (int channel = 0; channel < channels; ++channel) {
    const float* source =
        input.data() + static_cast<size_t>(channel) * mel_frames;
    float* destination =
        output.data() + static_cast<size_t>(channel) * *output_mel_frames;
    for (int repetition = 0; repetition < repetitions; ++repetition) {
      std::copy_n(source, valid_frames,
                  destination + static_cast<size_t>(repetition) * valid_frames);
    }
    std::copy_n(source + valid_frames, padding_frames,
                destination + *output_valid_frames);
  }
  return output;
}

}  // namespace

int main(int argc, char** argv) {
  constexpr char kReferenceDir[] = "models/reference";
  constexpr int kSpeakerCount = 4;
  constexpr int kProcessedChannels = 128;
  constexpr double kTolerance = 1e-5;

  try {
    const std::string config_path =
        argc > 1 ? argv[1] : "orator.toml";
    const std::string reference_path =
        argc > 2
            ? argv[2]
            : std::string(kReferenceDir) +
                  "/ref_stream_async_v21_long.f32";
    const int repetitions = argc > 3 ? std::stoi(argv[3]) : 3;
    if (repetitions < 1) {
      throw std::runtime_error("repetitions must be positive");
    }
    const std::vector<float> base_processed =
        ReadF32(std::string(kReferenceDir) + "/ref_stream_proc.f32");
    const std::vector<float> reference = ReadF32(reference_path);
    int32_t meta[3] = {0, 0, 0};
    std::ifstream metadata(std::string(kReferenceDir) + "/ref_stream_meta.i32",
                           std::ios::binary);
    if (!metadata) throw std::runtime_error("cannot open stream metadata");
    metadata.read(reinterpret_cast<char*>(meta), sizeof(meta));

    const int base_mel_frames = meta[0];
    const int base_valid_mel_frames = meta[1];
    if (base_processed.size() !=
        static_cast<size_t>(kProcessedChannels * base_mel_frames)) {
      throw std::runtime_error("processed-signal shape mismatch");
    }
    if (reference.size() % kSpeakerCount != 0) {
      throw std::runtime_error("oracle probability shape mismatch");
    }

    orator::pipeline::AuditoryStream::Config runtime_config;
    if (!orator::io::ApplyTomlConfig(config_path, runtime_config)) {
      throw std::runtime_error("cannot load " + config_path);
    }
    int mel_frames = 0;
    int valid_mel_frames = 0;
    const std::vector<float> processed = RepeatValidFrames(
        base_processed, kProcessedChannels, base_mel_frames,
        base_valid_mel_frames, repetitions, &mel_frames, &valid_mel_frames);

    orator::model::SortformerDiarizer diarizer;
    orator::model::SortformerTuning tuning;
    tuning.spkcache_len = runtime_config.diar_spkcache_len;
    tuning.fifo_len = runtime_config.diar_fifo_len;
    tuning.chunk_len = runtime_config.diar_chunk_len;
    tuning.spkcache_update_period = runtime_config.diar_spkcache_update_period;
    tuning.chunk_left_context = runtime_config.diar_chunk_left_context;
    tuning.chunk_right_context = runtime_config.diar_chunk_right_context;
    tuning.spkcache_sil_frames = runtime_config.diar_spkcache_sil_frames;
    diarizer.ApplyStreamingTuning(tuning);

    orator::core::DiarizationConfig config;
    config.sample_rate = 16000;
    config.max_speakers = runtime_config.max_speakers;
    diarizer.Initialize(config);
    bool rejected_late_tuning = false;
    try {
      diarizer.ApplyStreamingTuning(tuning);
    } catch (const std::logic_error&) {
      rejected_late_tuning = true;
    }
    if (!rejected_late_tuning) {
      throw std::runtime_error(
          "Sortformer accepted streaming tuning after initialization");
    }
    diarizer.LoadWeights(runtime_config.diarizer_weights);

    const orator::core::DiarizationFrames output = diarizer.RunStreaming(
        processed.data(), 128, mel_frames, valid_mel_frames, 0.0);
    const int reference_frames =
        static_cast<int>(reference.size() / kSpeakerCount);
    if (tuning.fifo_len <= 0 || reference_frames <= 2 * tuning.chunk_len) {
      throw std::runtime_error(
          "oracle fixture does not cross repeated async cache updates");
    }
    if (output.num_frames != reference_frames) {
      std::printf("FAIL: runtime async frame count ours=%d NeMo=%d\n",
                  output.num_frames, reference_frames);
      return 1;
    }

    double max_abs = 0.0;
    double sum_abs = 0.0;
    int argmax_matches = 0;
    int significant_argmax_mismatches = 0;
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
      if (output_top == reference_top) {
        ++argmax_matches;
      } else {
        double reference_top_value = reference[static_cast<size_t>(frame) *
                                               kSpeakerCount + reference_top];
        double reference_output_top_value =
            reference[static_cast<size_t>(frame) * kSpeakerCount + output_top];
        const double reference_margin =
            reference_top_value - reference_output_top_value;
        if (reference_margin > 2.0 * kTolerance) {
          ++significant_argmax_mismatches;
        }
        std::printf(
            "argmax mismatch frame=%d ours=%d NeMo=%d "
            "reference_margin=%.6g ours=[%.9g %.9g %.9g %.9g] "
            "NeMo=[%.9g %.9g %.9g %.9g]\n",
            frame, output_top, reference_top, reference_margin,
            output.probs[static_cast<size_t>(frame) * kSpeakerCount],
            output.probs[static_cast<size_t>(frame) * kSpeakerCount + 1],
            output.probs[static_cast<size_t>(frame) * kSpeakerCount + 2],
            output.probs[static_cast<size_t>(frame) * kSpeakerCount + 3],
            reference[static_cast<size_t>(frame) * kSpeakerCount],
            reference[static_cast<size_t>(frame) * kSpeakerCount + 1],
            reference[static_cast<size_t>(frame) * kSpeakerCount + 2],
            reference[static_cast<size_t>(frame) * kSpeakerCount + 3]);
      }
    }

    const double mean_abs = sum_abs / static_cast<double>(reference.size());
    std::printf(
        "runtime async Sortformer vs NeMo: frames=%d max_abs=%.6g "
        "mean_abs=%.6g "
        "argmax=%d/%d (tol %.0e)\n",
        reference_frames, max_abs, mean_abs, argmax_matches, reference_frames,
        kTolerance);
    if (max_abs < kTolerance && significant_argmax_mismatches == 0) {
      std::printf("test_diar_async_stream PASSED\n");
      return 0;
    }
    std::printf("test_diar_async_stream FAILED\n");
    return 1;
  } catch (const std::exception& error) {
    std::printf("FAIL: %s\n", error.what());
    return 1;
  }
}
