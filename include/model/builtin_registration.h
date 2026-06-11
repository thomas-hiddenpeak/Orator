#pragma once

// Registers all built-in model implementations with their registries.
//
// Call EnsureBuiltinsRegistered() once at startup before creating components by
// name. This is the single place that knows about concrete model classes; the
// pipeline only ever sees interfaces + string keys.

namespace orator {
namespace model {

void EnsureBuiltinsRegistered();

}  // namespace model
}  // namespace orator
