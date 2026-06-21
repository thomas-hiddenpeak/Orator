#include "io/config_reader.h"

#include <fstream>
#include <iostream>
#include <toml++/toml.h>

namespace orator {
namespace io {

bool ApplyTomlConfig(const std::string& path,
                     pipeline::AuditoryStream::Config& cfg) {
  // File is optional — silence is normal.
  {
    std::ifstream f(path);
    if (!f.good()) return false;
  }

  toml::parse_result config;
  try {
    config = toml::parse_file(path);
  } catch (const toml::parse_error& e) {
    std::cerr << "[config] parse error in " << path << ": "
              << e.description() << " (" << e.source().begin << ")"
              << std::endl;
    return false;
  }

  // ── [server] ──────────────────────────────────────────────────────
  if (auto* sec = config["server"].as_table()) {
    if (auto v = sec->get("port")) {
      if (auto n = v->value<int>()) cfg.port = *n;
    }
    if (auto v = sec->get("ui_port")) {
      if (auto n = v->value<int>()) cfg.ui_port = *n;
    }
    if (auto v = sec->get("ui_root")) {
      if (auto s = v->value<std::string>()) cfg.ui_root = *s;
    }
  }

  // ── [asr] ─────────────────────────────────────────────────────────
  if (auto* sec = config["asr"].as_table()) {
    if (auto v = sec->get("model_dir")) {
      if (auto s = v->value<std::string>()) cfg.asr_model_dir = *s;
    }
    if (auto v = sec->get("vad_gate")) {
      if (auto b = v->value<bool>()) cfg.asr_vad_gate = *b;
    }
    if (auto v = sec->get("vad_lead_ms")) {
      if (auto n = v->value<int>()) cfg.asr_vad_lead_ms = *n;
    }
    if (auto v = sec->get("vad_trail_sec")) {
      if (auto d = v->value<double>()) cfg.asr_vad_trail_sec = *d;
    }
    if (auto v = sec->get("max_new_tokens")) {
      if (auto n = v->value<int>()) cfg.asr_max_new_tokens = *n;
    }
    if (auto v = sec->get("max_audio_tokens")) {
      if (auto n = v->value<int>()) cfg.asr_max_audio_tokens = *n;
    }
    if (auto v = sec->get("segment_sec")) {
      if (auto d = v->value<double>()) cfg.asr_segment_sec = *d;
    }
    if (auto v = sec->get("language")) {
      if (auto s = v->value<std::string>()) cfg.asr_language = *s;
    }
    if (auto v = sec->get("system_prompt")) {
      if (auto s = v->value<std::string>()) cfg.asr_system_prompt = *s;
    }
    if (auto v = sec->get("ban_steps")) {
      if (auto n = v->value<int>()) cfg.asr_ban_steps = *n;
    }
    if (auto v = sec->get("decode_batch")) {
      if (auto n = v->value<int>()) cfg.asr_decode_batch = *n;
    }
    if (auto v = sec->get("profile")) {
      if (auto b = v->value<bool>()) cfg.asr_profile = *b;
    }
  }

  // ── [vad] ─────────────────────────────────────────────────────────
  if (auto* sec = config["vad"].as_table()) {
    if (auto v = sec->get("model")) {
      if (auto s = v->value<std::string>()) cfg.vad_model = *s;
    }
    if (auto v = sec->get("stream")) {
      if (auto b = v->value<bool>()) cfg.vad_stream = *b;
    }
    if (auto v = sec->get("threshold")) {
      if (auto f = v->value<float>()) cfg.vad_threshold = *f;
    }
    if (auto v = sec->get("min_speech_ms")) {
      if (auto n = v->value<int>()) cfg.vad_min_speech_ms = *n;
    }
    if (auto v = sec->get("min_silence_ms")) {
      if (auto n = v->value<int>()) cfg.vad_min_silence_ms = *n;
    }
    if (auto v = sec->get("speech_pad_ms")) {
      if (auto n = v->value<int>()) cfg.vad_speech_pad_ms = *n;
    }
  }

  // ── [diarizer] ────────────────────────────────────────────────────
  if (auto* sec = config["diarizer"].as_table()) {
    if (auto v = sec->get("model")) {
      if (auto s = v->value<std::string>()) cfg.diarizer_weights = *s;
    }
    if (auto v = sec->get("max_speakers")) {
      if (auto n = v->value<int>()) cfg.max_speakers = *n;
    }
    if (auto v = sec->get("threshold")) {
      if (auto f = v->value<float>()) cfg.diar_threshold = *f;
    }
    if (auto v = sec->get("merge_gap_sec")) {
      if (auto d = v->value<double>()) cfg.diar_merge_gap_sec = *d;
    }
    if (auto v = sec->get("deliver_interval_sec")) {
      if (auto d = v->value<double>()) cfg.diar_deliver_interval_sec = *d;
    }
  }

  // ── [storage] ─────────────────────────────────────────────────────
  if (auto* sec = config["storage"].as_table()) {
    if (auto v = sec->get("disk_path")) {
      if (auto s = v->value<std::string>()) cfg.storage_disk_path = *s;
    }
    if (auto v = sec->get("session_dir")) {
      if (auto s = v->value<std::string>()) cfg.session_dir = *s;
    }
  }

  // ── [telemetry] ───────────────────────────────────────────────────
  if (auto* sec = config["telemetry"].as_table()) {
    if (auto v = sec->get("gpu_interval_sec")) {
      if (auto d = v->value<double>()) cfg.gpu_telemetry_interval_sec = *d;
    }
  }

  // ── [debug] ──────────────────────────────────────────────────────
  if (auto* sec = config["debug"].as_table()) {
    if (auto v = sec->get("log_level")) {
      if (auto n = v->value<int>()) cfg.log_level = *n;
    }
    if (auto v = sec->get("timebase_check")) {
      if (auto b = v->value<bool>()) cfg.timebase_check = *b;
    }
    if (auto v = sec->get("stream_progress")) {
      if (auto b = v->value<bool>()) cfg.stream_progress = *b;
    }
    if (auto v = sec->get("gpu_scheduling")) {
      if (auto s = v->value<std::string>()) {
        if (*s == "serial") cfg.gpu_scheduling_mode = 1;
        else if (*s == "concurrent") cfg.gpu_scheduling_mode = 2;
        else cfg.gpu_scheduling_mode = 0;  // auto
      }
    }
  }

  return true;
}

}  // namespace io
}  // namespace orator
