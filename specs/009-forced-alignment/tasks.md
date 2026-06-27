# Tasks 009 — ASR Forced Alignment Pipeline

Ordered, independently verifiable. Implement only after spec/plan review.
Each task: build clean (`-Wall -Wextra`) + its stated check.

## Phase 0 — Oracle (ground truth)
- [ ] **T0.1** Add `tools/reference/aligner_oracle.py`: load
  `Qwen3ASRForTokenClassification` from `models/ForcedAligner` in the torch venv;
  run one fixed `(audio, transcript, language)`; dump mel, audio_features,
  input_ids, lm_last_hidden, score_logits, argmax_labels, decoded_words to
  `tools/reference/aligner_dump/`. Check: prints word/time table matching the
  model card example shape.
- [ ] **T0.2** Record the exact special token ids and `N_audio` for the sample.

## Phase 1 — Tokenisation
- [ ] **T1.1** Confirm/extend `io::BpeTokenizer` to load `tokenizer.json` and
  resolve `<|audio_start|>`, `<|audio_pad|>`(151676), `<|audio_end|>`,
  `<timestamp>`(151705). Unit test: round-trip + special id lookups.
- [ ] **T1.2** `split_words_for_alignment(text, lang)` (CJK per-char, space words,
  drop punctuation, keep letters/numbers/apostrophes/CJK). Unit test vs oracle
  `word_lists` for a zh and an en sample.

## Phase 2 — Audio path
- [ ] **T2.1** Load `model.audio_tower.*` into `AsrAudioTower` configured for the
  aligner (d_model=1024, output=ln_post 1024). Gate: audio-tower hidden vs oracle.
- [ ] **T2.2** Projector `linear_1→GELU→linear_2` via `asr_gemm::Linear`. Gate:
  audio_features (post-projector) vs oracle within tolerance.

## Phase 3 — Input assembly
- [ ] **T3.1** Build `input_ids` = `[audio_start][audio_pad×N][audio_end]` +
  `word_k <ts><ts>`…; verify exact equality with oracle `input_ids`.
- [ ] **T3.2** Embed ids + masked-scatter audio_features at pad positions; gate
  injected `inputs_embeds` vs oracle.

## Phase 4 — Language model + head
- [ ] **T4.1** `AlignerLmForward`: single causal full-sequence forward over the 28
  Qwen3 layers using `asr_ops`/`asr_gemm`, returning `[seq, 1024]` final hidden
  (RMSNorm-final applied). No KV cache, no generation. Gate: lm_last_hidden vs
  oracle (cosine ≥ threshold, max-abs ≤ gate).
- [ ] **T4.2** `score` head Linear(1024→5000). Gate: argmax labels at every
  `<timestamp>` position EXACTLY match oracle; logits close.

## Phase 5 — Decode
- [ ] **T5.1** `forced_align_decode`: gather argmax at `<timestamp>` positions,
  `×80ms`, `_fix_timestamps` (LIS + snap/interp), pair word k → (2k,2k+1). Unit
  test vs oracle decoded words (≤ 1 frame).

## Phase 6 — Model integration
- [x] **T6.1** `IForcedAligner` interface + `Qwen3ForcedAligner` implementation
  (`Align(pcm,n,transcript,language)`); register via `Registry`. CTest
  `test_aligner_e2e`: fixed sample end-to-end vs oracle words (9 real words 0 ms).

## Phase 7 — Pipeline
- [x] **T7.1** `kAlignUnits` topic; `AlignWorker` subscribing to
  `asr/transcript`, reading audio from a retain-window `RetainedAudioBuffer`
  (purpose-built sliding window, not the read-then-free `PipelineAudioCache`),
  calling `Align`, publishing `align/units`. Worker catches GPU faults and
  clamps unit times to segment bounds. CTest `test_retained_audio`.
- [x] **T7.2** AuditoryStream wiring: create aligner + retained buffer + worker
  when `[align].enable` + `model_dir` set; PushAudio fan-out; `{"type":"align"}`
  WS event. (Comprehensive-timeline align track deferred — units delivered via
  protocol topic + WS event.)
- [x] **T7.3** `orator.toml` `[align]` section + config loader fields
  (`enable`, `model_dir`, `language`, `max_segment_sec`, `retain_sec`).
- [x] **T7.4** Validated through real WS path (rate=0, 120 s): 85 align events /
  1441 per-character units, all within segment bounds, no regression to
  ASR/diar/VAD, server stable, memory bounded by `retain_sec`. `PROJECT_STATE.md`
  updated (Art. VIII).

## Exit criteria
- All stage gates pass vs oracle; `ctest` green (45/45) incl. the six aligner
  tests + `test_retained_audio`; end-to-end alignment validated on the real
  streaming path; docs synced.
- **Pending enhancement** (not blocking): per-`text_id` final-only dedup so
  partial-revision / hallucinated ASR text does not produce degenerate or
  duplicated alignment units. Streaming alignment quality follows ASR text
  quality; this does not affect the ASR/diar/VAD pipelines.

## Phase 8 — Alignment quality + timeline integration (DONE)
- [x] **T8.1** Finals-only alignment. `AsrWorker::TextSegmentSink` gains
  `is_final`; `HandleTextSink` routes finals → `asr/transcript`, partials →
  `asr/transcript_partial`. The aligner's `asr/transcript` subscription now
  receives only finalized segments → one alignment per segment (no partial
  re-alignment). Comprehensive timeline still sees both via `asr/+`.
- [x] **T8.2** Comprehensive-timeline align track. `ComprehensiveTimeline`
  gains `AlignUnitSeg`/`AlignGroup`, `UpsertAlign(text_id,...)` (idempotent by
  id), `SnapshotAlign()`; `HandleAlignSubscription` bridges `align/units` →
  `comp_`; AuditoryStream subscribes `kAlignUnits`; serialize emits an `align`
  track grouped by `text_id` (ties units back to the asr track). Times stay on
  the common time base.
- [x] **T8.3** Validated through real WS path (rate=0, 120 s): 39 align events =
  39 ASR finals, 0 duplicate ids, 0/564 units out of bounds; timeline tracks =
  diar/asr/vad/**align** (39:39 with asr track); real-speech segment units
  monotonic and spread across the segment. No regression to ASR/diar/VAD.
- **Remaining (ASR-layer, not alignment)**: ASR hallucinations (garbage text,
  system-prompt echo) appear identically in the asr and align tracks; the
  aligner faithfully aligns whatever ASR emits. Suppressing these belongs in the
  ASR decoding pipeline, not the aligner.
