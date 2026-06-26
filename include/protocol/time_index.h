#pragma once

// TimeIndex (Spec 004 Phase 10): time-ordered message index per topic.
//
// Maintains a sorted vector of IndexedMessage per topic. Supports range replay,
// last-message lookup, and retention-based eviction. Thread-safe via mutex.

#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "protocol/storage.h"

namespace orator {
namespace protocol {

class TimeIndex {
 public:
  // Append a message to the index for a topic. Messages are sorted by timestamp.
  // Returns true if the message was out-of-order (timestamp < last stored).
  bool Append(const std::string& topic, double timestamp_sec,
              const StorageRef& ref, uint64_t msg_id);

  // Replay all messages for a topic from `from_sec` onward.
  std::vector<IndexedMessage> Replay(const std::string& topic,
                                     double from_sec) const;

  // Get the last (most recent) message for a topic.
  IndexedMessage const* Last(const std::string& topic) const;

  // Get all indexed messages for a topic.
  std::vector<IndexedMessage> GetAll(const std::string& topic) const;

  // Remove messages older than `cutoff_sec` for a topic.
  void Retain(const std::string& topic, double cutoff_sec);

  // Clean up old entries across all topics to prevent memory accumulation
  void CleanupOldEntries(double keep_until_sec);

 private:
  std::map<std::string, std::vector<IndexedMessage>> index_;
  mutable std::mutex mutex_;
};

}  // namespace protocol
}  // namespace orator
