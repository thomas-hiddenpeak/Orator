#pragma once

// Protocol data type system (Spec 004 Phase 7).
//
// Schema definitions for typed fields carried on protocol topics.
// SchemaRegistry stores topic schemas by version, enabling consumers
// to validate payloads against the expected schema for a given topic.

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "protocol/topic.h"

namespace orator {
namespace protocol {

enum class FieldType {
  STRING,
  DOUBLE,
  FLOAT,
  INT32,
  INT64,
  BOOL,
  BYTES,
  LIST,
  STRUCT,
};

struct Field {
  std::string name;
  FieldType type = FieldType::STRING;
  bool optional = false;
  std::string default_value;
};

struct Schema {
  std::vector<Field> fields;
};

struct TopicSchema {
  Topic topic;
  uint32_t version = 1;
  Schema schema;
};

class SchemaRegistry {
 public:
  void Register(TopicSchema ts) {
    std::string key = ts.topic.to_string();
    auto& versions = schemas_[key];

    // If topic already exists with a different version, keep both.
    bool found = false;
    for (const auto& existing : versions) {
      if (existing.version == ts.version) {
        found = true;
        break;
      }
    }
    if (!found) {
      versions.push_back(std::move(ts));
    }
  }

  TopicSchema const* Get(const Topic& topic, uint32_t version = 0) const {
    std::string key = topic.to_string();
    auto it = schemas_.find(key);
    if (it == schemas_.end()) return nullptr;

    const auto& versions = it->second;
    if (versions.empty()) return nullptr;

    if (version == 0) {
      // Return the latest (last registered) version.
      return &versions.back();
    }

    for (const auto& ts : versions) {
      if (ts.version == version) return &ts;
    }
    return nullptr;
  }

  std::vector<TopicSchema> GetAll() const {
    std::vector<TopicSchema> all;
    for (const auto& kv : schemas_) {
      for (const auto& ts : kv.second) {
        all.push_back(ts);
      }
    }
    return all;
  }

 private:
  std::map<std::string, std::vector<TopicSchema>> schemas_;
};

}  // namespace protocol
}  // namespace orator
