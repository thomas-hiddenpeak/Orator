#pragma once

// ProtocolTimeline (Spec 004 Phase 11): integration layer that wires together
// PipelineRegistry, TopicRouter, StorageManager, TimeIndex, and SchemaRegistry
// into a single session-scoped object.
//
// ProtocolTimeline owns all protocol-layer components and provides the unified
// API: Register/Unregister pipelines, Publish messages (stored + routed),
// Replay/LastRetained queries, and internal subscription for pipeline data flow.

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "protocol/pipeline_registry.h"
#include "protocol/storage.h"
#include "protocol/topic.h"
#include "protocol/topic_router.h"

namespace orator {
namespace protocol {

// Forward declarations
class PipelineRegistry;
class TopicRouter;
class StorageManager;
class TimeIndex;
class SchemaRegistry;
class MemoryBackend;
class DiskBackend;

// ---------------------------------------------------------------------------
// ProtocolTimeline — owns and wires all protocol layers.
// ---------------------------------------------------------------------------
class ProtocolTimeline {
 public:
  // Create a protocol timeline with default storage backends.
  // mem_capacity: memory backend capacity in bytes (default 128 MB).
  // disk_path: directory for disk backend files.
  // session_id: unique session identifier for disk file naming.
  ProtocolTimeline(size_t mem_capacity = 128 * 1024 * 1024,
                   const std::string& disk_path = "/tmp/orator/storage/",
                   const std::string& session_id = "default");
  ~ProtocolTimeline();

  ProtocolTimeline(const ProtocolTimeline&) = delete;
  ProtocolTimeline& operator=(const ProtocolTimeline&) = delete;

  // Register a pipeline. Returns a handle for Publish/Subscribe.
  std::unique_ptr<PipelineHandle> RegisterPipeline(PipelineDescriptor desc);

  // Remove a pipeline by handle.
  void UnregisterPipeline(PipelineHandle& handle);

  // Publish a message through the timeline: store, index, route.
  // Rejects messages with negative timestamps.
  // Returns true on success, false if the message was rejected.
  bool Publish(PipelineHandle& handle, const Topic& topic, Message msg,
               QoS qos);

  // Describe all registered pipelines and schemas (JSON-like string).
  std::string Describe() const;

  // Replay messages for a topic from a timestamp.
  std::vector<Message> Replay(const std::string& topic, double from_sec) const;

  // Get the last retained message for a topic.
  // Returns nullptr if no message exists.
  Message const* LastRetained(const std::string& topic) const;

  // Configure per-topic retention/backend.
  void SetTopicRetention(const std::string& topic, const TopicRetention& config);

  // Internal: subscribe to a topic pattern for routing callbacks.
  // Used by ComprehensiveTimeline to receive pipeline data.
  using MessageHandler = std::function<void(const Message&)>;
  long SubscribeInternal(const TopicPattern& pattern, MessageHandler handler);
  void UnsubscribeInternal(long sub_id);

 private:
  // Publish a system event (pipeline online/offline, out-of-order).
  // Collects the event under mutex_, then dispatches via FireSystemEvents.
  void FireSystemEvent(const std::string& topic, const std::string& data);
  // Dispatch system events outside the lock.
  void FireSystemEvents(const std::vector<std::pair<std::string, std::string>>& events);

  std::unique_ptr<PipelineRegistry> registry_;
  std::unique_ptr<TopicRouter> router_;
  std::unique_ptr<StorageManager> storage_;
  std::unique_ptr<TimeIndex> time_index_;
  std::unique_ptr<SchemaRegistry> schema_;

  // Internal subscriptions for routing callbacks.
  struct InternalSub {
    long internal_id = 0;
    long router_sub_id = 0;
    std::string pattern_str;
    MessageHandler handler;
  };
  std::vector<InternalSub> internal_subs_;
  long next_internal_sub_id_ = 1;

  // Next message ID for auto-increment.
  uint64_t next_msg_id_ = 1;

  mutable Message last_message_cache_;

  mutable std::mutex mutex_;
};

}  // namespace protocol
}  // namespace orator
