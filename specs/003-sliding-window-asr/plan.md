# Plan 003 — Incremental KV-Cache Real-Time ASR

- **Feature**: `003-sliding-window-asr`
- **Spec**: [spec.md](spec.md)
- **Status**: Revised (incremental KV cache + windowed encoder)
- **Constitution**: v1.7.0

> HOW to satisfy [spec.md](spec.md). Standard terminology.

---

## 1. Mechanism

### 1.1 Prompt layout and what changes per step

The decode prompt for one streaming segment is:

```
[ system prefix S ] [ audio_start ] [ audio_pad x N ] [ audio_end ]
[ assistant header ] [ "language X" ] [ <asr_text> ] [ committed tail ]
```

The audio-pad block sits in the middle. As audio arrives, only N grows (new
audio-pad tokens are appended at the end of the audio block). The system prefix
and the earlier audio-pad tokens keep their absolute positions, so their KV
stays valid. Everything AFTER the audio block (audio_end ... committed tail)
shifts to later positions when N grows, so its KV must be recomputed each step;
it is short and bounded.

### 1.2 Per-segment streaming session state

- `cache_ckpt_` — cache length after `[S][audio_start][audio_pad x N]`
  (the persistent checkpoint; suffix KV is dropped back to here each step).
- `audio_tokens_` — N, the number of audio-pad tokens encoded so far.
- `running_ids_` — the full decoded token ids of the segment so far
  (committed + unfixed tail), used to build the suffix prefix.
- `unfixed_token_num` — K; the last K tokens of `running_ids_` are re-decoded
  each step (mirrors the official rollback); the rest are committed into the
  suffix prefix verbatim.
- `seg_base_sample_` — absolute sample index where this segment started, for
  timeline anchoring.

### 1.3 Per chunk (`StreamChunk(pcm, n)`)

1. Append `pcm` to the segment audio buffer; once at least `chunk_sec` of new
   audio is available, run a step.
2. **Encode new chunk only** (windowed encoder): mel + `AsrAudioTower::Forward`
   on the new chunk produces `dN` new audio tokens whose embeddings equal their
   slice of a full encode (chunk-local; AC2). Inject them into `dN` audio-pad
   embedding slots.
3. **Append audio KV**: `decoder.PrefillAt(new_audio_embeds, dN, pos0 =
   cache_ckpt_)` writes the new audio KV after the existing audio block. Advance
   `cache_ckpt_ += dN`, `audio_tokens_ += dN`.
4. **Suffix prefill + decode**: build the suffix embeds
   `[audio_end, assistant header, "language X", <asr_text>, committed_tail]` where
   `committed_tail` = `running_ids_` with the last K tokens dropped; prefill the
   suffix at `pos0 = cache_ckpt_`, then `DecodeGreedy(start_pos = cache_ckpt_ +
   suffix_len, ...)` to get the continuation tokens. The new `running_ids_` =
   committed_tail + continuation.
5. **Truncate**: `decoder.TruncateCache(cache_ckpt_)` drops the suffix +
   generated KV, leaving only `[S][audio_start][audio_pad x N]` cached for the
   next chunk.
6. Update the live transcript from `running_ids_` (decoded, language tag
   stripped). The leading stable part (all but the unfixed tail) is what gets
   committed to the timeline once it stops changing.

### 1.4 Boundary reset (bounded growth)

N, the audio-block KV, and per-token attention cost grow within a segment. To
bound them over a long stream, the session resets on a natural boundary.

**Primary trigger** (2026-06-17 revision): VAD-gated segment boundaries (§8).
When VAD detects the end of a speech region and the trailing window
(`asr_vad_trail_sec`) elapses without VAD returning to speech, the segment is
committed and reset. See §8 for the state machine.

**Safety fallback**: `max_segment_sec` (24.0 s) remains as a hard upper bound.
If VAD fails to detect silence or produces continuous false-speech, the segment
resets on the safety cap. This protects against VAD pipeline failure.

On reset: commit the whole running transcript of the segment to the timeline with
absolute times derived from `seg_base_sample_` and the segment audio length,
`decoder.ResetCache()`, clear the session state, and start a new segment at the
current sample. Per-step cost is therefore O(chunk) within a segment and the
segment length is bounded by VAD speech regions (or `max_segment_sec` as safety
cap) (FR5, AC5).

### 1.5 Why this is incremental

- Per step the decoder prefills only `dN` (new chunk) + `suffix_len` (~10 +
  bounded committed tail) tokens, not the whole prompt — O(chunk), not
  O(elapsed). The bulk (N audio tokens, which reaches thousands) is reused from
  cache (FR1–FR3, G1, AC1).
- The windowed encoder encodes only the new chunk (chunk-local), so audio
  encoding is also O(chunk) (FR4, G2, AC2).
- Boundary reset caps N so per-token decode attention (O(cache_len)) stays
  bounded over a long stream (FR5, G3, AC5).

## 2. Components

### 2.1 `AsrTextDecoder` (add two small primitives, no numeric change)
- `PrefillAt(const float* embeds, int T, int pos0, cudaStream_t)` — thin public
  wrapper over the existing `Forward(x, T, pos0)` that APPENDS KV at `pos0`
  instead of resetting to 0. (`Prefill` stays as `PrefillAt(.,.,0)`.)
- `TruncateCache(int len)` — set `cache_len_ = len` (buffers retained). Used to
  drop suffix/generated KV back to the audio-block checkpoint.
- `cache_len()` accessor for the session to read the current checkpoint.
  No kernel or numeric change; the KV write path (`Forward` at arbitrary `pos0`)
  and `GqaCacheAttnKernel` (attends over `pos0` cache positions) already exist.

### 2.2 `AsrAudioTower` (streaming chunk-local mode)
- The encoder already chunks mel into `n_window*2` frames with per-chunk conv +
  per-chunk positional embedding. For streaming we run the trained windowed
  attention (now typed as `[asr].windowed_encoder`) so a standalone chunk encode equals its slice
  of a full encode. T020 verifies the equivalence (AC2). Chunk size is chosen to
  be a whole number of encoder windows so boundaries are clean.

### 2.3 `Qwen3Asr` (incremental streaming session API)
- Add a small session object/state implementing §1.3–1.4 on top of the existing
  mel/encoder/decoder and tokenizer: `StreamReset(base_sample)`,
  `StreamChunk(pcm, n) -> live_text`, `StreamFinalize() -> committed_text`.
  Reuses `Embed`, `PrefillAt`, `TruncateCache`, `DecodeGreedy`, `tokenizer()`.
  `TranscribeText` (single-segment) and the energy-VAD `SegmentSpeech` stay for
  fallback and for the boundary detector.

### 2.4 `AsrWorker` (wire the session in)
- Drive the streaming session from `ProcessSpan`: feed arriving audio to
  `StreamChunk`, detect endpoints/cap to trigger `StreamReset`, append committed
  spans to the timeline. Public surface (`ProcessSpan`, `Finalize`, `Reset`,
  `processed_samples`, `compute_sec`, `Emit`) unchanged so the controller and WS
  handler need no change.

### 2.5 Historical CER diagnostic
- `tools/cer.py` reproduces the historical character error rate between a
  hypothesis transcript and `asrTest2Final.txt`. Under Constitution 1.7.0 it is
  not an accuracy evaluator and cannot compare candidates, choose defaults, or
  issue a verdict. Current product evaluation uses complete contextual semantic
  review only.

## 3. Validation

- **Chunk-local equivalence (AC2)**: a probe encodes 120 s full vs
  chunk-by-chunk-append in windowed mode and reports the max abs diff over the
  frozen earlier tokens (tolerance from the bf16 encoder, ~1e-2).
- **Per-step cost incremental (AC1)**: instrument the session to log per-step
  prefill + decode time within a segment; confirm a deep step is within a small
  bound of an early step (not linearly growing).
- **RTF (AC4)**: measure streaming RTF on 120 s, compare to the Silero-VAD
  baseline (3.27x ASR compute). Measured incremental seg-reset: 4.51x.
- **Accuracy (AC3)**: manually review every in-scope reference contribution and
  both outputs in complete conversational context, perform the second pass, and
  manually verify the result. The historical incremental 17.6% and Silero 30.8%
  CER values are diagnostics only and cannot decide AC3.
- **Determinism (AC7)**: two runs produce identical committed text.
- **Memory + bounded cost (AC5)**: VmRSS and per-step time bounded over a long
  run with boundary resets.
- **Tests/build (AC7)**: `ctest` green, `-Wall -Wextra` clean.

## 4. Parameter sweep (decide defaults from data, not assumption)

### 4.1 Initial design (pre-2026-06-17)

The encoder fixes the chunk granularity: windowed attention is block-diagonal
over `window_aftercnn = Wc * (n_window_infer/win) = 13*8 = 104` tokens = 8 s of
audio. T011 verified a standalone 8 s window encode equals its slice of a full
encode to 4.5e-3 (bf16 noise floor). So a streaming step appends whole 8 s
windows; `chunk_sec` is a positive multiple of 8 s (finer live latency would
require recomputing the trailing partial window, deferred). Sweep on 120 s of
`test.mp3`, reporting RTF and CER for each:
- `chunk_sec` in {8, 16} (whole encoder windows),
- `unfixed_token_num` (K) in {3, 5, 8},
- `endpoint_sec` in {0.6, 1.0} and `max_segment_sec` in {24, 32} (multiples of
  the 8 s window).

### 4.2 Refined defaults (2026-06-17, Web UI partial streaming)

The initial sweep constraints (8 s minimum chunk) were a design-time tradeoff
between encoder-window alignment and partial-emission latency. After integration
with the Web UI (Spec 006), the 8 s partial-emission interval was deemed
unacceptably slow for live microphone use. The parameters were refined:

**`kStreamWindowMel`: 800 (8 s) → 100 (1 s)** — the 1 s boundary (100 mel
frames at hop 160) is a clean sample boundary. AC2 chunk-local encode equivalence
still holds: the windowed encoder processes 100 mel frames standalone without
drifting from the full-encode reference (block-diagonal attention does not span
across chunks). Total segment cost is unchanged (same audio encoded once);
per-step prefill is 1/8 the original size; partial-emission latency improved
from 8 s to 1 s.

**`max_new_tokens`: 384 → 32** — the original value was inherited from the
non-streaming `Transcribe` path. With 1 s chunks producing ~6-18 tokens,
32 provides sufficient upper bound with early-EOS termination. Reduces
worst-case decode steps by 12× per chunk. Mirrors the official Qwen3-ASR
vLLM `max_new_tokens` default.

**`stream_unfixed_tokens_`: 5 → 15** — the original value (5) covered the
last ~3-4 Chinese characters. For high-speed speech (10-12 chars/s) with
1 s chunks, 5 tokens left most of the previous chunk unrevised. 15 tokens
covers the entire previous chunk's output, ensuring effective rollback.

**Chosen production defaults**:

```
kStreamWindowMel       = 100     (1 s per streaming step)
max_new_tokens         = 32      (per-chunk decode budget)
stream_unfixed_chunks  = 2       (first 2 steps: no prefix)
stream_unfixed_tokens  = 15      (rollback last 15 tokens per step)
max_segment_sec        = 24.0    (safety cap; primary reset is VAD-gated)
asr_vad_gate           = true    (VAD-gated segment boundaries)
asr_vad_lead_ms        = 200     (lead buffer for speech onset)
asr_vad_trail_sec      = 1.5     (trailing window before commit)
```

Reset cadence (endpoint/cap) trades context (accuracy) against bounded per-step
cost, unchanged from the initial design.

## 5. Constitution Check

- **I**: existing CUDA + std lib; CER tool is offline Python under tools/.
- **II**: AC3 forbids CER regression; numerics unchanged.
- **III**: ASR stays independent; timeline contract unchanged.
- **IV**: measured on the real streaming path; honest RTF + CER.
- **V**: small single-purpose worker; documented state; threaded path race-free.
- **VI**: standard terminology.
- **VII**: parameter defaults chosen from the §4 sweep, not assumed.

## 6. Risks and Mitigations

- **Windowed-encoder drift** → T020 verifies chunk-local encode equivalence
  (AC2) before any worker change; chunk size aligned to encoder windows.
- **Suffix re-prefill / truncate correctness** → the audio-block KV must be
  untouched by the suffix prefill + truncate; determinism (AC7) verifies it.
- **Rollback cutting a multi-byte character** → mirror the official `\ufffd`
  guard when dropping the unfixed tail.
- **Reset cadence** → endpoint/cap parameters swept in §4; trade context
  (accuracy) against bounded per-step cost.
- **Repetition at suffix join** → the existing repetition cleanup + EOS-ban
  steps apply; verified by AC3.

## 7. Out of Scope (per spec Non-Goals)

Quantization, per-word timestamps, GPU scheduling (Spec 002), diarization
changes.

## 8. VAD-Gated Segment Boundaries

### 8.1 Architecture

The ASR worker queries VAD speech regions through `ComprehensiveTimeline`, not
through `GpuVad` directly. This preserves the dual-pipeline independence
(Constitution Art. III). The data path:

```
GpuVad → ComprehensiveTimeline::AddVad() → ComprehensiveTimeline::vad_ track
                                                        ↓
                          AsrWorker queries SnapshotVad() → VAD event channel
```

`ComprehensiveTimeline::SnapshotVad()` returns a vector of VAD segments (start,
end on the shared clock). The ASR worker receives updates via a channel from the
AuditoryStream (or polls the timeline at chunk boundaries). The channel delivers
new VAD segments as they are appended, so ASR never blocks the VAD pipeline.

### 8.2 Ring buffer for lead buffer

When VAD detects the onset of a speech region, the ASR worker needs audio that
preceded the VAD event (the lead buffer). Because `SharedAudioBuffer` cursors
only move forward, the worker cannot seek backward. Instead, `AsrWorker`
maintains a ring buffer:

- **Size**: ~500 ms at 16 kHz mono = 8000 samples = ~128 KB float32.
- **Purpose**: captures the tail of incoming audio so that when VAD signals
  speech onset, the worker can pop `asr_vad_lead_ms` (200 ms) of preceding
  audio and prepend it to the segment.
- **Lifetime**: the ring buffer is active during IDLE state. Once PROCESSING
  begins, the ring buffer is flushed into the segment audio buffer and the
  worker feeds audio directly.

### 8.3 State machine

The ASR worker transitions through three states:

```
                    VAD speech onset                VAD silence
  (IDLE) ──────────────► (PROCESSING) ──────────────────────► (TRAILING)
       │                     │                                    │
       │  lead_ms prepended   │  ring buffer → segment buffer      │  trail_sec
       │                     │                                    │  elapsed
       └── TRAILING timeout ─┘                                    │
                                                                  │
                    TRAILING ─────────────► IDLE (reset + commit)
                              safety cap exceeded
                              (max_segment_sec exceeded)
```

**IDLE**: No active speech. Audio arrives but is not encoded. The ring buffer
accumulates the tail (~500 ms). On VAD-speech entry:

1. Pop `asr_vad_lead_ms` (200 ms) from the ring buffer.
2. Prepend to segment audio.
3. Call `StreamReset(base_sample)` with the prepended audio's start sample.
4. Transition to PROCESSING.

**PROCESSING**: VAD indicates active speech. Feed audio to `StreamChunk` as
usual (encode, prefill, decode, truncate). Ring buffer is inactive. On VAD
silence, transition to TRAILING.

**TRAILING**: VAD indicates silence. No new audio is encoded. The worker waits
`asr_vad_trail_sec` (1.5 s). Two outcomes:

1. **VAD returns to speech**: resume PROCESSING within the same segment. No
   reset occurred. The trailing period is transparent.
2. **Trailing timeout**: commit the segment transcript, `ResetCache()`, clear
   session state, transition to IDLE.
3. **Safety cap exceeded**: if `max_segment_sec` (24.0 s) is exceeded while in
   TRAILING (continuous VAD speech that somehow did not produce a silence),
   force the same commit + reset as timeout.

### 8.4 VAD data coupling

The ASR worker's VAD dependency is through `ComprehensiveTimeline::SnapshotVad()`
only. It does not link to `GpuVad` or include `GpuVad` headers. The
AuditoryStream wires the connection: when the VAD pipeline appends to the
timeline, it signals the ASR worker through the existing VAD event channel (the
same channel used to drive energy-VAD in the original pipeline, now repurposed).
The channel carries `(vad_start, vad_end)` pairs on the shared clock. The ASR
worker reads these events and maps them to its current audio position.

### 8.5 Parameter defaults

| Parameter | Default | Role in state machine |
|---|---|---|
| `asr_vad_gate` | `true` | Enable/disable the entire mechanism |
| `asr_vad_lead_ms` | `200` | Amount of pre-onset audio popped from ring on IDLE → PROCESSING |
| `asr_vad_trail_sec` | `1.5` | Time to wait in TRAILING before committing segment |
| `max_segment_sec` | `24.0` | Hard cap — forces commit in any state if exceeded |
