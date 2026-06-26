// Decodes a real audio file via the C++ minimp3-based reader and prints stats.
#include <cmath>
#include <iostream>
#include <string>

#include "io/audio_file.h"

using namespace orator;

int main(int argc, char** argv) {
  std::string path = argc > 1 ? argv[1] : "test.mp3";
  int sr = argc > 2 ? std::stoi(argv[2]) : 16000;
  std::cout << "Decoding: " << path << " -> mono " << sr << " Hz" << std::endl;

  auto a = io::LoadAudioMono(path, sr);
  std::cout << "sample_rate: " << a.sample_rate << std::endl;
  std::cout << "samples: " << a.samples.size() << std::endl;
  std::cout << "duration: " << a.DurationSec() << " s" << std::endl;

  double sum = 0, sumsq = 0, peak = 0;
  size_t nonzero = 0;
  for (float v : a.samples) {
    sum += v;
    sumsq += double(v) * v;
    double av = std::fabs(v);
    if (av > peak) peak = av;
    if (av > 1e-6) ++nonzero;
  }
  size_t n = a.samples.size();
  double mean = n ? sum / n : 0;
  double rms = n ? std::sqrt(sumsq / n) : 0;
  std::cout << "mean: " << mean << "  rms: " << rms << "  peak: " << peak
            << std::endl;
  std::cout << "nonzero fraction: " << (n ? double(nonzero) / n : 0)
            << std::endl;
  if (n == 0 || peak == 0.0) {
    std::cout << "DECODE FAILED (no audio)" << std::endl;
    return 1;
  }
  std::cout << "DECODE OK" << std::endl;
  return 0;
}
