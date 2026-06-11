#include "model/builtin_registration.h"

#include <memory>

#include "core/registry.h"
#include "core/stages.h"
#include "model/stub_diarizer.h"
#include "model/stub_embedder.h"
#include "model/streaming_sortformer.h"

namespace orator {
namespace model {

void EnsureBuiltinsRegistered() {
  static bool registered = false;
  if (registered) return;
  registered = true;

  auto& diar = core::Registry<core::IDiarizer>::Instance();
  diar.Register("stub", [] { return std::make_unique<StubDiarizer>(); });
  diar.Register("sortformer",
                [] { return std::make_unique<SortformerDiarizer>(); });

  auto& emb = core::Registry<core::ISpeakerEmbedder>::Instance();
  emb.Register("stub_embedder",
               [] { return std::make_unique<StubEmbedder>(); });
}

}  // namespace model
}  // namespace orator
