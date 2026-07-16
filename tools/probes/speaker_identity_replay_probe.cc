// Replay frozen diarization segments through the production speaker-identity
// stage. This is an evidence generator only: it never reads a reference or
// assigns correctness.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "core/time_base.h"
#include "core/types.h"
#include "io/audio_file.h"
#include "io/config_reader.h"
#include "model/speaker_database.h"
#include "model/titanet_embedder.h"
#include "pipeline/auditory_stream.h"
#include "pipeline/speaker_identity_stage.h"

namespace {

using orator::core::DiarSegment;
using orator::pipeline::AuditoryStream;
using orator::pipeline::SpeakerIdConfig;
using orator::pipeline::SpeakerIdentityStage;

std::vector<std::string> SplitComma(const std::string& line) {
  std::vector<std::string> values;
  std::stringstream stream(line);
  std::string value;
  while (std::getline(stream, value, ',')) values.push_back(value);
  return values;
}

std::vector<DiarSegment> ReadSegments(const std::string& path) {
  std::ifstream input(path);
  if (!input) throw std::runtime_error("cannot open segment CSV: " + path);
  std::vector<DiarSegment> segments;
  std::string line;
  if (!std::getline(input, line)) {
    throw std::runtime_error("empty segment CSV: " + path);
  }
  const auto header = SplitComma(line);
  auto column = [&](const std::string& name) {
    const auto found = std::find(header.begin(), header.end(), name);
    if (found == header.end()) {
      throw std::runtime_error("segment CSV missing column: " + name);
    }
    return static_cast<std::size_t>(found - header.begin());
  };
  const std::size_t start_column = column("start_sec");
  const std::size_t end_column = column("end_sec");
  const std::size_t local_column = column("local_speaker");
  const std::size_t confidence_column = column("confidence");
  while (std::getline(input, line)) {
    if (line.empty()) continue;
    const auto values = SplitComma(line);
    if (values.size() != header.size()) {
      throw std::runtime_error("invalid segment CSV row");
    }
    DiarSegment segment;
    segment.start_sec = std::stod(values[start_column]);
    segment.end_sec = std::stod(values[end_column]);
    segment.local_speaker = std::stoi(values[local_column]);
    segment.confidence = std::stof(values[confidence_column]);
    if (segment.start_sec < 0.0 || segment.end_sec <= segment.start_sec ||
        segment.local_speaker < 0) {
      throw std::runtime_error("invalid segment interval");
    }
    segments.push_back(std::move(segment));
  }
  std::stable_sort(segments.begin(), segments.end(),
                   [](const DiarSegment& left, const DiarSegment& right) {
                     if (left.start_sec != right.start_sec) {
                       return left.start_sec < right.start_sec;
                     }
                     if (left.end_sec != right.end_sec) {
                       return left.end_sec < right.end_sec;
                     }
                     return left.local_speaker < right.local_speaker;
                   });
  return segments;
}

struct EvidenceQuery {
  std::string evidence_id;
  std::string kind;
  long text_id = -1;
  int source_start = 0;
  int source_end = 0;
  double start_sec = 0.0;
  double end_sec = 0.0;
};

std::vector<std::string> SplitTab(const std::string& line) {
  std::vector<std::string> values;
  std::size_t start = 0;
  while (true) {
    const std::size_t tab = line.find('\t', start);
    values.push_back(line.substr(start, tab - start));
    if (tab == std::string::npos) break;
    start = tab + 1;
  }
  return values;
}

std::vector<EvidenceQuery> ReadEvidenceQueries(const std::string& path) {
  std::ifstream input(path);
  if (!input) throw std::runtime_error("cannot open query TSV: " + path);
  std::vector<EvidenceQuery> queries;
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty()) continue;
    const auto columns = SplitTab(line);
    if (columns[0] == "evidence_id") continue;
    if (columns.size() != 7) {
      throw std::runtime_error("invalid query TSV row");
    }
    EvidenceQuery query;
    query.evidence_id = columns[0];
    query.kind = columns[1];
    query.text_id = std::stol(columns[2]);
    query.source_start = std::stoi(columns[3]);
    query.source_end = std::stoi(columns[4]);
    query.start_sec = std::stod(columns[5]);
    query.end_sec = std::stod(columns[6]);
    if (query.evidence_id.empty() || query.kind.empty() ||
        query.source_end <= query.source_start ||
        query.end_sec <= query.start_sec) {
      throw std::runtime_error("invalid query interval");
    }
    queries.push_back(std::move(query));
  }
  return queries;
}

void WriteScores(
    std::ofstream& output,
    const std::vector<SpeakerIdentityStage::VoiceprintScore>& scores) {
  for (std::size_t index = 0; index < scores.size(); ++index) {
    if (index > 0) output << ',';
    output << scores[index].speaker_id << ':' << scores[index].score;
  }
}

void WriteEvidenceQueries(const std::string& path,
                          SpeakerIdentityStage* stage,
                          const std::vector<EvidenceQuery>& queries,
                          const std::vector<std::string>& active_ids,
                          const AuditoryStream::Config& config) {
  if (stage == nullptr) return;
  std::ofstream output(path);
  if (!output) throw std::runtime_error("cannot write evidence TSV: " + path);
  output << "evidence_id\tkind\ttext_id\tsource_start\tsource_end\tstart\tend\t"
            "embedding_available\trobust_gallery_complete\tsession_scores\t"
            "robust_scores\n";
  output << std::fixed << std::setprecision(9);
  for (const auto& query : queries) {
    const auto evidence = stage->EvaluateSpan(
        query.start_sec, query.end_sec, active_ids,
        config.speaker_fusion_min_embed_sec,
        config.speaker_fusion_edge_margin_sec,
        config.speaker_fusion_max_embed_window_sec);
    output << query.evidence_id << '\t' << query.kind << '\t' << query.text_id
           << '\t' << query.source_start << '\t' << query.source_end << '\t'
           << query.start_sec << '\t' << query.end_sec << '\t'
           << (evidence.embedding_available ? 1 : 0) << '\t'
           << (evidence.robust_gallery_complete ? 1 : 0) << '\t';
    WriteScores(output, evidence.session_scores);
    output << '\t';
    WriteScores(output, evidence.robust_scores);
    output << '\n';
  }
}

SpeakerIdConfig MakeStageConfig(const AuditoryStream::Config& config,
                                int embedding_dim) {
  SpeakerIdConfig stage;
  stage.embedding_dim = embedding_dim;
  stage.match_threshold = config.speaker_match_threshold;
  stage.min_embed_sec = config.speaker_min_embed_sec;
  stage.min_confidence = config.speaker_min_confidence;
  stage.overlap_eps_sec = config.speaker_overlap_eps_sec;
  stage.max_ref_segs = config.speaker_max_ref_segs;
  stage.edge_margin_sec = config.speaker_edge_margin_sec;
  stage.max_embed_window_sec = config.speaker_max_embed_window_sec;
  stage.enroll_min_refs = config.speaker_enroll_min_refs;
  stage.cross_session_match_min_refs =
      config.speaker_cross_session_match_min_refs;
  stage.defer_unmatched_cross_session =
      config.speaker_defer_unmatched_cross_session;
  stage.retain_sec = config.speaker_retain_sec;
  stage.speakers_per_session = config.speaker_speakers_per_session;
  stage.merge_threshold = config.speaker_merge_threshold;
  stage.cosession_merge_threshold = config.speaker_cosession_merge_threshold;
  stage.local_drift_threshold = config.speaker_local_drift_threshold;
  stage.local_drift_min_span_sec = config.speaker_local_drift_min_span_sec;
  stage.local_drift_min_epoch_sec = config.speaker_local_drift_min_epoch_sec;
  stage.local_drift_allow_same_session_match =
      config.speaker_local_drift_allow_same_session_match;
  stage.local_drift_competing_threshold =
      config.speaker_local_drift_competing_threshold;
  stage.local_drift_competing_margin =
      config.speaker_local_drift_competing_margin;
  stage.local_drift_competing_min_span_sec =
      config.speaker_local_drift_competing_min_span_sec;
  stage.local_drift_competing_candidate_threshold =
      config.speaker_local_drift_competing_candidate_threshold;
  stage.local_drift_competing_candidate_margin =
      config.speaker_local_drift_competing_candidate_margin;
  stage.local_drift_competing_candidate_min_confirmations =
      config.speaker_local_drift_competing_candidate_min_confirmations;
  stage.local_drift_competing_backfill_sec =
      config.speaker_local_drift_competing_backfill_sec;
  stage.local_drift_competing_backfill_gap_sec =
      config.speaker_local_drift_competing_backfill_gap_sec;
  return stage;
}

std::vector<DiarSegment> VisibleSegments(
    const std::vector<DiarSegment>& segments, double now_sec,
    double history_sec) {
  std::vector<DiarSegment> visible;
  const double history_start = std::max(0.0, now_sec - history_sec);
  for (const auto& source : segments) {
    if (source.start_sec >= now_sec) break;
    if (source.end_sec <= history_start) continue;
    DiarSegment segment = source;
    segment.end_sec = std::min(segment.end_sec, now_sec);
    if (segment.end_sec > segment.start_sec) {
      visible.push_back(std::move(segment));
    }
  }
  return visible;
}

void WriteSegments(const std::string& path,
                   const std::vector<DiarSegment>& segments) {
  std::ofstream output(path);
  if (!output) throw std::runtime_error("cannot write output: " + path);
  output << "start_sec\tend_sec\tlocal_speaker\tconfidence\tspeaker_id\n";
  output.setf(std::ios::fixed);
  output.precision(6);
  for (const auto& segment : segments) {
    output << segment.start_sec << '\t' << segment.end_sec << '\t'
           << segment.local_speaker << '\t' << segment.confidence << '\t'
           << segment.speaker_id << '\n';
  }
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 7 && argc != 9) {
    std::fprintf(stderr,
                 "usage: %s <audio.mp3/wav> <titanet.safetensors> "
                 "<registry-before> <segments.csv> <out.tsv> <config.toml> "
                 "[queries.tsv evidence.tsv]\n",
                 argv[0]);
    return 2;
  }
  try {
    const std::string audio_path = argv[1];
    const std::string weights_path = argv[2];
    const std::string registry_path = argv[3];
    const std::string segments_path = argv[4];
    const std::string output_path = argv[5];
    const std::string config_path = argv[6];
    const std::string query_path = argc == 9 ? argv[7] : std::string();
    const std::string evidence_path = argc == 9 ? argv[8] : std::string();

    AuditoryStream::Config config;
    if (!orator::io::ApplyTomlConfig(config_path, config)) {
      throw std::runtime_error("cannot load config: " + config_path);
    }
    if (config.diar_deliver_interval_sec <= 0.0) {
      throw std::runtime_error("deliver_interval_sec must be positive");
    }

    auto audio = orator::io::LoadAudioMono(audio_path, config.sample_rate);
    auto segments = ReadSegments(segments_path);
    orator::model::TitaNetEmbedder embedder;
    embedder.LoadWeights(weights_path);
    orator::model::SpeakerDatabase database(/*max_speakers=*/256,
                                            embedder.dim());
    if (!database.Load(registry_path)) {
      throw std::runtime_error("cannot load registry: " + registry_path);
    }
    const orator::core::TimeBase time_base(config.sample_rate);
    SpeakerIdentityStage stage(
        &embedder, &database, time_base,
        MakeStageConfig(config, embedder.dim()));

    const long total_samples = static_cast<long>(audio.samples.size());
    const long delivery_samples = std::max<long>(
        1, static_cast<long>(std::llround(
               config.diar_deliver_interval_sec * config.sample_rate)));
    const double history_sec = std::max(
        config.speaker_retain_sec,
        config.speaker_local_drift_competing_backfill_sec +
            config.speaker_local_drift_competing_backfill_gap_sec);
    long consumed = 0;
    while (consumed < total_samples) {
      const long next = std::min(total_samples, consumed + delivery_samples);
      stage.AppendAudio(audio.samples.data() + consumed,
                        static_cast<int>(next - consumed));
      consumed = next;
      const double now_sec = time_base.SecondsAt(consumed);
      auto visible = VisibleSegments(segments, now_sec, history_sec);
      stage.Process(visible);
    }

    // Old audio is intentionally unavailable here, so this final projection
    // cannot create historical evidence. It only applies the frozen epochs to
    // every final segment for business-view replay.
    stage.Process(segments);
    WriteSegments(output_path, segments);
    if (!query_path.empty()) {
      const auto queries = ReadEvidenceQueries(query_path);
      std::set<std::string> active_set;
      for (const auto& segment : segments) {
        if (!segment.speaker_id.empty()) active_set.insert(segment.speaker_id);
      }
      const std::vector<std::string> active_ids(active_set.begin(),
                                                active_set.end());
      WriteEvidenceQueries(evidence_path, &stage, queries, active_ids, config);
    }
    std::printf(
        "segments=%zu audio_sec=%.3f registry_before=%s enrolled_after=%d "
        "out=%s evidence=%s\n",
        segments.size(), audio.DurationSec(), registry_path.c_str(),
        stage.enrolled_count(), output_path.c_str(),
        evidence_path.empty() ? "(none)" : evidence_path.c_str());
    return 0;
  } catch (const std::exception& error) {
    std::fprintf(stderr, "speaker identity replay probe: %s\n", error.what());
    return 1;
  }
}
