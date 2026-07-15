# Spec 003 — Incremental KV-Cache Real-Time ASR

- **Feature**: `003-sliding-window-asr`
- **Status**: Implemented (incremental KV-cache); tasks.md audit 2026-06-21
- **Created**: 2026-06-12
- **Revised**: 2026-06-15 (pivot), 2026-06-17 (parameter refinement), 2026-06-17 (VAD gating + trailing window), 2026-06-18 (VAD decoupling)
- **Owner**: project owner
- **Constitution**: v1.7.0

> This spec describes WHAT to change and WHY. The incremental decode design and
> parameters are in `plan.md`. Terminology is standard.

> **Current evaluation governance:** Historical character error rate (CER)
> values in this document are retained as implementation history only. Under
> Constitution 1.7.0, no code, script, metric, formula, or algorithm may use
> CER or any other mechanical value to evaluate product accuracy, compare
> candidates, or choose parameters. Current ASR result evaluation requires
> complete item-by-item contextual semantic review.

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
- **G4** Transcription semantics are at least as good as the Silero-VAD baseline
  on the same span, established only by complete contextual semantic review.
  Historical CER values remain implementation diagnostics and do not evaluate
  accuracy or select parameters.
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
- **FR8 — Accuracy evaluation**: The streamed output of `test.mp3` SHALL receive
  complete item-by-item contextual semantic review. No code, script, test,
  metric, formula, query, or algorithm may assign judgments, calculate
  accuracy, compare/select parameters, or issue the verdict.

## 6. Acceptance Criteria

- **AC1** Per-step prefill cost is incremental: with a fixed chunk size, the
  prefill time of a step deep into a segment is within a small bound of an early
  step (it does not grow linearly with elapsed audio as the unbounded probe
  did). Measured first-third vs last-third within a segment. (FR1–FR3, G1)
- **AC2** Chunk-local encode equivalence: encoding the full audio and encoding
  chunk-by-chunk-and-appending produce token embeddings whose maximum absolute
  difference is within tolerance over the earlier (frozen) tokens. (FR4, G2)
- **AC3** Complete contextual semantic review establishes no regression against
  the Silero-VAD baseline on the same span. The historical 30.8%, 17.6%, and
  27.0% CER values are retained only as records of the original implementation
  process and cannot satisfy this current acceptance criterion. (G4, FR8)
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
- **Art. II (accuracy)**: AC3 requires no contextual semantic regression;
  numerical parity remains separate component evidence.
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
- **R5** Historical CER alignment: the gold transcript covers a known span; the
  CER harness compares normalized text over the same span. Defined in `plan.md`.

## 9. Parameter Refinement (2026-06-17)

The following parameters were refined after the initial implementation
(8cc31ab), driven by Web UI partial-streaming integration and high-speed
speech scenario requirements. All changes are verified on the real WebSocket
path and production `orator_ws` binary.

### 9.1 `kStreamWindowMel`: 800 (8 s) → 100 (1 s)

**Before**: 800 mel frames = 8 s per streaming step — one full encoder window.
**After**: 100 mel frames = 1 s per streaming step.

**Reasoning**:

The original 8 s window was inherited from plan §4's requirement that each
chunk must be a complete encoder window (104 tokens, verified by AC2 chunk-local
encode equivalence). However, the 8 s granularity was not a correctness
requirement — it was a parameter sweep constraint from the initial design.

The AC2 verification (T011) confirmed that the encoder's chunk-local windowed
mode produces numerically equivalent embeddings for any clean boundary (max abs
diff 4.5e-3). A 1 s boundary (100 mel frames at hop 160) is a clean sample
boundary in the WhisperMel pipeline. The encoder processes 100 mel frames
standalone without drifting from the full-encode reference, because the
windowed attention only spans within each chunk — there is no cross-chunk
dependency violated by a shorter chunk.

The motivation is the **Web UI partial streaming** requirement (Spec 006):
users expect draft text to appear every ~1 s during live microphone input,
not every 8 s. With 8 s windows, the `asr_partial` WebSocket message had an
8 s latency, making the UI feel unresponsive.

**Impact on AC2**: the chunk-local encode equivalence still holds because
100 mel frames is an integer number of hop steps (160-sample hop), and the
windowed encoder's block-diagonal attention is not violated by a shorter
chunk — it simply processes fewer tokens per append.

**Impact on AC1 (per-step incremental cost)**: each step's prefill now handles
~1/8 the audio tokens, making per-step cost even smaller. The number of steps
per segment increases 8×, but total segment cost is unchanged (same audio is
encoded once). The 1 s partial-emission cadence is a user-facing latency
improvement without throughput regression.

### 9.2 `max_new_tokens`: 384 → 32

**Before**: 384 tokens — generous but allows the model to over-generate
relative to the audio information available per chunk.
**After**: 32 tokens — mirrors the official Qwen3-ASR vLLM streaming
`max_new_tokens` default.

**Reasoning**: With a 1 s chunk, a speaker produces ~4-12 Chinese characters
(~6-18 tokens with BPE). `max_new_tokens = 32` provides sufficient upper
bound with early-EOS termination. The previous value (384) allowed the model
to generate far more text than the audio content supports, wasting decode
cycles and increasing the chance of hallucination in partial drafts.

32 tokens reduces worst-case decode steps by 12× per chunk (from 384 to 32).

### 9.3 `stream_unfixed_tokens_`: 5 → 15

**Before**: 5 — mirrors the official vLLM default for 2 s chunks.
**After**: 15 — adapted for high-speed speech (10+ chars/s) with 1 s chunks.

**Reasoning**: The official `unfixed_token_num = 5` was designed for
2 s chunks at normal speech rate (~6-8 chars/s), where 5 tokens covers
approximately the last 1 s of generated text. With 1 s chunks at high
speech rate (10-12 chars/s, "激情演讲"), one chunk can produce 12-18 tokens.
Rolling back only 5 tokens left 7-13 tokens of the previous chunk's output
unrevised when new audio context arrives.

15 tokens covers the entire previous 1 s chunk's output in most cases,
ensuring the rollback is effective. The decode overhead is minimal
(~10 extra tokens per step, negligible on GPU).

### 9.4 Unchanged Parameters

| Parameter | Value | Status |
|---|---|---|
| `stream_unfixed_chunks_` | 2 | Unchanged — first 2 chunks use no prefix |
| `max_segment_sec` | 24.0 s | Safety cap only — primary reset now driven by VAD gating (§10) |
| `min_silence_sec_` | 3.50 s | Unchanged — VAD endpoint threshold |
| `min_speech_sec_` | 0.20 s | Unchanged — drop spans < 200 ms |

### 9.5 Updated Default Configuration

Current production defaults (2026-06-17):

```
kStreamWindowMel       = 100     (1 s per streaming step)
max_new_tokens         = 32      (per-chunk decode budget)
stream_unfixed_chunks  = 2       (first 2 steps: no prefix)
stream_unfixed_tokens  = 15      (rollback last 15 tokens per step)
max_segment_sec        = 24.0    (safety cap only; primary reset is VAD-gated)
asr_vad_gate           = true    (VAD-gated segment boundaries enabled)
asr_vad_lead_ms        = 200     (lead buffer for speech onset)
asr_vad_trail_sec      = 1.5     (trailing window before commit)
```

## 10. VAD-Gated Segment Boundaries (2026-06-17 revision)

### Problem

The fixed `max_segment_sec = 24.0` cap terminates ASR segments on elapsed time
regardless of speech content. Two defects follow:

1. **Arbitrary cuts during continuous speech.** A 30-second monologue is split at
   24 s, mid-sentence. The decoder loses the leading context of the resumed
   segment, producing incomplete or garbled output at the splice point.
2. **Hallucination during non-speech.** Silence, music, and ambient noise still
   trigger ASR encoding and decoding because the segment timer keeps advancing.
   The model generates text for audio that contains no speech, degrading the
   transcript and wasting GPU cycles.

Both stem from the same root: the segment boundary timer is blind to the actual
presence or absence of speech in the audio stream.

### Solution

Replace the fixed timer with **VAD-gated segment boundaries**. The ASR worker
consults the VAD track to decide when speech is present, when it has ended, and
when enough silence has elapsed to commit a segment. The mechanism:

- **VAD-gated processing (FR9)**: ASR only processes audio that falls within
  VAD-marked speech regions. Non-speech audio is forwarded but not fed to the
  encoder, so silence produces no hallucinated text and no wasted compute.
- **Trailing window (FR10)**: When VAD transitions from speech to silence, the
  ASR worker enters a trailing window (`asr_vad_trail_sec`, default 1.5 s)
  before committing the segment. This guards against premature termination when
  VAD emits a brief false silence (breath, cough, or VAD miss). If VAD returns
  to speech within the trailing window, the worker resumes processing without
  resetting.
- **Lead buffer (FR11)**: When VAD transitions from silence to speech, the ASR
  worker prepends a short lead buffer (`asr_vad_lead_ms`, default 200 ms) of the
  audio that immediately preceded the VAD-speech onset. This prevents missing the
  first phoneme when VAD is late detecting onset.

The VAD signal source is `ComprehensiveTimeline::SnapshotVad()`, not `GpuVad`
directly. ASR reads the timeline and never receives VAD data via direct push
(no callback, no shared pointer, no atomic cursor). The VAD pipeline deposits
segments into the comprehensive timeline; ASR reads them under the timeline's
mutex. This is enforced by Constitution Article III §8. The
ComprehensiveTimeline already has `AddVad()` and `SnapshotVad()` methods
populated by the independent GpuVad worker.

### New Functional Requirements

- **FR9 — VAD-gated processing**: When `asr_vad_gate` is true, the ASR worker
  SHALL query the VAD track from `ComprehensiveTimeline::SnapshotVad()` to
  determine whether the current audio position falls within a speech region.
  Audio outside VAD-speech regions SHALL NOT be fed to the encoder/decoder,
  regardless of elapsed segment time. ASR reads VAD data only through the
  comprehensive timeline — never via direct push from the VAD pipeline
  (Constitution Article III §8).

- **FR10 — Trailing window**: When VAD transitions from speech to silence, the
  ASR worker SHALL enter a trailing window of `asr_vad_trail_sec` seconds before
  committing the current segment and resetting the cache. If VAD returns to
  speech within the trailing window, the worker SHALL resume processing the
  current segment (no reset). The trailing window is a commit guard, not an
  encoding window — no new audio is encoded during trailing.

- **FR11 — Lead buffer**: When VAD transitions from silence to speech (IDLE to
  PROCESSING), the ASR worker SHALL prepend `asr_vad_lead_ms` milliseconds of
  audio preceding the VAD-speech onset. The lead buffer is stored in a ring
  buffer (~500 ms, ~128 KB float32 at 16 kHz) in `AsrWriter` so it captures
  audio that arrived before VAD's speech event.

### Parameter changes

| Parameter | Value | Role |
|---|---|---|
| `asr_vad_gate` | `true` | Enable VAD-gated segment boundaries |
| `asr_vad_lead_ms` | `200` | Lead buffer size in milliseconds |
| `asr_vad_trail_sec` | `1.5` | Trailing window in seconds |
| `max_segment_sec` | `24.0` | Retained as hard safety cap only (see §9.4) |

`max_segment_sec` remains as a hard upper bound: if the trailing window and VAD
speech span exceed 24 s continuously, the segment resets on the safety cap. This
protects against VAD failure (missing silence, false continuous speech).
