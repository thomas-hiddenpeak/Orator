#include "protocol/protocol_timeline.h"

#include <algorithm>
#include <sstream>

#include "protocol/disk_backend.h"
#include "protocol/memory_backend.h"
#include "protocol/schema.h"
#include "protocol/time_index.h"

namespace orator {
namespace protocol {

ProtocolTimeline::ProtocolTimeline(size_t mem_capacity,
                                   const std::string& disk_path,
                                   const std::string& session_id)
    : registry_{std::make_unique<PipelineRegistry>()},
      router_{std::make_unique<TopicRouter>()},
      time_index_{std::make_unique<TimeIndex>()},
      schema_{std::make_unique<SchemaRegistry>()} {
  auto mem = std::make_unique<MemoryBackend>(mem_capacity);
  auto disk = std::make_unique<DiskBackend>(disk_path, session_id);
  storage_ = std::make_unique<StorageManager>(std::move(mem), std::move(disk));
}

ProtocolTimeline::~ProtocolTimeline() = default;

std::unique_ptr<PipelineHandle> ProtocolTimeline::RegisterPipeline(
    PipelineDescriptor desc) {
  std::vector<std::pair<std::string, std::string>> system_events;
  std::unique_ptr<PipelineHandle> handle;
  {
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto& kv : desc.schema) {
      schema_->Register(std::move(kv.second));
    }

    handle = registry_->Register(desc);

    if (desc.enabled) {
      for (const auto& pattern : desc.consumes) {
        router_->Subscribe(pattern, desc.name, QoS::AT_LEAST_ONCE, false);
      }
    }

    system_events.push_back({"system/pipeline/online", desc.name});
  }
  FireSystemEvents(system_events);
  return handle;
}

void ProtocolTimeline::UnregisterPipeline(PipelineHandle& handle) {
  std::vector<std::pair<std::string, std::string>> system_events;
  {
    std::lock_guard<std::mutex> lock(mutex_);

    router_->RemovePipeline(handle.name());
    registry_->Unregister(handle);
    system_events.push_back({"system/pipeline/offline", handle.name()});
  }
  FireSystemEvents(system_events);
}

bool ProtocolTimeline::Publish(PipelineHandle& handle, const Topic& topic,
                               Message msg, QoS qos) {
  std::vector<std::pair<long, Message>> pending_callbacks;
  std::vector<std::pair<std::string, std::string>> system_events;

  {
    std::lock_guard<std::mutex> lock(mutex_);

    if (msg.timestamp_sec < 0.0) return false;

    const std::string topic_str = topic.to_string();

    if (msg.msg_id == 0) {
      msg.msg_id = next_msg_id_++;
    }

    msg.topic = topic_str;
    msg.pipeline = handle.name();
    msg.pipeline_version = handle.descriptor().version;
    msg.qos = static_cast<uint8_t>(qos);

    StorageRef ref = storage_->Write(topic_str, msg);

    bool ooo =
        time_index_->Append(topic_str, msg.timestamp_sec, ref, msg.msg_id);

    if (ooo) {
      std::ostringstream oss;
      oss << "topic=" << topic_str << " msg_id=" << msg.msg_id
          << " timestamp=" << msg.timestamp_sec;
      system_events.push_back({"system/out_of_order", oss.str()});
    }

    auto deliveries = router_->Route(topic, handle.name(), qos);

    for (const auto& d : deliveries) {
      for (auto& isub : internal_subs_) {
        if (isub.router_sub_id == d.sub_id && isub.handler) {
          pending_callbacks.push_back({isub.router_sub_id, msg});
        }
      }
    }

    last_message_cache_ = msg;
  }

  // Dispatch all callbacks outside the lock.
  for (const auto& [sub_id, callback_msg] : pending_callbacks) {
    for (auto& isub : internal_subs_) {
      if (isub.router_sub_id == sub_id && isub.handler) {
        isub.handler(callback_msg);
      }
    }
  }
  FireSystemEvents(system_events);

  return true;
}

std::string ProtocolTimeline::Describe() const {
  std::lock_guard<std::mutex> lock(mutex_);

  std::ostringstream out;
  out << "{\n";
  out << "  \"type\": \"describe\",\n";

  auto pipelines = registry_->Describe();
  out << "  \"pipelines\": [\n";
  for (size_t i = 0; i < pipelines.size(); ++i) {
    out << "    {\"name\": \"" << pipelines[i].name << "\", \"version\": \""
        << pipelines[i].version
        << "\", \"enabled\": " << (pipelines[i].enabled ? "true" : "false")
        << "}";
    if (i + 1 < pipelines.size()) out << ",";
    out << "\n";
  }
  out << "  ],\n";
  out << "  \"schemas\": [\n";

  auto schemas = schema_->GetAll();
  for (size_t i = 0; i < schemas.size(); ++i) {
    out << "    {\"topic\": \"" << schemas[i].topic.to_string()
        << "\", \"version\": " << schemas[i].version << "}";
    if (i + 1 < schemas.size()) out << ",";
    out << "\n";
  }
  out << "  ]\n";

  out << "}\n";
  return out.str();
}

std::vector<Message> ProtocolTimeline::Replay(const std::string& topic,
                                              double from_sec) const {
  std::lock_guard<std::mutex> lock(mutex_);

  auto indexed = time_index_->Replay(topic, from_sec);
  std::vector<Message> result;
  result.reserve(indexed.size());
  for (const auto& im : indexed) {
    result.push_back(storage_->Read(im.ref));
  }
  return result;
}

Message const* ProtocolTimeline::LastRetained(const std::string& topic) const {
  std::lock_guard<std::mutex> lock(mutex_);

  auto* last = time_index_->Last(topic);
  if (!last) return nullptr;

  last_message_cache_ = storage_->Read(last->ref);
  return &last_message_cache_;
}

void ProtocolTimeline::SetTopicRetention(const std::string& topic,
                                         const TopicRetention& config) {
  std::lock_guard<std::mutex> lock(mutex_);
  storage_->SetTopicBackend(topic, config);
}

void ProtocolTimeline::CleanupOldData(double keep_until_sec) {
  std::lock_guard<std::mutex> lock(mutex_);

  // Cleanup time index
  time_index_->CleanupOldEntries(keep_until_sec);

  // Cleanup memory backend by evicting old entries
  // The MemoryBackend automatically evicts oldest entries when capacity is
  // exceeded We can trigger eviction by writing a small marker message
  uint8_t marker[1] = {0};
  storage_->GetMemoryBackend()->Write(marker, 1);
}

long ProtocolTimeline::SubscribeInternal(const TopicPattern& pattern,
                                         MessageHandler handler) {
  std::lock_guard<std::mutex> lock(mutex_);

  long sub_id = next_internal_sub_id_++;
  long router_id =
      router_->Subscribe(pattern, "__internal__", QoS::AT_MOST_ONCE, false);

  InternalSub isub;
  isub.internal_id = sub_id;
  isub.router_sub_id = router_id;
  isub.pattern_str = pattern.to_string();
  isub.handler = std::move(handler);
  internal_subs_.push_back(std::move(isub));

  return sub_id;
}

void ProtocolTimeline::UnsubscribeInternal(long sub_id) {
  std::lock_guard<std::mutex> lock(mutex_);

  for (auto it = internal_subs_.begin(); it != internal_subs_.end(); ++it) {
    if (it->internal_id == sub_id) {
      router_->Unsubscribe(it->router_sub_id);
      internal_subs_.erase(it);
      break;
    }
  }
}

void ProtocolTimeline::FireSystemEvent(const std::string& topic,
                                       const std::string& data) {
  std::vector<std::pair<std::string, std::string>> events;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    events.push_back({topic, data});
  }
  FireSystemEvents(events);
}

void ProtocolTimeline::FireSystemEvents(
    const std::vector<std::pair<std::string, std::string>>& events) {
  for (const auto& [topic, data] : events) {
    Message sys_msg;
    sys_msg.topic = topic;
    sys_msg.data = data;
    sys_msg.pipeline = "__system__";
    for (auto& isub : internal_subs_) {
      if (isub.handler) {
        TopicPattern pattern{isub.pattern_str};
        if (pattern.Matches(Topic{topic})) {
          isub.handler(sys_msg);
        }
      }
    }
  }
}

}  // namespace protocol
}  // namespace orator
