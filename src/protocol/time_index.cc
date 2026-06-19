#include "protocol/time_index.h"

#include <algorithm>

namespace orator {
namespace protocol {

bool TimeIndex::Append(const std::string& topic, double timestamp_sec,
                       const StorageRef& ref, uint64_t msg_id) {
  std::lock_guard<std::mutex> lock(mutex_);

  auto& entries = index_[topic];
  bool out_of_order = !entries.empty() &&
                      timestamp_sec < entries.back().timestamp_sec;

  IndexedMessage im;
  im.timestamp_sec = timestamp_sec;
  im.ref = ref;
  im.msg_id = msg_id;

  // Binary insert to maintain sorted order by timestamp.
  auto it = std::lower_bound(
      entries.begin(), entries.end(), timestamp_sec,
      [](const IndexedMessage& e, double ts) {
        return e.timestamp_sec < ts;
      });
  entries.insert(it, std::move(im));

  return out_of_order;
}

std::vector<IndexedMessage> TimeIndex::Replay(const std::string& topic,
                                              double from_sec) const {
  std::lock_guard<std::mutex> lock(mutex_);

  auto it = index_.find(topic);
  if (it == index_.end()) return {};

  const auto& entries = it->second;
  auto start = std::lower_bound(
      entries.begin(), entries.end(), from_sec,
      [](const IndexedMessage& e, double ts) {
        return e.timestamp_sec < ts;
      });

  return std::vector<IndexedMessage>(start, entries.end());
}

IndexedMessage const* TimeIndex::Last(const std::string& topic) const {
  std::lock_guard<std::mutex> lock(mutex_);

  auto it = index_.find(topic);
  if (it == index_.end() || it->second.empty()) return nullptr;

  return &it->second.back();
}

std::vector<IndexedMessage> TimeIndex::GetAll(const std::string& topic) const {
  std::lock_guard<std::mutex> lock(mutex_);

  auto it = index_.find(topic);
  if (it == index_.end()) return {};

  return it->second;
}

void TimeIndex::Retain(const std::string& topic, double cutoff_sec) {
  std::lock_guard<std::mutex> lock(mutex_);

  auto it = index_.find(topic);
  if (it == index_.end()) return;

  auto& entries = it->second;
  auto start = std::lower_bound(
      entries.begin(), entries.end(), cutoff_sec,
      [](const IndexedMessage& e, double ts) {
        return e.timestamp_sec < ts;
      });

  if (start != entries.begin()) {
    entries.erase(entries.begin(), start);
  }
}

}  // namespace protocol
}  // namespace orator
