#pragma once

#include <string>
#include <vector>

namespace orator {
namespace pipeline {

// ASR-only independent front VAD.
//
// It owns a local PCM buffer and extracts endpointed speech spans from that
// buffer. The diarization pipeline does not depend on this class.
class AsrSileroVad {
 public:
  struct Params {
    int sample_rate = 16000;
    double max_utterance_sec = 28.0;
    double min_utterance_sec = 0.20;

    std::string silero_model_path = "models/vad/silero_vad.safetensors";
    float silero_threshold = 0.5f;
    int silero_min_speech_ms = 250;
    int silero_min_silence_ms = 120;
    int silero_speech_pad_ms = 60;
  };

  explicit AsrSileroVad(const Params& params);

  void Push(const float* samples, int n);

  // Returns one completed span [begin,end) in samples relative to current
  // buffered front, and how many samples should be consumed afterward.
  bool NextSpan(bool finalize, int* begin, int* end, int* consume);

  // Endpoint-only detection for the incremental streaming path (Spec 003 T050).
  // Advances the VAD over buffered audio and reports the ABSOLUTE sample index
  // of the next speech endpoint (a silence gap after speech). Returns false if
  // none is available yet. Consumes processed audio to bound memory. Unlike
  // NextSpan it does NOT trim or return spans -- the caller feeds the original
  // continuous audio to the engine and only uses these endpoints to choose
  // segment reset points. Do not interleave with NextSpan (shared cursor).
  bool NextEndpoint(bool finalize, long* endpoint_abs_sample);

  // Numeric-gate probe (Spec 004 Phase 5, FR8): from a fresh state, return the
  // per-window speech probability for every full window in [pcm, pcm+n). This is
  // the reference of record the GPU detector (GpuVad) is gated against.
  std::vector<float> DebugWindowProbs(const float* pcm, int n);

  const float* data() const { return pcm_.data(); }
  long base_sample() const { return base_sample_; }
  void Consume(int n);
  void Reset();

 private:
  struct StepResult {
    float probability = 0.0f;
    bool is_speech = false;
    bool segment_start = false;
    bool segment_end = false;
  };

  bool InitModel();
  StepResult ProcessWindow(const float* pcm, int n_samples);

  static void ReflectPadRight(const float* in, int len, int pad, float* out);
  static void Conv1d(const float* input, int C_in, int L_in,
                     const float* weight, const float* bias,
                     int C_out, int K, int stride, int pad,
                     float* output, int L_out);
  static void ReluInplace(float* data, int n);
  static void LstmCellStep(const float* x, int input_size,
                           const float* Wih, const float* Whh,
                           const float* bih, const float* bhh,
                           float* h, float* c, int hidden);

  static constexpr int kContextSize = 64;
  static constexpr int kWindowSize = 512;
  static constexpr int kTotalInput = 576;
  static constexpr int kNfft = 256;
  static constexpr int kHopLength = 128;
  static constexpr int kPadRight = 64;
  static constexpr int kPaddedLen = 640;
  static constexpr int kStftBins = 129;
  static constexpr int kStftFrames = 4;
  static constexpr int kEnc0Out = 128;
  static constexpr int kEnc1Out = 64;
  static constexpr int kEnc2Out = 64;
  static constexpr int kEnc3Out = 128;
  static constexpr int kEnc0Frames = 4;
  static constexpr int kEnc1Frames = 2;
  static constexpr int kEnc2Frames = 1;
  static constexpr int kEnc3Frames = 1;
  static constexpr int kLstmHidden = 128;

  Params params_;
  bool initialized_ = false;

  std::vector<float> stft_basis_;
  std::vector<float> enc_w_[4];
  std::vector<float> enc_b_[4];
  std::vector<float> lstm_wih_;
  std::vector<float> lstm_whh_;
  std::vector<float> lstm_bih_;
  std::vector<float> lstm_bhh_;
  std::vector<float> dec_w_;
  float dec_b_ = 0.0f;

  std::vector<float> h_state_;
  std::vector<float> c_state_;
  std::vector<float> context_;

  bool in_speech_ = false;
  int speech_samples_ = 0;
  int silence_samples_ = 0;

  std::vector<float> pcm_;
  long base_sample_ = 0;
  int cursor_ = 0;
  bool segment_open_ = false;
  int segment_start_ = 0;
  int last_voiced_end_ = 0;
};

}  // namespace pipeline
}  // namespace orator
