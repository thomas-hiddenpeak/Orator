// Serialization functions for AuditoryStream.
//
// Extracted from auditory_stream.cc to keep the controller focused on
// lifecycle. All three functions are AuditoryStream member functions declared
// in include/pipeline/auditory_stream.h.

#include "pipeline/auditory_stream.h"

#include "core/log.h"
#include "core/types.h"
#include "model/speaker_database.h"
#include "pipeline/comprehensive_timeline.h"
#include "pipeline/json_util.h"

#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace orator {
namespace pipeline {

// Shared revision serializer defined in src/pipeline/json_util.cc.
std::string SerializeRevisionToJson(
    const ComprehensiveTimeline::Revision& r, const char* source,
    const std::map<std::string, std::string>* label_ids = nullptr);

// ---------------------------------------------------------------------------
// Serialize one revision (Spec 004) to a {"type":"revision",...} message.
// Delegates to the shared SerializeRevisionToJson helper in json_util.
// ---------------------------------------------------------------------------
std::string AuditoryStream::SerializeRevision(
    const ComprehensiveTimeline::Revision& r, const char* source) {
  return SerializeRevisionToJson(r, source);
}

// ---------------------------------------------------------------------------
// Serialize GPU telemetry snapshot (Spec 002 FR7).
// ---------------------------------------------------------------------------
std::string AuditoryStream::SerializeGpuTelemetry() const {
  const auto entries = scheduler_.Snapshot();
  const double audio = audio_sec();
  char buf[256];
  std::string out = "{\"type\":\"gpu_telemetry\",";
  std::snprintf(buf, sizeof(buf), "\"time_sec\":%.3f,\"sample_rate\":%d,",
                audio, config_.sample_rate);
  out += buf;
  out += "\"pipelines\":[";
  for (size_t i = 0; i < entries.size(); ++i) {
    const auto& e = entries[i];
    double compute = 0.0;
    if (e.name == "diarization")
      compute = diar_worker_ ? diar_worker_->compute_sec() : 0.0;
    else if (e.name == "asr")
      compute = asr_worker_ ? asr_worker_->compute_sec() : 0.0;
    else if (e.name == "vad")
      compute = vad_detector_ ? vad_detector_->compute_sec() : 0.0;
    const double rtf = compute > 0.0 ? audio / compute : 0.0;
    std::snprintf(buf, sizeof(buf),
                  "{\"name\":\"%s\",\"priority_index\":%d,\"class\":\"%s\","
                  "\"stream_active\":%s,\"cuda_priority\":%d,"
                  "\"compute_sec\":%.3f,\"real_time_factor\":%.3f}",
                  e.name.c_str(), e.priority_index,
                  e.background ? "background" : "foreground",
                  e.stream_active ? "true" : "false", e.cuda_priority, compute,
                  rtf);
    out += buf;
    if (i + 1 < entries.size()) out += ",";
  }
  out += "]}";
  return out;
}

// ---------------------------------------------------------------------------
// Serialize the comprehensive timeline JSON document.
// ---------------------------------------------------------------------------
std::string AuditoryStream::Serialize() {
  // Read both result sets from the timeline store under its lock, then build
  // the timeline document. The document has a shared time axis and three parts:
  //   - "tracks": one independent track per pipeline (diarization, asr), each a
  //     list of that pipeline's time-ordered entries.
  //   - "comprehensive": a derived view that attributes each ASR utterance to
  //     the diarization speaker with the greatest temporal overlap and groups
  //     consecutive same-speaker utterances. Its unit is the speaker turn: who
  //     spoke, from when to when, and the text spoken. Attribution is at
  //     utterance granularity (the ASR engine does not emit per-word times).
  // A consumer reads whichever part it needs. Adding a future pipeline adds a
  // track and, optionally, a contribution to the comprehensive view.
  // StreamTimeline removed — ASR track data now comes from
  // comp_.SnapshotRawTexts(). Spec 004 Step 2: the diarization worker is the
  // sole producer of the speaker view (it delivers live via ReplaceSpeakers +
  // keeps last_segments_ fresh); the ASR worker delivers text live via
  // UpsertText. So Serialize is a pure reader: snapshot everything under
  // comp_mutex_. No derivation, no upserts here.
  std::vector<core::DiarSegment> diar_view;
  std::vector<ComprehensiveTimeline::Entry> comp_view;
  std::vector<ComprehensiveTimeline::VadSeg> vad_view;
  std::vector<ComprehensiveTimeline::RawTextSeg> raw_texts;
  std::vector<ComprehensiveTimeline::AlignGroup> align_view;
  std::map<std::string, std::string> speaker_label_ids;
  {
    std::lock_guard<std::mutex> lk(comp_mutex_);
    diar_view = last_segments_;
    comp_view = comp_.Snapshot();
    vad_view = comp_.SnapshotVad();
    raw_texts = comp_.SnapshotRawTexts();
    align_view = comp_.SnapshotAlign();
    speaker_label_ids = comp_.SpeakerLabelIds();
  }
  // Populate last_transcript_ from raw_texts for the transcript() accessor.
  {
    core::Transcript transcript;
    for (const auto& r : raw_texts) {
      core::AsrToken tok;
      tok.start_sec = r.start;
      tok.end_sec = r.end;
      tok.text = r.text;
      transcript.tokens.push_back(std::move(tok));
    }
    last_transcript_ = std::move(transcript);
  }

  const double audio = audio_sec();
  const double diar_c = diar_compute_sec();
  const double asr_c = asr_compute_sec();
  const double wall_start = session_start_wall_sec_.load();
  const bool wclk_ok = wall_clock_ok_.load();

  char buf[256];
  std::string out = "{\"type\":\"timeline\",\"schema_version\":1,";
  std::snprintf(buf, sizeof(buf),
                "\"audio_sec\":%.3f,\"sample_rate\":%d,"
                "\"session_start_wall_sec\":%.3f,\"wall_clock_ok\":%s,",
                audio, config_.sample_rate, wall_start,
                wclk_ok ? "true" : "false");
  out += buf;
  out += "\"tracks\":[";

  // Track: speaker diarization.
  std::snprintf(buf, sizeof(buf),
                "{\"kind\":\"diarization\",\"source\":\"sortformer\","
                "\"compute_sec\":%.3f,\"real_time_factor\":%.3f,\"entries\":[",
                diar_c, diar_c > 0 ? audio / diar_c : 0.0);
  out += buf;
  for (size_t i = 0; i < diar_view.size(); ++i) {
    const auto& s = diar_view[i];
    std::snprintf(buf, sizeof(buf),
                  "{\"start\":%.3f,\"end\":%.3f,\"speaker\":%d",
                  s.start_sec, s.end_sec, s.local_speaker);
    out += buf;
    // Spec 010: surface the resolved global voiceprint identity (and optional
    // display name) alongside the diarizer-local index (backward compatible:
    // "speaker" stays integer).
    if (!s.speaker_id.empty()) {
      out += ",\"speaker_id\":\"" + s.speaker_id + "\"";
      const std::string nm =
          speaker_db_ ? speaker_db_->DisplayName(s.speaker_id) : std::string();
      if (!nm.empty()) out += ",\"speaker_name\":\"" + JsonEscape(nm) + "\"";
    }
    std::snprintf(buf, sizeof(buf), ",\"confidence\":%.3f}", s.confidence);
    out += buf;
    if (i + 1 < diar_view.size()) out += ",";
  }
  out += "]}";

  // Track: automatic speech recognition (present only when ASR is enabled).
  if (asr_) {
    std::snprintf(
        buf, sizeof(buf),
        ",{\"kind\":\"asr\",\"source\":\"qwen3_asr\","
        "\"compute_sec\":%.3f,\"real_time_factor\":%.3f,\"entries\":[",
        asr_c, asr_c > 0 ? audio / asr_c : 0.0);
    out += buf;
    for (size_t i = 0; i < last_transcript_.tokens.size(); ++i) {
      const auto& t = last_transcript_.tokens[i];
      std::snprintf(buf, sizeof(buf),
                    "{\"start\":%.3f,\"end\":%.3f,\"text\":\"", t.start_sec,
                    t.end_sec);
      out += std::string(buf) + JsonEscape(t.text) + "\"}";
      if (i + 1 < last_transcript_.tokens.size()) out += ",";
    }
    out += "]}";
  }

  // Track: voice activity (VAD). Present only when the VAD pipeline is enabled.
  if (config_.vad_stream) {
    const double vad_c = vad_detector_ ? vad_detector_->compute_sec() : 0.0;
    std::snprintf(
        buf, sizeof(buf),
        ",{\"kind\":\"vad\",\"source\":\"silero_gpu\","
        "\"compute_sec\":%.3f,\"real_time_factor\":%.3f,\"entries\":[",
        vad_c, vad_c > 0 ? audio / vad_c : 0.0);
    out += buf;
    for (size_t i = 0; i < vad_view.size(); ++i) {
      std::snprintf(buf, sizeof(buf), "{\"start\":%.3f,\"end\":%.3f}",
                    vad_view[i].start, vad_view[i].end);
      out += buf;
      if (i + 1 < vad_view.size()) out += ",";
    }
    out += "]}";
  }

  // Track: forced alignment (present only when the aligner is enabled). Refines
  // each ASR segment into per-unit timestamps on the common time base, grouped
  // by the source text_id so consumers can tie units back to the asr track.
  if (aligner_) {
    const double align_c = align_worker_ ? align_worker_->compute_sec() : 0.0;
    std::snprintf(
        buf, sizeof(buf),
        ",{\"kind\":\"align\",\"source\":\"qwen3_forced_aligner\","
        "\"compute_sec\":%.3f,\"real_time_factor\":%.3f,\"entries\":[",
        align_c, align_c > 0 ? audio / align_c : 0.0);
    out += buf;
    for (size_t i = 0; i < align_view.size(); ++i) {
      const auto& g = align_view[i];
      std::snprintf(buf, sizeof(buf),
                    "{\"text_id\":%ld,\"start\":%.3f,\"end\":%.3f,\"units\":[",
                    g.text_id, g.start, g.end);
      out += buf;
      for (size_t j = 0; j < g.units.size(); ++j) {
        const auto& u = g.units[j];
        std::snprintf(buf, sizeof(buf),
                      "{\"start\":%.3f,\"end\":%.3f,\"text\":\"", u.start,
                      u.end);
        out += std::string(buf) + JsonEscape(u.text) + "\"}";
        if (j + 1 < g.units.size()) out += ",";
      }
      out += "]}";
      if (i + 1 < align_view.size()) out += ",";
    }
    out += "]}";
  }
  out += "]";  // close "tracks"

  // Comprehensive view: speaker turns with their spoken text, ordered by time.
  out += ",\"comprehensive\":[";
  if (asr_) {
    for (size_t i = 0; i < comp_view.size(); ++i) {
      const auto& e = comp_view[i];
      // Extract numeric speaker index from "speaker_N" format.
      int spk_idx = -1;
      if (e.speaker.size() > 8 && e.speaker.substr(0, 8) == "speaker_") {
        try {
          spk_idx = std::stoi(e.speaker.substr(8));
        } catch (const std::invalid_argument&) {
          spk_idx = -1;
        } catch (const std::out_of_range&) {
          spk_idx = -1;
        }
      }
      std::snprintf(buf, sizeof(buf),
                    "{\"start\":%.3f,\"end\":%.3f,\"text_id\":%ld,"
                    "\"speaker\":%d",
                    e.start, e.end, e.text_id, spk_idx);
      out += buf;
      // Spec 010: the comprehensive turn carries the resolved global voiceprint
      // id (and optional display name) when diarization resolved one.
      auto id_it = speaker_label_ids.find(e.speaker);
      if (id_it != speaker_label_ids.end() && !id_it->second.empty()) {
        out += ",\"speaker_id\":\"" + id_it->second + "\"";
        const std::string nm =
            speaker_db_ ? speaker_db_->DisplayName(id_it->second)
                        : std::string();
        if (!nm.empty()) out += ",\"speaker_name\":\"" + JsonEscape(nm) + "\"";
      }
      out += ",\"text\":\"" + JsonEscape(e.text) + "\"}";
      if (i + 1 < comp_view.size()) out += ",";
    }
  }
  out += "]}";
  return out;
}

// ---------------------------------------------------------------------------
// Spec 006: speaker registry list + rename for the Web UI naming panel.
// ---------------------------------------------------------------------------
std::string AuditoryStream::SerializeSpeakers() const {
  // List every global identity resolved anywhere in this session (accumulated
  // in the comprehensive timeline, read under comp_mutex_), joined with the
  // display names from the speaker database (its own name mutex). Using the
  // accumulated set — not the <=N current diarizer-slot mappings — keeps the
  // speaker panel consistent with the transcript, which accumulates ids the
  // same way. (This avoids racing the diarization thread on the database's
  // embedding/id vectors.)
  std::vector<std::string> id_list;
  {
    std::lock_guard<std::mutex> lk(comp_mutex_);
    id_list = comp_.AllSpeakerIds();
  }
  std::string out = "{\"type\":\"speakers\",\"speakers\":[";
  bool first = true;
  for (const auto& id : id_list) {
    if (id.empty()) continue;
    if (!first) out += ",";
    first = false;
    const std::string nm =
        speaker_db_ ? speaker_db_->DisplayName(id) : std::string();
    out += "{\"id\":\"" + id + "\",\"name\":\"" + JsonEscape(nm) + "\"}";
  }
  out += "]}";
  return out;
}

bool AuditoryStream::RenameSpeaker(const std::string& speaker_id,
                                   const std::string& name) {
  if (!speaker_db_ || speaker_id.empty()) return false;
  speaker_db_->SetDisplayName(speaker_id, name);  // name-mutex protected
  // Persist so the name survives a restart. The names sidecar is small; the
  // rename is user-initiated and rare, so the brief overlap with enrollment is
  // acceptable.
  if (!config_.speaker_registry_path.empty())
    speaker_db_->Save(config_.speaker_registry_path);
  return true;
}

}  // namespace pipeline
}  // namespace orator
