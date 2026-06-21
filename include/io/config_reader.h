#pragma once

// ConfigReader: loads runtime tuning parameters from a TOML config file.
//
// Follows the loading-order convention:
//   compile-time defaults → orator.toml → environment variables → CLI args
//
// The TOML file is OPTIONAL. If it does not exist, startup continues silently
// with all defaults. If it has parse errors, a warning is logged and defaults
// are kept.
//
// Zero runtime dependencies: tomlplusplus is header-only, fetched at build
// time, and statically embedded.

#include <string>

#include "pipeline/auditory_stream.h"

namespace orator {
namespace io {

// Apply settings from a TOML config file to `cfg`.
// Returns true on success. Returns false if the file doesn't exist or fails
// to parse (cfg is left unchanged on failure).
bool ApplyTomlConfig(const std::string& path,
                     pipeline::AuditoryStream::Config& cfg);

}  // namespace io
}  // namespace orator
