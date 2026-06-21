#include "io/config_reader.h"

#include <iostream>
#include <toml++/toml.h>

namespace orator {
namespace io {

bool ApplyTomlConfig(const std::string& path,
                     pipeline::AuditoryStream::Config& cfg) {
  toml::parse_result config;
  try {
    config = toml::parse_file(path);
  } catch (const toml::parse_error& e) {
    std::cerr << "[config] parse error in " << path << ": "
              << e.description() << " (" << e.source().begin << ")"
              << std::endl;
    return false;
  }

  // ── ASR section ────────────────────────────────────────────────────
  if (auto* sec = config["asr"].as_table()) {
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
  }

  // ── VAD section ────────────────────────────────────────────────────
  if (auto* sec = config["vad"].as_table()) {
    if (auto v = sec->get("model")) {
      if (auto s = v->value<std::string>()) cfg.vad_model = *s;
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
  }

  // ── Diarizer section ───────────────────────────────────────────────
  if (auto* sec = config["diarizer"].as_table()) {
    if (auto v = sec->get("threshold")) {
      if (auto f = v->value<float>()) cfg.diar_threshold = *f;
    }
    if (auto v = sec->get("merge_gap_sec")) {
      if (auto d = v->value<double>()) cfg.diar_merge_gap_sec = *d;
    }
  }

  // ── Telemetry section ──────────────────────────────────────────────
  if (auto* sec = config["telemetry"].as_table()) {
    if (auto v = sec->get("gpu_interval_sec")) {
      if (auto d = v->value<double>()) cfg.gpu_telemetry_interval_sec = *d;
    }
  }

  return true;
}

}  // namespace io
}  // namespace orator
