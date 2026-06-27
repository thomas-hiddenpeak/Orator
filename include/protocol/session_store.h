#pragma once

// SessionStore — save/list/load timeline sessions on disk (Spec 004 §5.10).
//
// Each session is persisted as a standalone JSON file:
//   <session_dir>/<session_id>.json
//
// The JSON file contains the full timeline output from Serialize(), which
// includes session_start_wall_sec, audio_duration, segments, and text.
//
// A lightweight metadata index is built on the fly by reading file headers.
// For Phase 1 this is O(n) on List() — acceptable for < 1000 sessions.

#include <string>
#include <vector>

namespace orator {
namespace protocol {

// Lightweight metadata returned by List().
struct SessionInfo {
  std::string session_id;
  double wall_clock_sec = 0.0;  // session_start_wall_sec from timeline
  double audio_sec = 0.0;       // audio duration in seconds
  size_t file_size = 0;         // JSON file size in bytes
};

class SessionStore {
 public:
  // Construct with an explicit directory path. When dir is empty, Save() is a
  // no-op and List()/Load() return empty results (persistence disabled).
  explicit SessionStore(std::string dir);

  // Save a timeline JSON under a session ID. Written atomically (write to .tmp
  // then rename). Returns true on success.
  bool Save(const std::string& session_id, const std::string& timeline_json);

  // List all saved sessions with lightweight metadata (no timeline payload).
  // Returns empty vector when persistence is disabled or directory is empty.
  std::vector<SessionInfo> List() const;

  // Load the full timeline JSON for a session. Returns empty string if the
  // session does not exist or persistence is disabled.
  std::string Load(const std::string& session_id) const;

  // Returns true if persistence is enabled (non-empty directory).
  bool enabled() const { return !dir_.empty(); }

  // Returns the configured session directory.
  const std::string& dir() const { return dir_; }

 private:
  // Build a SessionInfo from a session ID and its JSON content (parses
  // wall_clock_sec and audio_duration from the first ~1 KB).
  static SessionInfo ParseInfo(const std::string& session_id,
                               const std::string& json);

  std::string dir_;  // empty = disabled
};

}  // namespace protocol
}  // namespace orator
