// orator_ws: real-time WebSocket server running diarization + ASR as two
// independent pipelines over one streamed audio source.
//
// Usage: orator_ws [port] [diar_weights.safetensors] [asr_model_dir]
//                 [asr_vad_model.safetensors]
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
  int port = argc > 1 ? std::atoi(argv[1]) : 8765;
  int ui_port = 0;
  pipeline::AuditoryStream::Config cfg;
  const bool asr_arg_provided = argc > 3;
  if (argc > 2) cfg.diarizer_weights = argv[2];
  if (asr_arg_provided) cfg.asr_model_dir = argv[3];
  if (argc > 4) cfg.asr_vad_model = argv[4];

  // Default startup policy: bring all pipelines to standby for production-like
  // readiness. Keep explicit CLI override behavior: passing an empty third
  // argument still disables ASR intentionally.
  if (!asr_arg_provided) cfg.asr_model_dir = "models/asr/Qwen/Qwen3-ASR-1.7B";

  // Runtime tuning knobs for automated sweeps (keep CLI stable).
  ReadEnvDouble("ORATOR_ASR_MAX_UTTERANCE_SEC", &cfg.asr_max_utterance_sec);
  ReadEnvDouble("ORATOR_ASR_MIN_UTTERANCE_SEC", &cfg.asr_min_utterance_sec);
  ReadEnvFloat("ORATOR_ASR_VAD_THRESHOLD", &cfg.asr_vad_threshold);
  ReadEnvInt("ORATOR_ASR_VAD_MIN_SPEECH_MS", &cfg.asr_vad_min_speech_ms);
  ReadEnvInt("ORATOR_ASR_VAD_MIN_SILENCE_MS", &cfg.asr_vad_min_silence_ms);
  ReadEnvInt("ORATOR_ASR_VAD_SPEECH_PAD_MS", &cfg.asr_vad_speech_pad_ms);
  ReadEnvInt("ORATOR_ASR_MAX_NEW_TOKENS", &cfg.asr_max_new_tokens);
  ReadEnvInt("ORATOR_ASR_ROLLBACK_TOKENS", &cfg.asr_rollback_tokens);
  ReadEnvString("ORATOR_ASR_LANGUAGE", &cfg.asr_language);
  ReadEnvString("ORATOR_ASR_PREPROC_MODE", &cfg.asr_preproc_mode);
  ReadEnvString("ORATOR_ASR_FRCRN_MODEL", &cfg.asr_frcrn_model);
  ReadEnvString("ORATOR_ASR_TFGRIDNET_MODEL", &cfg.asr_tfgridnet_model);
  ReadEnvString("ORATOR_ASR_MODEL_DIR", &cfg.asr_model_dir);
  const bool asr_disable = std::getenv("ORATOR_ASR_DISABLE") != nullptr;
  if (asr_disable) cfg.asr_model_dir.clear();
  ReadEnvInt("ORATOR_UI_PORT", &ui_port);
  if (ui_port <= 0) ui_port = port + 1;
  std::string ui_root = "web";
  ReadEnvString("ORATOR_UI_ROOT", &ui_root);
  // prewarm_on_boot removed: stream is always loaded before listening.
  (void)ReadEnvFlag;  // suppress unused-warning

  // Spec 003/004 streaming + comprehensive-timeline knobs (real WS path).
  // The incremental KV-cache ASR path and the VAD stream are the PRODUCTION
  // DEFAULT (cfg defaults are true). The env vars are explicit opt-OUT switches
  // for regression comparison against the legacy Silero-VAD path:
  //   ORATOR_ASR_INCREMENTAL=0  -> use the legacy Silero-VAD utterance path
  //   ORATOR_VAD_STREAM=0       -> drop the independent VAD speech-segment stream
  //   ORATOR_ASR_ENDPOINT_RESET=0 -> incremental resets on a fixed cap only
  auto env_flag = [](const char* name, bool dflt) {
    return ReadEnvFlag(name, dflt);
  };
  cfg.asr_incremental = env_flag("ORATOR_ASR_INCREMENTAL", cfg.asr_incremental);
  if (cfg.asr_incremental) {
    ReadEnvDouble("ORATOR_ASR_SEGMENT_SEC", &cfg.asr_incremental_segment_sec);
    cfg.asr_incremental_endpoint_reset =
        env_flag("ORATOR_ASR_ENDPOINT_RESET", cfg.asr_incremental_endpoint_reset);
    ReadEnvDouble("ORATOR_ASR_MIN_SEGMENT_SEC",
                  &cfg.asr_incremental_min_segment_sec);
  }
  cfg.vad_stream = env_flag("ORATOR_VAD_STREAM", cfg.vad_stream);

  // Spec 002 FR7: periodic GPU-scheduling telemetry interval (seconds). 0 off.
  ReadEnvDouble("ORATOR_GPU_TELEMETRY_SEC", &cfg.gpu_telemetry_interval_sec);

  std::signal(SIGPIPE, SIG_IGN);

  // Create the shared emit target and stream ONCE. Models are loaded here and
  // reused across all client connections. This avoids repeated GPU model loads
  // which would exhaust device memory on Jetson.
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

  net::WebSocketServer server(port);
  if (!server.Start()) {
    std::cerr << "failed to bind port " << port << std::endl;
    return 1;
  }
  net::HttpStaticServer ui(ui_port, ui_root);
  if (!ui.Start()) {
    std::cerr << "failed to bind UI port " << ui_port
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
            << "  asr_vad:  " << cfg.asr_vad_model << "\n"
            << "  asr_cfg:  max_utt=" << cfg.asr_max_utterance_sec
            << "s min_utt=" << cfg.asr_min_utterance_sec
            << "s thr=" << cfg.asr_vad_threshold
            << " min_speech=" << cfg.asr_vad_min_speech_ms
            << "ms min_silence=" << cfg.asr_vad_min_silence_ms
            << "ms pad=" << cfg.asr_vad_speech_pad_ms
            << "ms max_new=" << cfg.asr_max_new_tokens
            << " rollback=" << cfg.asr_rollback_tokens
            << "ms lang=" << cfg.asr_language
            << " preproc=" << cfg.asr_preproc_mode
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
