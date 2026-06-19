#include "protocol/ws_envelope.h"

#include <sstream>

namespace orator {
namespace protocol {

std::string WrapInTopicEnvelope(const Message& msg) {
  std::ostringstream out;
  out << "{";
  out << "\"topic\":\"" << msg.topic << "\",";
  out << "\"pipeline\":\"" << msg.pipeline << "\",";
  out << "\"pipeline_version\":\"" << msg.pipeline_version << "\",";
  out << "\"msg_id\":" << msg.msg_id << ",";
  out << "\"ts\":" << msg.timestamp_sec << ",";
  out << "\"qos\":" << static_cast<int>(msg.qos) << ",";
  out << "\"schema_version\":" << msg.schema_version << ",";
  out << "\"data\":" << msg.data;
  out << "}";
  return out.str();
}

}  // namespace protocol
}  // namespace orator
