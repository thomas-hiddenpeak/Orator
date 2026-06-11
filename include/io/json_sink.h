#pragma once

// JSON serialization of a timeline for the downstream LLM consumer.

#include <ostream>
#include <string>

#include "core/stages.h"

namespace orator {
namespace io {

// Produces a JSON document of the form:
// { "segments": [ {"start":0.0,"end":2.5,"speaker_id":"alice","text":"..."} ] }
std::string TimelineToJson(const core::Timeline& timeline, bool pretty = true);

// Sink that writes timelines as JSON to a std::ostream.
class JsonSink final : public core::ISink {
 public:
  explicit JsonSink(std::ostream& out, bool pretty = true)
      : out_(out), pretty_(pretty) {}
  void Consume(const core::Timeline& timeline) override;

 private:
  std::ostream& out_;
  bool pretty_;
};

}  // namespace io
}  // namespace orator
