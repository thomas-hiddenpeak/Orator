// orator_ws: real-time WebSocket server running diarization + ASR as two
// independent pipelines over one streamed audio source.
//
// Usage: orator_ws [port] [diar_weights.safetensors] [asr_model_dir]
// Streams a unified timeline (diarization segments + transcript) back to
// clients that push PCM audio. If asr_model_dir is omitted, ASR is disabled and
// only diarization runs.

#include <csignal>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "io/config_reader.h"
#include "net/auditory_ws_handler.h"
#include "net/http_static_server.h"
#include "net/websocket_server.h"

using namespace orator;

namespace {

bool ReadEnvInt(const char* name, int* out) {
  const char* v = std::getenv(name);
  if (v == nullptr || out == nullptr) return false;
  char* end = nullptr;
  const long x = std::strtol(v, &end, 10);
  if (end == v || *end != '\0') return false;
  *out = static_cast<int>(x);
  return true;
}

bool ReadEnvDouble(const char* name, double* out) {
  const char* v = std::getenv(name);
  if (v == nullptr || out == nullptr) return false;
  char* end = nullptr;
  const double x = std::strtod(v, &end);
  if (end == v || *end != '\0') return false;
  *out = x;
  return true;
}

bool ReadEnvFloat(const char* name, float* out) {
  double x = 0.0;
  if (!ReadEnvDouble(name, &x) || out == nullptr) return false;
  *out = static_cast<float>(x);
  return true;
}

void ReadEnvString(const char* name, std::string* out) {
  const char* v = std::getenv(name);
  if (v == nullptr || out == nullptr) return;
  *out = v;
}

bool ReadEnvFlag(const char* name, bool dflt) {
  const char* v = std::getenv(name);
  if (v == nullptr || v[0] == '\0') return dflt;
  return v[0] == '1' || v[0] == 't' || v[0] == 'T' || v[0] == 'y' || v[0] == 'Y';
}

}  // namespace

int main(int argc, char** argv) {
  // ── Step 1: compile-time defaults (Config struct initializers) ────
  pipeline::AuditoryStream::Config cfg;

  // ── Step 2: CLI args override defaults ────────────────────────────
  if (argc > 1) cfg.port = std::atoi(argv[1]);
  const bool asr_arg_provided = argc > 3;
  if (argc > 2) cfg.diarizer_weights = argv[2];
  if (asr_arg_provided) cfg.asr_model_dir = argv[3];

  // ── Step 3: TOML config file overrides defaults ──────────────────
  {
    const char* config_path = std::getenv("ORATOR_CONFIG");
    if (config_path == nullptr || config_path[0] == '\0') {
      config_path = "orator.toml";
    }
    io::ApplyTomlConfig(config_path, cfg);
  }

  // ── Step 4: sync toml params into env for deep getenv() code ─────
  // (Existing model code reads these via getenv; setting the env var is
  //  the least-invasive bridge. The loading order is preserved: env set
  //  here can still be overridden by the user's env below.)
  auto set_env_int = [](const char* name, int val) {
    setenv(name, std::to_string(val).c_str(), 0);
  };
  auto set_env_flag = [](const char* name, bool val) {
    setenv(name, val ? "1" : "0", 0);
  };
  set_env_int("ORATOR_LOG_LEVEL", cfg.log_level);
  set_env_flag("ORATOR_TIMEBASE_CHECK", cfg.timebase_check);
  set_env_flag("ORATOR_ASR_PROFILE", cfg.asr_profile);
  set_env_int("ORATOR_ASR_BAN_STEPS", cfg.asr_ban_steps);
  set_env_int("ORATOR_ASR_DECODE_BATCH", cfg.asr_decode_batch);
  set_env_flag("ORATOR_STREAM_PROGRESS", cfg.stream_progress);
  if (!cfg.asr_system_prompt.empty()) {
    setenv("ORATOR_ASR_SYSTEM_PROMPT", cfg.asr_system_prompt.c_str(), 0);
  }
  set_env_int("ORATOR_GPU_SERIAL", cfg.gpu_scheduling_mode == 1);
  set_env_int("ORATOR_GPU_CONCURRENT", cfg.gpu_scheduling_mode == 2);

  // ── Step 5: environment variables override toml + defaults ────────
  ReadEnvInt("ORATOR_ASR_MAX_NEW_TOKENS", &cfg.asr_max_new_tokens);
  ReadEnvDouble("ORATOR_ASR_SEGMENT_SEC", &cfg.asr_segment_sec);
  ReadEnvString("ORATOR_ASR_LANGUAGE", &cfg.asr_language);
  ReadEnvString("ORATOR_ASR_MODEL_DIR", &cfg.asr_model_dir);
  {
    const char* asr_disable = std::getenv("ORATOR_ASR_DISABLE");
    if (asr_disable && asr_disable[0] == '1')
      cfg.asr_model_dir.clear();
  }
  ReadEnvInt("ORATOR_PORT", &cfg.port);
  ReadEnvInt("ORATOR_UI_PORT", &cfg.ui_port);
  ReadEnvString("ORATOR_UI_ROOT", &cfg.ui_root);
  cfg.vad_stream = ReadEnvFlag("ORATOR_VAD_STREAM", cfg.vad_stream);
  ReadEnvString("ORATOR_VAD_MODEL", &cfg.vad_model);
  ReadEnvFloat("ORATOR_VAD_THRESHOLD", &cfg.vad_threshold);
  ReadEnvInt("ORATOR_VAD_MIN_SPEECH_MS", &cfg.vad_min_speech_ms);
  ReadEnvInt("ORATOR_VAD_MIN_SILENCE_MS", &cfg.vad_min_silence_ms);
  ReadEnvDouble("ORATOR_GPU_TELEMETRY_SEC", &cfg.gpu_telemetry_interval_sec);
  ReadEnvString("ORATOR_STORAGE_DISK_PATH", &cfg.storage_disk_path);
  ReadEnvString("ORATOR_SESSION_DIR", &cfg.session_dir);
  ReadEnvInt("ORATOR_LOG_LEVEL", &cfg.log_level);
  cfg.timebase_check = ReadEnvFlag("ORATOR_TIMEBASE_CHECK", cfg.timebase_check);
  cfg.asr_profile = ReadEnvFlag("ORATOR_ASR_PROFILE", cfg.asr_profile);
  ReadEnvInt("ORATOR_ASR_BAN_STEPS", &cfg.asr_ban_steps);
  ReadEnvInt("ORATOR_ASR_DECODE_BATCH", &cfg.asr_decode_batch);
  cfg.stream_progress = ReadEnvFlag("ORATOR_STREAM_PROGRESS", cfg.stream_progress);
  ReadEnvString("ORATOR_ASR_SYSTEM_PROMPT", &cfg.asr_system_prompt);
  ReadEnvDouble("ORATOR_DIAR_ONSET", &cfg.diar_onset);
  ReadEnvDouble("ORATOR_DIAR_OFFSET", &cfg.diar_offset);
  ReadEnvDouble("ORATOR_DIAR_PAD_ONSET", &cfg.diar_pad_onset);
  ReadEnvDouble("ORATOR_DIAR_PAD_OFFSET", &cfg.diar_pad_offset);
  ReadEnvDouble("ORATOR_DIAR_MIN_DUR_ON", &cfg.diar_min_dur_on);
  ReadEnvDouble("ORATOR_DIAR_MIN_DUR_OFF", &cfg.diar_min_dur_off);
  // gpu_scheduling_mode: env ORATOR_GPU_SERIAL=1 → mode 1, ORATOR_GPU_CONCURRENT=1 → mode 2
  int gpu_mode = 0;
  if (ReadEnvInt("ORATOR_GPU_SERIAL", &gpu_mode) && gpu_mode == 1) {
    cfg.gpu_scheduling_mode = 1;
  } else if (ReadEnvInt("ORATOR_GPU_CONCURRENT", &gpu_mode) && gpu_mode == 1) {
    cfg.gpu_scheduling_mode = 2;
  }

  // ── Step 6: finalize port / ui_port ───────────────────────────────
  if (cfg.ui_port <= 0) cfg.ui_port = cfg.port + 1;

  // ── Step 7: start pipeline + servers ──────────────────────────────
  auto emit_target = std::make_shared<net::SessionEmit>();
  auto stream = std::make_shared<pipeline::AuditoryStream>(
      cfg, [emit_target](const std::string& json) { emit_target->Send(json); });
  try {
    std::cout << "loading pipelines ..." << std::endl;
    stream->Start();
    std::cout << "pipelines ready" << std::endl;
  } catch (const std::exception& e) {
    std::cerr << "pipeline init failed: " << e.what() << std::endl;
    return 1;
  }

  net::WebSocketServer server(cfg.port);
  if (!server.Start()) {
    std::cerr << "failed to bind port " << cfg.port << std::endl;
    return 1;
  }
  net::HttpStaticServer ui(cfg.ui_port, cfg.ui_root);
  if (!ui.Start()) {
    std::cerr << "failed to bind UI port " << cfg.ui_port
              << " (set ORATOR_UI_PORT to override)" << std::endl;
    return 1;
  }

  std::thread ui_thread([&ui]() { ui.Serve(); });

  std::cout << "orator_ws listening on ws://0.0.0.0:" << server.port() << "\n"
            << "orator_ui listening on http://0.0.0.0:" << ui.port() << "\n"
            << "  ui_root:  " << ui.web_root() << "\n"
            << "  diarizer: " << cfg.diarizer_weights << "\n"
            << "  asr:      "
            << (cfg.asr_model_dir.empty() ? "(disabled)" : cfg.asr_model_dir)
            << "\n"
            << "  vad:      " << cfg.vad_model << "\n"
            << "  asr_cfg:  max_new=" << cfg.asr_max_new_tokens
            << " segment=" << cfg.asr_segment_sec
            << "s lang=" << cfg.asr_language
            << std::endl;
  std::cout << "  send binary PCM (int16le mono 16k); text {\"flush\"} or "
               "{\"end\"} to get the timeline." << std::endl;

  server.Serve([stream, emit_target]() -> std::unique_ptr<net::WebSocketHandler> {
    return std::make_unique<net::AuditoryWsHandler>(stream, emit_target);
  });

  ui.Stop();
  if (ui_thread.joinable()) ui_thread.join();
  return 0;
}
