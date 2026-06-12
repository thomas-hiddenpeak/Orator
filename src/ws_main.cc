// orator_ws: real-time WebSocket server running diarization + ASR as two
// independent pipelines over one streamed audio source.
//
// Usage: orator_ws [port] [diar_weights.safetensors] [asr_model_dir]
// Streams a unified timeline (diarization segments + transcript) back to
// clients that push PCM audio. If asr_model_dir is omitted, ASR is disabled and
// only diarization runs.

#include <csignal>
#include <cstdlib>
#include <iostream>
#include <memory>

#include "net/auditory_ws_handler.h"
#include "net/websocket_server.h"

using namespace orator;

int main(int argc, char** argv) {
  int port = argc > 1 ? std::atoi(argv[1]) : 8765;
  pipeline::AuditoryStream::Config cfg;
  if (argc > 2) cfg.diarizer_weights = argv[2];
  if (argc > 3) cfg.asr_model_dir = argv[3];

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
            << std::endl;
  std::cout << "  send binary PCM (int16le mono 16k); text {\"flush\"} or "
               "{\"end\"} to get the timeline." << std::endl;

  server.Serve([&cfg]() -> std::unique_ptr<net::WebSocketHandler> {
    return std::make_unique<net::AuditoryWsHandler>(cfg);
  });
  return 0;
}
