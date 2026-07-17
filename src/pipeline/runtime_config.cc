#include "pipeline/runtime_config.h"

#include <iomanip>
#include <locale>
#include <sstream>
#include <string>

#include "pipeline/json_util.h"

namespace orator {
namespace pipeline {
namespace {

class JsonObject {
 public:
  void Add(const std::string& key, const std::string& value) {
    AddRaw(key, "\"" + JsonEscape(value) + "\"");
  }

  void Add(const std::string& key, bool value) {
    AddRaw(key, value ? "true" : "false");
  }

  void Add(const std::string& key, int value) {
    AddRaw(key, std::to_string(value));
  }

  void Add(const std::string& key, size_t value) {
    AddRaw(key, std::to_string(value));
  }

  void Add(const std::string& key, double value) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(17) << value;
    AddRaw(key, out.str());
  }

  void AddObject(const std::string& key, const std::string& value) {
    AddRaw(key, value);
  }

  std::string Finish() { return json_ + "}"; }

 private:
  void AddRaw(const std::string& key, const std::string& value) {
    json_ += first_ ? "{" : ",";
    first_ = false;
    json_ += "\"" + JsonEscape(key) + "\":" + value;
  }

  std::string json_;
  bool first_ = true;
};

std::string GpuSchedulingName(int mode) {
  if (mode == 1) return "serial";
  if (mode == 2) return "concurrent";
  return "auto";
}

}  // namespace

std::string SerializeResolvedConfig(const AuditoryStream::Config& c) {
  JsonObject root;
  root.Add("schema_version", 1);
  root.Add("config_source_path", c.config_source_path);
  root.Add("sample_rate", c.sample_rate);

  JsonObject server;
  server.Add("port", c.port);
  server.Add("ui_port", c.ui_port);
  server.Add("ui_root", c.ui_root);
  root.AddObject("server", server.Finish());

  JsonObject hardware;
  hardware.Add("gpu_scheduling", GpuSchedulingName(c.gpu_scheduling_mode));
  root.AddObject("hardware", hardware.Finish());

  JsonObject asr;
  asr.Add("model_dir", c.asr_model_dir);
  asr.Add("vad_gate", c.asr_vad_gate);
  asr.Add("vad_lead_ms", c.asr_vad_lead_ms);
  asr.Add("vad_trail_sec", c.asr_vad_trail_sec);
  asr.Add("vad_min_overlap_sec", c.asr_vad_min_overlap_sec);
  asr.Add("max_audio_tokens", c.asr_max_audio_tokens);
  asr.Add("max_new_tokens", c.asr_max_new_tokens);
  asr.Add("segment_sec", c.asr_segment_sec);
  asr.Add("language", c.asr_language);
  asr.Add("system_prompt", c.asr_system_prompt);
  asr.Add("ban_steps", c.asr_ban_steps);
  asr.Add("decode_batch", c.asr_decode_batch);
  asr.Add("profile", c.asr_profile);
  asr.Add("windowed_encoder", c.asr_windowed_encoder);
  asr.Add("cuda_graph_enabled", c.asr_cuda_graph_enabled);
  root.AddObject("asr", asr.Finish());

  JsonObject align;
  align.Add("enable", c.align_enable);
  align.Add("model_dir", c.align_model_dir);
  align.Add("language", c.align_language);
  align.Add("max_segment_sec", c.align_max_segment_sec);
  align.Add("retain_sec", c.align_retain_sec);
  align.Add("profile", c.align_profile);
  root.AddObject("align", align.Finish());

  JsonObject timeline;
  timeline.Add("align_snap_pause_sec", c.timeline_align_snap_pause_sec);
  timeline.Add("align_boundary_split_tolerance_sec",
               c.timeline_align_boundary_split_tolerance_sec);
  timeline.Add("speaker_support_min_coverage_ratio",
               c.timeline_speaker_support_min_coverage_ratio);
  timeline.Add("speaker_support_max_gap_sec",
               c.timeline_speaker_support_max_gap_sec);
  timeline.Add("speaker_support_max_islands",
               c.timeline_speaker_support_max_islands);
  timeline.Add("gap_fill_enabled", c.timeline_gap_fill_enabled);
  timeline.Add("speaker_overlap_tie_policy",
               c.timeline_speaker_overlap_tie_policy);
  root.AddObject("timeline", timeline.Finish());

  JsonObject vad;
  vad.Add("model", c.vad_model);
  vad.Add("stream", c.vad_stream);
  vad.Add("threshold", static_cast<double>(c.vad_threshold));
  vad.Add("min_speech_ms", c.vad_min_speech_ms);
  vad.Add("min_silence_ms", c.vad_min_silence_ms);
  vad.Add("speech_pad_ms", c.vad_speech_pad_ms);
  root.AddObject("vad", vad.Finish());

  JsonObject diar;
  diar.Add("model_weights", c.diarizer_weights);
  diar.Add("max_speakers", c.max_speakers);
  diar.Add("threshold", static_cast<double>(c.diar_threshold));
  diar.Add("merge_gap_sec", c.diar_merge_gap_sec);
  diar.Add("deliver_interval_sec", c.diar_deliver_interval_sec);
  diar.Add("spkcache_len", c.diar_spkcache_len);
  diar.Add("chunk_len", c.diar_chunk_len);
  diar.Add("spkcache_update_period", c.diar_spkcache_update_period);
  diar.Add("chunk_left_context", c.diar_chunk_left_context);
  diar.Add("chunk_right_context", c.diar_chunk_right_context);
  diar.Add("spkcache_sil_frames", c.diar_spkcache_sil_frames);
  diar.Add("fifo_len", c.diar_fifo_len);
  diar.Add("onset", c.diar_onset);
  diar.Add("offset", c.diar_offset);
  diar.Add("pad_onset", c.diar_pad_onset);
  diar.Add("pad_offset", c.diar_pad_offset);
  diar.Add("min_dur_on", c.diar_min_dur_on);
  diar.Add("min_dur_off", c.diar_min_dur_off);
  diar.Add("reset_period_sec", c.diar_reset_period_sec);
  root.AddObject("diarizer", diar.Finish());

  JsonObject speaker;
  speaker.Add("enable", c.speaker_enable);
  speaker.Add("model_dir", c.speaker_model_dir);
  speaker.Add("registry_path", c.speaker_registry_path);
  speaker.Add("match_threshold",
              static_cast<double>(c.speaker_match_threshold));
  speaker.Add("min_embed_sec", c.speaker_min_embed_sec);
  speaker.Add("min_confidence", static_cast<double>(c.speaker_min_confidence));
  speaker.Add("retain_sec", c.speaker_retain_sec);
  speaker.Add("overlap_eps_sec", c.speaker_overlap_eps_sec);
  speaker.Add("max_ref_segs", c.speaker_max_ref_segs);
  speaker.Add("edge_margin_sec", c.speaker_edge_margin_sec);
  speaker.Add("max_embed_window_sec", c.speaker_max_embed_window_sec);
  speaker.Add("enroll_min_refs", c.speaker_enroll_min_refs);
  speaker.Add("speakers_per_session", c.speaker_speakers_per_session);
  speaker.Add("merge_threshold",
              static_cast<double>(c.speaker_merge_threshold));
  speaker.Add("cosession_merge_threshold",
              static_cast<double>(c.speaker_cosession_merge_threshold));
  speaker.Add("cross_session_match_min_refs",
              c.speaker_cross_session_match_min_refs);
  speaker.Add("defer_unmatched_cross_session",
              c.speaker_defer_unmatched_cross_session);
  speaker.Add("local_drift_threshold",
              static_cast<double>(c.speaker_local_drift_threshold));
  speaker.Add("local_drift_min_span_sec", c.speaker_local_drift_min_span_sec);
  speaker.Add("local_drift_min_epoch_sec", c.speaker_local_drift_min_epoch_sec);
  speaker.Add("local_drift_allow_same_session_match",
              c.speaker_local_drift_allow_same_session_match);
  speaker.Add("local_drift_competing_threshold",
              static_cast<double>(c.speaker_local_drift_competing_threshold));
  speaker.Add("local_drift_competing_margin",
              static_cast<double>(c.speaker_local_drift_competing_margin));
  speaker.Add("local_drift_competing_min_span_sec",
              c.speaker_local_drift_competing_min_span_sec);
  speaker.Add(
      "local_drift_competing_candidate_threshold",
      static_cast<double>(c.speaker_local_drift_competing_candidate_threshold));
  speaker.Add(
      "local_drift_competing_candidate_margin",
      static_cast<double>(c.speaker_local_drift_competing_candidate_margin));
  speaker.Add("local_drift_competing_candidate_min_confirmations",
              c.speaker_local_drift_competing_candidate_min_confirmations);
  speaker.Add("local_drift_competing_backfill_sec",
              c.speaker_local_drift_competing_backfill_sec);
  speaker.Add("local_drift_competing_backfill_gap_sec",
              c.speaker_local_drift_competing_backfill_gap_sec);
  root.AddObject("speaker", speaker.Finish());

  JsonObject speaker_fusion;
  speaker_fusion.Add("enable", c.speaker_fusion_enable);
  speaker_fusion.Add("min_embed_sec", c.speaker_fusion_min_embed_sec);
  speaker_fusion.Add("edge_margin_sec", c.speaker_fusion_edge_margin_sec);
  speaker_fusion.Add("max_embed_window_sec",
                     c.speaker_fusion_max_embed_window_sec);
  speaker_fusion.Add("phrase_min_sec", c.speaker_fusion_phrase_min_sec);
  speaker_fusion.Add("phrase_max_sec", c.speaker_fusion_phrase_max_sec);
  speaker_fusion.Add("punctuation", c.speaker_fusion_punctuation);
  speaker_fusion.Add(
      "frame_activity_threshold",
      static_cast<double>(c.speaker_fusion_frame_activity_threshold));
  speaker_fusion.Add("minimum_gallery_size",
                     c.speaker_fusion_minimum_gallery_size);
  speaker_fusion.Add("short_max_sec", c.speaker_fusion_short_max_sec);
  speaker_fusion.Add("short_min_score",
                     static_cast<double>(c.speaker_fusion_short_min_score));
  speaker_fusion.Add("short_min_margin",
                     static_cast<double>(c.speaker_fusion_short_min_margin));
  speaker_fusion.Add("regular_min_score",
                     static_cast<double>(c.speaker_fusion_regular_min_score));
  speaker_fusion.Add("regular_min_margin",
                     static_cast<double>(c.speaker_fusion_regular_min_margin));
  speaker_fusion.Add("four_view_min_aligned_units",
                     c.speaker_fusion_four_view_min_aligned_units);
  speaker_fusion.Add("precompute_interval_sec",
                     c.speaker_fusion_precompute_interval_sec);
  speaker_fusion.Add("precompute_max_spans_per_cycle",
                     c.speaker_fusion_precompute_max_spans_per_cycle);
  root.AddObject("speaker_fusion", speaker_fusion.Finish());

  JsonObject storage;
  storage.Add("disk_path", c.storage_disk_path);
  storage.Add("session_dir", c.session_dir);
  root.AddObject("storage", storage.Finish());

  JsonObject buffer;
  buffer.Add("max_samples", c.buffer_max_samples);
  buffer.Add("shrink_threshold", c.buffer_shrink_threshold);
  root.AddObject("buffer", buffer.Finish());

  JsonObject cursor;
  cursor.Add("interval_sec", c.cursor_telemetry_interval_sec);
  cursor.Add("lag_warn_samples", c.cursor_lag_warn_samples);
  cursor.Add("lag_critical_samples", c.cursor_lag_critical_samples);
  JsonObject telemetry;
  telemetry.Add("gpu_interval_sec", c.gpu_telemetry_interval_sec);
  telemetry.AddObject("cursor", cursor.Finish());
  root.AddObject("telemetry", telemetry.Finish());

  JsonObject debug;
  debug.Add("log_level", c.log_level);
  debug.Add("timebase_check", c.timebase_check);
  debug.Add("stream_progress", c.stream_progress);
  debug.Add("ws_text_log_path", c.ws_text_log_path);
  root.AddObject("debug", debug.Finish());

  return root.Finish();
}

}  // namespace pipeline
}  // namespace orator
