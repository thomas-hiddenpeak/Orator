# Plan 009 — ASR Forced Alignment Pipeline

Status: Draft (2026-06-27). Implement only after review.
Spec: [spec.md](spec.md)

## 1. Architecture & reuse map

The aligner is a single NAR forward pass. Maximise reuse of the ASR stack; add
only the projector wiring, score head, input builder, and timestamp decoder.

| Stage | Reuse | New work |
|---|---|---|
| log-mel (128, hop160, nfft400) | `feature::WhisperMel` as-is | — |
| audio tower (24L bidir + CNN) | `model::AsrAudioTower` (config d_model=1024, output_dim=1024) | confirm weight-name map `model.audio_tower.*`; output is ln_post (1024), NOT the ASR 2048 projection |
| multimodal projector | — | `linear_1(1024→1024)+GELU+linear_2(1024→1024)` via `asr_gemm::Linear` |
| Qwen3 LM (28L causal) | decoder layer math (`asr_ops` RMSNorm/RopeHalf/GqaAttention(causal=true)/SwiGLU, `asr_gemm::Linear`) | a **single full-prefill forward returning ALL positions' final hidden states** (no KV-cache reuse, no generation) |
| score head | — | `score.weight` Linear(1024→5000), no bias |
| tokenizer | `io::BpeTokenizer` (load `tokenizer.json`/vocab) | `split_words_for_alignment` (CJK/space, drop punct) + special-id lookup |
| input build | embedding via decoder embed | assemble `[audio_start][audio_pad×N][audio_end][word<ts><ts>]…`, scatter audio embeds at pad positions |
| timestamp decode | — | argmax@`<timestamp>` → ×80ms → `_fix_timestamps` (LIS) → word start/end |
| weights | `io::ShardedSafeTensors` (single shard) | bf16→fp32 load helpers (existing `LoadBf16/LoadF32`) |
| pipeline wiring | per-pipeline `PipelineAudioCache`, `ProtocolTimeline`, `ComprehensiveTimeline` | new `IForcedAligner` interface + `AlignWorker` + AuditoryStream wiring |

Key correction vs. initial survey: the LM is **causal** (standard Qwen3); we do
NOT need a non-causal attention mode. Audio precedes text, so `<timestamp>`
tokens see all audio under causal masking.

## 2. New components

1. `include/core/stages.h`: `IForcedAligner` interface (or extend model registry).
   - `Align(const float* pcm, int n, const std::string& transcript,
     const std::string& language) -> std::vector<AlignUnit>` where
     `AlignUnit{std::string text; double start_sec; double end_sec;}`.
   - `Initialize`, `LoadWeights(dir)`, `Reset`, `name()`.
2. `include/model/qwen3_forced_aligner.{h}` + `src/model/qwen3_forced_aligner.{cc,cu}`:
   - Owns `WhisperMel`, an `AsrAudioTower` (1024), the projector + LM weights + score head.
   - `Align()` runs: mel → tower → projector → build ids → embed+scatter →
     causal LM full forward (all hidden) → score → decode.
3. `src/model/qwen3_aligner_decoder.cu` (or extend `AsrTextDecoder`):
   a single-pass causal stack returning `[seq, hidden]` hidden states. Decision in
   §5; preferred = a focused `AlignerLmForward` over `asr_ops`/`asr_gemm` (no KV
   cache, no generation, no banning) to keep the NAR path simple and auditable.
4. `src/model/forced_align_decode.{h,cc}`: pure-CPU `_fix_timestamps` (LIS +
   interpolation) and word pairing. Unit-testable in isolation.
5. `include/pipeline/align_worker.h` + `src/pipeline/align_worker.cc`:
   subscribes to `asr/transcript` (segment {id,start,end,text}); reads the audio
   span [start,end] from its `PipelineAudioCache`; calls `IForcedAligner::Align`;
   publishes `align/units` to the protocol; deposits into `ComprehensiveTimeline`.
6. `include/protocol/topic.h`: `kAlignUnits{"align/units"}` (+ schema entry).
7. AuditoryStream: create the aligner + its audio cache + the worker thread when
   `config_.align_model_dir` is set; fan PushAudio to the aligner cache; wire the
   asr/transcript subscription.
8. `orator.toml` + config: `[align] model_dir`, `enable`, `max_segment_sec=300`.

## 3. Validation strategy (Constitution Art. II)

Python oracle first (the project's established pattern, `tools/reference/`):
- `tools/reference/aligner_oracle.py` — load `Qwen3ASRForTokenClassification` via
  transformers-from-git in the torch venv; run on a fixed `(audio, transcript,
  language)`; dump to `.f32/.npy`: mel, audio_features (post-projector),
  input_ids, LM last_hidden_state, score logits, argmax labels, decoded words.
- Stage gates (mirror Spec 003): mel, audio-features, LM-hidden (cosine/max-abs),
  score-argmax exact match at `<timestamp>` positions, final word times ≤ 1 frame.
- If transformers-from-git cannot be installed on this box, fall back to dumping
  with the official `qwen_asr` reference repo; record the chosen oracle.

## 4. Pipeline integration & threading

- The aligner runs **per ASR segment**, off the ASR hot path, on its own thread +
  GPU stream (scheduler priority below ASR/diar so it never starves the live
  pipelines).
- Audio retention: the aligner's `PipelineAudioCache` must retain audio until the
  matching `asr/transcript` arrives. Since transcript lags its audio, add a
  retain-window variant (or a small dedicated ring keyed by absolute sample)
  trimmed past the oldest un-aligned segment start. Bounded by `max_segment_sec`.
- Output on `ComprehensiveTimeline`: a new `align` track carrying per-unit times,
  source-tagged; does not mutate the `asr` track (Art. III decoupling).
- Time base: unit times = span_start_sec + model_local_time; all via `TimeBase`.

## 5. Risks / decisions
- **R1 (perf)**: a 5-min segment = large `N_audio` + long text → one big causal
  attention (O(L²)). Mitigate: cap per-call segment length; the live ASR already
  emits sub-segments. Measure compute; aligner is off the live path.
- **R2 (decoder reuse)**: extending `AsrTextDecoder` to return all-position hidden
  states vs. a fresh `AlignerLmForward`. Decision: fresh focused forward (no KV
  cache) — lower coupling, matches NAR, easier to validate. Reuse only the
  kernels (`asr_ops`, `asr_gemm`), not the autoregressive machinery.
- **R3 (tokenizer)**: `BpeTokenizer` currently loads `vocab.json`/`merges.txt`;
  the aligner ships `tokenizer.json`. Confirm loader compatibility or add a
  `tokenizer.json` path; verify special ids (151676/151705/audio bos/eos).
- **R4 (audio-token count)**: N_audio from `_get_audio_token_length`; must equal
  the audio-pad count or the masked_scatter fails. Validate against oracle ids.
- **R5 (out-of-order timestamps)**: `_fix_timestamps` LIS repair is mandatory;
  implement and unit-test against the oracle's fixed output.

## 6. Phased implementation (each phase build-clean + a check)
- **P0** Oracle: dump reference tensors for one sample. (Unblocks all gates.)
- **P1** Tokenizer + `split_words_for_alignment` + special ids; unit test ids vs oracle.
- **P2** mel + audio tower (1024) + projector; gate audio_features vs oracle.
- **P3** input build + embed/scatter; gate input_ids + injected embeds vs oracle.
- **P4** causal LM full forward (all hidden) + score head; gate logits/argmax vs oracle.
- **P5** `forced_align_decode` (LIS) + word pairing; unit test vs oracle words.
- **P6** `IForcedAligner` + registration + standalone CTest end-to-end vs oracle.
- **P7** `AlignWorker` + AuditoryStream wiring + retain-window cache; align track
  in timeline; validate through real WS path; update `PROJECT_STATE.md`.

## 7. tasks
See [tasks.md](tasks.md).
