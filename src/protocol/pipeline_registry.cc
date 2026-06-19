#include "protocol/pipeline_registry.h"

#include <chrono>
#include <stdexcept>
#include <utility>

namespace orator {
namespace protocol {

// ---------------------------------------------------------------------------
// PipelineHandle
// ---------------------------------------------------------------------------

PipelineHandle::PipelineHandle(PipelineRegistry* registry, long id,
                               PipelineDescriptor desc)
    : registry_(registry), id_(id), desc_(std::move(desc)), valid_(true) {}

PipelineHandle::~PipelineHandle() {
  if (valid_ && registry_) {
    registry_->Unregister(*this);
  }
}

PipelineHandle::PipelineHandle(PipelineHandle&& other) noexcept
    : registry_(other.registry_), id_(other.id_),
      desc_(std::move(other.desc_)), valid_(other.valid_) {
  other.registry_ = nullptr;
  other.id_ = -1;
  other.valid_ = false;
}

PipelineHandle& PipelineHandle::operator=(PipelineHandle&& other) noexcept {
  if (this != &other) {
    // Unregister current if still valid
    if (valid_ && registry_) {
      registry_->Unregister(*this);
    }
    registry_ = other.registry_;
    id_ = other.id_;
    desc_ = std::move(other.desc_);
    valid_ = other.valid_;

    other.registry_ = nullptr;
    other.id_ = -1;
    other.valid_ = false;
  }
  return *this;
}

const PipelineDescriptor& PipelineHandle::descriptor() const { return desc_; }

std::string PipelineHandle::name() const { return desc_.name; }

bool PipelineHandle::valid() const { return valid_; }

void PipelineHandle::Heartbeat() {
  if (valid_ && registry_) {
    registry_->Heartbeat(*this);
  }
}

// ---------------------------------------------------------------------------
// PipelineRegistry
// ---------------------------------------------------------------------------

std::unique_ptr<PipelineHandle> PipelineRegistry::Register(PipelineDescriptor desc) {
  std::unique_lock<std::mutex> lock(mutex_);

  if (desc.name.empty()) {
    throw std::invalid_argument("PipelineRegistry: pipeline name must not be empty");
  }

  if (pipelines_.find(desc.name) != pipelines_.end()) {
    throw std::runtime_error("PipelineRegistry: pipeline '" + desc.name +
                             "' is already registered");
  }

  long id = next_handle_id_++;
  std::string name = desc.name;
  pipelines_[name] = PipelineEntry{
      std::move(desc),
      id,
      std::chrono::steady_clock::now(),
  };

  PipelineEntry& entry = pipelines_[name];
  std::string payload = "{\"name\":\"" + entry.desc.name +
                        "\",\"version\":\"" + entry.desc.version + "\"}";

  EventHandler handler = system_event_handler_;
  lock.unlock();

  if (handler) {
    handler(kSystemPipelineOnline.to_string(), payload);
  }

  return std::unique_ptr<PipelineHandle>(new PipelineHandle(this, id, entry.desc));
}

void PipelineRegistry::Unregister(PipelineHandle& handle) {
  if (!handle.valid_) {
    return;
  }

  std::unique_lock<std::mutex> lock(mutex_);

  auto it = pipelines_.find(handle.name());
  if (it == pipelines_.end() || it->second.handle_id != handle.id_) {
    handle.valid_ = false;
    return;
  }

  std::string name = it->first;
  pipelines_.erase(it);
  handle.valid_ = false;

  std::string payload = "{\"name\":\"" + name + "\"}";
  EventHandler handler = system_event_handler_;
  lock.unlock();

  if (handler) {
    handler(kSystemPipelineOffline.to_string(), payload);
  }
}

std::vector<PipelineDescriptor> PipelineRegistry::Describe() const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<PipelineDescriptor> descs;
  descs.reserve(pipelines_.size());
  for (const auto& kv : pipelines_) {
    descs.push_back(kv.second.desc);
  }
  return descs;
}

void PipelineRegistry::Heartbeat(PipelineHandle& handle) {
  if (!handle.valid_) {
    return;
  }

  std::lock_guard<std::mutex> lock(mutex_);

  auto it = pipelines_.find(handle.name());
  if (it != pipelines_.end() && it->second.handle_id == handle.id_) {
    it->second.last_heartbeat = std::chrono::steady_clock::now();
  }
}

std::vector<std::string> PipelineRegistry::HealthCheck(double timeout_sec) const {
  std::lock_guard<std::mutex> lock(mutex_);

  auto now = std::chrono::steady_clock::now();
  auto timeout = std::chrono::duration<double>(timeout_sec);

  std::vector<std::string> unhealthy;
  for (const auto& kv : pipelines_) {
    auto elapsed = now - kv.second.last_heartbeat;
    if (elapsed > timeout) {
      unhealthy.push_back(kv.first);
    }
  }
  return unhealthy;
}

void PipelineRegistry::OnSystemEvent(EventHandler handler) {
  std::lock_guard<std::mutex> lock(mutex_);
  system_event_handler_ = std::move(handler);
}

void PipelineRegistry::fire_system_event_(const std::string& topic,
                                          const std::string& data) {
  if (system_event_handler_) {
    system_event_handler_(topic, data);
  }
}

}  // namespace protocol
}  // namespace orator
