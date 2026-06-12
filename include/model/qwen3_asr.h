#pragma once

// Qwen3-ASR engine: the full native ASR pipeline behind the core::IAsr contract.
//
//   PCM -> WhisperMel -> AsrAudioTower (encoder) -> build prompt (audio_pad x N)
//        -> embed + inject encoder output -> AsrTextDecoder prefill
//        -> greedy autoregressive decode -> BPE decode -> Transcript
//
// FP32 compute throughout, numerically verified stage-by-stage against the
// PyTorch oracle (see test_whisper_mel / test_asr_encoder / test_asr_decoder /
// test_bpe). Registered as "qwen3_asr"; LoadWeights() takes the model dir.

#include <memory>
#include <string>
#include <vector>

#include "core/stages.h"
#include "feature/whisper_mel.h"
#include "io/bpe_tokenizer.h"
#include "io/sharded_safetensor.h"
#include "model/asr_audio_tower.h"
#include "model/asr_text_decoder.h"

namespace orator {
namespace model {

class Qwen3Asr final : public core::IAsr {
 public:
  Qwen3Asr();

  void Initialize(const core::AsrConfig& config) override;
  void LoadWeights(const std::string& path) override;  // path = model dir
  void Reset() override;
  core::Transcript Transcribe(const core::AudioChunk& audio) override;

  std::string name() const override { return "qwen3_asr"; }

  void set_max_new_tokens(int n) { max_new_tokens_ = n; }
  void set_language(const std::string& l) { language_ = l; }

  // Transcribe raw mono-16k samples into text (no AudioChunk wrapper). This is
  // the single-segment path: the caller guarantees a bounded (<=~30s) span so
  // the decoder context stays short.
  std::string TranscribeText(const float* samples, int num_samples);

  // Transcribe `samples` with an optional committed text prefix. The prefix is
  // appended to the prompt after the <asr_text> tag so the model continues from
  // it; only the newly generated continuation is returned (the caller holds the
  // committed prefix). This is the growing-window streaming primitive used to
  // evaluate the official Qwen3-ASR streaming method (audio_accum + prefix
  // rollback). `samples` is all audio from the stream start.
  std::string TranscribeWindow(const float* samples, int num_samples,
                               const std::string& prefix_text);

  // Energy-VAD speech segmentation. Splits [0,num_samples) into bounded speech
  // spans (sample offsets) separated by silence, capping each at
  // max_segment_sec and bridging gaps below min_silence_sec. Independent of
  // diarization -- this is how the ASR pipeline segments itself.
  struct Span { int begin; int end; };  // sample offsets [begin, end)
  std::vector<Span> SegmentSpeech(const float* samples, int num_samples,
                                  int sample_rate) const;

  // Tokenizer access for the streaming caller's prefix rollback (encode the
  // committed text, drop the last K tokens, decode back to a prefix string).
  const io::BpeTokenizer& tokenizer() const { return tokenizer_; }

 private:
  std::string BuildAndRun(const std::vector<float>& encoder_out, int n_tokens,
                          const std::string& prefix_text);

  core::AsrConfig cfg_;
  std::string language_ = "Chinese";
  int max_new_tokens_ = 256;
  bool loaded_ = false;

  // VAD / segmentation knobs (energy-based, dependency-free).
  bool vad_enabled_ = true;
  double max_segment_sec_ = 28.0;   // cap context length per ASR call
  double min_silence_sec_ = 3.50;   // only long dead-air splits (few large segments)
  double min_speech_sec_ = 0.20;    // drop spans shorter than this
  double speech_pad_sec_ = 0.16;    // pad each span so words aren't clipped
  float vad_rel_threshold_ = 0.08f; // speech if frame RMS > thr * peak RMS

  std::unique_ptr<io::ShardedSafeTensors> weights_;
  std::unique_ptr<feature::WhisperMel> mel_;
  std::unique_ptr<AsrAudioTower> encoder_;
  std::unique_ptr<AsrTextDecoder> decoder_;
  io::BpeTokenizer tokenizer_;

  // Special token ids (Qwen3-ASR).
  static constexpr int kImStart = 151644;
  static constexpr int kImEnd = 151645;
  static constexpr int kEndOfText = 151643;
  static constexpr int kAudioStart = 151669;
  static constexpr int kAudioEnd = 151670;
  static constexpr int kAudioPad = 151676;
  static constexpr int kAsrText = 151704;
  static constexpr int kSystem = 8948;
  static constexpr int kUser = 872;
  static constexpr int kAssistant = 77091;
  static constexpr int kNewline = 198;
};

}  // namespace model
}  // namespace orator
