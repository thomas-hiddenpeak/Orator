#pragma once

#include <string>

namespace orator {
namespace net {

// Minimal dependency-free HTTP static file server for local UI hosting.
// It is intentionally simple: GET-only, one request per connection, no TLS.
class HttpStaticServer {
 public:
  HttpStaticServer(int port, std::string web_root);
  ~HttpStaticServer();

  bool Start();
  void Serve();
  void Stop();

  int port() const { return port_; }
  const std::string& web_root() const { return web_root_; }

 private:
  bool HandleClient(int fd);
  bool SendResponse(int fd, int status, const char* reason,
                    const char* content_type, const std::string& body);
  bool LoadFileForPath(const std::string& raw_path, std::string* content,
                       std::string* content_type) const;

  int port_;
  std::string web_root_;
  int listen_fd_ = -1;
  bool running_ = false;
};

}  // namespace net
}  // namespace orator
