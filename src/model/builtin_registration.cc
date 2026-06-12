#include "model/builtin_registration.h"

#include <memory>

#include "core/registry.h"
#include "core/stages.h"
#include "model/qwen3_asr.h"
#include "model/stub_asr.h"
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

  auto& asr = core::Registry<core::IAsr>::Instance();
  asr.Register("stub", [] { return std::make_unique<StubAsr>(); });
  // Native Qwen3-ASR (models/asr/Qwen/Qwen3-ASR-1.7B). Same core::IAsr contract,
  // so consumers (pipeline / WS) need no changes; select via config.asr.
  asr.Register("qwen3_asr", [] { return std::make_unique<Qwen3Asr>(); });
}

}  // namespace model
}  // namespace orator

