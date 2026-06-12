# Spec 001 — Real-Time Streaming Dual Pipeline (Diarization + ASR) on a Unified Timeline

- **Feature**: `001-streaming-pipeline`
- **Status**: Draft (awaiting review)
- **Created**: 2026-06-12
- **Owner**: project owner
- **Constitution**: v1.0.0

> This spec describes WHAT the system must do and WHY. It avoids implementation
> detail (thread/buffer design lives in `plan.md`).

---

## 1. Summary

Orator ingests a single real-time mono-audio stream over WebSocket and produces
a **comprehensive timeline**: a single document with a shared absolute time axis
that contains one **track** per pipeline (a speaker-diarization track and an ASR
transcript track, with room for additional tracks later). Speaker separation and
ASR run as **two independent pipelines** that share only the input audio and the
time base; neither blocks the other. The system is validated end-to-end by
sending audio through the real WebSocket transport and asserting on the actual
terminal output (the timeline JSON).

## 2. Background and Problem

The codebase already has:
- A verified **streaming diarizer** (Sortformer, incremental, linear-time in the
  input length, with persistent speaker identity across the session). Validated
  against NeMo within recorded tolerances.
- A verified native **Qwen3-ASR engine** (mel → audio encoder → text decoder →
  BPE), validated against a PyTorch reference.

They are not yet combined into a streaming system with the required properties:
- `orator_ws` previously served diarization only; ASR was added as an inline
  call over the whole accumulated buffer.
- The two pipelines ran **sequentially in one thread**, so total wall time was
  approximately the sum of the two compute times, not the larger of the two.
- Streaming was previously measured by passing a complete recording to a single
  non-streaming call. That measurement omitted the per-utterance fixed cost
  incurred during real streaming and therefore overstated the real-time factor.

This feature implements the streaming dual pipeline with independent execution
and validates it on the real streaming path.

## 3. Goals

- **G1** Audio received over WebSocket feeds both pipelines from one shared
  buffer; the pipelines are independent (neither reads the other's results;
  neither blocks the other).
- **G2** The pipelines run on **independent threads**; a controller (the main
  process / connection handler) owns their lifecycle (start, stop, drain to
  completion, join).
- **G3** Both pipelines write results into **one comprehensive timeline**, each
  into its own track, keyed by the shared absolute time base. The terminal
  output always contains the diarization track and, when ASR is enabled, the
  ASR track.
- **G4** Streaming is validated through the **real WebSocket transport** by
  sending an incremental stream (optionally faster than real time); ASR runs at
  its own maximum rate. A non-streaming whole-recording call does not count as
  streaming validation.
- **G5** The full terminal output is **written to a JSON file** for inspection
  and later comparison.
- **G6** Diarization quality and ASR transcript quality are preserved (no
  regression against the verified references).

## 4. Non-Goals

- **NG1** ASR throughput optimization (endpoint parameter tuning, per-call
  fixed-cost reduction, batching). Deferred by the owner; not in this feature.
- **NG2** Quantization or any reduction in numerical fidelity. Deferred until
  explicitly scheduled (Constitution II.3).
- **NG3** Per-word speaker attribution and overlapping-speech attribution. The
  `comprehensive` view attributes whole utterances to speakers (the ASR engine
  does not emit per-word timestamps), and represents each turn as a single
  speaker. Per-word timing and simultaneous-speaker handling are future work.
- **NG4** Concurrent handling of multiple connections. One stream at a time
  matches the edge deployment.

## 5. User Scenarios

### Scenario A — Live transcription with speaker separation
A client opens a WebSocket connection and sends microphone PCM as it is
captured. While audio is received, the server sends incremental ASR utterance
messages and updates speaker-activity results. When the client sends `flush` (or
`end`), it receives a comprehensive timeline containing the diarization track and
the ASR transcript track on one time base.

### Scenario B — Faster-than-real-time validation
A test client sends a known recording (`test.mp3`, decoded to PCM) through the
WebSocket faster than real time (frames sent back-to-back). The ASR pipeline
processes the shared buffer at its own maximum rate; the diarization pipeline
does the same. The final comprehensive timeline is written to a JSON file and
compared against expectations.

## 6. Functional Requirements

Each requirement is testable.

- **FR1 — Ingest**: The server SHALL accept binary WebSocket frames of mono PCM
  (int16 little-endian by default; float32 selectable via a text control) and
  append the decoded samples to a single shared buffer indexed by absolute
  sample position.
- **FR2 — Independent consumption**: Diarization and ASR SHALL each read the
  buffered audio independently and concurrently. Neither SHALL read the other's
  output; neither SHALL wait on the other to make progress.
- **FR3 — Shared time base**: All emitted results (diarization segments and ASR
  transcript entries) SHALL carry timestamps in one absolute time base computed
  from the cumulative ingested sample count.
- **FR4 — Independent threads and controller**: The two pipelines SHALL run on
  separate worker threads. A single controller SHALL own their lifecycle: start
  on session open; on flush/end, stop accepting new input, process all buffered
  audio to completion, and join the threads.
- **FR5 — Incremental ASR output**: As ASR completes an utterance, the server
  SHALL send an incremental result message (`{"type":"asr", start, end, text}`).
- **FR6 — Comprehensive timeline output**: On flush/end, the server SHALL send
  one timeline document. The document SHALL contain a `tracks` array with one
  track per pipeline (a diarization track and, when ASR is enabled, an ASR
  track), each track holding time-ordered entries on the shared time base. The
  document SHALL also contain a `comprehensive` array: a derived view whose unit
  is the speaker turn, listing for each turn the start time, end time, speaker,
  and spoken text, ordered by time. The `comprehensive` view is present when ASR
  is enabled (`{"type":"timeline", ...}`; schema in §8).
- **FR7 — Control protocol**: The server SHALL honor the text controls `reset`
  (discard all state), `flush` (send the timeline accumulated so far; streaming
  continues), and `end` (process all buffered audio to completion, then send the
  final timeline). `flush` and `end` SHALL send the timeline only **after** all
  audio received before the control has been fully processed, so that no
  completed audio is omitted from the result.
- **FR8 — Faster-than-real-time input**: The system SHALL behave correctly when
  frames arrive faster than real time; the shared buffer retains the input and
  ASR processes it at its own maximum rate. (Buffer retention policy is
  specified in `plan.md`.)
- **FR9 — JSON file output**: The streaming test client SHALL write every
  received message (incremental `asr` messages and the final `timeline`) to a
  JSON file for inspection and later comparison.
- **FR10 — Metrics from the streaming path**: The test SHALL report per-pipeline
  real-time factors measured on the streaming path, plus the total wall-clock
  time to process all input, under stated clock conditions.

## 7. Acceptance Criteria

- **AC1** With ASR enabled, streaming `test.mp3` through the real WebSocket
  produces a final `{"type":"timeline",...}` whose `tracks` array contains a
  diarization track with at least one entry AND an ASR track with at least one
  entry, with all timestamps on one monotonically non-decreasing time base.
  (FR1–FR3, FR6)
- **AC2** Diarization and ASR run on separate threads under a controller. With
  two threads sharing one GPU, the GPU lock serializes device work, so total
  wall-clock time is approximately the sum of the two pipelines' compute times;
  the threads still overlap their CPU-side work. (FR2, FR4)
- **AC3** The validation sends audio **through the WebSocket** faster than real
  time (frames sent back-to-back); no part of the test passes a complete
  recording to a single non-streaming transcription call. (FR4, FR8)
- **AC4** `flush` and `end` return a timeline that accounts for all audio
  received before the control (no omitted final segment). (FR7)
- **AC5** The complete message log and final timeline are written to a JSON
  file; the file is valid JSON and contains both the diarization and ASR tracks.
  (FR9)
- **AC6** Diarization output matches the verified streaming diarizer within its
  recorded tolerance; the ASR transcript matches the verified engine's output
  for the same audio (no quality regression). (G6)
- **AC7** Build produces no new warnings under `-Wall -Wextra`; the full test
  suite passes; the threaded path has no data race (verified). (Constitution V)

## 8. Output Contract (JSON schemas)

Incremental ASR message (sent as each utterance completes):
```json
{ "type": "asr", "start": 12.34, "end": 15.67, "text": "..." }
```

Comprehensive timeline (sent on flush/end):
```json
{
  "type": "timeline",
  "schema_version": 1,
  "audio_sec": 120.0,
  "sample_rate": 16000,
  "tracks": [
    {
      "kind": "diarization",
      "source": "sortformer",
      "compute_sec": 12.5,
      "real_time_factor": 9.6,
      "entries": [
        { "start": 0.00, "end": 4.32, "speaker": 0, "confidence": 0.94 }
      ]
    },
    {
      "kind": "asr",
      "source": "qwen3_asr",
      "compute_sec": 46.4,
      "real_time_factor": 2.6,
      "entries": [
        { "start": 0.00, "end": 9.80, "text": "..." }
      ]
    }
  ],
  "comprehensive": [
    { "start": 0.00, "end": 9.80, "speaker": "speaker_0", "text": "..." }
  ]
}
```

Schema rules:
- Times are seconds on the shared absolute time base; `end >= start`.
- A track's `kind` identifies the pipeline (`diarization`, `asr`, and future
  kinds); `source` names the model that produced it.
- Track `entries` are ordered by `start`. Entry fields depend on `kind`:
  diarization entries carry `speaker` (a diarizer-local slot index) and
  `confidence`; ASR entries carry `text` (UTF-8, JSON-escaped).
- `comprehensive` entries are speaker turns ordered by `start`, each with
  `start`, `end`, `speaker` (the diarizer-local slot, formatted `speaker_N`),
  and `text`. Each ASR utterance is assigned to the diarization speaker with the
  greatest temporal overlap; consecutive utterances of the same speaker are
  combined into one turn. Granularity is the utterance, because the ASR engine
  does not produce per-word timestamps.
- Adding a pipeline adds a new track; existing tracks and the document schema
  are unchanged. A consumer reads whichever part it needs.
- The document is the terminal output and is the unit captured for inspection
  and comparison.

## 9. Constitution Check

- **Art. I (no dependencies)**: transport uses the existing POSIX WebSocket;
  threading uses `std::thread` and the C++ standard library. No new dependency.
- **Art. II (accuracy)**: no fidelity changes; AC6 requires no quality
  regression; quantization is NG2.
- **Art. III (independent pipelines, comprehensive timeline)**: the goals G1–G3
  and the track-based output contract (§8).
- **Art. IV (streaming validation through the real transport)**: AC3 requires
  WebSocket streaming and excludes whole-recording calls; AC1/AC5 assert on the
  actual terminal output.
- **Art. V (quality)**: AC7 requires a clean build, passing tests, and a
  race-free threaded path; the threading design is in `plan.md`.
- **Art. VI (terminology)**: this spec uses standard engineering terms and no
  metaphors.
- **Art. VII (SDD)**: this is the spec phase; plan and tasks follow.

## 10. Open Questions and Risks

- **Q1 (resolved)** Whole-recording call vs streaming: streaming through the
  WebSocket is required (AC3).
- **Q2 (resolved)** Threading: independent threads with a controller (FR4), per
  owner direction.
- **R1** Input arriving faster than ASR can process it: ASR may fall behind a
  fast sender, increasing buffer occupancy. The buffer retention policy (bounded
  vs unbounded) is decided in `plan.md`; to let ASR run at its own maximum rate,
  the default retains all unprocessed audio, and the total processing time is
  measured.
- **R2** Completion on `end`: the controller must process all buffered audio and
  join the worker threads after the sender signals end-of-stream, without
  omitting the final partial utterance (AC4).
- **R3** Metrics: real-time factors are reported only from the streaming path
  (Art. IV.4).
  (Art. IV.4).
