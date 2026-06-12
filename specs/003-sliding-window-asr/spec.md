# Spec 003 — Bounded Sliding-Window Real-Time ASR

- **Feature**: `003-sliding-window-asr`
- **Status**: Draft (in progress)
- **Created**: 2026-06-12
- **Owner**: project owner
- **Constitution**: v1.1.0

> This spec describes WHAT to change and WHY. The window/commit design and
> parameters are in `plan.md`. Terminology is standard; "window" means the
> bounded span of recent audio re-decoded each step.

---

## 1. Summary

Replace the ASR pipeline's energy-VAD micro-segmentation (which transcribes many
short, zero-context utterances) with a **bounded sliding window** that follows
the official Qwen3-ASR streaming method but caps per-step cost. The window holds
the most recent N seconds of audio; already-decoded text older than the window
is **committed** and supplied to the model as a text prefix; only the unfixed
tail of the window is re-decoded and revised as new audio arrives. This keeps the
model's cross-utterance context (higher accuracy) while bounding the per-step
GPU cost (sustained real-time on a long stream).

## 2. Background and Problem

Two measured results define the problem (recorded in PROJECT_STATE and memory):

- **Current method (energy-VAD micro-segmentation)**: 120 s of `test.mp3`
  produces 27 short utterances, each transcribed independently with no
  cross-segment context. Measured streaming ASR real-time factor 4.99x. Each
  call pays the full fixed cost (encoder pass + decode seed + prefill), and the
  output is split, sometimes cuts words at boundaries, and shows repetition on
  short noisy segments.
- **Official unbounded growing window** (probe `asr_stream_window_probe`):
  re-feeds all audio from the stream start every step. Per-step time grows
  linearly (716 ms at 2 s to 9054 ms at 120 s), crosses below real time at about
  40 s, overall 0.49x, total wall 243.8 s — O(n²), unusable for a long stream.
  Memory is not the limit (VmRSS grew only 356 MB). Its accuracy is clearly
  higher: continuous, punctuated text with self-correction.

The unbounded window's cost is GPU compute that scales with elapsed audio. A
bounded window makes that cost constant while preserving the context that gives
the higher accuracy.

## 3. Goals

- **G1** The ASR pipeline transcribes a continuous stream using a bounded
  sliding window: each decode step processes at most a fixed window of recent
  audio, so per-step cost does not grow with stream length.
- **G2** Decoded text that falls behind the window is committed once and reused
  as a prefix prompt, so the model keeps cross-window context without re-decoding
  committed text.
- **G3** On a long stream the per-step real-time factor is stable (does not
  trend toward zero as in the unbounded window).
- **G4** Transcription accuracy is at least as good as the current energy-VAD
  method, measured as character error rate (CER) against the gold transcript,
  and is expected to improve from the added context.
- **G5** The pipeline remains independent of diarization and emits onto the same
  comprehensive timeline; the output contract is unchanged.

## 4. Non-Goals

- **NG1** Model numerics changes (quantization, precision, kernel fusion).
  Deferred (Constitution II.3).
- **NG2** Per-word timestamps (require Qwen3-ForcedAligner; separate feature).
- **NG3** GPU scheduling / streams (Spec 002, deferred to a later time).
- **NG4** Changing the diarization pipeline.

## 5. Functional Requirements

- **FR1 — Bounded window**: The ASR worker SHALL maintain a window of at most
  `window_sec` seconds of the most recent audio. Audio older than the window is
  dropped from the re-decode buffer after its text is committed.
- **FR2 — Periodic decode**: Every `step_sec` seconds of newly arrived audio,
  the worker SHALL decode the current window (with the committed prefix) and
  update the live transcript.
- **FR3 — Commit and prefix**: Text decoded from audio that has passed out of
  the window SHALL be committed once and supplied to the model as a prefix prompt
  on subsequent steps; only the unfixed tail SHALL be re-decoded. The committed
  text SHALL NOT change on later steps.
- **FR4 — Timeline output**: Committed transcript spans SHALL be appended to the
  shared timeline with absolute start/end times on the shared clock; the
  comprehensive (speaker-attributed) view SHALL be built as before.
- **FR5 — Finalize**: On end of stream, the worker SHALL decode and commit the
  remaining window tail so no audio is left untranscribed.
- **FR6 — Stable cost**: Per-step decode cost SHALL be bounded by `window_sec`
  (independent of total stream length).
- **FR7 — Accuracy measurement**: A CER measurement against the gold transcript
  SHALL be available for the streamed output of `test.mp3`.

## 6. Acceptance Criteria

- **AC1** Streaming `test.mp3` (120 s) through the ASR pipeline with the bounded
  window produces a continuous transcript on the timeline; per-step decode time
  does not grow with elapsed audio (compare first-third vs last-third step
  times; the ratio stays within a small bound rather than trending up linearly).
  (FR1, FR2, FR6)
- **AC2** Sustained streaming real-time factor on 120 s is at least the current
  4.99x; the target is materially higher from fewer redundant decodes. The
  number is measured and reported honestly. (G3)
- **AC3** CER of the bounded-window transcript against the gold transcript is
  less than or equal to the CER of the current energy-VAD method on the same
  audio. (G4, FR7)
- **AC4** The committed transcript is stable: re-running the same audio yields
  the same committed text (determinism). (FR3)
- **AC5** Memory is bounded: VmRSS does not grow without limit over a long run
  (the window and committed text are bounded; only the plain committed string
  accumulates). (FR1)
- **AC6** The output contract is unchanged: timeline document with diarization
  and asr tracks plus the comprehensive view. (G5)
- **AC7** Build is clean under `-Wall -Wextra`; the test suite passes; the
  threaded path has no data race. (Constitution V)

## 7. Constitution Check

- **Art. I (no dependencies)**: only existing CUDA + standard library.
- **Art. II (accuracy)**: AC3 requires no CER regression; numerics unchanged.
- **Art. III (independent pipelines, comprehensive timeline)**: unchanged; only
  the ASR worker's internal method changes.
- **Art. IV (streaming validation)**: measured through the real streaming path;
  CER and RTF reported from that path.
- **Art. V (quality)**: AC7.
- **Art. VI (terminology)**: standard terms; no metaphor.
- **Art. VII (SDD)**: spec now; plan and tasks follow.

## 8. Open Questions and Risks

- **R1** Window and step sizes trade latency, accuracy, and cost. They are
  parameters swept in `plan.md`; defaults chosen from the sweep, not assumed.
- **R2** Commit boundary: committing too eagerly loses the context that improves
  accuracy; committing too late grows the window. The commit point is the audio
  that has left the window minus an unfixed margin (mirrors the official
  `unfixed_token_num` idea but in the time domain). Validated by AC3/AC4.
- **R3** Word splitting at the window's trailing edge: the unfixed margin and
  re-decode of the tail are intended to absorb this; verified by AC3.
- **R4** Gold alignment for CER: the gold transcript covers a known span; the
  CER harness compares normalized text over the same span. Defined in `plan.md`.
