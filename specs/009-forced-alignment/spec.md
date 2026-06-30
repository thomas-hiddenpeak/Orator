# Spec 009 — ASR Forced Alignment Pipeline (Qwen3-ForcedAligner-0.6B)

Status: Implemented (2026-06-30) — Qwen3ForcedAligner ported + registered, AlignWorker
wired into AuditoryStream behind `[align]`, comprehensive-timeline align track. Full
transcript coverage validated on the real `rate=1` 60-min all-features stream: 119/119
ASR segments aligned (100%), 13594 character units, 0 out-of-bounds / 0 non-monotonic,
RTF ~35x, no CUDA errors. A grid y-dim overflow that dropped every long segment was
found and fixed (commit fa5f2ad).
Owner: pipeline
Depends on: Spec 003 (ASR), Spec 004 (comprehensive timeline + protocol)

## 1. WHAT & WHY

### Problem
The ASR pipeline (Spec 003) emits transcript segments whose time codes are coarse
(segment-level, derived from the VAD gate / engine cadence). Downstream consumers
need **character/word-level timestamps**: precise start/end time for each spoken
unit. The Qwen3-ForcedAligner-0.6B model produces exactly this, in a single
non-autoregressive (NAR) forward pass, for up to ~5 minutes of audio in 11
languages.

### Goal
Add a new pipeline that, given (a) an audio span and (b) the ASR transcript for
that span, produces per-unit timestamps `{text, start_sec, end_sec}` and deposits
them into the `ComprehensiveTimeline` so they can be served alongside ASR/diar/VAD.

This also **decouples segmentation accuracy from timing accuracy**: once the
aligner provides exact time codes, the ASR gate no longer needs precise cut
points (relevant to the deferred VAD-horizon decision).

### Non-goals (this spec)
- No streaming/incremental alignment. The aligner runs **per completed ASR
  segment** (offline within the live session), as the model is NAR over a whole
  segment.
- No new languages beyond what the model supports.
- No retraining / fine-tuning. Inference only.
- Output-contract change to existing `asr` events is out of scope; alignment is
  delivered on its own track/topic.

## 2. Model contract (verified against the HF `qwen3_asr` source + checkpoint)

`models/ForcedAligner/` — `Qwen3ASRForTokenClassification`, 917M params, bf16,
single `model.safetensors` (708 tensors).

Architecture (all reusable from the existing ASR stack unless noted):
- **Audio tower** `model.audio_tower.*`: 3× Conv2d(stride 2) frontend + Linear
  `conv_out` + 24 **bidirectional** windowed transformer layers + `ln_post`.
  `d_model=1024`, 16 heads, ffn 4096, 128 mel bins. (Same family as the ASR
  `AsrAudioTower`; output dim **1024**, not 2048.)
- **Multimodal projector** `model.multi_modal_projector.linear_{1,2}`:
  Linear(1024→1024) → GELU → Linear(1024→1024). Projects audio features before
  injection at audio-placeholder positions.
- **Language model** `model.language_model.*`: Qwen3 decoder, **causal**, 28
  layers, hidden 1024, 16 q / 8 kv heads, head_dim 128, intermediate 3072,
  RMSNorm eps 1e-6, q_norm/k_norm, RoPE theta 1e6, vocab 152064.
- **Score head** `score.weight`: Linear(1024→5000), **no bias**.

Forward (single pass):
1. mel(128) → audio_tower → `[N_audio, 1024]` → multi_modal_projector → `[N_audio, 1024]`.
2. Build `input_ids` = `<|audio_start|>` `<|audio_pad|>`×N_audio `<|audio_end|>`
   then `word_1 <timestamp><timestamp> word_2 <timestamp><timestamp> … word_K <timestamp><timestamp>`.
3. Embed `input_ids`; replace audio-pad embeddings with the projected audio features.
4. Run the **causal** Qwen3 LM over the full sequence (one prefill; no generation).
   Because the audio tokens precede all text, every `<timestamp>` token attends to
   the full (bidirectionally-encoded) audio plus its preceding text — causal LM
   attention is sufficient.
5. Apply `score` head to **all** positions → logits `[seq, 5000]`.
6. Decode: at each `<timestamp>` position take `argmax`; `time_ms = label × 80`
   (`timestamp_segment_time=80`); apply `_fix_timestamps` (monotonic LIS repair);
   for word k, `start = ms[2k]/1000`, `end = ms[2k+1]/1000` (seconds).

Tokenisation (`split_words_for_alignment`): CJK chars emitted individually,
space-delimited scripts produce whole words, punctuation dropped, keep
letters/numbers/apostrophes/CJK. (Japanese/Korean use external morphology libs —
out of scope; supported langs here cover zh/en/yue/fr/de/it/ja/ko/pt/ru/es, we
implement the default CJK/space tokenizer; ja/ko fall back to the default.)

Special token ids (to confirm from `tokenizer.json`): `audio_token_id=151676`
(pad), `timestamp_token_id=151705`, audio bos/eos (`<|audio_start|>`/`<|audio_end|>`).

## 3. Functional requirements

- **FR1**: Given a mono 16 kHz audio span (≤ 5 min) and its transcript string +
  language, produce an ordered list of `{text, start_sec, end_sec}` units whose
  times are on the session common time base (`TimeBase`), offset by the span's
  absolute start.
- **FR2**: Pure C++20/CUDA runtime, zero new third-party runtime dependencies
  (Constitution Art. I). Reuse the existing mel, audio tower, decoder, tokenizer,
  safetensor loader, and GPU kernels; add only the projector wiring, the score
  head, the input builder, and the timestamp decoder.
- **FR3**: Numerically validated against a PyTorch oracle stage-by-stage (mel,
  audio features, LM hidden, score logits, argmax labels) and end-to-end
  (per-word start/end within tolerance), per Constitution Art. II.
- **FR4**: Integrated as an independent pipeline (Art. III): consumes
  `asr/transcript` segments + audio from its own `PipelineAudioCache`; produces
  alignment into the `ComprehensiveTimeline` via the protocol layer. It does not
  read other pipelines' internal state and never blocks them.
- **FR5**: Configurable + registered like other models (`Registry<IForcedAligner>`
  or extend `IAsr`-style interface); model dir from `orator.toml`.
- **FR6**: A standalone CTest validates the single-forward path on a fixed
  audio+transcript against the oracle output.

## 4. Acceptance criteria
- Build clean under `-Wall -Wextra`; new unit/integration test passes in `ctest`.
- Oracle gates: mel ≤ existing mel gate; audio features and LM hidden within
  tolerance; score-argmax labels match the oracle at all `<timestamp>` positions
  on the reference sample; decoded word times match the oracle within ≤ 1 frame
  (80 ms) after `_fix_timestamps`.
- End-to-end through the real WS path: alignment track present for ASR segments,
  times monotonic and within the segment bounds.
- `PROJECT_STATE.md` updated with evidence (Art. VIII).

## 5. Open questions (resolve during Plan/oracle)
- Exact audio-token count formula vs. the ASR tower (the aligner uses the same
  `_get_audio_token_length`); confirm N_audio matches the audio-pad count.
- RoPE position ids for the packed `[audio | text]` sequence (contiguous 0..L-1?).
- Whether to reuse `AsrTextDecoder::Prefill` (extended to return all-position
  hidden states) or write a dedicated single-pass causal forward over `asr_ops`.
- Audio retention policy for the aligner cache (retain until the matching
  `asr/transcript` segment arrives; trim past the oldest pending segment).
