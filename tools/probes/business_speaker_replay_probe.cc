// Replay frozen producer tracks through the production business-speaker view.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "core/time_base.h"
#include "io/config_reader.h"
#include "pipeline/auditory_stream.h"
#include "pipeline/business_speaker_pipeline.h"
#include "pipeline/comprehensive_timeline.h"
#include "pipeline/json_util.h"

namespace {

using orator::pipeline::AuditoryStream;
using orator::pipeline::BusinessSpeakerPipeline;
using orator::pipeline::ComprehensiveTimeline;

std::vector<std::string> SplitTab(const std::string& line) {
  std::vector<std::string> out;
  std::size_t start = 0;
  while (true) {
    const std::size_t tab = line.find('\t', start);
    out.push_back(line.substr(start, tab - start));
    if (tab == std::string::npos) break;
    start = tab + 1;
  }
  return out;
}

std::vector<std::string> SplitComma(const std::string& line) {
  std::vector<std::string> out;
  std::size_t start = 0;
  while (true) {
    const std::size_t comma = line.find(',', start);
    out.push_back(line.substr(start, comma - start));
    if (comma == std::string::npos) break;
    start = comma + 1;
  }
  return out;
}

int HexNibble(char value) {
  if (value >= '0' && value <= '9') return value - '0';
  if (value >= 'a' && value <= 'f') return value - 'a' + 10;
  if (value >= 'A' && value <= 'F') return value - 'A' + 10;
  return -1;
}

std::string DecodeHex(const std::string& value) {
  if (value.size() % 2 != 0) throw std::runtime_error("odd text hex length");
  std::string out;
  out.reserve(value.size() / 2);
  for (std::size_t i = 0; i < value.size(); i += 2) {
    const int high = HexNibble(value[i]);
    const int low = HexNibble(value[i + 1]);
    if (high < 0 || low < 0) throw std::runtime_error("invalid text hex");
    out.push_back(static_cast<char>((high << 4) | low));
  }
  return out;
}

std::vector<ComprehensiveTimeline::SpeakerInput> ReadDiar(
    const std::string& path) {
  std::ifstream input(path);
  if (!input) throw std::runtime_error("cannot open diar TSV: " + path);
  std::vector<ComprehensiveTimeline::SpeakerInput> out;
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty()) continue;
    const auto columns = SplitTab(line);
    if (columns[0] == "start_sec") continue;
    if (columns.size() != 5) throw std::runtime_error("invalid diar TSV row");
    ComprehensiveTimeline::SpeakerInput item;
    item.start = std::stod(columns[0]);
    item.end = std::stod(columns[1]);
    const int local = std::stoi(columns[2]);
    item.speaker = "speaker_" + std::to_string(local);
    item.conf = std::stof(columns[3]);
    item.speaker_id = columns[4];
    out.push_back(std::move(item));
  }
  return out;
}

std::vector<ComprehensiveTimeline::RawTextSeg> ReadAsr(
    const std::string& path) {
  std::ifstream input(path);
  if (!input) throw std::runtime_error("cannot open ASR TSV: " + path);
  std::vector<ComprehensiveTimeline::RawTextSeg> out;
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty()) continue;
    const auto columns = SplitTab(line);
    if (columns[0] == "text_id") continue;
    if (columns.size() != 4) throw std::runtime_error("invalid ASR TSV row");
    ComprehensiveTimeline::RawTextSeg item;
    item.id = std::stol(columns[0]);
    item.start = std::stod(columns[1]);
    item.end = std::stod(columns[2]);
    item.text = DecodeHex(columns[3]);
    out.push_back(std::move(item));
  }
  return out;
}

std::vector<ComprehensiveTimeline::AlignGroup> ReadAlign(
    const std::string& path) {
  std::ifstream input(path);
  if (!input) throw std::runtime_error("cannot open align TSV: " + path);
  std::map<long, ComprehensiveTimeline::AlignGroup> grouped;
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty()) continue;
    const auto columns = SplitTab(line);
    if (columns[0] == "text_id") continue;
    if (columns.size() != 6) {
      throw std::runtime_error("invalid align TSV row");
    }
    const long text_id = std::stol(columns[0]);
    auto& group = grouped[text_id];
    group.text_id = text_id;
    group.start = std::stod(columns[1]);
    group.end = std::stod(columns[2]);
    group.units.push_back({std::stod(columns[3]), std::stod(columns[4]),
                           DecodeHex(columns[5])});
  }
  std::vector<ComprehensiveTimeline::AlignGroup> out;
  out.reserve(grouped.size());
  for (auto& item : grouped) out.push_back(std::move(item.second));
  return out;
}

std::vector<ComprehensiveTimeline::VoiceprintScore> ParseScores(
    const std::string& value) {
  std::vector<ComprehensiveTimeline::VoiceprintScore> out;
  std::size_t cursor = 0;
  while (cursor < value.size()) {
    const std::size_t comma = value.find(',', cursor);
    const std::string item = value.substr(cursor, comma - cursor);
    const std::size_t colon = item.rfind(':');
    if (colon == std::string::npos) {
      throw std::runtime_error("invalid voiceprint score list");
    }
    out.push_back({item.substr(0, colon), std::stof(item.substr(colon + 1))});
    if (comma == std::string::npos) break;
    cursor = comma + 1;
  }
  return out;
}

std::vector<ComprehensiveTimeline::SpeakerVoiceprintEvidence> ReadVoiceprint(
    const std::string& path) {
  std::ifstream input(path);
  if (!input) throw std::runtime_error("cannot open voiceprint TSV: " + path);
  std::vector<ComprehensiveTimeline::SpeakerVoiceprintEvidence> out;
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty()) continue;
    const auto columns = SplitTab(line);
    if (columns[0] == "evidence_id") continue;
    if (columns.size() != 11 && columns.size() != 12) {
      throw std::runtime_error("invalid voiceprint TSV row");
    }
    ComprehensiveTimeline::SpeakerVoiceprintEvidence item;
    item.evidence_id = columns[0];
    item.kind = columns[1];
    item.text_id = std::stol(columns[2]);
    item.source_start = std::stoi(columns[3]);
    item.source_end = std::stoi(columns[4]);
    item.start = std::stod(columns[5]);
    item.end = std::stod(columns[6]);
    item.embedding_available = columns[7] == "1";
    const bool explicit_session_complete = columns.size() == 12;
    item.session_gallery_complete =
        explicit_session_complete && columns[8] == "1";
    const std::size_t robust_index = explicit_session_complete ? 9 : 8;
    item.robust_gallery_complete = columns[robust_index] == "1";
    item.session_scores = ParseScores(columns[robust_index + 1]);
    item.robust_scores = ParseScores(columns[robust_index + 2]);
    out.push_back(std::move(item));
  }
  return out;
}

std::vector<ComprehensiveTimeline::DiarFrameBlock> ReadDiarFrames(
    const std::string& path) {
  constexpr double kCsvTimeToleranceSec = 2e-6;
  std::ifstream input(path);
  if (!input) throw std::runtime_error("cannot open diar frame CSV: " + path);

  std::string line;
  if (!std::getline(input, line)) {
    throw std::runtime_error("empty diar frame CSV");
  }
  const auto header = SplitComma(line);
  const std::vector<std::string> prefix = {
      "frame",       "time_sec",  "session",     "top1", "top1_prob",
      "top2",        "top2_prob", "margin",      "active_count"};
  if (header.size() <= prefix.size() ||
      !std::equal(prefix.begin(), prefix.end(), header.begin())) {
    throw std::runtime_error("invalid diar frame CSV header");
  }
  const int num_speakers =
      static_cast<int>(header.size() - prefix.size());
  for (int speaker = 0; speaker < num_speakers; ++speaker) {
    if (header[prefix.size() + speaker] !=
        "spk" + std::to_string(speaker)) {
      throw std::runtime_error("invalid diar frame probability columns");
    }
  }

  struct FrameRow {
    long frame = -1;
    double time = 0.0;
    int session = -1;
    std::vector<float> probabilities;
  };
  std::vector<FrameRow> rows;
  while (std::getline(input, line)) {
    if (line.empty()) continue;
    const auto columns = SplitComma(line);
    if (columns.size() != header.size()) {
      throw std::runtime_error("invalid diar frame CSV row width");
    }
    FrameRow row;
    row.frame = std::stol(columns[0]);
    row.time = std::stod(columns[1]);
    row.session = std::stoi(columns[2]);
    if (row.frame < 0 || !std::isfinite(row.time) || row.time < 0.0 ||
        row.session < 0) {
      throw std::runtime_error("invalid diar frame coordinates");
    }
    row.probabilities.reserve(num_speakers);
    for (int speaker = 0; speaker < num_speakers; ++speaker) {
      const float probability =
          std::stof(columns[prefix.size() + speaker]);
      if (!std::isfinite(probability) || probability < 0.0f ||
          probability > 1.0f) {
        throw std::runtime_error("invalid diar frame probability");
      }
      row.probabilities.push_back(probability);
    }
    if (!rows.empty()) {
      const auto& previous = rows.back();
      if (row.frame != previous.frame + 1 || row.session < previous.session ||
          row.time <= previous.time) {
        throw std::runtime_error("non-monotonic diar frame CSV");
      }
    }
    rows.push_back(std::move(row));
  }
  if (rows.empty()) throw std::runtime_error("insufficient diar frame rows");

  std::vector<ComprehensiveTimeline::DiarFrameBlock> blocks;
  std::optional<double> fallback_period_sec;
  for (std::size_t begin = 0; begin < rows.size();) {
    std::size_t end = begin + 1;
    while (end < rows.size() &&
           rows[end].session == rows[begin].session) {
      ++end;
    }
    if (end - begin > 1) {
      fallback_period_sec =
          (rows[end - 1].time - rows[begin].time) /
          static_cast<double>(end - begin - 1);
      break;
    }
    begin = end;
  }
  if (!fallback_period_sec || *fallback_period_sec <= 0.0) {
    throw std::runtime_error("insufficient diar frame period evidence");
  }

  for (std::size_t begin = 0; begin < rows.size();) {
    std::size_t end = begin + 1;
    while (end < rows.size() &&
           rows[end].session == rows[begin].session) {
      ++end;
    }
    const double frame_period_sec =
        end - begin > 1
            ? (rows[end - 1].time - rows[begin].time) /
                  static_cast<double>(end - begin - 1)
            : *fallback_period_sec;
    if (frame_period_sec <= 0.0) {
      throw std::runtime_error("invalid diar frame period");
    }
    ComprehensiveTimeline::DiarFrameBlock block;
    block.start = rows[begin].time;
    block.frame_period_sec = frame_period_sec;
    block.num_frames = static_cast<int>(end - begin);
    block.num_speakers = num_speakers;
    block.local_speaker_offset = rows[begin].session * num_speakers;
    block.probabilities.reserve(
        static_cast<std::size_t>(block.num_frames) * num_speakers);
    for (std::size_t index = begin; index < end; ++index) {
      const double expected_time =
          block.start + (index - begin) * block.frame_period_sec;
      if (std::abs(rows[index].time - expected_time) >
          kCsvTimeToleranceSec) {
        throw std::runtime_error("non-contiguous diar frame session");
      }
      if (index > begin &&
          std::abs((rows[index].time - rows[index - 1].time) -
                   block.frame_period_sec) > kCsvTimeToleranceSec) {
        throw std::runtime_error("inconsistent diar frame period");
      }
      block.probabilities.insert(block.probabilities.end(),
                                 rows[index].probabilities.begin(),
                                 rows[index].probabilities.end());
    }
    blocks.push_back(std::move(block));
    begin = end;
  }
  return blocks;
}

int SpeakerIndex(const std::string& speaker) {
  if (speaker.rfind("speaker_", 0) != 0) return -1;
  return std::stoi(speaker.substr(8));
}

void WriteCandidate(const std::string& path,
                    const std::vector<ComprehensiveTimeline::Entry>& entries,
                    double audio_sec, int sample_rate) {
  std::ofstream output(path);
  if (!output) throw std::runtime_error("cannot write candidate: " + path);
  output << std::fixed << std::setprecision(6);
  output << "{\n  \"schema_version\": 1,\n"
            "  \"kind\": \"orator_frozen_speaker_candidate\",\n"
            "  \"candidate_kind\": "
            "\"production_business_speaker_replay\",\n"
         << "  \"audio_sec\": " << audio_sec << ",\n"
         << "  \"sample_rate\": " << sample_rate << ",\n";
  std::set<std::string> active_speaker_ids;
  for (const auto& entry : entries) {
    if (!entry.speaker_id.empty()) {
      active_speaker_ids.insert(entry.speaker_id);
    }
  }
  output << "  \"active_speaker_ids\": [";
  std::size_t active_index = 0;
  for (const auto& speaker_id : active_speaker_ids) {
    if (active_index++ > 0) output << ", ";
    output << "\"" << orator::pipeline::JsonEscape(speaker_id) << "\"";
  }
  output << "],\n"
         << "  \"turn_count\": " << entries.size() << ",\n"
            "  \"track\": [\n";
  for (std::size_t i = 0; i < entries.size(); ++i) {
    const auto& entry = entries[i];
    output << "    {\"turn_id\": \"business_speaker_replay:" << i
           << "\", \"start\": " << entry.start << ", \"end\": "
           << entry.end << ", \"text_id\": " << entry.text_id
           << ", \"text\": \""
           << orator::pipeline::JsonEscape(entry.text)
           << "\", \"speaker\": " << SpeakerIndex(entry.speaker);
    if (!entry.speaker_id.empty()) {
      output << ", \"speaker_id\": \""
             << orator::pipeline::JsonEscape(entry.speaker_id) << "\"";
    }
    output << ", \"speaker_uncertain\": "
           << (entry.speaker_uncertain ? "true" : "false")
           << ", \"decision_reason\": \""
           << orator::pipeline::JsonEscape(entry.speaker_decision.reason)
           << "\"}" << (i + 1 < entries.size() ? "," : "") << "\n";
  }
  output << "  ]\n}\n";
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 6) {
    std::fprintf(stderr,
                 "usage: %s <diar.tsv> <asr.tsv> <align.tsv> <out.json> "
                 "<audio_sec> [config=orator.toml] [primary.tsv] "
                 "[voiceprint.tsv] [diar_frames.csv]\n",
                 argv[0]);
    return 2;
  }
  try {
    const std::string diar_path = argv[1];
    const std::string asr_path = argv[2];
    const std::string align_path = argv[3];
    const std::string output_path = argv[4];
    const double audio_sec = std::stod(argv[5]);
    const std::string config_path = argc > 6 ? argv[6] : "orator.toml";

    AuditoryStream::Config config;
    if (!orator::io::ApplyTomlConfig(config_path, config)) {
      throw std::runtime_error("cannot load config: " + config_path);
    }
    BusinessSpeakerPipeline::Config business_config;
    business_config.align_snap_pause_sec =
        config.timeline_align_snap_pause_sec;
    business_config.align_boundary_split_tolerance_sec =
        config.timeline_align_boundary_split_tolerance_sec;
    business_config.speaker_support_min_coverage_ratio =
        config.timeline_speaker_support_min_coverage_ratio;
    business_config.speaker_support_max_gap_sec =
        config.timeline_speaker_support_max_gap_sec;
    business_config.speaker_support_max_islands =
        config.timeline_speaker_support_max_islands;
    business_config.gap_fill_enabled = config.timeline_gap_fill_enabled;
    business_config.voiceprint_fusion_enabled = config.speaker_fusion_enable;
    business_config.voiceprint_short_max_sec =
        config.speaker_fusion_short_max_sec;
    business_config.voiceprint_short_min_score =
        config.speaker_fusion_short_min_score;
    business_config.voiceprint_short_min_margin =
        config.speaker_fusion_short_min_margin;
    business_config.voiceprint_regular_min_score =
        config.speaker_fusion_regular_min_score;
    business_config.voiceprint_regular_min_margin =
        config.speaker_fusion_regular_min_margin;
    business_config.voiceprint_primary_consensus_min_sec =
        config.speaker_fusion_min_embed_sec;
    business_config.voiceprint_phrase_max_sec =
        config.speaker_fusion_phrase_max_sec;
    business_config.voiceprint_four_view_min_aligned_units =
        config.speaker_fusion_four_view_min_aligned_units;
    business_config.voiceprint_future_epoch_lookahead_sec =
        config.speaker_fusion_future_epoch_lookahead_sec;
    business_config.posterior_future_epoch_enabled =
        config.speaker_fusion_posterior_future_epoch_enable;
    business_config.source_leading_primary_prefix_enabled =
        config.speaker_fusion_source_leading_primary_prefix_enable;
    business_config.right_bounded_short_primary_unit_enabled =
        config.speaker_fusion_right_bounded_short_primary_unit_enable;
    business_config.posterior_frame_activity_threshold =
        config.speaker_fusion_frame_activity_threshold;
    business_config.posterior_identity_backfill_sec =
        config.speaker_local_drift_competing_backfill_sec;
    business_config.voiceprint_punctuation = config.speaker_fusion_punctuation;
    if (config.timeline_speaker_overlap_tie_policy == "higher_confidence") {
      business_config.speaker_overlap_tie_policy =
          BusinessSpeakerPipeline::SpeakerOverlapTiePolicy::kHigherConfidence;
    } else if (config.timeline_speaker_overlap_tie_policy ==
               "primary_speaker") {
      business_config.speaker_overlap_tie_policy =
          BusinessSpeakerPipeline::SpeakerOverlapTiePolicy::kPrimarySpeaker;
    } else {
      business_config.speaker_overlap_tie_policy =
          BusinessSpeakerPipeline::SpeakerOverlapTiePolicy::kShorterSpan;
    }

    ComprehensiveTimeline timeline;
    BusinessSpeakerPipeline business(
        &timeline, business_config, orator::core::TimeBase(config.sample_rate),
        [&timeline](const ComprehensiveTimeline::Revision& revision) {
          timeline.DepositBusinessSpeakerRevision(revision);
        });
    business.Start();
    const auto diar = ReadDiar(diar_path);
    const auto primary =
        argc > 7
            ? ReadDiar(argv[7])
            : std::vector<ComprehensiveTimeline::SpeakerInput>{};
    const auto asr = ReadAsr(asr_path);
    const auto align = ReadAlign(align_path);
    const auto voiceprint =
        argc > 8
            ? ReadVoiceprint(argv[8])
            : std::vector<
                  ComprehensiveTimeline::SpeakerVoiceprintEvidence>{};
    const auto diar_frames =
        argc > 9
            ? ReadDiarFrames(argv[9])
            : std::vector<ComprehensiveTimeline::DiarFrameBlock>{};
    for (const auto& block : diar_frames) {
      if (timeline.DepositDiarFrameBlock(block) !=
          ComprehensiveTimeline::DepositResult::kInserted) {
        throw std::runtime_error("rejected diar frame block");
      }
    }
    timeline.DepositDiarization(diar);
    if (!primary.empty()) timeline.DepositPrimarySpeaker(primary);
    for (const auto& item : asr) timeline.DepositAsrFinal(item);
    for (const auto& item : align) timeline.DepositAlignment(item);
    if (!voiceprint.empty()) {
      timeline.DepositSpeakerVoiceprint(voiceprint);
    }
    const long total_samples = static_cast<long>(
        audio_sec * static_cast<double>(config.sample_rate) + 0.5);
    business.Finalize(total_samples);
    business.Stop();
    const auto entries = timeline.Snapshot();
    WriteCandidate(output_path, entries, audio_sec, config.sample_rate);
    std::printf(
        "diar=%zu primary=%zu asr=%zu align=%zu voiceprint=%zu "
        "frame_blocks=%zu business=%zu out=%s\n",
        diar.size(), primary.size(), asr.size(), align.size(),
        voiceprint.size(), diar_frames.size(), entries.size(),
        output_path.c_str());
    return 0;
  } catch (const std::exception& error) {
    std::fprintf(stderr, "business speaker replay probe: %s\n", error.what());
    return 1;
  }
}
