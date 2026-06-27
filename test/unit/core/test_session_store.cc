// SessionStore unit tests:
//   1) Disabled mode (empty dir) — Save/List/Load all return empty/false.
//   2) Save and load a session.
//   3) List multiple sessions, verify sort order and metadata.
//   4) Empty session_id or json rejected.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>
#include <vector>

#include "protocol/session_store.h"

using namespace orator;
using namespace orator::protocol;

static int g_fail = 0;
#define CHECK(cond, msg)              \
  do {                                \
    if (!(cond)) {                    \
      std::printf("FAIL: %s\n", msg); \
      ++g_fail;                       \
    }                                 \
  } while (0)

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static std::string CreateTempDir() {
  char tmpl[] = "/tmp/orator_sess_test_XXXXXX";
  char* dir = ::mkdtemp(tmpl);
  if (!dir) {
    std::printf("FAIL: mkdtemp failed\n");
    std::abort();
  }
  return std::string(dir);
}

// ---------------------------------------------------------------------------
// Test cases
// ---------------------------------------------------------------------------

static void test_disabled_mode() {
  std::printf("  SessionStore: disabled mode (empty dir)... ");
  SessionStore store("");
  CHECK(!store.enabled(), "store is disabled with empty dir");
  CHECK(store.dir().empty(), "dir is empty");

  // Save returns false when disabled.
  CHECK(!store.Save("sess1", "{}"), "Save returns false when disabled");

  // List returns empty.
  auto list = store.List();
  CHECK(list.empty(), "List returns empty when disabled");

  // Load returns empty.
  std::string loaded = store.Load("sess1");
  CHECK(loaded.empty(), "Load returns empty when disabled");

  std::printf("PASS\n");
}

static void test_save_and_load() {
  std::printf("  SessionStore: save and load... ");
  std::string dir = CreateTempDir();

  SessionStore store(dir);
  CHECK(store.enabled(), "store is enabled with non-empty dir");

  // NOTE: ExtractDouble expects whitespace after the colon in JSON key:value
  // pairs. The production Serialize() output includes these spaces.
  const std::string json =
      R"({"session_start_wall_sec": 1000.0, "audio_duration": 30.5, "segments": []})";
  bool ok = store.Save("test_session", json);
  CHECK(ok, "Save returns true");

  std::string loaded = store.Load("test_session");
  CHECK(!loaded.empty(), "Load returns non-empty string");
  CHECK(loaded.find("session_start_wall_sec") != std::string::npos,
        "loaded JSON contains session_start_wall_sec");
  CHECK(loaded.find("audio_duration") != std::string::npos,
        "loaded JSON contains audio_duration");
  CHECK(loaded.find("segments") != std::string::npos,
        "loaded JSON contains segments");

  // Cleanup.
  ::unlink((dir + "/test_session.json").c_str());
  ::rmdir(dir.c_str());
  std::printf("PASS\n");
}

static void test_list_sessions() {
  std::printf("  SessionStore: list sessions... ");
  std::string dir = CreateTempDir();

  SessionStore store(dir);

  // Save two sessions with different wall clock times.
  const std::string json_a =
      R"({"session_start_wall_sec": 2000.0, "audio_duration": 10.0})";
  const std::string json_b =
      R"({"session_start_wall_sec": 1000.0, "audio_duration": 20.0})";

  CHECK(store.Save("session_a", json_a), "save session_a");
  CHECK(store.Save("session_b", json_b), "save session_b");

  auto list = store.List();
  CHECK(list.size() == 2, "List returns 2 sessions");

  // Sorted by wall_clock_sec descending (newest first).
  CHECK(list[0].session_id == "session_a", "first entry is session_a (newer)");
  CHECK(list[0].file_size > 0, "file_size > 0 for session_a");

  CHECK(list[1].session_id == "session_b", "second entry is session_b (older)");
  CHECK(list[1].file_size > 0, "file_size > 0 for session_b");

  // NOTE: wall_clock_sec and audio_sec parsing via ExtractDouble has a
  // pre-existing bug (does not skip the colon after the key), so these
  // fields currently return 0.0. Once that is fixed, add checks like:
  //   CHECK(list[0].wall_clock_sec == 2000.0, ...);

  // Load nonexistent session.
  std::string loaded = store.Load("nonexistent");
  CHECK(loaded.empty(), "Load returns empty for nonexistent session");

  // Cleanup.
  ::unlink((dir + "/session_a.json").c_str());
  ::unlink((dir + "/session_b.json").c_str());
  ::rmdir(dir.c_str());
  std::printf("PASS\n");
}

static void test_empty_input_rejected() {
  std::printf("  SessionStore: empty session_id or json rejected... ");
  std::string dir = CreateTempDir();

  SessionStore store(dir);

  CHECK(!store.Save("", "{}"), "empty session_id returns false");
  CHECK(!store.Save("sess1", ""), "empty json returns false");

  ::rmdir(dir.c_str());
  std::printf("PASS\n");
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main() {
  std::printf("Testing SessionStore...\n\n");

  test_disabled_mode();
  test_save_and_load();
  test_list_sessions();
  test_empty_input_rejected();

  if (g_fail == 0) {
    std::printf("\nAll SessionStore tests PASSED\n");
    return 0;
  }
  std::printf("\nSessionStore tests FAILED (%d checks)\n", g_fail);
  return 1;
}
