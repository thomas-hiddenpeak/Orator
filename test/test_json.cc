#include <cstdio>
#include <iostream>
#include <sstream>
#include <string>

#include "core/types.h"
#include "io/json_sink.h"

using namespace orator;

static int fails = 0;

#define CHECK(cond, msg)                                                \
  do {                                                                  \
    if (!(cond)) {                                                      \
      std::fprintf(stderr, "  FAIL [%s:%d] %s\n", __FILE__, __LINE__, msg); \
      ++fails;                                                          \
    } else {                                                            \
      std::printf("  OK %s\n", msg);                                    \
    }                                                                   \
  } while (0)

// Helper: check that a string contains a substring.
static bool Contains(const std::string& s, const std::string& sub) {
  return s.find(sub) != std::string::npos;
}

int main() {
  std::printf("=== JsonSink / TimelineToJson unit tests ===\n\n");

  // ── TimelineToJson: empty timeline ──────────────────────────────────
  std::printf("-- Empty timeline --\n");
  {
    core::Timeline tl;
    std::string json = io::TimelineToJson(tl, false);
    CHECK(json == "{\"segments\":[]}", "empty timeline (compact)");
    std::printf("  got: %s\n", json.c_str());

    std::string pretty = io::TimelineToJson(tl, true);
    CHECK(Contains(pretty, "\"segments\""), "empty timeline (pretty) has segments key");
    CHECK(Contains(pretty, "["), "empty timeline (pretty) has opening bracket");
    CHECK(Contains(pretty, "]"), "empty timeline (pretty) has closing bracket");
    std::printf("  pretty: %s\n", pretty.c_str());
  }

  // ── TimelineToJson: single segment ──────────────────────────────────
  std::printf("\n-- Single segment --\n");
  {
    core::Timeline tl;
    tl.segments.push_back({0.0, 2.5, "alice", "Hello world."});

    std::string json = io::TimelineToJson(tl, false);
    CHECK(Contains(json, "\"start\":0.000"), "single segment start");
    CHECK(Contains(json, "\"end\":2.500"), "single segment end");
    CHECK(Contains(json, "\"speaker_id\":\"alice\""), "single segment speaker_id");
    CHECK(Contains(json, "\"text\":\"Hello world.\""), "single segment text");
    std::printf("  compact: %s\n", json.c_str());
  }

  // ── TimelineToJson: multiple segments ───────────────────────────────
  std::printf("\n-- Multiple segments --\n");
  {
    core::Timeline tl;
    tl.segments.push_back({0.0, 1.5, "speaker_0", "First utterance."});
    tl.segments.push_back({1.5, 3.0, "speaker_1", "Second utterance."});

    std::string json = io::TimelineToJson(tl, false);
    CHECK(Contains(json, "\"speaker_id\":\"speaker_0\""), "multi segment speaker_0");
    CHECK(Contains(json, "\"speaker_id\":\"speaker_1\""), "multi segment speaker_1");
    CHECK(Contains(json, "\"text\":\"First utterance.\""), "multi segment text 0");
    CHECK(Contains(json, "\"text\":\"Second utterance.\""), "multi segment text 1");
    // Verify it's valid JSON-ish: starts with {, ends with }
    CHECK(json.front() == '{' && json.back() == '}', "multi segment JSON wrapper");
    std::printf("  compact: %s\n", json.c_str());
  }

  // ── TimelineToJson: special characters ──────────────────────────────
  std::printf("\n-- Special characters --\n");
  {
    core::Timeline tl;
    tl.segments.push_back({0.0, 1.0, "test", "Line1\nLine2\tTabbed\"Quoted\"\\End"});

    std::string json = io::TimelineToJson(tl, false);
    CHECK(Contains(json, "\\n"), "special char newline escaped");
    CHECK(Contains(json, "\\t"), "special char tab escaped");
    CHECK(Contains(json, "\\\""), "special char double-quote escaped");
    CHECK(Contains(json, "\\\\"), "special char backslash escaped");
    std::printf("  escaped: %s\n", json.c_str());
  }

  // ── TimelineToJson: pretty formatting ───────────────────────────────
  std::printf("\n-- Pretty formatting --\n");
  {
    core::Timeline tl;
    tl.segments.push_back({0.0, 1.0, "alice", "Hi"});

    std::string pretty = io::TimelineToJson(tl, true);
    // Pretty output has newlines and indentation
    CHECK(Contains(pretty, "\n"), "pretty output has newlines");
    CHECK(Contains(pretty, "  "), "pretty output has indentation");
    std::printf("  pretty:\n%s\n", pretty.c_str());

    std::string compact = io::TimelineToJson(tl, false);
    CHECK(!Contains(compact, "\n"), "compact output has no newlines");
    std::printf("  compact: %s\n", compact.c_str());
  }

  // ── JsonSink::Consume ───────────────────────────────────────────────
  std::printf("\n-- JsonSink::Consume --\n");
  {
    core::Timeline tl;
    tl.segments.push_back({1.0, 2.0, "bob", "Test message."});

    std::ostringstream oss;
    io::JsonSink sink(oss, false);
    sink.Consume(tl);

    std::string output = oss.str();
    CHECK(Contains(output, "\"speaker_id\":\"bob\""), "JsonSink output has speaker_id");
    CHECK(Contains(output, "\"text\":\"Test message.\""), "JsonSink output has text");
    // Consume appends a newline
    CHECK(!output.empty() && output.back() == '\n', "JsonSink output ends with newline");
    std::printf("  output: %s", output.c_str());
  }

  // ── JsonSink::Consume with pretty ───────────────────────────────────
  std::printf("\n-- JsonSink::Consume (pretty) --\n");
  {
    core::Timeline tl;
    tl.segments.push_back({0.5, 1.5, "charlie", "Pretty output."});

    std::ostringstream oss;
    io::JsonSink sink(oss, true);
    sink.Consume(tl);

    std::string output = oss.str();
    CHECK(Contains(output, "\n"), "pretty JsonSink output has newlines");
    CHECK(Contains(output, "  "), "pretty JsonSink output has indentation");
    std::printf("  output:\n%s", output.c_str());
  }

  // ── Summary ─────────────────────────────────────────────────────────
  std::printf("\n");
  if (fails) {
    std::printf("FAIL: %d test(s) failed\n", fails);
    return 1;
  }
  std::printf("All JSON sink tests PASSED\n");
  return 0;
}
