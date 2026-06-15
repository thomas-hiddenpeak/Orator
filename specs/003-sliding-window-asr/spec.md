# Spec 003 — Incremental KV-Cache Real-Time ASR

- **Feature**: `003-sliding-window-asr`
- **Status**: Revised (pivot from time-domain text-commit window to incremental KV cache)
- **Created**: 2026-06-12
- **Revised**: 2026-06-15
- **Owner**: project owner
- **Constitution**: v1.1.0

> This spec describes WHAT to change and WHY. The incremental decode design and
> parameters are in `plan.md`. Terminology is standard.

---

## 0. Pivot note (why this spec was rewritten, not replaced)

The original Spec 003 proposed a time-domain bounded sliding window that
re-decoded the most recent N seconds each step and committed text that left the
window. Five variants were implemented and measured; all were worse than the
energy-VAD baseline (CER 30.2% / RTF 4.99x) or sub-real-time, because (a) without
per-word timestamps there is no reliable map from "audio leaving the window" to
"characters to commit" (audio/text desync, head loss), and (b) every step
re-encoded and re-decoded the whole window, fighting an O(n^2) recompute wall.

The root cause of that wall is now understood from the official vLLM streaming
backend: the naive method (re-feed all audio + growing prompt each step) is
inherently quadratic, and vLLM only makes it tractable by **reusing the KV cache
of unchanged earlier tokens** (automatic prefix caching) together with a
**chunk-local windowed audio encoder** that freezes earlier audio embeddings.
Our engine already has the foundational machinery — a persistent per-layer KV
cache and a `Forward(x, Tq, pos0)` that appends KV at an arbitrary position — but
the streaming primitive (`BuildAndRun`) throws it away by calling `ResetCache()`
and re-prefilling the entire prompt every call. This spec adopts the vLLM
essence in pure C++/CUDA: keep the audio-block KV across steps, prefill only the
new audio chunk plus the short suffix, and bound total growth by resetting the
cache on natural segment boundaries.

## 1. Summary

Replace the ASR streaming primitive's per-call `ResetCache` + full re-prefill
with an **incremental KV-cache decode**: across streaming steps the decoder
retains the KV of the already-encoded audio block and the fixed prompt prefix,
and each step prefills only (i) the newly arrived audio chunk's tokens and
(ii) the short fixed suffix (audio-end, assistant header, language tag,
`<asr_text>`, committed text). The audio encoder runs in its trained
chunk-local windowed mode so a newly arrived chunk can be encoded standalone and
appended without changing earlier audio embeddings (preserving cache validity).
Total context growth is bounded by resetting the cache on a natural boundary
(speech endpoint or speaker turn), so per-step cost is O(new chunk), not
O(elapsed audio). This keeps the model's cross-utterance context (higher
accuracy) while sustaining real-time on an arbitrarily long stream.

## 2. Background and Problem

The fair baseline is the production Silero-VAD pipeline (model VAD with
cross-segment text context via `pending_prefix_`), measured on the same span as
any new method. (The earlier energy-VAD micro-segmentation, CER 30.2%, is a dead
lower bound that production already replaced; do not anchor to it.) Three
measured results define the problem (recorded in PROJECT_STATE and memory):

- **Silero-VAD pipeline (production, fair baseline)**: on the first 120 s of
  `test.mp3` it cuts 49 short utterances, each transcribed with limited context;
  CER 30.8%, ASR real-time factor 3.27x. It drops the opening and flips a
  negation ("拿不出" -> "拿出") — the kind of error cross-window context fixes. Over
  the full 600 s it reaches about 90% (much easier average than this hard,
  disfluent opening).
- **Unbounded growing window** (probe `asr_stream_window_probe`): re-feeds all
  audio from the stream start every step. Per-step time grows linearly
  (716 ms at 2 s to 9054 ms at 120 s), crosses below real time at about 40 s,
  overall 0.49x — O(n^2). CER 27.0% (better; continuous, self-correcting text).
  Memory was not the limit (VmRSS grew only 356 MB).
- **vLLM streaming backend (source read 2026-06-15)**: the same growing-window
  algorithm, made real-time by KV reuse of unchanged earlier tokens plus a
  chunk-local windowed encoder. This is the mechanism this spec ports.

The unbounded window's cost is GPU compute that scales with elapsed audio because
our engine recomputes the whole prompt each step. Reusing the KV of the
unchanged audio block makes that cost incremental; resetting on a boundary keeps
it bounded for an arbitrarily long stream.

## 3. Goals

- **G1** The streaming primitive retains the KV cache of the already-encoded
  audio block and fixed prompt prefix across steps; each step prefills only the
  new audio chunk's tokens plus the short fixed suffix — per-step prefill cost is
  O(new chunk), independent of elapsed audio within a segment.
- **G2** The audio encoder runs in chunk-local windowed mode so a newly arrived
  chunk is encoded standalone and appended, producing the same earlier-token
  embeddings as a full encode (cache stays valid). Verified numerically.
- **G3** Total context (audio KV + cache length) is bounded by resetting the
  cache on a natural boundary (speech endpoint / speaker turn), so per-step cost
  does not trend upward over a long stream.
- **G4** Transcription accuracy is at least as good as the Silero-VAD baseline,
  measured as CER against the gold transcript on the same span, and is expected
  to improve from the retained cross-chunk context.
- **G5** The pipeline remains independent of diarization and emits onto the same
  comprehensive timeline; the output contract is unchanged.

## 4. Non-Goals

- **NG1** Model numerics changes beyond switching the encoder to its trained
  windowed mode (no quantization, no precision change). Quantization deferred
  (Constitution II.3).
- **NG2** Per-word timestamps (require Qwen3-ForcedAligner; the streaming path
  emits none — the project time base anchors text onto the timeline).
- **NG3** GPU scheduling / streams (Spec 002, deferred).
- **NG4** Changing the diarization pipeline.

## 5. Functional Requirements

- **FR1 — Persistent audio-block KV**: The streaming session SHALL keep the KV
  cache of the system prefix and the audio-pad block across steps, and SHALL NOT
  reset it between steps within a segment.
- **FR2 — Incremental append**: On each step the session SHALL encode only the
  newly arrived audio chunk and append its audio-pad token KV at the current
  cache position (no recompute of earlier audio tokens).
- **FR3 — Suffix re-prefill + decode**: Each step SHALL prefill the fixed suffix
  (audio-end, assistant header, language tag, `<asr_text>`, committed text tail)
  after the audio block, decode the continuation, then truncate the cache back
  to the end of the audio block so the suffix KV does not persist.
- **FR4 — Windowed encoder**: The encoder SHALL run in chunk-local windowed mode
  for the streaming path so appended chunks do not change earlier embeddings.
- **FR5 — Bounded growth**: The session SHALL reset the audio-block KV on a
  natural boundary (speech endpoint or speaker turn) so the cache length, and
  thus per-step attention cost, is bounded over a long stream.
- **FR6 — Timeline output**: Committed transcript spans SHALL be appended to the
  shared timeline with absolute start/end on the shared clock; the comprehensive
  (speaker-attributed) view SHALL be built as before.
- **FR7 — Finalize**: On end of stream the session SHALL decode and commit the
  remaining tail so no audio is left untranscribed.
- **FR8 — Accuracy measurement**: A CER measurement against the gold transcript
  SHALL be available for the streamed output of `test.mp3`.

## 6. Acceptance Criteria

- **AC1** Per-step prefill cost is incremental: with a fixed chunk size, the
  prefill time of a step deep into a segment is within a small bound of an early
  step (it does not grow linearly with elapsed audio as the unbounded probe
  did). Measured first-third vs last-third within a segment. (FR1–FR3, G1)
- **AC2** Chunk-local encode equivalence: encoding the full audio and encoding
  chunk-by-chunk-and-appending produce token embeddings whose maximum absolute
  difference is within tolerance over the earlier (frozen) tokens. (FR4, G2)
- **AC3** CER of the incremental-streaming transcript against the gold is less
  than or equal to the Silero-VAD baseline on the same span (measured 30.8% on
  120 s); the measured incremental result is 17.6% (toward the unbounded-window
  27.0%). (G4, FR8)
- **AC4** Sustained streaming real-time factor on 120 s is at least the
  Silero-VAD baseline (3.27x ASR compute); the measured incremental seg-reset
  result is 4.51x. The number is measured and reported honestly. (G1, G3)
- **AC5** Memory and per-step cost are bounded over a long run: with boundary
  resets, VmRSS and per-step time do not grow without limit. (FR5)
- **AC6** The output contract is unchanged: timeline document with diarization
  and asr tracks plus the comprehensive view. (G5)
- **AC7** Build is clean under `-Wall -Wextra`; the test suite passes; the
  threaded path has no data race; transcript determinism holds across two runs.
  (Constitution V)

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

- **R1** Windowed-encoder numeric drift: the streaming path uses the trained
  chunk-local windowed attention instead of the full bidirectional attention the
  oracle uses. Earlier-token embeddings must stay stable across chunk appends;
  AC2 verifies the equivalence and tolerance. Chunk size must align with the
  encoder window (`n_window*2` mel frames) so chunk boundaries are clean.
- **R2** Suffix re-prefill correctness: the suffix (audio-end ... `<asr_text>` +
  committed tail) sits after the growing audio block, so its absolute positions
  shift each step and its KV must be recomputed every step, then truncated. The
  cache-truncate-and-re-prefill path must leave the audio-block KV untouched;
  determinism (AC7) verifies it.
- **R3** Boundary reset policy: resetting too often loses context (toward the
  VAD baseline); resetting too rarely grows per-step attention cost. The reset
  trigger (speech endpoint / max segment cap) is a parameter swept in `plan.md`.
- **R4** Committed-text rollback: only the last K tokens of the running output
  are \"unfixed\" and re-decoded as more audio arrives; the rest is committed into
  the suffix prefix. The rollback must not cut a multi-byte UTF-8 character\n  (mirror the official `\\ufffd` guard). Validated by AC3/AC7.
- **R5** Gold alignment for CER: the gold transcript covers a known span; the
  CER harness compares normalized text over the same span. Defined in `plan.md`.
