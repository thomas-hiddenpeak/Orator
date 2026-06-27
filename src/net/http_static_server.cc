#include "net/http_static_server.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <fstream>
#include <sstream>

namespace orator {
namespace net {
namespace {

bool WriteAll(int fd, const char* data, size_t n) {
  size_t sent = 0;
  while (sent < n) {
    const ssize_t w = ::send(fd, data + sent, n - sent, MSG_NOSIGNAL);
    if (w <= 0) return false;
    sent += static_cast<size_t>(w);
  }
  return true;
}

std::string GuessContentType(const std::string& path) {
  const size_t dot = path.find_last_of('.');
  const std::string ext = dot == std::string::npos ? "" : path.substr(dot);
  if (ext == ".html") return "text/html; charset=utf-8";
  if (ext == ".css") return "text/css; charset=utf-8";
  if (ext == ".js") return "application/javascript; charset=utf-8";
  if (ext == ".json") return "application/json; charset=utf-8";
  if (ext == ".svg") return "image/svg+xml";
  if (ext == ".png") return "image/png";
  if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
  if (ext == ".ico") return "image/x-icon";
  return "text/plain; charset=utf-8";
}

bool IsSafePath(const std::string& p) {
  if (p.empty() || p[0] != '/') return false;
  if (p.find("..") != std::string::npos) return false;
  if (p.find('\\') != std::string::npos) return false;
  return true;
}

}  // namespace

HttpStaticServer::HttpStaticServer(int port, std::string web_root)
    : port_(port), web_root_(std::move(web_root)) {}

HttpStaticServer::~HttpStaticServer() { Stop(); }

bool HttpStaticServer::Start() {
  listen_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
  if (listen_fd_ < 0) return false;

  int yes = 1;
  ::setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(static_cast<uint16_t>(port_));
  if (::bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) <
      0) {
    ::close(listen_fd_);
    listen_fd_ = -1;
    return false;
  }

  socklen_t alen = sizeof(addr);
  if (::getsockname(listen_fd_, reinterpret_cast<sockaddr*>(&addr), &alen) == 0)
    port_ = ntohs(addr.sin_port);

  if (::listen(listen_fd_, 8) < 0) {
    ::close(listen_fd_);
    listen_fd_ = -1;
    return false;
  }

  running_ = true;
  return true;
}

void HttpStaticServer::Stop() {
  running_ = false;
  if (listen_fd_ >= 0) {
    ::close(listen_fd_);
    listen_fd_ = -1;
  }
}

void HttpStaticServer::Serve() {
  if (listen_fd_ < 0) return;
  while (running_) {
    sockaddr_in caddr{};
    socklen_t clen = sizeof(caddr);
    const int cfd =
        ::accept(listen_fd_, reinterpret_cast<sockaddr*>(&caddr), &clen);
    if (cfd < 0) {
      if (errno == EINTR) continue;
      if (!running_) break;
      continue;
    }
    HandleClient(cfd);
    ::close(cfd);
  }
}

bool HttpStaticServer::HandleClient(int fd) {
  std::string req;
  req.reserve(2048);
  char buf[1024];
  while (req.find("\r\n\r\n") == std::string::npos) {
    const ssize_t r = ::recv(fd, buf, sizeof(buf), 0);
    if (r <= 0) return false;
    req.append(buf, static_cast<size_t>(r));
    if (req.size() > 16384) {
      const std::string body = "Request header too large\n";
      return SendResponse(fd, 413, "Payload Too Large", "text/plain", body);
    }
  }

  const size_t line_end = req.find("\r\n");
  if (line_end == std::string::npos) {
    return SendResponse(fd, 400, "Bad Request", "text/plain", "Bad request\n");
  }

  const std::string line = req.substr(0, line_end);
  const size_t m1 = line.find(' ');
  const size_t m2 =
      m1 == std::string::npos ? std::string::npos : line.find(' ', m1 + 1);
  if (m1 == std::string::npos || m2 == std::string::npos) {
    return SendResponse(fd, 400, "Bad Request", "text/plain",
                        "Bad request line\n");
  }

  const std::string method = line.substr(0, m1);
  const std::string path = line.substr(m1 + 1, m2 - (m1 + 1));
  if (method != "GET") {
    return SendResponse(fd, 405, "Method Not Allowed", "text/plain",
                        "Only GET is supported\n");
  }

  std::string content;
  std::string content_type;
  if (!LoadFileForPath(path, &content, &content_type)) {
    return SendResponse(fd, 404, "Not Found", "text/plain", "Not found\n");
  }
  return SendResponse(fd, 200, "OK", content_type.c_str(), content);
}

bool HttpStaticServer::SendResponse(int fd, int status, const char* reason,
                                    const char* content_type,
                                    const std::string& body) {
  std::ostringstream oss;
  oss << "HTTP/1.1 " << status << " " << reason << "\r\n"
      << "Content-Type: " << content_type << "\r\n"
      << "Content-Length: " << body.size() << "\r\n"
      << "Connection: close\r\n"
      << "Cache-Control: no-store\r\n\r\n";
  const std::string head = oss.str();
  return WriteAll(fd, head.data(), head.size()) &&
         WriteAll(fd, body.data(), body.size());
}

bool HttpStaticServer::LoadFileForPath(const std::string& raw_path,
                                       std::string* content,
                                       std::string* content_type) const {
  if (content == nullptr || content_type == nullptr) return false;

  std::string path = raw_path;
  const size_t q = path.find('?');
  if (q != std::string::npos) path = path.substr(0, q);
  if (path.empty()) path = "/";
  if (path == "/") path = "/index.html";
  if (!IsSafePath(path)) return false;

  const std::string full = web_root_ + path;
  std::ifstream f(full, std::ios::binary);
  if (!f.good()) return false;

  std::ostringstream ss;
  ss << f.rdbuf();
  *content = ss.str();
  *content_type = GuessContentType(path);
  return true;
}

}  // namespace net
}  // namespace orator
