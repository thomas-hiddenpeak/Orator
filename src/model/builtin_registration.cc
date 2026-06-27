#include "model/builtin_registration.h"

#include <memory>

#include "core/registry.h"
#include "core/stages.h"
#include "model/qwen3_asr.h"
#include "model/qwen3_forced_aligner.h"
#include "model/streaming_sortformer.h"
#include "pipeline/gpu_vad.h"

namespace orator {
namespace model {

void EnsureBuiltinsRegistered() {
  static bool registered = false;
  if (registered) return;
  registered = true;

  auto& diar = core::Registry<core::IDiarizer>::Instance();
  diar.Register("sortformer",
                [] { return std::make_unique<SortformerDiarizer>(); });

  auto& asr = core::Registry<core::IAsr>::Instance();
  // Native Qwen3-ASR (models/asr/Qwen/Qwen3-ASR-1.7B). Same core::IAsr
  // contract, so consumers (pipeline / WS) need no changes; select via
  // config.asr.
  asr.Register("qwen3_asr", [] { return std::make_unique<Qwen3Asr>(); });

  auto& vad = core::Registry<core::IVad>::Instance();
  vad.Register("silero_vad", [] {
    return std::make_unique<pipeline::GpuVad>(pipeline::GpuVad::Params{});
  });

  auto& aligner = core::Registry<core::IForcedAligner>::Instance();
  aligner.Register("qwen3_forced_aligner",
                   [] { return std::make_unique<Qwen3ForcedAligner>(); });
}

}  // namespace model
}  // namespace orator
