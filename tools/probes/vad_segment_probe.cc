// Export production GpuVad speech intervals without product evaluation.

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <stdexcept>
#include <string>
#include <vector>

#include "core/stages.h"
#include "io/audio_file.h"
#include "io/config_reader.h"
#include "pipeline/auditory_stream.h"
#include "pipeline/gpu_vad.h"

int main(int argc, char** argv) {
  if (argc != 4) {
    std::fprintf(stderr,
                 "usage: %s <audio.wav/mp3> <config.toml> <out.tsv>\n",
                 argv[0]);
    return 2;
  }
  try {
    const std::string audio_path = argv[1];
    const std::string config_path = argv[2];
    const std::string output_path = argv[3];
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
    orator::pipeline::GpuVad vad(params);

    std::vector<orator::core::VadSegmentResult> all_segments;
    std::vector<orator::core::VadSegmentResult> drained;
    const int chunk_samples = config.sample_rate;
    for (size_t offset = 0; offset < audio.samples.size();) {
      const int count = static_cast<int>(std::min(
          static_cast<size_t>(chunk_samples), audio.samples.size() - offset));
      vad.Push(audio.samples.data() + offset, count);
      drained.clear();
      vad.DrainSegments(false, &drained);
      all_segments.insert(all_segments.end(), drained.begin(), drained.end());
      offset += static_cast<size_t>(count);
    }
    drained.clear();
    vad.DrainSegments(true, &drained);
    all_segments.insert(all_segments.end(), drained.begin(), drained.end());

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
    for (size_t index = 0; index < all_segments.size(); ++index) {
      const auto& segment = all_segments[index];
      const double start = static_cast<double>(segment.start_sample) /
                           config.sample_rate;
      const double end =
          static_cast<double>(segment.end_sample) / config.sample_rate;
      output << "vad:" << index << '\t' << start << '\t' << end << '\t'
             << (end - start) << '\n';
    }
    std::printf("config=%s audio=%s segments=%zu out=%s\n",
                config_path.c_str(), audio_path.c_str(), all_segments.size(),
                output_path.c_str());
    return 0;
  } catch (const std::exception& error) {
    std::fprintf(stderr, "vad segment probe: %s\n", error.what());
    return 1;
  }
}
