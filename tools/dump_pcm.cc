// Decode an audio file to raw int16 mono PCM (16kHz) for WS stress testing.
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "io/audio_file.h"

int main(int argc, char** argv) {
  std::string in = argc > 1 ? argv[1] : "test.mp3";
  std::string out = argc > 2 ? argv[2] : "/tmp/base.pcm";
  auto audio = orator::io::LoadAudioMono(in, 16000);
  std::vector<int16_t> pcm(audio.samples.size());
  for (size_t i = 0; i < audio.samples.size(); ++i) {
    float v = audio.samples[i] * 32767.0f;
    if (v > 32767.0f) v = 32767.0f;
    if (v < -32768.0f) v = -32768.0f;
    pcm[i] = static_cast<int16_t>(v);
  }
  FILE* f = std::fopen(out.c_str(), "wb");
  std::fwrite(pcm.data(), sizeof(int16_t), pcm.size(), f);
  std::fclose(f);
  std::printf("wrote %zu samples (%.2fs) to %s\n", pcm.size(),
              audio.DurationSec(), out.c_str());
  return 0;
}
