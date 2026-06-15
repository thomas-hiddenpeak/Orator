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
#include <iostream>
#include <memory>
#include <string>

#include "net/auditory_ws_handler.h"
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

}  // namespace

int main(int argc, char** argv) {
  int port = argc > 1 ? std::atoi(argv[1]) : 8765;
  pipeline::AuditoryStream::Config cfg;
  if (argc > 2) cfg.diarizer_weights = argv[2];
  if (argc > 3) cfg.asr_model_dir = argv[3];
  if (argc > 4) cfg.asr_vad_model = argv[4];

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

  std::signal(SIGPIPE, SIG_IGN);

  net::WebSocketServer server(port);
  if (!server.Start()) {
    std::cerr << "failed to bind port " << port << std::endl;
    return 1;
  }
  std::cout << "orator_ws listening on ws://0.0.0.0:" << server.port() << "\n"
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

  server.Serve([&cfg]() -> std::unique_ptr<net::WebSocketHandler> {
    return std::make_unique<net::AuditoryWsHandler>(cfg);
  });
  return 0;
}
