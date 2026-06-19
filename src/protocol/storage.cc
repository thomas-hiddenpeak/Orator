#include "protocol/storage.h"

#include <cstring>

#include "protocol/disk_backend.h"
#include "protocol/memory_backend.h"

namespace orator {
namespace protocol {

// ---------------------------------------------------------------------------
// Serialization helpers
// ---------------------------------------------------------------------------

namespace {

// Encode a uint32_t as 4 bytes little-endian.
void WriteLE32(uint8_t* buf, uint32_t val) {
  buf[0] = static_cast<uint8_t>(val & 0xFF);
  buf[1] = static_cast<uint8_t>((val >> 8) & 0xFF);
  buf[2] = static_cast<uint8_t>((val >> 16) & 0xFF);
  buf[3] = static_cast<uint8_t>((val >> 24) & 0xFF);
}

uint32_t ReadLE32(const uint8_t* buf) {
  return static_cast<uint32_t>(buf[0]) |
         (static_cast<uint32_t>(buf[1]) << 8) |
         (static_cast<uint32_t>(buf[2]) << 16) |
         (static_cast<uint32_t>(buf[3]) << 24);
}

// Encode a string as length-prefixed bytes.
// Returns number of bytes written (4 + string length).
uint32_t WriteString(std::string& out, const std::string& s) {
  uint32_t len = static_cast<uint32_t>(s.size());
  uint8_t buf[4];
  WriteLE32(buf, len);
  out.append(reinterpret_cast<char*>(buf), 4);
  if (!s.empty()) {
    out.append(s);
  }
  return 4 + len;
}

// Decode a string from a position in a byte buffer.
// Advances pos by (4 + string length).
std::string ReadString(const std::string& data, size_t& pos) {
  uint32_t len = ReadLE32(reinterpret_cast<const uint8_t*>(data.data()) + pos);
  pos += 4;
  std::string result;
  if (len > 0) {
    result = data.substr(static_cast<size_t>(pos), len);
    pos += len;
  }
  return result;
}

}  // namespace

std::string StorageManager::Serialize(const Message& msg) {
  std::string out;

  // Reserve approximate space to avoid reallocations.
  size_t est = sizeof(uint64_t) + sizeof(double) * 2 + sizeof(uint8_t) +
               sizeof(uint32_t) +
               4 + msg.topic.size() +
               4 + msg.pipeline.size() +
               4 + msg.pipeline_version.size() +
               4 + msg.data.size();
  out.reserve(est);

  // msg_id (uint64_t)
  out.append(reinterpret_cast<const char*>(&msg.msg_id), sizeof(msg.msg_id));
  // topic
  WriteString(out, msg.topic);
  // pipeline
  WriteString(out, msg.pipeline);
  // pipeline_version
  WriteString(out, msg.pipeline_version);
  // timestamp_sec (double)
  out.append(reinterpret_cast<const char*>(&msg.timestamp_sec),
             sizeof(msg.timestamp_sec));
  // wall_clock_sec (double)
  out.append(reinterpret_cast<const char*>(&msg.wall_clock_sec),
             sizeof(msg.wall_clock_sec));
  // qos (uint8_t)
  out.append(reinterpret_cast<const char*>(&msg.qos), sizeof(msg.qos));
  // schema_version (uint32_t)
  out.append(reinterpret_cast<const char*>(&msg.schema_version),
             sizeof(msg.schema_version));
  // data (string)
  WriteString(out, msg.data);

  return out;
}

Message StorageManager::Deserialize(const std::string& bytes) {
  Message msg;
  size_t pos = 0;

  // msg_id (uint64_t)
  std::memcpy(&msg.msg_id, bytes.data() + pos, sizeof(msg.msg_id));
  pos += sizeof(msg.msg_id);

  // topic
  msg.topic = ReadString(bytes, pos);
  // pipeline
  msg.pipeline = ReadString(bytes, pos);
  // pipeline_version
  msg.pipeline_version = ReadString(bytes, pos);
  // timestamp_sec (double)
  std::memcpy(&msg.timestamp_sec, bytes.data() + pos, sizeof(msg.timestamp_sec));
  pos += sizeof(msg.timestamp_sec);
  // wall_clock_sec (double)
  std::memcpy(&msg.wall_clock_sec, bytes.data() + pos, sizeof(msg.wall_clock_sec));
  pos += sizeof(msg.wall_clock_sec);
  // qos (uint8_t)
  std::memcpy(&msg.qos, bytes.data() + pos, sizeof(msg.qos));
  pos += sizeof(msg.qos);
  // schema_version (uint32_t)
  std::memcpy(&msg.schema_version, bytes.data() + pos, sizeof(msg.schema_version));
  pos += sizeof(msg.schema_version);
  // data (string)
  msg.data = ReadString(bytes, pos);

  return msg;
}

// ---------------------------------------------------------------------------
// StorageManager
// ---------------------------------------------------------------------------

StorageManager::StorageManager(std::unique_ptr<MemoryBackend> mem,
                               std::unique_ptr<DiskBackend> disk)
    : mem_backend_{std::move(mem)}, disk_backend_{std::move(disk)} {}

StorageRef StorageManager::Write(const std::string& topic, const Message& msg) {
  std::lock_guard<std::mutex> lock(mutex_);

  // Look up topic config.
  TopicRetention config;
  auto it = topic_config_.find(topic);
  if (it != topic_config_.end()) {
    config = it->second;
  }
  // Default: MEMORY, 120s.

  // Serialize message.
  std::string serialized = Serialize(msg);
  uint32_t payload_size = static_cast<uint32_t>(serialized.size());

  StorageRef ref;
  if (config.backend == Backend::DISK) {
    ref.backend = Backend::DISK;
    ref.offset = disk_backend_->Write(
        reinterpret_cast<const uint8_t*>(serialized.data()), payload_size);
    ref.size = payload_size;
  } else {
    ref.backend = Backend::MEMORY;
    ref.offset = mem_backend_->Write(
        reinterpret_cast<const uint8_t*>(serialized.data()), payload_size);
    ref.size = payload_size;
  }

  return ref;
}

Message StorageManager::Read(const StorageRef& ref) const {
  std::lock_guard<std::mutex> lock(mutex_);

  std::vector<uint8_t> buffer(ref.size);
  uint32_t bytes_read = 0;

  if (ref.backend == Backend::DISK) {
    bytes_read = disk_backend_->Read(ref.offset, ref.size, buffer.data());
  } else {
    bytes_read = mem_backend_->Read(ref.offset, ref.size, buffer.data());
  }

  std::string bytes(buffer.begin(), buffer.begin() + bytes_read);
  return Deserialize(bytes);
}

void StorageManager::SetTopicBackend(const std::string& topic,
                                     const TopicRetention& config) {
  std::lock_guard<std::mutex> lock(mutex_);
  topic_config_[topic] = config;
}

TopicRetention StorageManager::GetTopicRetention(const std::string& topic) const {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = topic_config_.find(topic);
  if (it != topic_config_.end()) {
    return it->second;
  }
  return TopicRetention{};  // default: MEMORY, 120s
}

}  // namespace protocol
}  // namespace orator
