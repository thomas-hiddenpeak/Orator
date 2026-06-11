// Orator MVP entrypoint.
//
// Demonstrates the full decoupled auditory pipeline end to end:
//   audio chunks -> diarizer (swappable) -> speaker segments
//                -> (optional) speaker-id resolution via registry
//                -> timeline fusion with ASR -> JSON for the LLM consumer
//
// The diarizer is selected by name from the registry, proving model decoupling:
// switching "stub" <-> "sortformer" changes no consumer code.

#include <cmath>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "core/registry.h"
#include "gpu/memory.h"
#include "io/json_sink.h"
#include "model/builtin_registration.h"
#include "model/speaker_database.h"
#include "pipeline/auditory_pipeline.h"

using namespace orator;

namespace {

std::vector<float> SynthesizeSpeech(int sample_rate, double seconds, double freq) {
  const int n = static_cast<int>(sample_rate * seconds);
  std::vector<float> sig(static_cast<size_t>(n));
  for (int i = 0; i < n; ++i) {
    sig[i] = 0.25f * std::sin(2.0 * 3.14159265 * freq * i / sample_rate);
  }
  return sig;
}

std::string PickDiarizer(int argc, char** argv) {
  for (int i = 1; i + 1 < argc; ++i) {
    if (std::string(argv[i]) == "--diarizer") return argv[i + 1];
  }
  return "stub";
}

}  // namespace

int main(int argc, char** argv) {
  try {
    if (gpu::GpuMemory::GetDeviceCount() == 0) {
      std::cerr << "No CUDA-capable devices found!" << std::endl;
      return 1;
    }
    gpu::GpuMemory::SetDevice(0);

    model::EnsureBuiltinsRegistered();

    const std::string diarizer = PickDiarizer(argc, argv);
    std::cout << "=== Orator auditory pipeline (diarizer=" << diarizer
              << ") ===" << std::endl;

    std::cout << "Available diarizers:";
    for (const auto& k : core::Registry<core::IDiarizer>::Instance().Keys()) {
      std::cout << " " << k;
    }
    std::cout << std::endl;

    // Optional speaker registry (1:N matching) shared with the pipeline.
    auto registry = std::make_shared<model::SpeakerDatabase>(2000, 64);

    pipeline::PipelineConfig cfg;
    cfg.diarizer = diarizer;
    cfg.embedder = "stub_embedder";  // enable speaker-id resolution
    cfg.sample_rate = 16000;
    cfg.max_speakers = 4;

    auto pipe = pipeline::AuditoryPipeline::FromConfig(cfg, registry);
    pipe->Start();

    // Stream 4 seconds of synthetic audio in 1-second chunks.
    const int sr = cfg.sample_rate;
    auto signal = SynthesizeSpeech(sr, 4.0, 220.0);
    for (int c = 0; c < 4; ++c) {
      core::AudioChunk chunk;
      chunk.samples = signal.data() + static_cast<size_t>(c) * sr;
      chunk.num_samples = sr;
      chunk.sample_rate = sr;
      chunk.t_start_sec = static_cast<double>(c);
      pipe->ProcessAudio(chunk);
    }
    std::cout << "Diarization segments: " << pipe->diar_segments().size()
              << std::endl;

    // Upstream ASR result (would come from the ASR system in production).
    core::Transcript asr;
    asr.tokens.push_back({0.2, 1.0, "hello"});
    asr.tokens.push_back({1.0, 2.0, "everyone"});
    asr.tokens.push_back({2.2, 3.0, "lets"});
    asr.tokens.push_back({3.0, 3.8, "begin"});

    core::Timeline timeline = pipe->Finalize(asr);

    std::cout << "\n--- Timeline (JSON for LLM consumer) ---" << std::endl;
    io::JsonSink sink(std::cout, /*pretty=*/true);
    sink.Consume(timeline);

    std::cout << "\n=== Done ===" << std::endl;
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }
}
