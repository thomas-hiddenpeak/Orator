#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <vector>

#include "io/audio_file.h"

using namespace orator;

// Write a minimal PCM16 mono WAV.
static void WriteWav(const std::string& path, const std::vector<float>& samples,
                     int sample_rate) {
  std::ofstream f(path, std::ios::binary);
  const uint32_t data_bytes = static_cast<uint32_t>(samples.size() * 2);
  const uint32_t chunk_size = 36 + data_bytes;
  const uint16_t audio_format = 1, channels = 1, bits = 16;
  const uint32_t byte_rate = sample_rate * channels * bits / 8;
  const uint16_t block_align = channels * bits / 8;

  f.write("RIFF", 4);
  f.write(reinterpret_cast<const char*>(&chunk_size), 4);
  f.write("WAVE", 4);
  f.write("fmt ", 4);
  uint32_t fmt_size = 16;
  f.write(reinterpret_cast<const char*>(&fmt_size), 4);
  f.write(reinterpret_cast<const char*>(&audio_format), 2);
  f.write(reinterpret_cast<const char*>(&channels), 2);
  f.write(reinterpret_cast<const char*>(&sample_rate), 4);
  f.write(reinterpret_cast<const char*>(&byte_rate), 4);
  f.write(reinterpret_cast<const char*>(&block_align), 2);
  f.write(reinterpret_cast<const char*>(&bits), 2);
  f.write("data", 4);
  f.write(reinterpret_cast<const char*>(&data_bytes), 4);
  for (float v : samples) {
    int16_t s = static_cast<int16_t>(std::lround(v * 32767.0f));
    f.write(reinterpret_cast<const char*>(&s), 2);
  }
}

int main() {
  std::cout << "Testing audio file reader..." << std::endl;

  const char* path = "/tmp/orator_test_audio.wav";
  const int sr = 16000;
  std::vector<float> sig(sr);  // 1s
  for (int i = 0; i < sr; ++i) {
    sig[i] = 0.3f * std::sin(2.0 * 3.14159265 * 440.0 * i / sr);
  }
  WriteWav(path, sig, sr);

  // Same-rate load: samples should round-trip within PCM16 quantization.
  auto a = io::LoadAudioMono(path, sr);
  assert(a.sample_rate == sr);
  assert(static_cast<int>(a.samples.size()) == sr);
  double max_err = 0.0;
  for (int i = 0; i < sr; ++i) {
    max_err = std::max(max_err, std::abs(double(a.samples[i]) - sig[i]));
  }
  std::cout << "Same-rate max error=" << max_err << std::endl;
  assert(max_err < 1e-3);  // PCM16 quantization bound

  // Resample to 8k: length halves (approximately).
  auto b = io::LoadAudioMono(path, 8000);
  assert(b.sample_rate == 8000);
  const int approx = static_cast<int>(sr * 0.5);
  assert(std::abs(static_cast<int>(b.samples.size()) - approx) < 4);
  std::cout << "Resample length OK (" << b.samples.size() << ")" << std::endl;

  std::remove(path);
  std::cout << "\nAll audio reader tests passed!" << std::endl;
  return 0;
}
