// Serialization functions for AuditoryStream.
//
// Extracted from auditory_stream.cc to keep the controller focused on lifecycle.
// All three functions are AuditoryStream member functions declared in
// include/pipeline/auditory_stream.h.

#include "pipeline/auditory_stream.h"

#include "core/log.h"
#include "core/types.h"
#include "pipeline/comprehensive_timeline.h"
#include "pipeline/json_util.h"

#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <vector>

namespace orator {
namespace pipeline {

// Shared revision serializer defined in src/pipeline/json_util.cc.
std::string SerializeRevisionToJson(
    const ComprehensiveTimeline::Revision& r, const char* source);

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
                  e.stream_active ? "true" : "false", e.cuda_priority,
                  compute, rtf);
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
  // Read both result sets from the timeline store under its lock, then build the
  // timeline document. The document has a shared time axis and three parts:
  //   - "tracks": one independent track per pipeline (diarization, asr), each a
  //     list of that pipeline's time-ordered entries.
  //   - "comprehensive": a derived view that attributes each ASR utterance to
  //     the diarization speaker with the greatest temporal overlap and groups
  //     consecutive same-speaker utterances. Its unit is the speaker turn: who
  //     spoke, from when to when, and the text spoken. Attribution is at
  //     utterance granularity (the ASR engine does not emit per-word times).
  // A consumer reads whichever part it needs. Adding a future pipeline adds a
  // track and, optionally, a contribution to the comprehensive view.
  // StreamTimeline removed — ASR track data now comes from comp_.SnapshotRawTexts().
  // Spec 004 Step 2: the diarization worker is the sole producer of the speaker
  // view (it delivers live via ReplaceSpeakers + keeps last_segments_ fresh);
  // the ASR worker delivers text live via UpsertText. So Serialize is a pure
  // reader: snapshot everything under comp_mutex_. No derivation, no upserts here.
  std::vector<core::DiarSegment> diar_view;
  std::vector<ComprehensiveTimeline::Entry> comp_view;
  std::vector<ComprehensiveTimeline::VadSeg> vad_view;
  std::vector<ComprehensiveTimeline::RawTextSeg> raw_texts;
  {
    std::lock_guard<std::mutex> lk(comp_mutex_);
    diar_view = last_segments_;
    comp_view = comp_.Snapshot();
    vad_view = comp_.SnapshotVad();
    raw_texts = comp_.SnapshotRawTexts();
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
                  "{\"start\":%.3f,\"end\":%.3f,\"speaker\":%d,"
                  "\"confidence\":%.3f}",
                  s.start_sec, s.end_sec, s.local_speaker, s.confidence);
    out += buf;
    if (i + 1 < diar_view.size()) out += ",";
  }
  out += "]}";

  // Track: automatic speech recognition (present only when ASR is enabled).
  if (asr_) {
    std::snprintf(buf, sizeof(buf),
                  ",{\"kind\":\"asr\",\"source\":\"qwen3_asr\","
                  "\"compute_sec\":%.3f,\"real_time_factor\":%.3f,\"entries\":[",
                  asr_c, asr_c > 0 ? audio / asr_c : 0.0);
    out += buf;
    for (size_t i = 0; i < last_transcript_.tokens.size(); ++i) {
      const auto& t = last_transcript_.tokens[i];
      std::snprintf(buf, sizeof(buf), "{\"start\":%.3f,\"end\":%.3f,\"text\":\"",
                    t.start_sec, t.end_sec);
      out += std::string(buf) + JsonEscape(t.text) + "\"}";
      if (i + 1 < last_transcript_.tokens.size()) out += ",";
    }
    out += "]}";
  }

  // Track: voice activity (VAD). Present only when the VAD pipeline is enabled.
  if (config_.vad_stream) {
    const double vad_c = vad_detector_ ? vad_detector_->compute_sec() : 0.0;
    std::snprintf(buf, sizeof(buf),
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
  out += "]";  // close "tracks"

  // Comprehensive view: speaker turns with their spoken text, ordered by time.
  out += ",\"comprehensive\":[";
  if (asr_) {
    for (size_t i = 0; i < comp_view.size(); ++i) {
      const auto& e = comp_view[i];
      // Extract numeric speaker index from "speaker_N" format.
      int spk_idx = -1;
      if (e.speaker.size() > 8 && e.speaker.substr(0, 8) == "speaker_") {
      try { spk_idx = std::stoi(e.speaker.substr(8)); }
      catch (const std::invalid_argument&) { spk_idx = -1; }
      catch (const std::out_of_range&) { spk_idx = -1; }
      }
      std::snprintf(buf, sizeof(buf),
                    "{\"start\":%.3f,\"end\":%.3f,\"text_id\":%ld,"
                    "\"speaker\":%d,\"text\":\"",
                    e.start, e.end, e.text_id, spk_idx);
      out += std::string(buf) + JsonEscape(e.text) + "\"}";
      if (i + 1 < comp_view.size()) out += ",";
    }
  }
  out += "]}";
  return out;
}

}  // namespace pipeline
}  // namespace orator
