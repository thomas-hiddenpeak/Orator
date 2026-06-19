#pragma once

#include <string>
#include "protocol/storage.h"

namespace orator {
namespace protocol {

// Wrap a Message in topic-based envelope format (FR9.1)
std::string WrapInTopicEnvelope(const Message& msg);

}  // namespace protocol
}  // namespace orator
