#include "model/qwen3_forced_aligner.h"

#include <chrono>
#include <cstdlib>
#include <stdexcept>

#include "core/log.h"
#include "io/sharded_safetensor.h"

namespace orator {
namespace model {
namespace {
AsrAudioConfig AlignerAudioConfig() {
  AsrAudioConfig c;     // defaults already match the aligner encoder
  c.output_dim = 1024;  // aligner projects to 1024 (ASR uses 2048)
  return c;
}
constexpr double kTimestampSegmentMs = 80.0;
}  // namespace

Qwen3ForcedAligner::Qwen3ForcedAligner() : tower_(AlignerAudioConfig()) {}

void Qwen3ForcedAligner::LoadWeights(const std::string& model_dir) {
  if (!tok_.Load(model_dir))
    throw std::runtime_error("aligner tokenizer load failed: " + model_dir);
  io::ShardedSafeTensors w(model_dir);
  AsrAudioTower::WeightNames names;
  names.prefix = "model.audio_tower.";
  names.proj1 = "model.multi_modal_projector.linear_1";
  names.proj2 = "model.multi_modal_projector.linear_2";
  tower_.LoadWeights(w, names);
  lm_.LoadWeights(w);
  loaded_ = true;
}

std::vector<core::AlignUnit> Qwen3ForcedAligner::Align(
    const float* pcm, int n, const std::string& transcript,
    const std::string& language) const {
  if (!loaded_) throw std::runtime_error("Qwen3ForcedAligner not loaded");
  if (pcm == nullptr || n <= 0 || transcript.empty()) return {};

  const bool prof = std::getenv("ORATOR_ALIGN_PROFILE") != nullptr;
  auto now = [] { return std::chrono::steady_clock::now(); };
  const auto t0 = now();

  // 1. log-mel -> 2. audio tower + projector -> [n_audio, 1024].
  int n_frames = 0;
  std::vector<float> mel = mel_.Compute(pcm, n, &n_frames);
  const auto t_mel = now();
  int n_audio = 0;
  std::vector<float> audio_feats =
      tower_.Forward(mel.data(), n_frames, &n_audio);
  const auto t_tower = now();

  // 3. word tokenisation -> 4. assemble input_ids.
  std::vector<std::string> words = SplitWordsForAlignment(transcript, language);
  if (words.empty()) return {};
  std::vector<std::vector<int>> word_ids;
  word_ids.reserve(words.size());
  for (const auto& wd : words) word_ids.push_back(tok_.Encode(wd));
  std::vector<int> input_ids = BuildAlignerInputIds(
      word_ids, n_audio, kAudioStart, kAudioPad, kAudioEnd, kTimestamp);
  const auto t_asm = now();

  // 5. single causal forward + score head -> logits [T, num_labels].
  std::vector<float> logits =
      lm_.Forward(input_ids, audio_feats.data(), n_audio, kAudioPad);
  const auto t_lm = now();

  // 6. argmax label at each <timestamp> position -> ms; repair; pair to words.
  const int NL = lm_.config().num_labels;
  std::vector<double> raw_ms;
  raw_ms.reserve(words.size() * 2);
  for (size_t t = 0; t < input_ids.size(); ++t) {
    if (input_ids[t] != kTimestamp) continue;
    const float* row = logits.data() + t * NL;
    int best = 0;
    float bestv = row[0];
    for (int j = 1; j < NL; ++j)
      if (row[j] > bestv) {
        bestv = row[j];
        best = j;
      }
    raw_ms.push_back(static_cast<double>(best) * kTimestampSegmentMs);
  }
  std::vector<long> fixed = FixTimestamps(raw_ms);
  std::vector<core::AlignUnit> units = PairWordTimestamps(words, fixed);

  if (prof) {
    auto ms = [](auto a, auto b) {
      return std::chrono::duration<double, std::milli>(b - a).count();
    };
    LOG_INFO(
        "[align-profile] mel=%.1fms tower=%.1fms asm=%.1fms lm=%.1fms "
        "decode=%.1fms (T=%zu n_audio=%d words=%zu)\n",
        ms(t0, t_mel), ms(t_mel, t_tower), ms(t_tower, t_asm), ms(t_asm, t_lm),
        ms(t_lm, now()), input_ids.size(), n_audio, words.size());
  }
  return units;
}

}  // namespace model
}  // namespace orator
