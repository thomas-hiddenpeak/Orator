// orator_ws: real-time diarization WebSocket server.
//
// Usage: orator_ws [port] [weights.safetensors] [max_buffer_sec]
// Streams diarized speaker segments back to clients that push PCM audio.

#include <csignal>
#include <cstdlib>
#include <iostream>
#include <memory>

#include "net/diarization_ws_handler.h"
#include "net/websocket_server.h"

using namespace orator;

int main(int argc, char** argv) {
  int port = argc > 1 ? std::atoi(argv[1]) : 8765;
  net::DiarizationWsConfig cfg;
  if (argc > 2) cfg.weights = argv[2];
  if (argc > 3) cfg.max_buffer_sec = std::atof(argv[3]);

  std::signal(SIGPIPE, SIG_IGN);

  net::WebSocketServer server(port);
  if (!server.Start()) {
    std::cerr << "failed to bind port " << port << std::endl;
    return 1;
  }
  std::cout << "orator_ws listening on ws://0.0.0.0:" << server.port()
            << "  (weights: " << cfg.weights << ")" << std::endl;
  std::cout << "  send binary PCM (int16le mono 16k); text {\"flush\"} or "
               "{\"end\"} to get the timeline." << std::endl;

  server.Serve([&cfg]() -> std::unique_ptr<net::WebSocketHandler> {
    return std::make_unique<net::DiarizationWsHandler>(cfg);
  });
  return 0;
}
