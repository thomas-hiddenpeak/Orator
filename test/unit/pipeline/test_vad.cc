// test_vad: numeric gate for the GPU endpoint detector (Spec 004 Phase 5, FR8).
//
// The CPU AsrSileroVad is the reference of record for the endpoint detector.
// The GPU GpuVad must reproduce its per-window speech probability with the SAME
// weights. This test feeds an identical deterministic signal to both and
// asserts the per-window probabilities match within a recorded tolerance. Run
// from the repo root so models/vad/silero_vad.safetensors resolves.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

#include "asr_vad_cpu.h"
#include "io/audio_file.h"
#include "pipeline/gpu_vad.h"

using namespace orator;

int main() {
  std::printf("Testing GPU VAD vs CPU reference (Spec 004 Phase 5 FR8)...\n");

  // Deterministic signal: amplitude-modulated tone sum so probabilities vary
  // across windows (exercises both speech-like and quiet regions). The values
  // need not be "real speech" -- the gate is GPU == CPU on identical input.
  const int sr = 16000;
  const int n = sr * 8;  // 8 s -> 250 windows of 512 samples
  std::vector<float> pcm(n);
  for (int i = 0; i < n; ++i) {
    const double t = static_cast<double>(i) / sr;
    const double env =
        0.5 * (1.0 + std::sin(2.0 * M_PI * 0.7 * t));  // 0..1 slow
    const double s = std::sin(2.0 * M_PI * 220.0 * t) +
                     0.5 * std::sin(2.0 * M_PI * 740.0 * t) +
                     0.3 * std::sin(2.0 * M_PI * 1600.0 * t);
    pcm[i] = static_cast<float>(0.3 * env * s);
  }

  const std::string model = "models/vad/silero_vad.safetensors";

  pipeline::AsrSileroVad::Params cp;
  cp.sample_rate = sr;
  cp.silero_model_path = model;
  pipeline::AsrSileroVad cpu(cp);
  std::vector<float> cpu_probs = cpu.DebugWindowProbs(pcm.data(), n);

  pipeline::GpuVad::Params gp;
  gp.sample_rate = sr;
  gp.silero_model_path = model;
  pipeline::GpuVad gpu(gp);
  std::vector<float> gpu_probs = gpu.DebugWindowProbs(pcm.data(), n);

  if (cpu_probs.size() != gpu_probs.size() || cpu_probs.empty()) {
    std::printf("FAIL: window count mismatch (cpu=%zu gpu=%zu)\n",
                cpu_probs.size(), gpu_probs.size());
    return 1;
  }

  double max_abs = 0.0;
  int worst = -1;
  for (size_t i = 0; i < cpu_probs.size(); ++i) {
    const double d = std::fabs(cpu_probs[i] - gpu_probs[i]);
    if (d > max_abs) {
      max_abs = d;
      worst = static_cast<int>(i);
    }
  }

  const double tol = 2e-3;  // fp32 with differing reduction order
  std::printf(
      "windows=%zu  max |prob_gpu - prob_cpu|=%.2e (window %d)  tol=%.0e\n",
      cpu_probs.size(), max_abs, worst, tol);
  if (worst >= 0) {
    std::printf("  worst: cpu=%.6f gpu=%.6f\n", cpu_probs[worst],
                gpu_probs[worst]);
  }

  if (max_abs > tol) {
    std::printf("FAIL: GPU VAD probabilities exceed tolerance\n");
    return 1;
  }

  auto audio = io::LoadAudioMono("test/data/audio/test.mp3", sr);
  audio.samples.resize(std::min(
      audio.samples.size(), static_cast<size_t>(30 * sr)));
  pipeline::GpuVad::Params zero_pad_params = gp;
  zero_pad_params.silero_min_silence_ms = 300;
  zero_pad_params.silero_speech_pad_ms = 0;
  pipeline::GpuVad zero_pad(zero_pad_params);
  zero_pad.Push(audio.samples.data(), static_cast<int>(audio.samples.size()));
  std::vector<core::VadSegmentResult> raw_segments;
  zero_pad.DrainSegments(true, &raw_segments);

  pipeline::GpuVad::Params padded_params = zero_pad_params;
  padded_params.silero_speech_pad_ms = 60;
  pipeline::GpuVad padded(padded_params);
  padded.Push(audio.samples.data(), static_cast<int>(audio.samples.size()));
  std::vector<core::VadSegmentResult> padded_segments;
  padded.DrainSegments(true, &padded_segments);

  if (raw_segments.empty() || raw_segments.size() != padded_segments.size()) {
    std::printf("FAIL: padded VAD endpoint count differs (raw=%zu padded=%zu)\n",
                raw_segments.size(), padded_segments.size());
    return 1;
  }
  const long pad_samples = 60L * sr / 1000;
  const long processed_horizon =
      static_cast<long>(audio.samples.size() / 512) * 512;
  long prior_end = 0;
  for (size_t i = 0; i < raw_segments.size(); ++i) {
    const long expected_start = std::max(
        prior_end, std::max(0L, raw_segments[i].start_sample - pad_samples));
    const long expected_end = std::min(
        processed_horizon, raw_segments[i].end_sample + pad_samples);
    if (padded_segments[i].start_sample != expected_start ||
        padded_segments[i].end_sample != expected_end) {
      std::printf(
          "FAIL: padded VAD interval %zu differs "
          "(got=%ld-%ld expected=%ld-%ld)\n",
          i, padded_segments[i].start_sample, padded_segments[i].end_sample,
          expected_start, expected_end);
      return 1;
    }
    prior_end = expected_end;
  }
  std::printf("GPU VAD speech padding gate PASSED (%zu segments)\n",
              padded_segments.size());
  std::printf("GPU VAD numeric gate PASSED\n");
  return 0;
}
