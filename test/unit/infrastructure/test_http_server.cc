// HttpStaticServer unit tests:
//   1) Constructor and accessors.
//   2) Serve a real file (index.html) over loopback.
//   3) 404 for nonexistent file.
//   4) MIME type detection (.js, .css).

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <thread>

#include "net/http_static_server.h"

using namespace orator;
using namespace orator::net;

static int g_fail = 0;
#define CHECK(cond, msg)                \
  do {                                  \
    if (!(cond)) {                      \
      std::printf("FAIL: %s\n", msg);   \
      ++g_fail;                         \
    }                                   \
  } while (0)

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Create a temp directory with a few test files and return its path.
static std::string CreateTempWebRoot() {
  char tmpl[] = "/tmp/orator_http_test_XXXXXX";
  char* dir = ::mkdtemp(tmpl);
  assert(dir != nullptr);

  {
    std::ofstream f(std::string(dir) + "/index.html");
    f << "<html><body>Hello World</body></html>";
  }
  {
    std::ofstream f(std::string(dir) + "/test.js");
    f << "console.log('test');";
  }
  {
    std::ofstream f(std::string(dir) + "/style.css");
    f << "body { color: red; }";
  }

  return std::string(dir);
}

static void CleanupTempDir(const std::string& dir) {
  ::unlink((dir + "/index.html").c_str());
  ::unlink((dir + "/test.js").c_str());
  ::unlink((dir + "/style.css").c_str());
  ::rmdir(dir.c_str());
}

// Stop the server and unblock accept() by connecting to the port.
// Closing the listen fd from another thread may not reliably interrupt
// accept(), so we connect once to make it return.
static void StopServer(HttpStaticServer& server, int port) {
  server.Stop();
  int tmp = ::socket(AF_INET, SOCK_STREAM, 0);
  if (tmp >= 0) {
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    ::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    ::connect(tmp, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    ::close(tmp);
  }
}

// Connect to 127.0.0.1:port, send a raw HTTP request, return the full response.
static std::string SendRequest(int port, const std::string& request) {
  int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  assert(fd >= 0);

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(static_cast<uint16_t>(port));
  ::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

  int cr = ::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
  assert(cr == 0);

  ::send(fd, request.data(), request.size(), 0);

  // Shutdown write side so the server sees EOF and closes.
  ::shutdown(fd, SHUT_WR);

  std::string resp;
  char buf[4096];
  ssize_t n;
  while ((n = ::recv(fd, buf, sizeof(buf), 0)) > 0) {
    resp.append(buf, static_cast<size_t>(n));
  }

  ::close(fd);
  return resp;
}

// ---------------------------------------------------------------------------
// Test cases
// ---------------------------------------------------------------------------

static void test_constructor() {
  std::printf("  HttpStaticServer: constructor and accessors... ");
  HttpStaticServer server(8080, "/tmp/web");
  CHECK(server.port() == 8080, "port matches constructor arg");
  CHECK(server.web_root() == "/tmp/web", "web_root matches constructor arg");
  std::printf("PASS\n");
}

static void test_serve_index_html() {
  std::printf("  HttpStaticServer: serve index.html... ");
  std::string root = CreateTempWebRoot();

  HttpStaticServer server(0, root);
  CHECK(server.Start(), "server starts on OS-assigned port");
  int port = server.port();
  CHECK(port > 0, "port is non-zero");

  std::thread srv([&]() { server.Serve(); });

  // Give the server a moment to start accepting.
  ::usleep(100000);

  std::string resp = SendRequest(port, "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n");

  CHECK(resp.find("200 OK") != std::string::npos,
        "response contains 200 OK");
  CHECK(resp.find("Content-Type: text/html") != std::string::npos,
        "Content-Type is text/html");
  CHECK(resp.find("Hello World") != std::string::npos,
        "response body contains file content");

  StopServer(server, port);
  srv.join();
  CleanupTempDir(root);
  std::printf("PASS\n");
}

static void test_404_not_found() {
  std::printf("  HttpStaticServer: 404 for nonexistent file... ");
  std::string root = CreateTempWebRoot();

  HttpStaticServer server(0, root);
  CHECK(server.Start(), "server starts");
  int port = server.port();

  std::thread srv([&]() { server.Serve(); });
  ::usleep(100000);

  std::string resp =
      SendRequest(port, "GET /nonexistent.html HTTP/1.1\r\nHost: localhost\r\n\r\n");

  CHECK(resp.find("404 Not Found") != std::string::npos,
        "response contains 404 Not Found");

  StopServer(server, port);
  srv.join();
  CleanupTempDir(root);
  std::printf("PASS\n");
}

static void test_mime_types() {
  std::printf("  HttpStaticServer: MIME type detection... ");
  std::string root = CreateTempWebRoot();

  HttpStaticServer server(0, root);
  CHECK(server.Start(), "server starts");
  int port = server.port();

  std::thread srv([&]() { server.Serve(); });
  ::usleep(100000);

  // .js
  {
    std::string resp =
        SendRequest(port, "GET /test.js HTTP/1.1\r\nHost: localhost\r\n\r\n");
    CHECK(resp.find("200 OK") != std::string::npos, "JS file returns 200");
    CHECK(resp.find("application/javascript") != std::string::npos,
          "JS Content-Type is application/javascript");
  }

  // .css
  {
    std::string resp =
        SendRequest(port, "GET /style.css HTTP/1.1\r\nHost: localhost\r\n\r\n");
    CHECK(resp.find("200 OK") != std::string::npos, "CSS file returns 200");
    CHECK(resp.find("text/css") != std::string::npos,
          "CSS Content-Type is text/css");
  }

  StopServer(server, port);
  srv.join();
  CleanupTempDir(root);
  std::printf("PASS\n");
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main() {
  std::printf("Testing HttpStaticServer...\n\n");

  test_constructor();
  test_serve_index_html();
  test_404_not_found();
  test_mime_types();

  if (g_fail == 0) {
    std::printf("\nAll HTTP server tests PASSED\n");
    return 0;
  }
  std::printf("\nHTTP server tests FAILED (%d checks)\n", g_fail);
  return 1;
}
