#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

#include "core/registry.h"
#include "model/builtin_registration.h"
#include "model/stub_asr.h"
#include "pipeline/auditory_pipeline.h"

using namespace orator;

// Voiced tone for [start,end) seconds, silence elsewhere, within a `seconds`
// long buffer.
static std::vector<float> MakeVoiced(int sr, double seconds, double start,
                                     double end) {
  const int n = static_cast<int>(sr * seconds);
  std::vector<float> sig(static_cast<size_t>(n), 0.0f);
  const int a = static_cast<int>(sr * start);
  const int b = std::min(n, static_cast<int>(sr * end));
  for (int i = a; i < b; ++i) {
    sig[i] = 0.3f * std::sin(2.0 * 3.14159265 * 220.0 * i / sr);
  }
  return sig;
}

int main() {
  std::cout << "Testing ASR stage + unified timeline..." << std::endl;

  model::EnsureBuiltinsRegistered();

  // 1) Registry exposes a swappable ASR engine (decoupling check).
  auto& asr_reg = core::Registry<core::IAsr>::Instance();
  assert(asr_reg.Contains("stub"));
  std::cout << "Registry exposes stub ASR" << std::endl;

  const int sr = 16000;

  // 2) StubAsr emits a timed token over voiced audio and nothing over silence.
  {
    model::StubAsr asr;
    core::AsrConfig cfg;
    cfg.sample_rate = sr;
    asr.Initialize(cfg);

    auto voiced = MakeVoiced(sr, 3.0, 1.0, 2.0);  // speech in [1,2)
    core::AudioChunk chunk;
    chunk.samples = voiced.data();
    chunk.num_samples = static_cast<int>(voiced.size());
    chunk.sample_rate = sr;
    chunk.t_start_sec = 5.0;  // absolute offset must propagate
    core::Transcript t = asr.Transcribe(chunk);
    assert(!t.tokens.empty());
    // Token timing should be anchored to the absolute stream time.
    assert(t.tokens.front().start_sec >= 5.0);
    assert(t.tokens.back().end_sec <= 8.01);
    std::cout << "StubAsr tokens=" << t.tokens.size()
              << " first=[" << t.tokens.front().start_sec << ","
              << t.tokens.front().end_sec << "]" << std::endl;

    std::vector<float> silence(static_cast<size_t>(sr), 0.0f);
    core::AudioChunk sil;
    sil.samples = silence.data();
    sil.num_samples = static_cast<int>(silence.size());
    sil.sample_rate = sr;
    sil.t_start_sec = 0.0;
    assert(asr.Transcribe(sil).tokens.empty());
  }

  // 3) Pipeline-owned ASR: a single Finalize() yields a unified, speaker-
  //    attributed, transcribed timeline using only the stub components.
  {
    pipeline::PipelineConfig cfg;
    cfg.diarizer = "stub";
    cfg.asr = "stub";
    cfg.sample_rate = sr;
    cfg.max_speakers = 4;

    auto pipe = pipeline::AuditoryPipeline::FromConfig(cfg, nullptr);
    pipe->Start();

    auto signal = MakeVoiced(sr, 4.0, 0.0, 4.0);  // voiced throughout
    for (int c = 0; c < 4; ++c) {
      core::AudioChunk chunk;
      chunk.samples = signal.data() + static_cast<size_t>(c) * sr;
      chunk.num_samples = sr;
      chunk.sample_rate = sr;
      chunk.t_start_sec = static_cast<double>(c);
      pipe->ProcessAudio(chunk);
    }

    core::Timeline timeline = pipe->Finalize();
    assert(!pipe->transcript().tokens.empty());
    assert(!timeline.segments.empty());
    for (const auto& seg : timeline.segments) {
      assert(!seg.speaker_id.empty());
      assert(seg.end_sec >= seg.start_sec);
    }
    std::cout << "Unified timeline segments=" << timeline.segments.size()
              << " (asr tokens=" << pipe->transcript().tokens.size() << ")"
              << std::endl;
  }

  std::cout << "ASR + unified timeline test PASSED" << std::endl;
  return 0;
}
