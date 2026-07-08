// Offline probe for speaker-identity drift.
//
// Reads diarization spans exported as TSV:
//   start_sec<TAB>end_sec<TAB>local_speaker<TAB>speaker_id<TAB>confidence
// and embeds clean spans from an audio file with TitaNet. It groups spans by
// local speaker and fixed time bucket, then prints centroid cosine values so a
// reviewer can see whether a diarizer-local slot changed real speakers.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "core/types.h"
#include "io/audio_file.h"
#include "model/titanet_embedder.h"

using namespace orator;

namespace {

struct Span {
  double start = 0.0;
  double end = 0.0;
  int local = -1;
  std::string speaker_id;
  float confidence = 0.0f;
};

struct GroupKey {
  int local = -1;
  int bucket = 0;

  bool operator<(const GroupKey& o) const {
    if (local != o.local) return local < o.local;
    return bucket < o.bucket;
  }
};

struct Ref {
  double quality = 0.0;
  Span span;
  std::vector<float> emb;
};

std::vector<std::string> SplitTab(const std::string& line) {
  std::vector<std::string> out;
  std::string item;
  std::stringstream ss(line);
  while (std::getline(ss, item, '\t')) out.push_back(item);
  return out;
}

std::vector<Span> ReadSpans(const std::string& path) {
  std::ifstream f(path);
  if (!f) throw std::runtime_error("cannot open spans TSV: " + path);
  std::vector<Span> spans;
  std::string line;
  while (std::getline(f, line)) {
    if (line.empty() || line[0] == '#') continue;
    auto cols = SplitTab(line);
    if (cols.size() < 5) continue;
    Span s;
    s.start = std::atof(cols[0].c_str());
    s.end = std::atof(cols[1].c_str());
    s.local = std::atoi(cols[2].c_str());
    s.speaker_id = cols[3] == "null" ? "" : cols[3];
    s.confidence = std::atof(cols[4].c_str());
    if (s.end > s.start && s.local >= 0) spans.push_back(std::move(s));
  }
  std::sort(spans.begin(), spans.end(), [](const Span& a, const Span& b) {
    if (a.start != b.start) return a.start < b.start;
    return a.end < b.end;
  });
  return spans;
}

bool IsClean(const Span& s, const std::vector<Span>& all, double min_sec,
             float min_conf, double overlap_eps) {
  if (s.end - s.start < min_sec) return false;
  if (s.confidence < min_conf) return false;
  for (const auto& o : all) {
    if (&o == &s) continue;
    if (o.local == s.local) continue;
    const double a = std::max(s.start, o.start);
    const double b = std::min(s.end, o.end);
    if (b - a > overlap_eps) return false;
  }
  return true;
}

std::vector<float> ClipAudio(const io::AudioData& audio, double start,
                             double end, double edge_margin,
                             double max_window) {
  double a = start;
  double b = end;
  if ((b - a) > 2 * edge_margin + 0.5) {
    a += edge_margin;
    b -= edge_margin;
  }
  if (b - a > max_window) {
    const double mid = 0.5 * (a + b);
    a = mid - 0.5 * max_window;
    b = mid + 0.5 * max_window;
  }
  long s0 = static_cast<long>(std::llround(a * audio.sample_rate));
  long s1 = static_cast<long>(std::llround(b * audio.sample_rate));
  s0 = std::max(0L, std::min(s0, static_cast<long>(audio.samples.size())));
  s1 = std::max(s0, std::min(s1, static_cast<long>(audio.samples.size())));
  return std::vector<float>(audio.samples.begin() + s0,
                            audio.samples.begin() + s1);
}

double Cosine(const std::vector<float>& a, const std::vector<float>& b) {
  if (a.empty() || a.size() != b.size()) return 0.0;
  double dot = 0.0;
  double na = 0.0;
  double nb = 0.0;
  for (size_t i = 0; i < a.size(); ++i) {
    dot += static_cast<double>(a[i]) * b[i];
    na += static_cast<double>(a[i]) * a[i];
    nb += static_cast<double>(b[i]) * b[i];
  }
  return dot / (std::sqrt(na) * std::sqrt(nb) + 1e-12);
}

std::vector<float> Centroid(const std::vector<Ref>& refs, int dim) {
  std::vector<float> c(static_cast<size_t>(dim), 0.0f);
  for (const auto& r : refs) {
    for (int i = 0; i < dim; ++i) c[static_cast<size_t>(i)] += r.emb[i];
  }
  double nrm = 0.0;
  for (float v : c) nrm += static_cast<double>(v) * v;
  nrm = std::sqrt(nrm) + 1e-12;
  for (float& v : c) v = static_cast<float>(v / nrm);
  return c;
}

std::string Label(const GroupKey& k, double bucket_sec) {
  char buf[64];
  std::snprintf(buf, sizeof(buf), "L%d@%04.0f-%04.0f", k.local,
                k.bucket * bucket_sec, (k.bucket + 1) * bucket_sec);
  return std::string(buf);
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 4) {
    std::fprintf(
        stderr,
        "usage: %s <audio.mp3/wav> <titanet.safetensors> <spans.tsv> "
        "[bucket_sec=600] [min_sec=5] [max_refs=3]\n",
        argv[0]);
    return 2;
  }

  const std::string audio_path = argv[1];
  const std::string weights = argv[2];
  const std::string spans_path = argv[3];
  const double bucket_sec = argc > 4 ? std::atof(argv[4]) : 600.0;
  const double min_sec = argc > 5 ? std::atof(argv[5]) : 5.0;
  const int max_refs = argc > 6 ? std::atoi(argv[6]) : 3;
  const float min_conf = 0.5f;
  const double overlap_eps = 0.1;
  const double edge_margin = 0.3;
  const double max_window = 10.0;

  std::printf("loading audio: %s\n", audio_path.c_str());
  io::AudioData audio = io::LoadAudioMono(audio_path, 16000);
  std::printf("  samples=%zu sr=%d dur=%.3fs\n", audio.samples.size(),
              audio.sample_rate, audio.DurationSec());

  auto spans = ReadSpans(spans_path);
  std::printf("loaded spans=%zu from %s\n", spans.size(), spans_path.c_str());

  model::TitaNetEmbedder embedder;
  embedder.LoadWeights(weights);
  const int dim = embedder.dim();

  std::map<GroupKey, std::vector<Ref>> groups;
  int clean_count = 0;
  for (const auto& s : spans) {
    if (!IsClean(s, spans, min_sec, min_conf, overlap_eps)) continue;
    ++clean_count;
    const int bucket = static_cast<int>(std::floor(s.start / bucket_sec));
    GroupKey key{s.local, bucket};
    auto pcm = ClipAudio(audio, s.start, s.end, edge_margin, max_window);
    if (pcm.empty()) continue;
    core::AudioChunk chunk;
    chunk.samples = pcm.data();
    chunk.num_samples = static_cast<int>(pcm.size());
    chunk.sample_rate = audio.sample_rate;
    chunk.t_start_sec = s.start;
    auto e = embedder.Embed(chunk);
    if (static_cast<int>(e.size()) != dim) continue;
    const double q = static_cast<double>(s.confidence) * (s.end - s.start);
    groups[key].push_back(Ref{q, s, std::move(e)});
  }

  std::printf("clean spans=%d  groups=%zu  bucket=%.1fs min_sec=%.1f\n",
              clean_count, groups.size(), bucket_sec, min_sec);

  std::vector<GroupKey> keys;
  std::map<GroupKey, std::vector<float>> centroids;
  for (auto& kv : groups) {
    auto& refs = kv.second;
    std::sort(refs.begin(), refs.end(),
              [](const Ref& a, const Ref& b) { return a.quality > b.quality; });
    if (static_cast<int>(refs.size()) > max_refs) refs.resize(max_refs);
    centroids[kv.first] = Centroid(refs, dim);
    keys.push_back(kv.first);
  }

  std::printf("\n== groups ==\n");
  for (const auto& key : keys) {
    const auto& refs = groups[key];
    double span0 = refs.empty() ? 0.0 : refs.front().span.start;
    double span1 = refs.empty() ? 0.0 : refs.front().span.end;
    std::printf("%-15s refs=%zu best=[%.2f-%.2f] q=%.3f id=%s\n",
                Label(key, bucket_sec).c_str(), refs.size(), span0, span1,
                refs.empty() ? 0.0 : refs.front().quality,
                refs.empty() ? "" : refs.front().span.speaker_id.c_str());
  }

  std::printf("\n== local adjacent bucket cosine ==\n");
  for (size_t i = 0; i < keys.size(); ++i) {
    for (size_t j = i + 1; j < keys.size(); ++j) {
      if (keys[i].local != keys[j].local) continue;
      if (keys[j].bucket - keys[i].bucket != 1) continue;
      std::printf("%-15s vs %-15s  %.3f\n", Label(keys[i], bucket_sec).c_str(),
                  Label(keys[j], bucket_sec).c_str(),
                  Cosine(centroids[keys[i]], centroids[keys[j]]));
    }
  }

  std::printf("\n== all group cosine ==\n");
  std::printf("%-15s", "");
  for (const auto& key : keys) std::printf(" %11s", Label(key, bucket_sec).c_str());
  std::printf("\n");
  for (const auto& a : keys) {
    std::printf("%-15s", Label(a, bucket_sec).c_str());
    for (const auto& b : keys) {
      std::printf(" %11.3f", Cosine(centroids[a], centroids[b]));
    }
    std::printf("\n");
  }

  return 0;
}
