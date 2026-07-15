// Offline per-turn TitaNet evidence export for Spec 013.
//
// Input spans are candidate business-turn intervals only. The probe does not
// read Sortformer labels, reference speakers, or correctness judgments. Each
// sufficiently long interval is embedded independently and scored against
// every identity in the frozen SpeakerDatabase registry.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
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
#include "model/speaker_database.h"
#include "model/titanet_embedder.h"
#include "pipeline/auditory_stream.h"

namespace {

using orator::pipeline::AuditoryStream;

struct TurnSpan {
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

std::vector<TurnSpan> ReadSpans(const std::string& path) {
  std::ifstream input(path);
  if (!input) throw std::runtime_error("cannot open spans TSV: " + path);

  std::vector<TurnSpan> spans;
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty() || line[0] == '#') continue;
    const auto columns = SplitTab(line);
    if (columns.size() < 3 || columns[0] == "evidence_id") continue;
    TurnSpan span;
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
    const TurnSpan& span, const AuditoryStream::Config& config) {
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

std::vector<float> RegistryScores(
    const std::vector<float>& embedding,
    const orator::model::SpeakerDatabase& database) {
  std::vector<float> scores(static_cast<size_t>(database.Size()), 0.0f);
  const float* enrolled = database.Embeddings();
  const int dimension = database.EmbeddingDim();
  for (int speaker = 0; speaker < database.Size(); ++speaker) {
    double score = 0.0;
    const float* reference =
        enrolled + static_cast<size_t>(speaker) * dimension;
    for (int index = 0; index < dimension; ++index) {
      score += static_cast<double>(embedding[static_cast<size_t>(index)]) *
               reference[index];
    }
    scores[static_cast<size_t>(speaker)] = static_cast<float>(score);
  }
  return scores;
}

std::pair<int, int> BestTwo(const std::vector<float>& scores) {
  int best = -1;
  int second = -1;
  for (int index = 0; index < static_cast<int>(scores.size()); ++index) {
    if (best < 0 || scores[index] > scores[best]) {
      second = best;
      best = index;
    } else if (second < 0 || scores[index] > scores[second]) {
      second = index;
    }
  }
  return {best, second};
}

void WriteEmptyEvidence(std::ofstream& output, const TurnSpan& span,
                        const std::string& status, int registry_size) {
  output << span.evidence_id << '\t' << span.start_sec << '\t' << span.end_sec
         << '\t' << (span.end_sec - span.start_sec) << '\t' << status
         << "\t\t\t\t\t\t\t";
  for (int index = 0; index < registry_size; ++index) output << '\t';
  output << '\n';
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 6) {
    std::fprintf(
        stderr,
        "usage: %s <audio.wav/mp3> <titanet.safetensors> <registry> "
        "<spans.tsv> <out.tsv> [config=orator.toml]\n",
        argv[0]);
    return 2;
  }

  try {
    const std::string audio_path = argv[1];
    const std::string weights_path = argv[2];
    const std::string registry_path = argv[3];
    const std::string spans_path = argv[4];
    const std::string output_path = argv[5];
    const std::string config_path = argc > 6 ? argv[6] : "orator.toml";

    AuditoryStream::Config config;
    if (!orator::io::ApplyTomlConfig(config_path, config)) {
      throw std::runtime_error("cannot load config: " + config_path);
    }
    orator::io::AudioData audio =
        orator::io::LoadAudioMono(audio_path, config.sample_rate);
    const orator::core::TimeBase time_base(config.sample_rate);
    const std::vector<TurnSpan> spans = ReadSpans(spans_path);

    orator::model::TitaNetEmbedder embedder;
    embedder.LoadWeights(weights_path);
    orator::model::SpeakerDatabase database(256, embedder.dim());
    if (!database.Load(registry_path)) {
      throw std::runtime_error("cannot load speaker registry: " +
                               registry_path);
    }
    if (database.Size() < 2) {
      throw std::runtime_error(
          "speaker registry needs at least two enrolled identities");
    }

    std::ofstream output(output_path);
    if (!output) {
      throw std::runtime_error("cannot write evidence TSV: " + output_path);
    }
    output << "evidence_id\tstart_sec\tend_sec\tduration_sec\tstatus\t"
              "embed_start_sec\tembed_end_sec\tbest_id\tbest_score\t"
              "second_id\tsecond_score\tmargin";
    for (int index = 0; index < database.Size(); ++index) {
      output << "\tscore_" << database.SpeakerIdAt(index);
    }
    output << '\n' << std::fixed << std::setprecision(6);

    int embedded_count = 0;
    int insufficient_count = 0;
    for (const auto& span : spans) {
      if (span.end_sec - span.start_sec < config.speaker_min_embed_sec) {
        WriteEmptyEvidence(output, span, "insufficient_duration",
                           database.Size());
        ++insufficient_count;
        continue;
      }
      const auto window = EmbeddingWindow(span, config);
      std::vector<float> pcm =
          ClipAudio(audio, time_base, window.first, window.second);
      if (pcm.empty()) {
        WriteEmptyEvidence(output, span, "empty_audio", database.Size());
        continue;
      }

      orator::core::AudioChunk chunk;
      chunk.samples = pcm.data();
      chunk.num_samples = static_cast<int>(pcm.size());
      chunk.sample_rate = config.sample_rate;
      chunk.t_start_sec = window.first;
      std::vector<float> embedding = embedder.Embed(chunk);
      if (static_cast<int>(embedding.size()) != embedder.dim()) {
        WriteEmptyEvidence(output, span, "embedding_failed",
                           database.Size());
        continue;
      }

      const std::vector<float> scores = RegistryScores(embedding, database);
      const auto best_two = BestTwo(scores);
      const float best_score =
          best_two.first >= 0 ? scores[best_two.first] : 0.0f;
      const float second_score =
          best_two.second >= 0 ? scores[best_two.second] : 0.0f;
      output << span.evidence_id << '\t' << span.start_sec << '\t'
             << span.end_sec << '\t' << (span.end_sec - span.start_sec)
             << "\tok\t" << window.first << '\t' << window.second << '\t'
             << (best_two.first >= 0
                     ? database.SpeakerIdAt(best_two.first)
                     : std::string())
             << '\t' << best_score << '\t'
             << (best_two.second >= 0
                     ? database.SpeakerIdAt(best_two.second)
                     : std::string())
             << '\t' << second_score << '\t'
             << (best_score - second_score);
      for (float score : scores) output << '\t' << score;
      output << '\n';
      ++embedded_count;
    }

    std::printf(
        "config=%s audio=%s spans=%zu registry=%d embedded=%d "
        "insufficient=%d out=%s\n",
        config_path.c_str(), audio_path.c_str(), spans.size(), database.Size(),
        embedded_count, insufficient_count, output_path.c_str());
    return 0;
  } catch (const std::exception& error) {
    std::fprintf(stderr, "speaker turn evidence probe: %s\n", error.what());
    return 1;
  }
}
