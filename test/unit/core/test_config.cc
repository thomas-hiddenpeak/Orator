#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "io/config_reader.h"
#include "pipeline/auditory_stream.h"
#include "pipeline/runtime_config.h"

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

  // Spec 013: both fallback startup and the checked-in acceptance config use
  // the same v2.1 closing baseline. The removed v2 checkpoint must not return.
  std::printf("-- v2.1 closing baseline --\n");
  {
    pipeline::AuditoryStream::Config defaults;
    CHECK(defaults.diarizer_weights ==
              "models/sortformer_4spk_v2.1.safetensors",
          "compile-time diarizer default is v2.1");
    CHECK(defaults.asr_vad_gate_chunk_ms > 0,
          "compile-time ASR VAD gate chunk is positive");
    CHECK(defaults.diar_chunk_len == 340,
          "compile-time v2.1 chunk length is 340");
    CHECK(defaults.diar_chunk_right_context == 1,
          "compile-time v2.1 right context is 1");
    CHECK(defaults.diar_fifo_len == 188,
          "compile-time v2.1 FIFO length is 188");
    CHECK(defaults.diar_spkcache_update_period == 188,
          "compile-time v2.1 cache update period is 188");

    pipeline::AuditoryStream::Config checked_in;
    const std::string config_path =
        std::string(ORATOR_TEST_SOURCE_DIR) + "/orator.toml";
    CHECK(io::ApplyTomlConfig(config_path, checked_in),
          "checked-in orator.toml loads");
    CHECK(checked_in.diarizer_weights ==
              "models/sortformer_4spk_v2.1.safetensors",
          "checked-in diarizer is v2.1");
    CHECK(checked_in.diar_chunk_len == 340,
          "checked-in v2.1 chunk length is 340");
    CHECK(checked_in.diar_chunk_right_context == 1,
          "checked-in v2.1 right context is 1");
    CHECK(checked_in.diar_fifo_len == 188,
          "checked-in v2.1 FIFO length is 188");
    CHECK(checked_in.diar_spkcache_update_period == 188,
          "checked-in v2.1 cache update period is 188");
    CHECK(checked_in.asr_vad_gate_chunk_ms == 100,
          "checked-in ASR VAD gate chunk is frozen at 100 ms");
    const std::string removed_v2_path =
        std::string(ORATOR_TEST_SOURCE_DIR) +
        "/models/sortformer_4spk_v2.safetensors";
    CHECK(!std::filesystem::exists(removed_v2_path),
          "deprecated v2 checkpoint is absent");
  }

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
vad_gate_chunk_ms = 80
vad_trail_sec = 2.5
vad_min_overlap_sec = 0.25
max_new_tokens = 64
max_audio_tokens = 2000
segment_sec = 30.0
language = "English"
system_prompt = "Translate to French"
ban_steps = 5
decode_batch = 8
profile = true
windowed_encoder = true
cuda_graph_enabled = false

[align]
enable = true
model_dir = "/models/align"
language = "English"
max_segment_sec = 120.0
retain_sec = 150.0
profile = true

[speaker]
enable = true
model_dir = "/models/speaker"
registry_path = "/tmp/speakers.bin"
match_threshold = 0.75
min_embed_sec = 4.0
min_confidence = 0.625
retain_sec = 240.0
overlap_eps_sec = 0.05
max_ref_segs = 8
edge_margin_sec = 0.4
max_embed_window_sec = 12.0
enroll_min_refs = 2
speakers_per_session = 6
merge_threshold = 0.875
cosession_merge_threshold = 0.95
cross_session_match_min_refs = 3
defer_unmatched_cross_session = true
local_drift_threshold = 0.5
local_drift_min_span_sec = 6.0
local_drift_min_epoch_sec = 45.0
local_drift_allow_same_session_match = false
local_drift_competing_threshold = 0.72
local_drift_competing_margin = 0.08
local_drift_competing_min_span_sec = 3.0
local_drift_competing_candidate_threshold = 0.58
local_drift_competing_candidate_margin = 0.04
local_drift_competing_candidate_min_confirmations = 2
local_drift_competing_backfill_sec = 120.0
local_drift_competing_backfill_gap_sec = 4.0

[speaker_fusion]
enable = true
min_embed_sec = 0.4
edge_margin_sec = 0.0
max_embed_window_sec = 3.0
phrase_min_sec = 0.5
phrase_max_sec = 3.0
punctuation = "，。？！"
frame_activity_threshold = 0.5
minimum_gallery_size = 4
short_max_sec = 1.5
short_min_score = 0.0
short_min_margin = 0.04
regular_min_score = 0.55
regular_min_margin = 0.04
four_view_min_aligned_units = 3
future_epoch_lookahead_sec = 90.0
posterior_future_epoch_enable = true
source_leading_primary_prefix_enable = true
precompute_interval_sec = 7.5
precompute_max_spans_per_cycle = 4

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

[timeline]
align_snap_pause_sec = 0.125
align_boundary_split_tolerance_sec = 0.075
speaker_support_min_coverage_ratio = 0.625
speaker_support_max_gap_sec = 1.25
speaker_support_max_islands = 2
gap_fill_enabled = false
speaker_overlap_tie_policy = "higher_confidence"

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
ws_text_log_path = "/tmp/ws-frames.jsonl"
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
    CHECK(cfg.asr_vad_gate_chunk_ms == 80, "cfg.asr_vad_gate_chunk_ms == 80");
    CHECK(cfg.asr_vad_trail_sec == 2.5, "cfg.asr_vad_trail_sec == 2.5");
    CHECK(cfg.asr_vad_min_overlap_sec == 0.25,
          "cfg.asr_vad_min_overlap_sec == 0.25");
    CHECK(cfg.asr_max_new_tokens == 64, "cfg.asr_max_new_tokens == 64");
    CHECK(cfg.asr_max_audio_tokens == 2000, "cfg.asr_max_audio_tokens == 2000");
    CHECK(cfg.asr_segment_sec == 30.0, "cfg.asr_segment_sec == 30.0");
    CHECK(cfg.asr_language == "English", "cfg.asr_language == English");
    CHECK(cfg.asr_system_prompt == "Translate to French",
          "cfg.asr_system_prompt");
    CHECK(cfg.asr_ban_steps == 5, "cfg.asr_ban_steps == 5");
    CHECK(cfg.asr_decode_batch == 8, "cfg.asr_decode_batch == 8");
    CHECK(cfg.asr_profile == true, "cfg.asr_profile == true");
    CHECK(cfg.asr_windowed_encoder == true, "cfg.asr_windowed_encoder == true");
    CHECK(cfg.asr_cuda_graph_enabled == false,
          "cfg.asr_cuda_graph_enabled == false");

    // [align]
    CHECK(cfg.align_enable == true, "cfg.align_enable == true");
    CHECK(cfg.align_model_dir == "/models/align", "cfg.align_model_dir");
    CHECK(cfg.align_language == "English", "cfg.align_language == English");
    CHECK(cfg.align_max_segment_sec == 120.0,
          "cfg.align_max_segment_sec == 120.0");
    CHECK(cfg.align_retain_sec == 150.0, "cfg.align_retain_sec == 150.0");
    CHECK(cfg.align_profile == true, "cfg.align_profile == true");

    // [speaker]
    CHECK(cfg.speaker_enable == true, "cfg.speaker_enable == true");
    CHECK(cfg.speaker_model_dir == "/models/speaker", "cfg.speaker_model_dir");
    CHECK(cfg.speaker_registry_path == "/tmp/speakers.bin",
          "cfg.speaker_registry_path");
    CHECK(cfg.speaker_match_threshold == 0.75f,
          "cfg.speaker_match_threshold == 0.75");
    CHECK(cfg.speaker_min_embed_sec == 4.0, "cfg.speaker_min_embed_sec == 4.0");
    CHECK(cfg.speaker_min_confidence == 0.625f,
          "cfg.speaker_min_confidence == 0.625");
    CHECK(cfg.speaker_retain_sec == 240.0, "cfg.speaker_retain_sec == 240.0");
    CHECK(cfg.speaker_overlap_eps_sec == 0.05,
          "cfg.speaker_overlap_eps_sec == 0.05");
    CHECK(cfg.speaker_max_ref_segs == 8, "cfg.speaker_max_ref_segs == 8");
    CHECK(cfg.speaker_edge_margin_sec == 0.4,
          "cfg.speaker_edge_margin_sec == 0.4");
    CHECK(cfg.speaker_max_embed_window_sec == 12.0,
          "cfg.speaker_max_embed_window_sec == 12.0");
    CHECK(cfg.speaker_enroll_min_refs == 2, "cfg.speaker_enroll_min_refs == 2");
    CHECK(cfg.speaker_speakers_per_session == 6,
          "cfg.speaker_speakers_per_session == 6");
    CHECK(cfg.speaker_merge_threshold == 0.875f,
          "cfg.speaker_merge_threshold == 0.875");
    CHECK(cfg.speaker_cosession_merge_threshold == 0.95f,
          "cfg.speaker_cosession_merge_threshold == 0.95");
    CHECK(cfg.speaker_cross_session_match_min_refs == 3,
          "cfg.speaker_cross_session_match_min_refs == 3");
    CHECK(cfg.speaker_defer_unmatched_cross_session == true,
          "cfg.speaker_defer_unmatched_cross_session == true");
    CHECK(cfg.speaker_local_drift_threshold == 0.5f,
          "cfg.speaker_local_drift_threshold == 0.5");
    CHECK(cfg.speaker_local_drift_min_span_sec == 6.0,
          "cfg.speaker_local_drift_min_span_sec == 6.0");
    CHECK(cfg.speaker_local_drift_min_epoch_sec == 45.0,
          "cfg.speaker_local_drift_min_epoch_sec == 45.0");
    CHECK(cfg.speaker_local_drift_allow_same_session_match == false,
          "cfg.speaker_local_drift_allow_same_session_match == false");
    CHECK(cfg.speaker_local_drift_competing_threshold == 0.72f,
          "cfg.speaker_local_drift_competing_threshold == 0.72");
    CHECK(cfg.speaker_local_drift_competing_margin == 0.08f,
          "cfg.speaker_local_drift_competing_margin == 0.08");
    CHECK(cfg.speaker_local_drift_competing_min_span_sec == 3.0,
          "cfg.speaker_local_drift_competing_min_span_sec == 3.0");
    CHECK(cfg.speaker_local_drift_competing_candidate_threshold == 0.58f,
          "cfg.speaker_local_drift_competing_candidate_threshold == 0.58");
    CHECK(cfg.speaker_local_drift_competing_candidate_margin == 0.04f,
          "cfg.speaker_local_drift_competing_candidate_margin == 0.04");
    CHECK(cfg.speaker_local_drift_competing_candidate_min_confirmations == 2,
          "candidate confirmation count == 2");
    CHECK(cfg.speaker_local_drift_competing_backfill_sec == 120.0,
          "cfg.speaker_local_drift_competing_backfill_sec == 120.0");
    CHECK(cfg.speaker_local_drift_competing_backfill_gap_sec == 4.0,
          "cfg.speaker_local_drift_competing_backfill_gap_sec == 4.0");
    CHECK(cfg.speaker_fusion_enable, "cfg.speaker_fusion_enable == true");
    CHECK(cfg.speaker_fusion_min_embed_sec == 0.4,
          "cfg.speaker_fusion_min_embed_sec == 0.4");
    CHECK(cfg.speaker_fusion_minimum_gallery_size == 4,
          "cfg.speaker_fusion_minimum_gallery_size == 4");
    CHECK(cfg.speaker_fusion_regular_min_score == 0.55f,
          "cfg.speaker_fusion_regular_min_score == 0.55");
    CHECK(cfg.speaker_fusion_four_view_min_aligned_units == 3,
          "cfg.speaker_fusion_four_view_min_aligned_units == 3");
    CHECK(cfg.speaker_fusion_future_epoch_lookahead_sec == 90.0,
          "cfg.speaker_fusion_future_epoch_lookahead_sec == 90.0");
    CHECK(cfg.speaker_fusion_posterior_future_epoch_enable,
          "cfg.speaker_fusion_posterior_future_epoch_enable == true");
    CHECK(cfg.speaker_fusion_source_leading_primary_prefix_enable,
          "cfg.speaker_fusion_source_leading_primary_prefix_enable == true");
    CHECK(cfg.speaker_fusion_precompute_interval_sec == 7.5,
          "cfg.speaker_fusion_precompute_interval_sec == 7.5");
    CHECK(cfg.speaker_fusion_precompute_max_spans_per_cycle == 4,
          "cfg.speaker_fusion_precompute_max_spans_per_cycle == 4");

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

    // [timeline]
    CHECK(cfg.timeline_align_snap_pause_sec == 0.125,
          "cfg.timeline_align_snap_pause_sec == 0.125");
    CHECK(cfg.timeline_align_boundary_split_tolerance_sec == 0.075,
          "cfg.timeline_align_boundary_split_tolerance_sec == 0.075");
    CHECK(cfg.timeline_speaker_support_min_coverage_ratio == 0.625,
          "cfg.timeline_speaker_support_min_coverage_ratio == 0.625");
    CHECK(cfg.timeline_speaker_support_max_gap_sec == 1.25,
          "cfg.timeline_speaker_support_max_gap_sec == 1.25");
    CHECK(cfg.timeline_speaker_support_max_islands == 2,
          "cfg.timeline_speaker_support_max_islands == 2");
    CHECK(cfg.timeline_gap_fill_enabled == false,
          "cfg.timeline_gap_fill_enabled == false");
    CHECK(cfg.timeline_speaker_overlap_tie_policy == "higher_confidence",
          "cfg.timeline_speaker_overlap_tie_policy == higher_confidence");

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
    CHECK(cfg.ws_text_log_path == "/tmp/ws-frames.jsonl",
          "cfg.ws_text_log_path");

    const std::string resolved = pipeline::SerializeResolvedConfig(cfg);
    CHECK(resolved.find("\"windowed_encoder\":true") != std::string::npos,
          "resolved config contains ASR execution mode");
    CHECK(resolved.find("\"vad_gate_chunk_ms\":80") != std::string::npos,
          "resolved config contains deterministic ASR gate chunk");
    CHECK(resolved.find("\"ws_text_log_path\":\"/tmp/ws-frames.jsonl\"") !=
              std::string::npos,
          "resolved config contains transport diagnostics");
    CHECK(resolved.find("\"minimum_gallery_size\":4") != std::string::npos,
          "resolved config contains speaker-fusion tuning");
    CHECK(
        resolved.find("\"four_view_min_aligned_units\":3") != std::string::npos,
        "resolved config contains four-view aligned-unit gate");
    CHECK(resolved.find("\"precompute_interval_sec\":7.5") != std::string::npos,
          "resolved config contains speaker-fusion precompute cadence");
    CHECK(resolved.find("\"precompute_max_spans_per_cycle\":4") !=
              std::string::npos,
          "resolved config contains speaker-fusion precompute cycle limit");
    CHECK(resolved.find(
              "\"local_drift_competing_candidate_min_confirmations\":2") !=
              std::string::npos,
          "resolved config contains candidate confirmation count");

    std::remove(path.c_str());
  }

  // ── Primary-speaker tie policy is typed and rejects unknown values ──
  std::printf("\n-- Primary speaker tie policy --\n");
  {
    std::string path = WriteTemp(R"(
[timeline]
speaker_overlap_tie_policy = "primary_speaker"
)");
    pipeline::AuditoryStream::Config cfg;
    CHECK(io::ApplyTomlConfig(path, cfg),
          "primary_speaker tie policy is accepted");
    CHECK(cfg.timeline_speaker_overlap_tie_policy == "primary_speaker",
          "primary_speaker tie policy is loaded exactly");
    std::remove(path.c_str());

    path = WriteTemp(R"(
[timeline]
speaker_overlap_tie_policy = "unbounded_override"
)");
    pipeline::AuditoryStream::Config invalid;
    CHECK(!io::ApplyTomlConfig(path, invalid),
          "unknown speaker tie policy is rejected");
    std::remove(path.c_str());
  }

  // ── CLI is the final configuration layer ───────────────────────────
  std::printf("\n-- Command-line final override --\n");
  {
    pipeline::AuditoryStream::Config cfg;
    cfg.port = 7000;
    cfg.diarizer_weights = "/toml/diar";
    cfg.asr_model_dir = "/env/asr";
    char arg0[] = "orator_ws";
    char arg1[] = "9000";
    char arg2[] = "/cli/diar";
    char arg3[] = "/cli/asr";
    char* argv[] = {arg0, arg1, arg2, arg3};
    io::ApplyCommandLineConfig(4, argv, cfg);
    CHECK(cfg.port == 9000, "CLI port overrides earlier layers");
    CHECK(cfg.diarizer_weights == "/cli/diar",
          "CLI diarizer path overrides earlier layers");
    CHECK(cfg.asr_model_dir == "/cli/asr",
          "CLI ASR path overrides earlier layers");
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

  // ── Removed non-NeMo controls fail instead of being ignored ─────────
  std::printf("\n-- Removed diarizer controls --\n");
  {
    const char* removed = R"(
[server]
port = 7777

[diarizer]
spkcache_refresh_rate = 100
)";
    std::string path = WriteTemp(removed);

    pipeline::AuditoryStream::Config cfg;
    cfg.port = 1234;
    bool ok = io::ApplyTomlConfig(path, cfg);
    CHECK(!ok, "removed diarizer control is rejected");
    CHECK(cfg.port == 1234,
          "config remains unchanged when removed control is present");

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
