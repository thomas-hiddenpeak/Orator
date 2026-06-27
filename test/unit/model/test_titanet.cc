// Validates the C++/CUDA TitaNetEmbedder against the NeMo reference oracle
// (tools/reference/titanet_oracle.py -> models/reference/speaker/). For each
// dumped span we feed the exact same waveform the oracle used and require the
// resulting 192-d embedding to match (cosine ~ 1) and the cross-span cosine
// structure to reproduce the oracle's. Skips cleanly if the dumps or weights
// are absent.

#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include "core/types.h"
#include "model/titanet_embedder.h"

using namespace orator;

static std::vector<float> ReadF32(const std::string& p, bool* ok) {
  std::ifstream f(p, std::ios::binary | std::ios::ate);
  if (!f) {
    *ok = false;
    return {};
  }
  std::streamsize n = f.tellg();
  f.seekg(0);
  std::vector<float> v(n / sizeof(float));
  f.read(reinterpret_cast<char*>(v.data()), n);
  *ok = true;
  return v;
}

static double Cosine(const std::vector<float>& a, const std::vector<float>& b) {
  double dot = 0, na = 0, nb = 0;
  for (size_t i = 0; i < a.size(); ++i) {
    dot += static_cast<double>(a[i]) * b[i];
    na += static_cast<double>(a[i]) * a[i];
    nb += static_cast<double>(b[i]) * b[i];
  }
  return dot / (std::sqrt(na) * std::sqrt(nb) + 1e-12);
}

int main() {
  const std::string weights = "models/speaker/titanet_large.safetensors";
  const std::string ref = "models/reference/speaker/";
  {
    std::ifstream w(weights, std::ios::binary);
    bool d;
    auto probe = ReadF32(ref + "span0_emb.f32", &d);
    if (!w || !d) {
      std::printf(
          "[skip] need %s and oracle dumps (titanet_oracle.py)\n",
          weights.c_str());
      return 0;
    }
  }

  model::TitaNetEmbedder emb;
  emb.LoadWeights(weights);

  const int kSpans = 3;
  std::vector<std::vector<float>> ours(kSpans), refs(kSpans);
  bool all_ok = true;
  for (int i = 0; i < kSpans; ++i) {
    bool a, b;
    auto wave = ReadF32(ref + "span" + std::to_string(i) + "_wave.f32", &a);
    auto remb = ReadF32(ref + "span" + std::to_string(i) + "_emb.f32", &b);
    if (!(a && b)) {
      std::printf("[skip] missing span %d dump\n", i);
      return 0;
    }
    core::AudioChunk chunk;
    chunk.samples = wave.data();
    chunk.num_samples = static_cast<int>(wave.size());
    chunk.sample_rate = 16000;
    auto e = emb.Embed(chunk);
    ours[i] = e;
    refs[i] = remb;
    double cos = Cosine(e, remb);
    std::printf("span%d: dim=%zu  cosine(C++, oracle)=%.6f\n", i, e.size(), cos);
    if (e.size() != remb.size() || cos < 0.999) all_ok = false;
  }

  // Cross-span cosine structure must reproduce the oracle's.
  std::printf("cross-span cosine (C++ / oracle):\n");
  for (int a = 0; a < kSpans; ++a) {
    for (int b = 0; b < kSpans; ++b) {
      double co = Cosine(ours[a], ours[b]);
      double cr = Cosine(refs[a], refs[b]);
      std::printf("  [%d,%d] %+.4f / %+.4f", a, b, co, cr);
      if (std::abs(co - cr) > 0.02) all_ok = false;
    }
    std::printf("\n");
  }

  if (!all_ok) {
    std::printf("FAIL: TitaNetEmbedder diverges from the NeMo oracle\n");
    return 1;
  }
  std::printf("TitaNet embedder test PASSED\n");
  return 0;
}
