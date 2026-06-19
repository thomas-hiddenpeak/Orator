#pragma once

// Storage types & StorageManager (Spec 004 Phase 10): protocol message storage.
//
// Messages are serialized to length-prefixed byte blobs and routed to a MEMORY
// or DISK backend based on per-topic retention configuration. StorageRef is the
// opaque handle returned by Write() and consumed by Read().

#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>

namespace orator {
namespace protocol {

// ---------------------------------------------------------------------------
// Backend enum
// ---------------------------------------------------------------------------

enum class Backend : uint8_t { MEMORY, DISK };

// ---------------------------------------------------------------------------
// StorageRef — opaque pointer to stored data
// ---------------------------------------------------------------------------

struct StorageRef {
  Backend backend;
  uint64_t offset;  // offset within the backend
  uint32_t size;    // payload size in bytes
};

// ---------------------------------------------------------------------------
// Message — protocol message unit
// ---------------------------------------------------------------------------

struct Message {
  uint64_t msg_id = 0;
  std::string topic;
  std::string pipeline;
  std::string pipeline_version;
  double timestamp_sec = 0.0;
  double wall_clock_sec = 0.0;  // optional, 0 = not set
  uint8_t qos = 0;
  uint32_t schema_version = 1;
  std::string data;  // JSON payload
};

// ---------------------------------------------------------------------------
// IndexedMessage — for time index
// ---------------------------------------------------------------------------

struct IndexedMessage {
  double timestamp_sec;
  StorageRef ref;
  uint64_t msg_id;
};

// ---------------------------------------------------------------------------
// TopicRetention — per-topic storage configuration
// ---------------------------------------------------------------------------

struct TopicRetention {
  Backend backend = Backend::MEMORY;
  double retention_sec = 120.0;  // seconds to retain messages
};

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------

class MemoryBackend;
class DiskBackend;

// ---------------------------------------------------------------------------
// StorageManager — routes writes/reads to the correct backend
// ---------------------------------------------------------------------------

class StorageManager {
 public:
  explicit StorageManager(std::unique_ptr<MemoryBackend> mem,
                          std::unique_ptr<DiskBackend> disk);

  // Write a message to the correct backend based on topic config.
  // Returns a StorageRef pointing to the stored data.
  StorageRef Write(const std::string& topic, const Message& msg);

  // Read a message from its storage reference.
  Message Read(const StorageRef& ref) const;

  // Configure which backend a topic uses.
  void SetTopicBackend(const std::string& topic, const TopicRetention& config);

  // Get retention config for a topic (defaults to MEMORY, 120s).
  TopicRetention GetTopicRetention(const std::string& topic) const;

 private:
  // Serialize a Message to a length-prefixed byte vector.
  // Format: uint32_t size (LE) + data bytes.
  static std::string Serialize(const Message& msg);

  // Deserialize a Message from bytes stored at the given backend.
  static Message Deserialize(const std::string& bytes);

  std::unique_ptr<MemoryBackend> mem_backend_;
  std::unique_ptr<DiskBackend> disk_backend_;
  std::map<std::string, TopicRetention> topic_config_;
  mutable std::mutex mutex_;
};

}  // namespace protocol
}  // namespace orator
