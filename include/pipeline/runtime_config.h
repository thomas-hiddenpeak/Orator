#pragma once

#include <string>

#include "pipeline/auditory_stream.h"

namespace orator {
namespace pipeline {

// Canonical JSON representation of every resolved runtime field after defaults,
// TOML, environment, and CLI precedence have been applied.
std::string SerializeResolvedConfig(const AuditoryStream::Config& config);

}  // namespace pipeline
}  // namespace orator
