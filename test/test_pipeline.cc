#include <cassert>
#include <cmath>
#include <fstream>
#include <iostream>
#include <vector>

#include "core/registry.h"
#include "io/json_sink.h"
#include "model/builtin_registration.h"
#include "pipeline/auditory_pipeline.h"

using namespace orator;

// Build a synthetic 4-second mono signal: 2s of tone, 2s of tone (energy
// present throughout) so the stub diarizer emits activity.
static std::vector<float> MakeSignal(int sample_rate, double seconds) {
  const int n = static_cast<int>(sample_rate * seconds);
  std::vector<float> sig(static_cast<size_t>(n));
  for (int i = 0; i < n; ++i) {
    sig[i] = 0.2f * std::sin(2.0 * 3.14159265 * 220.0 * i / sample_rate);
  }
  return sig;
}

int main() {
  std::cout << "Testing end-to-end auditory pipeline..." << std::endl;

  model::EnsureBuiltinsRegistered();

  // Verify the registry exposes swappable diarizers (decoupling check).
  auto& diar_reg = core::Registry<core::IDiarizer>::Instance();
  assert(diar_reg.Contains("stub"));
  assert(diar_reg.Contains("sortformer"));
  std::cout << "Registry exposes stub + sortformer diarizers" << std::endl;

  // Run the SAME pipeline config against BOTH diarizers to prove the consumer
  // code is identical regardless of model. The real sortformer requires its
  // safetensors weights; if they are not present, skip that iteration so the
  // decoupling test stays self-contained.
  const std::string kCandidates[] = {
      "models/sortformer_4spk_v2.safetensors",
      "../models/sortformer_4spk_v2.safetensors",
      "../../models/sortformer_4spk_v2.safetensors"};
  std::string kSortformerWeights;
  for (const auto& c : kCandidates)
    if (std::ifstream(c).good()) {
      kSortformerWeights = c;
      break;
    }
  bool have_weights = !kSortformerWeights.empty();
  for (const std::string& model_key : {std::string("stub"),
                                       std::string("sortformer")}) {
    if (model_key == "sortformer" && !have_weights) {
      std::cout << "[sortformer] weights not found, skipping real-model path"
                << std::endl;
      continue;
    }
    pipeline::PipelineConfig cfg;
    cfg.diarizer = model_key;
    cfg.sample_rate = 16000;
    cfg.max_speakers = 4;
    if (model_key == "sortformer") cfg.diarizer_weights = kSortformerWeights;

    auto pipe = pipeline::AuditoryPipeline::FromConfig(cfg, nullptr);
    pipe->Start();

    const int sr = cfg.sample_rate;
    auto signal = MakeSignal(sr, 4.0);
    // Feed in 1-second chunks with correct absolute timing.
    for (int c = 0; c < 4; ++c) {
      core::AudioChunk chunk;
      chunk.samples = signal.data() + static_cast<size_t>(c) * sr;
      chunk.num_samples = sr;
      chunk.sample_rate = sr;
      chunk.t_start_sec = static_cast<double>(c);
      pipe->ProcessAudio(chunk);
    }

    // The always-on stub guarantees activity on the synthetic tone; the real
    // sortformer correctly reports little/no speaker activity on a pure sine
    // (non-speech), so segment presence is only asserted for the stub. The
    // decoupling guarantee under test is that identical consumer code drives
    // both models and yields a well-formed timeline + JSON.
    const bool strict = (model_key == "stub");
    if (strict) assert(!pipe->diar_segments().empty());

    // Provide an ASR transcript and fuse.
    core::Transcript asr;
    asr.tokens.push_back({0.2, 1.0, "hello"});
    asr.tokens.push_back({1.0, 2.0, "world"});
    asr.tokens.push_back({2.2, 3.0, "foo"});
    asr.tokens.push_back({3.0, 3.8, "bar"});

    core::Timeline tl = pipe->Finalize(asr);
    if (strict) {
      assert(!tl.segments.empty());
      // Every token must be accounted for in the timeline text.
      std::string all;
      for (const auto& s : tl.segments) {
        if (!all.empty()) all += " ";
        all += s.text;
      }
      assert(all.find("hello") != std::string::npos);
      assert(all.find("bar") != std::string::npos);
    }

    std::string json = io::TimelineToJson(tl, false);
    assert(json.find("\"segments\"") != std::string::npos);
    assert(json.find("\"speaker_id\"") != std::string::npos);

    std::cout << "[" << model_key << "] segments=" << tl.segments.size()
              << " json_ok" << std::endl;
  }

  std::cout << "\nAll pipeline tests passed!" << std::endl;
  return 0;
}
