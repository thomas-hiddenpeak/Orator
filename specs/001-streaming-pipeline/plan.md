# Plan 001 — Real-Time Streaming Dual Pipeline

- **Feature**: `001-streaming-pipeline`
- **Spec**: [spec.md](spec.md)
- **Status**: Implemented
- **Constitution**: v1.1.0

> This plan describes HOW to satisfy [spec.md](spec.md). It is the design of
> record for the threaded WebSocket dual-pipeline architecture. Terminology
> follows Constitution Article VI: standard engineering terms, no metaphors.

---

## 1. Architecture Overview

```
                    WebSocket (POSIX sockets)
                              |  binary PCM frames
                              v
                    +-------------------+
                    |  Ingest (net)     |  decode int16/f32 -> float, append
                    +---------+---------+
                              |  PushAudio(samples, n)
                              v
                    +-------------------+
                    | SharedAudioBuffer |  mutex + condition variable;
                    |                   |  absolute sample index
                    +----+---------+----+
            read cursor 0 |         | read cursor 1
                          v         v
            +------------------+ +------------------+
            | Diarization      | | ASR              |  two worker threads
            | worker thread    | | worker thread    |
            | (Sortformer)     | | (endpoint+Qwen3) |
            +--------+---------+ +--------+---------+
                     | track entries      | track entries
                     v                    v
                    +-------------------+
                    |  StreamTimeline   |  mutex-guarded:
                    |  (diar + asr)     |  diar frames + transcript
                    +---------+---------+
                              |  flush/end -> serialize
                              v
            WebSocket text frames (asr messages, timeline JSON)
```

A single **controller** (the connection handler, owned by the main process)
creates the buffer, starts the two worker threads, routes control messages, and
performs ordered shutdown: signal end-of-stream, process all buffered audio,
join the threads, then serialize the timeline.

Both worker threads call CUDA on one physical GPU. A process-wide GPU mutex
(`gpu::DeviceLock()`) makes each worker's GPU-using region mutually exclusive;
see §4.

## 2. Components

### 2.1 `SharedAudioBuffer` (`pipeline/shared_audio_buffer.{h,cc}`)
- Holds the session PCM indexed by absolute sample position (sample 0 is the
  start of the stream).
- Producer: the ingest thread appends samples under `mutex_` and increases
  `total_samples_`.
- Consumers: each pipeline holds its own read cursor (an absolute sample index).
  `WaitAndRead(cursor)` returns a copy of the samples from the cursor to the
  current end and advances the cursor.
- Synchronization: `std::mutex` plus `std::condition_variable`. A consumer
  blocks on the condition variable until samples beyond its cursor exist or the
  end-of-stream flag is set, rather than polling.
- End-of-stream: the producer sets `closed_` and notifies; each consumer reads
  the remaining samples and then `WaitAndRead` returns false, ending its loop.
- Memory retention (spec R1): the buffer retains all unread audio so ASR can run
  at its own maximum rate (spec FR8). It removes a prefix only after every
  consumer's cursor has advanced past it (the minimum of all cursors). No
  consumer can lose unread audio.

### 2.2 Worker threads (`DiarizationWorker`, `AsrWorker`)
- `DiarizationWorker::ProcessSpan`: acquire the GPU mutex, run
  `SortformerDiarizer::StreamAudio` on the span, release the mutex, append the
  produced frames to `StreamTimeline`. `Finalize` flushes the diarizer's
  remaining frames at end-of-stream. Persistent diarizer state is owned by this
  worker.
- `AsrWorker::ProcessSpan`: append the span to a private PCM buffer and run
  energy-based endpointing. For each completed utterance, acquire the GPU mutex,
  run `Qwen3Asr::TranscribeText`, release the mutex, append a transcript entry to
  `StreamTimeline`, and send an incremental `asr` message. `Finalize`
  transcribes the trailing utterance at end-of-stream.
- Each worker measures its own GPU compute time, used for the per-pipeline
  real-time factor.

### 2.3 `StreamTimeline` (`pipeline/stream_timeline.{h,cc}`)
- Mutex-guarded store: accumulated diarization frames and the transcript entry
  list, guarded by one mutex.
- Workers append through `AppendDiarFrames` / `AppendToken`. The controller
  reads consistent copies through `SnapshotDiarFrames` / `SnapshotTranscript`.

### 2.4 `AuditoryStream` (controller, `pipeline/auditory_stream.{h,cc}`)
- Owns the `SharedAudioBuffer`, the `StreamTimeline`, and the two worker threads.
- `PushAudio(samples, n)` appends to the buffer and returns immediately.
- `EmitTimeline(finalize=false)` (flush): wait until both workers' processed
  sample counts reach the current `total_samples_`, then serialize and send the
  timeline. Streaming continues; worker state is retained.
- `EmitTimeline(finalize=true)` (end): close the buffer, join both workers
  (each transcribes/flushes its remaining audio in its loop), then serialize and
  send the timeline.
- `Serialize()` builds the comprehensive timeline document (spec §8): one track
  per pipeline. Speaker-to-text attribution is out of scope (spec NG3).
- Provides the transport-independent `Emit` callback (the handler sends to the
  client; the test writes to a file). This class contains no socket code.

### 2.5 `AuditoryWsHandler` (transport adapter, `net/auditory_ws_handler.{h,cc}`)
- Decodes binary frames to float PCM and calls `stream_->PushAudio(...)`.
- Routes `reset` / `flush` / `end` to the controller, and forwards each `Emit`
  JSON string to the connection.

## 3. Thread Lifecycle (FR4)

- **Start**: the controller starts the diarization and ASR worker threads when
  the session opens.
- **Run**: the WebSocket read loop is the producer; the two workers are
  consumers. Neither worker reads the other's state.
- **flush**: the controller records the current `total_samples_` and waits on a
  condition variable until both workers' processed-sample counts reach it, then
  serializes the timeline. Streaming continues.
- **end**: the controller closes the buffer; each worker reads its remaining
  audio, runs its final computation, and exits; the controller joins both
  threads and serializes the final timeline. `Reset` then re-creates the workers
  for a subsequent session.
- **Ownership**: the controller is the sole owner of the thread objects; both
  threads are joined before the controller is destroyed. No thread is detached.

## 4. Concurrency Safety (Constitution V.5)

- Each shared field names its guarding lock:
  - `SharedAudioBuffer`: `mutex_` guards `samples_`, `base_sample_`,
    `total_samples_`, `cursors_`, `closed_`; `cv_` signals new data or
    end-of-stream.
  - `StreamTimeline`: its `mutex_` guards the diarization frames and transcript.
  - `AuditoryStream`: `progress_mutex_`/`progress_cv_` guard the flush wait;
    `emit_mutex_` serializes `Emit` calls from the two worker threads and the
    controller.
- Each worker keeps its own non-shared state (the diarizer object, the ASR PCM
  buffer, its read cursor); these need no lock.
- Access pattern: copy the required samples out under the buffer lock, then
  compute without holding any lock; acquire the timeline lock only to append or
  to read a snapshot. No lock is held across CUDA calls.
- **Single-GPU constraint**: both workers issue CUDA work on one GPU. On the
  Jetson unified-memory platform, a host read of device-managed memory while
  another thread has a kernel running causes a segmentation fault. Therefore each
  worker holds `gpu::DeviceLock()` (a process-wide `std::mutex`) around its
  GPU-using region. The threads still overlap their CPU-side work (buffering,
  endpointing, JSON serialization); only the GPU regions are mutually exclusive.
- Verification: run the buffer test repeatedly and confirm identical output; run
  the through-WebSocket streaming test and confirm it completes without fault and
  produces a valid timeline. Use `compute-sanitizer --tool racecheck` where
  available.


## 5. Time Base (FR3)

- The absolute sample index is the single reference. `t_sec = sample / rate`.
- Diarization frames carry `frame_period_sec`; a segment's start and end times
  are computed from the frame index and the period.
- An ASR transcript entry's start and end times are computed from the absolute
  sample offsets of the endpointed utterance. Both pipelines therefore produce
  times on the same axis without coordination between them.

## 6. Test and Tooling Design (FR4, FR8–FR10)

### 6.1 WebSocket streaming client (`tools/ws_stream_client.py`)
- Uses only the Python standard library; performs the RFC 6455 handshake.
- Reads a pre-decoded int16 mono 16 kHz PCM file (produced by `dump_pcm`) and
  sends int16 little-endian frames through the socket at a configurable rate
  (`--rate R` sends R times faster than real time; `--rate 0` sends frames
  back-to-back at the maximum rate).
- A dedicated reader thread reads every server frame, recording each incremental
  `asr` message and the final `timeline` document.
- Sends `end`, waits for the final timeline, and writes the complete message log
  to a JSON file (FR9): `{ "meta": {...}, "events": [ ...asr... ],
  "timeline": {...} }`.

### 6.2 Metrics (FR10)
- The client reports the total wall-clock processing time and the server-reported
  per-track `compute_sec` and `real_time_factor` values
  (real_time_factor = audio_sec / compute_sec).
- All numbers are labeled as streaming-path measurements under stated clock
  conditions (GPU fixed at 1.3 GHz, power mode MaxN).

### 6.3 Component test (`test/test_shared_buffer.cc`)
- A producer thread and two consumer threads with separate cursors verify that
  each consumer reads every sample exactly once and in order, and that the buffer
  removes a prefix only after both cursors pass it. The authoritative streaming
  validation is the through-WebSocket client (§6.1).

## 7. Constitution Check

- **I**: `std::thread`, the C++ standard library, and the existing POSIX
  WebSocket; no new dependency.
- **II**: no change to numerical fidelity; the pipelines are unchanged.
- **III**: two independent worker threads writing one track each into one
  comprehensive timeline.
- **IV**: validation is through the real WebSocket faster than real time, with
  the timeline written to JSON.
- **V**: each shared field names its lock; threads are owned and joined by the
  controller; the buffer test is deterministic; components are small and
  single-purpose.
- **VI**: this plan uses standard terminology and no metaphors.

## 8. Risks and Mitigations

- **Incomplete processing on `end`**: the controller closes the buffer and joins
  both workers before serializing, so all buffered audio is processed (FR7/AC4).
- **Buffer growth when input arrives faster than ASR processes it**: the buffer
  removes a prefix only after both cursors pass it; occupancy is bounded by the
  backlog of the slower pipeline.
- **Lock held across a CUDA call**: prohibited by design; samples are copied out
  before computation, and the GPU mutex covers only the GPU region.
- **Lost client messages**: a dedicated reader thread reads all server frames.

## 9. Out of Scope (per spec Non-Goals)

ASR throughput tuning, quantization, combining tracks across pipelines
(speaker-to-text attribution), and concurrent handling of multiple connections.
