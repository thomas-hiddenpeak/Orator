// asr_encoder_chunk_probe (Spec 003 T011): verify the audio encoder's
// chunk-local equivalence that the incremental KV-cache streaming design relies
// on.
//
// Claim under test: in the trained windowed-attention mode the encoder's
// attention is block-diagonal over windows of `window_aftercnn` tokens
// (= Wc * n_window_infer/win = 13*8 = 104 tokens = 8 s of audio), and the conv
// front-end is per-100-mel-frame with zero padding at chunk edges. Therefore an
// encode of a single window's mel frames, taken standalone, must equal that
// window's slice of a full encode -- when both are sliced from the SAME mel
// (mel is computed once over the full accumulated PCM; interior frames are
// stable as the stream grows). This is what lets a streaming session encode
// only the newly arrived window and APPEND its audio tokens, reusing the KV of
// the earlier, frozen audio tokens.
//
// The probe: compute mel over the full audio, run a full windowed encode (the
// reference), then re-encode each requested mel slice standalone and compare
// the standalone tokens to the corresponding reference slice. Eight seconds is
// the known T011 control; one second is the current production append unit.
// Reports the max absolute difference per slice and overall. These are
// numerical implementation contracts only and do not evaluate transcript
// correctness.
//
// Usage: asr_encoder_chunk_probe <audio> <asr_model_dir> [dur_sec] [slice_sec]

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

#include "feature/whisper_mel.h"
#include "io/audio_file.h"
#include "io/sharded_safetensor.h"
#include "model/asr_audio_tower.h"

using namespace orator;

int main(int argc, char** argv) {
  if (argc < 3) {
    std::fprintf(stderr,
                 "usage: %s <audio> <asr_model_dir> [dur_sec] [slice_sec]\n",
                 argv[0]);
    return 2;
  }
  const std::string audio_path = argv[1];
  const std::string model_dir = argv[2];
  const double dur_sec = argc > 3 ? std::atof(argv[3]) : 24.0;
  const double slice_sec = argc > 4 ? std::atof(argv[4]) : 8.0;
  const int sr = 16000;

  if (!std::ifstream(model_dir + "/model.safetensors.index.json").good()) {
    std::printf("[skip] need weights at %s\n", model_dir.c_str());
    return 0;
  }

  std::printf(">> decoding %s\n", audio_path.c_str());
  io::AudioData a = io::LoadAudioMono(audio_path, sr);
  long total = std::min(static_cast<long>(dur_sec * sr),
                        static_cast<long>(a.samples.size()));
  std::printf(">> %ld samples (%.1fs), standalone slice=%.1fs\n", total,
              total / double(sr), slice_sec);

  io::ShardedSafeTensors w(model_dir);
  feature::WhisperMel mel_engine;
  model::AsrAudioConfig tower_config;
  tower_config.windowed_attention = true;
  model::AsrAudioTower tower{tower_config};
  tower.LoadWeights(w);

  // ---- mel over the full audio (interior frames are stream-stable) ----
  int n_frames = 0;
  std::vector<float> mel =
      mel_engine.Compute(a.samples.data(), static_cast<int>(total), &n_frames);
  if (n_frames <= 0) {
    std::printf("FAIL: no mel frames\n");
    return 1;
  }
  const int out_dim = tower.config().output_dim;  // 2048
  std::printf(">> mel frames=%d\n", n_frames);

  // ---- full windowed encode = reference ----
  int ref_tokens = 0;
  std::vector<float> ref = tower.Forward(mel.data(), n_frames, &ref_tokens);
  std::printf(">> full encode tokens=%d dim=%d\n", ref_tokens, out_dim);

  // The encoder chunks mel into 100-frame convolution chunks; one trained
  // attention window is n_window_infer/win = 8 chunks = 800 mel frames. A
  // diagnostic slice must preserve convolution boundaries and either divide or
  // contain a whole number of trained attention windows.
  constexpr int kConvChunkMel = 100;
  constexpr int kAttentionWindowMel = 800;
  const int slice_mel =
      static_cast<int>(std::llround(slice_sec * kConvChunkMel));
  const bool aligned = slice_mel > 0 && slice_mel % kConvChunkMel == 0;
  const bool attention_aligned =
      aligned && (kAttentionWindowMel % slice_mel == 0 ||
                  slice_mel % kAttentionWindowMel == 0);
  if (!attention_aligned) {
    std::printf(
        "FAIL: slice_sec=%.3f must align to 1s convolution chunks and an "
        "8s attention window\n",
        slice_sec);
    return 1;
  }

  const int slice_tokens = model::AsrAudioTower::OutputLength(slice_mel);
  std::printf(
      ">> attention window=%d mel frames; standalone slice=%d mel frames -> "
      "%d tokens\n",
      kAttentionWindowMel, slice_mel, slice_tokens);

  // ---- encode each slice standalone, compare to the full-reference slice ----
  double overall_max = 0.0;
  int ref_off = 0;  // token offset into ref
  bool ok = true;
  for (int f0 = 0; f0 + slice_mel <= n_frames; f0 += slice_mel) {
    const int chunk_frames = slice_mel;
    // Slice mel columns [f0, f0+chunk_frames) into a contiguous [128, chunk].
    const int F = 128;
    std::vector<float> sub(static_cast<size_t>(F) * chunk_frames);
    for (int fb = 0; fb < F; ++fb)
      for (int t = 0; t < chunk_frames; ++t)
        sub[static_cast<size_t>(fb) * chunk_frames + t] =
            mel[static_cast<size_t>(fb) * n_frames + (f0 + t)];

    int sub_tokens = 0;
    std::vector<float> enc =
        tower.Forward(sub.data(), chunk_frames, &sub_tokens);

    // Compare the standalone tokens to the reference slice [ref_off,
    // +sub_tokens).
    double mx = 0.0;
    const int cmp = std::min(sub_tokens, ref_tokens - ref_off);
    for (int i = 0; i < cmp; ++i)
      for (int d = 0; d < out_dim; ++d) {
        const double e = std::abs(
            static_cast<double>(enc[static_cast<size_t>(i) * out_dim + d]) -
            ref[static_cast<size_t>(ref_off + i) * out_dim + d]);
        if (e > mx) mx = e;
      }
    if (sub_tokens != slice_tokens || cmp != sub_tokens) {
      std::printf(
          "FAIL: slice @frame %d has unexpected token extent "
          "(tokens=%d expected=%d compared=%d)\n",
          f0, sub_tokens, slice_tokens, cmp);
      return 1;
    }
    std::printf("   slice @frame %5d: tokens=%d cmp=%d  max abs diff=%.3e\n",
                f0, sub_tokens, cmp, mx);
    overall_max = std::max(overall_max, mx);
    ref_off += sub_tokens;
  }

  std::printf("\n>> overall max abs diff over standalone slices = %.3e\n",
              overall_max);
  // bf16 GEMM noise floor is ~1e-1 in absolute logit/feature units; the encoder
  // feature magnitudes are O(1..10). Use a tolerance comfortably above the
  // observed conv/gemm noise but far below a real logic divergence.
  const double tol = 5e-2;
  if (overall_max > tol) {
    std::printf(
        "FAIL: standalone-slice encode diverges from full encode "
        "(%.3e > %.3e) -- chunk-locality does NOT hold; streaming must "
        "re-encode context\n",
        overall_max, tol);
    return 1;
  }
  std::printf(
      "PASS: slice-local windowed encode confirmed (<= %.3e). A "
      "streaming session may encode each matching slice standalone and "
      "append its audio tokens.\n",
      tol);
  return ok ? 0 : 1;
}
