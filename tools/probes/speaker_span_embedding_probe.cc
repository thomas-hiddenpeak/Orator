// Export normalized TitaNet embeddings for absolute-clock evidence spans.
// This probe never reads diar labels, transcripts, or reference judgments.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "core/time_base.h"
#include "core/types.h"
#include "io/audio_file.h"
#include "io/config_reader.h"
#include "model/titanet_embedder.h"
#include "pipeline/auditory_stream.h"

namespace {

using orator::pipeline::AuditoryStream;

constexpr double kDurationEpsilonSec = 1e-6;

struct EvidenceSpan {
  std::string evidence_id;
  double start_sec = 0.0;
  double end_sec = 0.0;
};

std::vector<std::string> SplitTab(const std::string& line) {
  std::vector<std::string> out;
  std::stringstream stream(line);
  std::string item;
  while (std::getline(stream, item, '\t')) out.push_back(item);
  return out;
}

std::vector<EvidenceSpan> ReadSpans(const std::string& path) {
  std::ifstream input(path);
  if (!input) throw std::runtime_error("cannot open spans TSV: " + path);
  std::vector<EvidenceSpan> spans;
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty() || line[0] == '#') continue;
    const auto columns = SplitTab(line);
    if (columns.size() < 3 || columns[0] == "evidence_id") continue;
    EvidenceSpan span;
    span.evidence_id = columns[0];
    span.start_sec = std::stod(columns[1]);
    span.end_sec = std::stod(columns[2]);
    if (span.evidence_id.empty() || span.start_sec < 0.0 ||
        span.end_sec <= span.start_sec) {
      throw std::runtime_error("invalid span row: " + line);
    }
    spans.push_back(std::move(span));
  }
  return spans;
}

std::pair<double, double> EmbeddingWindow(
    const EvidenceSpan& span, const AuditoryStream::Config& config) {
  double start = span.start_sec;
  double end = span.end_sec;
  if (end - start > 2.0 * config.speaker_edge_margin_sec + 0.5) {
    start += config.speaker_edge_margin_sec;
    end -= config.speaker_edge_margin_sec;
  }
  if (end - start > config.speaker_max_embed_window_sec) {
    const double middle = 0.5 * (start + end);
    start = middle - 0.5 * config.speaker_max_embed_window_sec;
    end = middle + 0.5 * config.speaker_max_embed_window_sec;
  }
  return {start, end};
}

std::vector<float> ClipAudio(const orator::io::AudioData& audio,
                             const orator::core::TimeBase& time_base,
                             double start_sec, double end_sec) {
  long start_sample = time_base.SampleAt(start_sec);
  long end_sample = time_base.SampleAt(end_sec);
  const long sample_count = static_cast<long>(audio.samples.size());
  start_sample = std::clamp(start_sample, 0L, sample_count);
  end_sample = std::clamp(end_sample, start_sample, sample_count);
  return std::vector<float>(audio.samples.begin() + start_sample,
                            audio.samples.begin() + end_sample);
}

void WriteEmpty(std::ofstream& output, const EvidenceSpan& span,
                const std::string& status, int embedding_dim) {
  output << span.evidence_id << '\t' << span.start_sec << '\t' << span.end_sec
         << '\t' << (span.end_sec - span.start_sec) << '\t' << status
         << "\t\t";
  for (int index = 0; index < embedding_dim; ++index) output << '\t';
  output << '\n';
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 6) {
    std::fprintf(stderr,
                 "usage: %s <audio.wav/mp3> <titanet.safetensors> "
                 "<spans.tsv> <out.tsv> <config.toml>\n",
                 argv[0]);
    return 2;
  }
  try {
    const std::string audio_path = argv[1];
    const std::string weights_path = argv[2];
    const std::string spans_path = argv[3];
    const std::string output_path = argv[4];
    const std::string config_path = argv[5];

    AuditoryStream::Config config;
    if (!orator::io::ApplyTomlConfig(config_path, config)) {
      throw std::runtime_error("cannot load config: " + config_path);
    }
    const orator::io::AudioData audio =
        orator::io::LoadAudioMono(audio_path, config.sample_rate);
    const orator::core::TimeBase time_base(config.sample_rate);
    const std::vector<EvidenceSpan> spans = ReadSpans(spans_path);
    orator::model::TitaNetEmbedder embedder;
    embedder.LoadWeights(weights_path);

    std::ofstream output(output_path);
    if (!output) throw std::runtime_error("cannot write output: " + output_path);
    output << "evidence_id\tstart_sec\tend_sec\tduration_sec\tstatus\t"
              "embed_start_sec\tembed_end_sec";
    for (int index = 0; index < embedder.dim(); ++index) {
      output << "\temb_" << std::setfill('0') << std::setw(3) << index;
    }
    output << '\n' << std::fixed << std::setprecision(9) << std::setfill(' ');

    int embedded = 0;
    int insufficient = 0;
    for (const auto& span : spans) {
      if (span.end_sec - span.start_sec + kDurationEpsilonSec <
          config.speaker_min_embed_sec) {
        WriteEmpty(output, span, "insufficient_duration", embedder.dim());
        ++insufficient;
        continue;
      }
      const auto window = EmbeddingWindow(span, config);
      std::vector<float> pcm =
          ClipAudio(audio, time_base, window.first, window.second);
      if (pcm.empty()) {
        WriteEmpty(output, span, "empty_audio", embedder.dim());
        continue;
      }
      orator::core::AudioChunk chunk;
      chunk.samples = pcm.data();
      chunk.num_samples = static_cast<int>(pcm.size());
      chunk.sample_rate = config.sample_rate;
      chunk.t_start_sec = window.first;
      const std::vector<float> embedding = embedder.Embed(chunk);
      if (static_cast<int>(embedding.size()) != embedder.dim()) {
        WriteEmpty(output, span, "embedding_failed", embedder.dim());
        continue;
      }
      double norm = 0.0;
      for (float value : embedding) norm += static_cast<double>(value) * value;
      if (std::abs(std::sqrt(norm) - 1.0) > 1e-3) {
        WriteEmpty(output, span, "embedding_not_normalized", embedder.dim());
        continue;
      }
      output << span.evidence_id << '\t' << span.start_sec << '\t'
             << span.end_sec << '\t' << (span.end_sec - span.start_sec)
             << "\tok\t" << window.first << '\t' << window.second;
      for (float value : embedding) output << '\t' << value;
      output << '\n';
      ++embedded;
    }
    std::printf("config=%s spans=%zu embedded=%d insufficient=%d out=%s\n",
                config_path.c_str(), spans.size(), embedded, insufficient,
                output_path.c_str());
    return 0;
  } catch (const std::exception& error) {
    std::fprintf(stderr, "speaker span embedding probe: %s\n", error.what());
    return 1;
  }
}
