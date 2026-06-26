#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

#include "feature/mel_spectrogram.h"

using namespace orator;

int main() {
  std::cout << "Testing mel-spectrogram front-end..." << std::endl;

  feature::MelConfig cfg;  // 16k, 25ms/10ms, 512 fft, 128 mel
  feature::MelSpectrogram mel(cfg);

  // 1 second sine at 1000 Hz.
  const int sr = cfg.sample_rate;
  const double freq = 1000.0;
  std::vector<float> sig(sr);
  for (int i = 0; i < sr; ++i) {
    sig[i] = 0.5f * std::sin(2.0 * 3.14159265 * freq * i / sr);
  }

  int frames = 0;
  std::vector<float> feats = mel.Compute(sig.data(), sr, &frames);

  // Expected frame count.
  const int expected = mel.NumFrames(sr);
  assert(frames == expected);
  assert(static_cast<int>(feats.size()) == frames * mel.n_mels());
  std::cout << "Frames=" << frames << " n_mels=" << mel.n_mels()
            << " (shape OK)" << std::endl;

  // Find the dominant mel bin in a middle frame; map its center freq.
  const int mid = frames / 2;
  int argmax = 0;
  float best = -1e30f;
  for (int m = 0; m < mel.n_mels(); ++m) {
    const float v = feats[static_cast<size_t>(mid) * mel.n_mels() + m];
    if (v > best) {
      best = v;
      argmax = m;
    }
  }

  // Convert the peak mel bin back to an approximate Hz via the same mel mapping.
  auto hz_to_mel = [](float hz) {
    return 2595.0f * std::log10(1.0f + hz / 700.0f);
  };
  auto mel_to_hz = [](float mel) {
    return 700.0f * (std::pow(10.0f, mel / 2595.0f) - 1.0f);
  };
  const float mel_min = hz_to_mel(cfg.fmin);
  const float mel_max = hz_to_mel(cfg.fmax);
  const float peak_mel = mel_min + (mel_max - mel_min) * (argmax + 1) /
                                       (cfg.n_mels + 1);
  const float peak_hz = mel_to_hz(peak_mel);
  std::cout << "Peak mel bin=" << argmax << " ~= " << peak_hz << " Hz"
            << std::endl;

  // The dominant bin should be reasonably near 1000 Hz (within ~250 Hz given
  // 128-mel resolution around 1 kHz).
  assert(std::abs(peak_hz - freq) < 300.0f);
  std::cout << "Dominant frequency localization OK" << std::endl;

  // Silence => uniformly low (log of eps) features.
  std::vector<float> silence(sr, 0.0f);
  int sframes = 0;
  auto sfeats = mel.Compute(silence.data(), sr, &sframes);
  float smax = -1e30f;
  for (float v : sfeats) smax = std::max(smax, v);
  assert(smax < best);  // silence energy well below tone energy
  std::cout << "Silence vs tone energy ordering OK" << std::endl;

  std::cout << "\nAll mel tests passed!" << std::endl;
  return 0;
}
