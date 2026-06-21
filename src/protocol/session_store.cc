#include "protocol/session_store.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <sys/stat.h>

#include <dirent.h>
#include <unistd.h>

#include "core/log.h"

namespace orator {
namespace protocol {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace {

// Extract a double value from a JSON key:value pair.
// Example: ExtractDouble(json, "\"session_start_wall_sec\":") -> 1234.567
// Returns 0.0 if not found.
double ExtractDouble(const std::string& json, const std::string& key) {
  auto pos = json.find(key);
  if (pos == std::string::npos) return 0.0;
  pos += key.size();
  // Skip whitespace and optional decimal point/comma
  while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) ++pos;
  if (pos >= json.size()) return 0.0;
  // Skip commas
  if (json[pos] == ',') ++pos;
  while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) ++pos;
  // Parse number
  char* end = nullptr;
  double val = std::strtod(json.data() + pos, &end);
  (void)end;
  return val;
}

// Ensure directory exists; create if missing.
bool EnsureDir(const std::string& path) {
  struct stat st;
  if (::stat(path.c_str(), &st) == 0) {
    if (S_ISDIR(st.st_mode)) return true;
    LOG_ERROR("SessionStore: %s exists but is not a directory\n", path.c_str());
    return false;
  }
  if (::mkdir(path.c_str(), 0755) == 0) return true;
  LOG_ERROR("SessionStore: failed to create directory %s\n", path.c_str());
  return false;
}

}  // namespace

// ---------------------------------------------------------------------------
// SessionStore
// ---------------------------------------------------------------------------

SessionStore::SessionStore(std::string dir) : dir_(std::move(dir)) {
  if (!dir_.empty() && dir_.back() != '/') dir_.push_back('/');
  if (!dir_.empty()) {
    EnsureDir(dir_);
  }
}

bool SessionStore::Save(const std::string& session_id,
                        const std::string& timeline_json) {
  if (dir_.empty()) return false;  // disabled
  if (session_id.empty() || timeline_json.empty()) {
    LOG_WARN("SessionStore: Save called with empty session_id or json\n");
    return false;
  }

  const std::string path = dir_ + session_id + ".json";
  const std::string tmp_path = path + ".tmp";

  // Atomic write: write to .tmp, then rename.
  {
    std::ofstream ofs(tmp_path, std::ios::binary);
    if (!ofs.is_open()) {
      LOG_ERROR("SessionStore: failed to open %s for writing\n", tmp_path.c_str());
      return false;
    }
    ofs.write(timeline_json.data(),
              static_cast<std::streamsize>(timeline_json.size()));
    ofs.close();
    if (ofs.fail()) {
      LOG_ERROR("SessionStore: write error on %s\n", tmp_path.c_str());
      ::unlink(tmp_path.c_str());
      return false;
    }
  }

  if (::rename(tmp_path.c_str(), path.c_str()) != 0) {
    LOG_ERROR("SessionStore: rename %s -> %s failed\n", tmp_path.c_str(),
              path.c_str());
    ::unlink(tmp_path.c_str());
    return false;
  }

  LOG_INFO("SessionStore: saved session %s (%zu bytes)\n",
           session_id.c_str(), timeline_json.size());
  return true;
}

std::vector<SessionInfo> SessionStore::List() const {
  std::vector<SessionInfo> result;
  if (dir_.empty()) return result;

  DIR* d = ::opendir(dir_.c_str());
  if (!d) {
    LOG_WARN("SessionStore: cannot open directory %s\n", dir_.c_str());
    return result;
  }

  struct dirent* entry;
  while ((entry = ::readdir(d)) != nullptr) {
    // Match *.json files only.
    const std::string name(entry->d_name);
    if (name.size() < 6 || name.compare(name.size() - 5, 5, ".json") != 0) {
      continue;
    }
    if (name.size() > 4 && name.compare(name.size() - 4, 4, ".tmp") == 0) {
      continue;  // skip orphaned .tmp files
    }

    const std::string session_id = name.substr(0, name.size() - 5);
    const std::string full_path = dir_ + name;

    // Read the first 2 KB for lightweight metadata extraction.
    std::ifstream ifs(full_path, std::ios::binary | std::ios::ate);
    if (!ifs.is_open()) continue;
    size_t file_size = static_cast<size_t>(ifs.tellg());
    size_t read_size = std::min(file_size, size_t{2048});
    ifs.seekg(0);
    std::string header(static_cast<size_t>(read_size), '\0');
    ifs.read(&header[0], static_cast<std::streamsize>(read_size));
    ifs.close();

    SessionInfo info = ParseInfo(session_id, header);
    info.file_size = file_size;
    result.push_back(std::move(info));
  }

  ::closedir(d);

  // Sort by wall clock descending (newest first).
  std::sort(result.begin(), result.end(),
            [](const SessionInfo& a, const SessionInfo& b) {
              return a.wall_clock_sec > b.wall_clock_sec;
            });

  return result;
}

std::string SessionStore::Load(const std::string& session_id) const {
  if (dir_.empty() || session_id.empty()) return {};

  const std::string path = dir_ + session_id + ".json";
  std::ifstream ifs(path, std::ios::binary | std::ios::ate);
  if (!ifs.is_open()) {
    LOG_WARN("SessionStore: session %s not found\n", session_id.c_str());
    return {};
  }

  size_t size = static_cast<size_t>(ifs.tellg());
  ifs.seekg(0);
  std::string json(size, '\0');
  ifs.read(&json[0], static_cast<std::streamsize>(size));
  ifs.close();

  return json;
}

SessionInfo SessionStore::ParseInfo(const std::string& session_id,
                                    const std::string& json) {
  SessionInfo info;
  info.session_id = session_id;
  info.wall_clock_sec = ExtractDouble(json, "\"session_start_wall_sec\"");
  info.audio_sec = ExtractDouble(json, "\"audio_duration\"");
  return info;
}

}  // namespace protocol
}  // namespace orator
