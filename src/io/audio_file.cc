#include "io/audio_file.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <stdexcept>

#define MINIMP3_IMPLEMENTATION
#define MINIMP3_FLOAT_OUTPUT
#include "minimp3_ex.h"

namespace orator {
namespace io {

namespace {

bool HasSuffix(const std::string& s, const std::string& suffix) {
  if (s.size() < suffix.size()) return false;
  std::string tail = s.substr(s.size() - suffix.size());
  std::transform(tail.begin(), tail.end(), tail.begin(), ::tolower);
  return tail == suffix;
}

std::vector<float> DownmixToMono(const float* interleaved, size_t total_samples,
                                 int channels) {
  if (channels <= 1) {
    return std::vector<float>(interleaved, interleaved + total_samples);
  }
  const size_t frames = total_samples / channels;
  std::vector<float> mono(frames, 0.0f);
  for (size_t f = 0; f < frames; ++f) {
    float acc = 0.0f;
    for (int c = 0; c < channels; ++c) {
      acc += interleaved[f * channels + c];
    }
    mono[f] = acc / channels;
  }
  return mono;
}

// Simple linear resampler.
std::vector<float> Resample(const std::vector<float>& in, int in_rate,
                            int out_rate) {
  if (in_rate == out_rate || in.empty()) return in;
  const double ratio = static_cast<double>(out_rate) / in_rate;
  const size_t out_len = static_cast<size_t>(in.size() * ratio);
  std::vector<float> out(out_len, 0.0f);
  for (size_t i = 0; i < out_len; ++i) {
    const double src_pos = i / ratio;
    const size_t i0 = static_cast<size_t>(src_pos);
    const size_t i1 = std::min(i0 + 1, in.size() - 1);
    const double frac = src_pos - i0;
    out[i] = static_cast<float>(in[i0] * (1.0 - frac) + in[i1] * frac);
  }
  return out;
}

AudioData LoadMp3(const std::string& path, int target_rate) {
  mp3dec_t decoder;
  mp3dec_file_info_t info;
  std::memset(&info, 0, sizeof(info));
  if (mp3dec_load(&decoder, path.c_str(), &info, nullptr, nullptr) != 0 ||
      info.buffer == nullptr) {
    throw std::runtime_error("Failed to decode MP3: " + path);
  }

  std::vector<float> mono =
      DownmixToMono(info.buffer, static_cast<size_t>(info.samples),
                    info.channels);
  free(info.buffer);

  AudioData out;
  out.samples = Resample(mono, info.hz, target_rate);
  out.sample_rate = target_rate;
  return out;
}

// Minimal RIFF/WAVE reader for PCM16 and IEEE float32.
AudioData LoadWav(const std::string& path, int target_rate) {
  std::ifstream f(path, std::ios::binary);
  if (!f.is_open()) throw std::runtime_error("Cannot open WAV: " + path);

  char riff[4];
  f.read(riff, 4);
  if (std::strncmp(riff, "RIFF", 4) != 0) {
    throw std::runtime_error("Not a RIFF file: " + path);
  }
  f.ignore(4);  // chunk size
  char wave[4];
  f.read(wave, 4);
  if (std::strncmp(wave, "WAVE", 4) != 0) {
    throw std::runtime_error("Not a WAVE file: " + path);
  }

  uint16_t audio_format = 1;
  uint16_t channels = 1;
  uint32_t sample_rate = target_rate;
  uint16_t bits_per_sample = 16;
  std::vector<float> mono;

  while (f && f.peek() != EOF) {
    char id[4];
    f.read(id, 4);
    uint32_t size = 0;
    f.read(reinterpret_cast<char*>(&size), 4);
    if (!f) break;

    if (std::strncmp(id, "fmt ", 4) == 0) {
      f.read(reinterpret_cast<char*>(&audio_format), 2);
      f.read(reinterpret_cast<char*>(&channels), 2);
      f.read(reinterpret_cast<char*>(&sample_rate), 4);
      f.ignore(4);  // byte rate
      f.ignore(2);  // block align
      f.read(reinterpret_cast<char*>(&bits_per_sample), 2);
      if (size > 16) f.ignore(size - 16);
    } else if (std::strncmp(id, "data", 4) == 0) {
      const uint32_t num_bytes = size;
      std::vector<float> interleaved;
      if (audio_format == 3 && bits_per_sample == 32) {  // float32
        const size_t n = num_bytes / 4;
        interleaved.resize(n);
        f.read(reinterpret_cast<char*>(interleaved.data()), num_bytes);
      } else if (audio_format == 1 && bits_per_sample == 16) {  // PCM16
        const size_t n = num_bytes / 2;
        std::vector<int16_t> pcm(n);
        f.read(reinterpret_cast<char*>(pcm.data()), num_bytes);
        interleaved.resize(n);
        for (size_t i = 0; i < n; ++i) {
          interleaved[i] = pcm[i] / 32768.0f;
        }
      } else {
        throw std::runtime_error("Unsupported WAV format in: " + path);
      }
      mono = DownmixToMono(interleaved.data(), interleaved.size(), channels);
    } else {
      f.ignore(size);
    }
  }

  AudioData out;
  out.samples = Resample(mono, static_cast<int>(sample_rate), target_rate);
  out.sample_rate = target_rate;
  return out;
}

}  // namespace

AudioData LoadAudioMono(const std::string& path, int target_rate) {
  if (HasSuffix(path, ".mp3")) {
    return LoadMp3(path, target_rate);
  }
  if (HasSuffix(path, ".wav")) {
    return LoadWav(path, target_rate);
  }
  throw std::runtime_error("Unsupported audio extension: " + path);
}

}  // namespace io
}  // namespace orator
