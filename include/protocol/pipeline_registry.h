#pragma once

// PipelineRegistry (Spec 004 Phase 8): registers named pipelines, tracks
// their lifecycle (online/offline), records heartbeats, and fires system
// events through a user-supplied callback.
//
// PipelineHandle is an RAII token returned by Register(). When the handle
// is destroyed (or explicitly Unregister()'d) the pipeline is removed and
// a system/pipeline/offline event fires.
//
// Thread-safe: every public method acquires a mutex.

#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "protocol/schema.h"
#include "protocol/topic.h"

namespace orator {
namespace protocol {

// Forward declaration
class PipelineRegistry;

struct PipelineDescriptor {
  std::string name;
  std::string version;
  std::vector<Topic> produces;
  std::vector<TopicPattern> consumes;
  std::map<std::string, TopicSchema> schema;
  bool enabled = true;
};

// ---------------------------------------------------------------------------
// PipelineHandle — RAII token representing a registered pipeline.
// Move-only. Destructor auto-unregisters if still valid.
// ---------------------------------------------------------------------------
class PipelineHandle {
 public:
  ~PipelineHandle();

  PipelineHandle(const PipelineHandle&) = delete;
  PipelineHandle& operator=(const PipelineHandle&) = delete;
  PipelineHandle(PipelineHandle&& other) noexcept;
  PipelineHandle& operator=(PipelineHandle&& other) noexcept;

  const PipelineDescriptor& descriptor() const;
  std::string name() const;
  bool valid() const;

  // Update last-seen timestamp.
  void Heartbeat();

 private:
  // Only PipelineRegistry can construct handles.
  PipelineHandle(PipelineRegistry* registry, long id, PipelineDescriptor desc);
  friend class PipelineRegistry;

  PipelineRegistry* registry_ = nullptr;
  long id_ = -1;
  PipelineDescriptor desc_;
  bool valid_ = false;
};

// ---------------------------------------------------------------------------
// PipelineRegistry — central registry for named pipelines.
// ---------------------------------------------------------------------------
class PipelineRegistry {
 public:
  using EventHandler =
      std::function<void(const std::string& topic, const std::string& data)>;

  // Register a pipeline. Returns a handle for all subsequent operations.
  // Throws std::invalid_argument if name is empty.
  // Throws std::runtime_error if name is already registered.
  std::unique_ptr<PipelineHandle> Register(PipelineDescriptor desc);

  // Remove a pipeline by handle. Clears subscriptions, publishes offline event.
  void Unregister(PipelineHandle& handle);

  // Return all registered pipeline descriptors (copies).
  std::vector<PipelineDescriptor> Describe() const;

  // Update heartbeat timestamp for a pipeline.
  void Heartbeat(PipelineHandle& handle);

  // Return list of pipeline names whose last heartbeat exceeds timeout_sec.
  std::vector<std::string> HealthCheck(double timeout_sec) const;

  // Set callback for system events (pipeline online/offline/unhealthy).
  void OnSystemEvent(EventHandler handler);

 private:
  // Internal entry stored in the map.
  struct PipelineEntry {
    PipelineDescriptor desc;
    long handle_id;
    std::chrono::steady_clock::time_point last_heartbeat;
  };

  // Fire a system event through the registered handler (if any).
  void fire_system_event_(const std::string& topic, const std::string& data);

  std::map<std::string, PipelineEntry> pipelines_;
  long next_handle_id_ = 1;
  EventHandler system_event_handler_;
  mutable std::mutex mutex_;
};

}  // namespace protocol
}  // namespace orator
