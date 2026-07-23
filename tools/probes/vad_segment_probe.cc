// Export production GpuVad speech intervals without product evaluation.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include "core/stages.h"
#include "core/time_base.h"
#include "io/audio_file.h"
#include "io/config_reader.h"
#include "pipeline/auditory_stream.h"
#include "pipeline/gpu_vad.h"

namespace {

using orator::core::VadStateResult;

constexpr long kVadWindowSamples = 512;

struct EndpointObservation {
  VadStateResult state;
  bool final = false;
};

double SecondsAt(long sample, const orator::core::TimeBase& time_base) {
  return sample < 0 ? -1.0 : time_base.SecondsAt(sample);
}

void WriteWindowProbabilities(const std::string& path,
                              const std::vector<float>& probabilities,
                              int sample_rate) {
  const orator::core::TimeBase time_base(sample_rate);
  std::ofstream output(path);
  if (!output) {
    throw std::runtime_error("cannot write VAD window evidence: " + path);
  }
  output << "evidence_id\tstart_sample\tend_sample\tstart_sec\tend_sec\t"
            "speech_probability\n";
  output << std::fixed << std::setprecision(9);
  for (size_t index = 0; index < probabilities.size(); ++index) {
    const float probability = probabilities[index];
    if (!std::isfinite(probability) || probability < 0.0f ||
        probability > 1.0f) {
      throw std::runtime_error("invalid VAD window probability");
    }
    const long start = static_cast<long>(index) * kVadWindowSamples;
    const long end = start + kVadWindowSamples;
    output << "vad_window:" << index << '\t' << start << '\t' << end << '\t'
           << SecondsAt(start, time_base) << '\t'
           << SecondsAt(end, time_base) << '\t' << probability << '\n';
  }
}

void ValidateEndpointState(const VadStateResult& state) {
  if (state.observed_until_sample < 0) {
    throw std::runtime_error("negative VAD observed frontier");
  }
  if (state.active_start_sample > state.observed_until_sample ||
      state.active_stable_until_sample > state.observed_until_sample ||
      state.silence_stable_until_sample > state.observed_until_sample) {
    throw std::runtime_error("VAD state exceeds observed frontier");
  }
  if (state.in_speech &&
      (state.active_start_sample < 0 ||
       state.active_stable_until_sample < state.active_start_sample)) {
    throw std::runtime_error("invalid active VAD frontier");
  }
}

void WriteEndpointStates(const std::string& path,
                         const std::vector<EndpointObservation>& observations,
                         int sample_rate) {
  const orator::core::TimeBase time_base(sample_rate);
  std::ofstream output(path);
  if (!output) {
    throw std::runtime_error("cannot write VAD endpoint evidence: " + path);
  }
  output << "evidence_id\tfinal\tin_speech\tobserved_until_sample\t"
            "observed_until_sec\tactive_start_sample\tactive_start_sec\t"
            "active_stable_until_sample\tactive_stable_until_sec\t"
            "silence_stable_until_sample\tsilence_stable_until_sec\n";
  output << std::fixed << std::setprecision(9);
  for (size_t index = 0; index < observations.size(); ++index) {
    const auto& observation = observations[index];
    const auto& state = observation.state;
    ValidateEndpointState(state);
    output << "vad_state:" << index << '\t'
           << (observation.final ? "true" : "false") << '\t'
           << (state.in_speech ? "true" : "false") << '\t'
           << state.observed_until_sample << '\t'
           << SecondsAt(state.observed_until_sample, time_base) << '\t'
           << state.active_start_sample << '\t'
           << SecondsAt(state.active_start_sample, time_base) << '\t'
           << state.active_stable_until_sample << '\t'
           << SecondsAt(state.active_stable_until_sample, time_base) << '\t'
           << state.silence_stable_until_sample << '\t'
           << SecondsAt(state.silence_stable_until_sample, time_base) << '\n';
  }
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 4 && argc != 6) {
    std::fprintf(stderr,
                 "usage: %s <audio.wav/mp3> <config.toml> <segments.tsv> "
                 "[window_probabilities.tsv endpoint_states.tsv]\n",
                 argv[0]);
    return 2;
  }
  try {
    const std::string audio_path = argv[1];
    const std::string config_path = argv[2];
    const std::string output_path = argv[3];
    const bool capture_raw_evidence = argc == 6;
    orator::pipeline::AuditoryStream::Config config;
    if (!orator::io::ApplyTomlConfig(config_path, config)) {
      throw std::runtime_error("cannot load config: " + config_path);
    }
    const auto audio =
        orator::io::LoadAudioMono(audio_path, config.sample_rate);

    orator::pipeline::GpuVad::Params params;
    params.sample_rate = config.sample_rate;
    params.silero_model_path = config.vad_model;
    params.silero_threshold = config.vad_threshold;
    params.silero_min_speech_ms = config.vad_min_speech_ms;
    params.silero_min_silence_ms = config.vad_min_silence_ms;
    params.silero_speech_pad_ms = config.vad_speech_pad_ms;

    std::vector<float> window_probabilities;
    if (capture_raw_evidence) {
      if (audio.samples.size() >
          static_cast<size_t>(std::numeric_limits<int>::max())) {
        throw std::runtime_error("audio is too large for VAD evidence capture");
      }
      orator::pipeline::GpuVad probability_vad(params);
      window_probabilities = probability_vad.DebugWindowProbs(
          audio.samples.data(), static_cast<int>(audio.samples.size()));
      const size_t expected_windows =
          audio.samples.size() / static_cast<size_t>(kVadWindowSamples);
      if (window_probabilities.size() != expected_windows) {
        throw std::runtime_error("VAD probability window count mismatch");
      }
    }

    orator::pipeline::GpuVad vad(params);

    std::vector<orator::core::VadSegmentResult> all_segments;
    std::vector<orator::core::VadSegmentResult> drained;
    std::vector<EndpointObservation> endpoint_observations;
    const int chunk_samples = config.sample_rate;
    for (size_t offset = 0; offset < audio.samples.size();) {
      const int count = static_cast<int>(std::min(
          static_cast<size_t>(chunk_samples), audio.samples.size() - offset));
      vad.Push(audio.samples.data() + offset, count);
      drained.clear();
      vad.DrainSegments(false, &drained);
      all_segments.insert(all_segments.end(), drained.begin(), drained.end());
      if (capture_raw_evidence) {
        endpoint_observations.push_back({vad.state(), false});
      }
      offset += static_cast<size_t>(count);
    }
    drained.clear();
    vad.DrainSegments(true, &drained);
    all_segments.insert(all_segments.end(), drained.begin(), drained.end());
    if (capture_raw_evidence) {
      endpoint_observations.push_back({vad.state(), true});
    }

    long prior_end = 0;
    for (const auto& segment : all_segments) {
      if (segment.start_sample < prior_end ||
          segment.end_sample <= segment.start_sample ||
          segment.end_sample > static_cast<long>(audio.samples.size())) {
        throw std::runtime_error("VAD output violates time-base contract");
      }
      prior_end = segment.end_sample;
    }

    std::ofstream output(output_path);
    if (!output) {
      throw std::runtime_error("cannot write output: " + output_path);
    }
    output << "evidence_id\tstart_sec\tend_sec\tduration_sec\n";
    output << std::fixed << std::setprecision(6);
    const orator::core::TimeBase time_base(config.sample_rate);
    for (size_t index = 0; index < all_segments.size(); ++index) {
      const auto& segment = all_segments[index];
      const double start = time_base.SecondsAt(segment.start_sample);
      const double end = time_base.SecondsAt(segment.end_sample);
      output << "vad:" << index << '\t' << start << '\t' << end << '\t'
             << time_base.Duration(segment.end_sample - segment.start_sample)
             << '\n';
    }
    if (capture_raw_evidence) {
      WriteWindowProbabilities(argv[4], window_probabilities,
                               config.sample_rate);
      WriteEndpointStates(argv[5], endpoint_observations, config.sample_rate);
    }
    if (capture_raw_evidence) {
      std::printf(
          "config=%s audio=%s segments=%zu windows=%zu states=%zu out=%s\n",
          config_path.c_str(), audio_path.c_str(), all_segments.size(),
          window_probabilities.size(), endpoint_observations.size(),
          output_path.c_str());
    } else {
      std::printf("config=%s audio=%s segments=%zu out=%s\n",
                  config_path.c_str(), audio_path.c_str(), all_segments.size(),
                  output_path.c_str());
    }
    return 0;
  } catch (const std::exception& error) {
    std::fprintf(stderr, "vad segment probe: %s\n", error.what());
    return 1;
  }
}
