#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>

#include "io/config_reader.h"
#include "pipeline/auditory_stream.h"

using namespace orator;

static int fails = 0;

#define CHECK(cond, msg)                                                    \
  do {                                                                      \
    if (!(cond)) {                                                          \
      std::fprintf(stderr, "  FAIL [%s:%d] %s\n", __FILE__, __LINE__, msg); \
      ++fails;                                                              \
    } else {                                                                \
      std::printf("  OK %s\n", msg);                                        \
    }                                                                       \
  } while (0)

// Write a string to a temp file and return the path.
static std::string WriteTemp(const char* content) {
  char path[] = "/tmp/orator_test_config_XXXXXX";
  int fd = mkstemp(path);
  if (fd == -1) {
    std::perror("mkstemp");
    std::abort();
  }
  FILE* f = fdopen(fd, "w");
  if (!f) {
    std::perror("fdopen");
    std::abort();
  }
  std::fprintf(f, "%s", content);
  std::fclose(f);
  return path;
}

int main() {
  std::printf("=== ConfigReader unit tests ===\n\n");

  // ── Valid TOML file ─────────────────────────────────────────────────
  std::printf("-- Valid TOML --\n");
  {
    const char* toml = R"(
[server]
port = 9090
ui_port = 9091
ui_root = "/custom/web"

[asr]
model_dir = "/models/my_asr"
vad_gate = false
vad_lead_ms = 150
vad_trail_sec = 2.5
max_new_tokens = 64
max_audio_tokens = 2000
segment_sec = 30.0
language = "English"
system_prompt = "Translate to French"
ban_steps = 5
decode_batch = 8
profile = true

[vad]
model = "/models/vad/custom.safetensors"
stream = false
threshold = 0.3
min_speech_ms = 300
min_silence_ms = 400
speech_pad_ms = 80

[diarizer]
model_weights = "/models/diar/custom.safetensors"
max_speakers = 8
threshold = 0.6
merge_gap_sec = 1.0
deliver_interval_sec = 2.0

[storage]
disk_path = "/custom/storage"
session_dir = "/custom/sessions"

[telemetry]
gpu_interval_sec = 5.0

[telemetry.cursor]
interval_sec = 1.5
lag_warn_samples = 16000
lag_critical_samples = 48000

[debug]
log_level = 1
timebase_check = true
stream_progress = true
gpu_scheduling = "concurrent"
)";
    std::string path = WriteTemp(toml);

    pipeline::AuditoryStream::Config cfg;
    bool ok = io::ApplyTomlConfig(path, cfg);
    CHECK(ok, "ApplyTomlConfig returns true for valid TOML");

    // [server]
    CHECK(cfg.port == 9090, "cfg.port == 9090");
    CHECK(cfg.ui_port == 9091, "cfg.ui_port == 9091");
    CHECK(cfg.ui_root == "/custom/web", "cfg.ui_root == /custom/web");

    // [asr]
    CHECK(cfg.asr_model_dir == "/models/my_asr", "cfg.asr_model_dir");
    CHECK(cfg.asr_vad_gate == false, "cfg.asr_vad_gate == false");
    CHECK(cfg.asr_vad_lead_ms == 150, "cfg.asr_vad_lead_ms == 150");
    CHECK(cfg.asr_vad_trail_sec == 2.5, "cfg.asr_vad_trail_sec == 2.5");
    CHECK(cfg.asr_max_new_tokens == 64, "cfg.asr_max_new_tokens == 64");
    CHECK(cfg.asr_max_audio_tokens == 2000, "cfg.asr_max_audio_tokens == 2000");
    CHECK(cfg.asr_segment_sec == 30.0, "cfg.asr_segment_sec == 30.0");
    CHECK(cfg.asr_language == "English", "cfg.asr_language == English");
    CHECK(cfg.asr_system_prompt == "Translate to French",
          "cfg.asr_system_prompt");
    CHECK(cfg.asr_ban_steps == 5, "cfg.asr_ban_steps == 5");
    CHECK(cfg.asr_decode_batch == 8, "cfg.asr_decode_batch == 8");
    CHECK(cfg.asr_profile == true, "cfg.asr_profile == true");

    // [vad]
    CHECK(cfg.vad_model == "/models/vad/custom.safetensors", "cfg.vad_model");
    CHECK(cfg.vad_stream == false, "cfg.vad_stream == false");
    CHECK(cfg.vad_threshold == 0.3f, "cfg.vad_threshold == 0.3");
    CHECK(cfg.vad_min_speech_ms == 300, "cfg.vad_min_speech_ms == 300");
    CHECK(cfg.vad_min_silence_ms == 400, "cfg.vad_min_silence_ms == 400");
    CHECK(cfg.vad_speech_pad_ms == 80, "cfg.vad_speech_pad_ms == 80");

    // [diarizer]
    CHECK(cfg.diarizer_weights == "/models/diar/custom.safetensors",
          "cfg.diarizer_weights");
    CHECK(cfg.max_speakers == 8, "cfg.max_speakers == 8");
    CHECK(cfg.diar_threshold == 0.6f, "cfg.diar_threshold == 0.6");
    CHECK(cfg.diar_merge_gap_sec == 1.0, "cfg.diar_merge_gap_sec == 1.0");
    CHECK(cfg.diar_deliver_interval_sec == 2.0,
          "cfg.diar_deliver_interval_sec == 2.0");

    // [storage]
    CHECK(cfg.storage_disk_path == "/custom/storage", "cfg.storage_disk_path");
    CHECK(cfg.session_dir == "/custom/sessions", "cfg.session_dir");

    // [telemetry]
    CHECK(cfg.gpu_telemetry_interval_sec == 5.0,
          "cfg.gpu_telemetry_interval_sec == 5.0");

    // [telemetry.cursor] — nested table (regression: toml++ operator[] is
    // single-key, so config["telemetry.cursor"] never matched the section).
    CHECK(cfg.cursor_telemetry_interval_sec == 1.5,
          "cfg.cursor_telemetry_interval_sec == 1.5");
    CHECK(cfg.cursor_lag_warn_samples == 16000,
          "cfg.cursor_lag_warn_samples == 16000");
    CHECK(cfg.cursor_lag_critical_samples == 48000,
          "cfg.cursor_lag_critical_samples == 48000");

    // [debug]
    CHECK(cfg.log_level == 1, "cfg.log_level == 1");
    CHECK(cfg.timebase_check == true, "cfg.timebase_check == true");
    CHECK(cfg.stream_progress == true, "cfg.stream_progress == true");
    CHECK(cfg.gpu_scheduling_mode == 2,
          "cfg.gpu_scheduling_mode == 2 (concurrent)");

    std::remove(path.c_str());
  }

  // ── Missing file ────────────────────────────────────────────────────
  std::printf("\n-- Missing file --\n");
  {
    pipeline::AuditoryStream::Config cfg;
    // Set some fields to non-default values to verify they are unchanged.
    cfg.port = 1234;
    cfg.asr_vad_gate = false;
    cfg.vad_threshold = 0.9f;

    bool ok =
        io::ApplyTomlConfig("/tmp/orator_nonexistent_config_XXXXXX.toml", cfg);
    CHECK(!ok, "ApplyTomlConfig returns false for missing file");
    CHECK(cfg.port == 1234, "cfg.port unchanged after missing file");
    CHECK(cfg.asr_vad_gate == false,
          "cfg.asr_vad_gate unchanged after missing file");
    CHECK(cfg.vad_threshold == 0.9f,
          "cfg.vad_threshold unchanged after missing file");
  }

  // ── Malformed TOML ──────────────────────────────────────────────────
  std::printf("\n-- Malformed TOML --\n");
  {
    const char* bad_toml = R"(
[server
port = 8080
)";
    std::string path = WriteTemp(bad_toml);

    pipeline::AuditoryStream::Config cfg;
    cfg.port = 9999;  // non-default

    bool ok = io::ApplyTomlConfig(path, cfg);
    CHECK(!ok, "ApplyTomlConfig returns false for malformed TOML");
    CHECK(cfg.port == 9999, "cfg.port unchanged after malformed TOML");

    std::remove(path.c_str());
  }

  // ── Partial TOML (only one section) ─────────────────────────────────
  std::printf("\n-- Partial TOML --\n");
  {
    const char* partial = R"(
[server]
port = 7777
)";
    std::string path = WriteTemp(partial);

    pipeline::AuditoryStream::Config cfg;
    cfg.port = 0;
    cfg.asr_vad_gate = true;  // default is true

    bool ok = io::ApplyTomlConfig(path, cfg);
    CHECK(ok, "ApplyTomlConfig returns true for partial TOML");
    CHECK(cfg.port == 7777, "cfg.port == 7777 from partial TOML");
    // Fields not in the TOML should retain their original values
    CHECK(cfg.asr_vad_gate == true,
          "cfg.asr_vad_gate unchanged (not in partial TOML)");

    std::remove(path.c_str());
  }

  // ── Summary ─────────────────────────────────────────────────────────
  std::printf("\n");
  if (fails) {
    std::printf("FAIL: %d test(s) failed\n", fails);
    return 1;
  }
  std::printf("All config reader tests PASSED\n");
  return 0;
}
