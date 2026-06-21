# Spec 004 — Time Base + Comprehensive Timeline + Protocol Layer

- **Feature**: `004-comprehensive-timeline`
- **Status**: Implemented (time base + comprehensive timeline + pipeline decoupling + protocol layer). Supersedes Spec 005 (time base) and Spec 007 (protocol layer). All phases 1-12 complete.
- **Created**: 2026-06-15
- **Revised**: 2026-06-18 (merged 005 + 007; Constitution v1.3.0 alignment)
- **Owner**: project owner
- **Constitution**: v1.3.0

> WHAT the time base system, comprehensive timeline, and protocol layer must provide
> and WHY. The data structures and threading model are in `plan.md`.

---

## 1. Summary

Replace the one-shot, recompute-on-flush comprehensive view with a **native,
stateful comprehensive timeline** that is built incrementally as each pipeline
deposits results and is **revised in place** when a pipeline updates a region it
already reported. Every pipeline places its data on one **common time base**
(absolute sample index), and the correspondence between *who is speaking*
(diarization) and *what is said* (ASR) is determined purely by **time alignment**
on that timeline. Revisions are pushed to the WebSocket consumer.

The time base system is the foundational mechanism that enables all pipelines to
exist as independent units and coordinate through the comprehensive timeline.
Every registered pipeline MUST acquire its time base from the shared audio buffer
and derive all time codes from it — without exception.

The protocol layer replaces the hard-coded pipeline interfaces on
`ComprehensiveTimeline` (`AddVad`, `UpsertText`, `ReplaceSpeakers`) with a
**generic, topic-based protocol** where pipelines self-register their capabilities
and the timeline routes data by subject. The protocol treats the WebSocket data
stream as a first-class pipeline at equal status with VAD, ASR, and diarization.

The protocol has three layers:
1. **RAW logs** — per-pipeline debug output
2. **Protocol timeline** — normalized pipeline data, routed by topic
3. **Comprehensive view** — human-readable alignment of "who said what when"

Pipelines interact **only** with the protocol timeline. No direct
pipeline-to-pipeline communication.

## 1a. Core invariant — the comprehensive layer is a pure time-alignment layer

The comprehensive timeline **never modifies, infers, substitutes, or back-fills
any pipeline's content**. Each pipeline is solely responsible for its own output
content AND its own accurate relative time on the common base. The comprehensive
layer's only job is to guarantee that whatever each pipeline emits can be aligned
in time. Consequences:

- It does **not** split a pipeline's content (e.g. it does not cut ASR text to
  match speaker boundaries — that would be the timeline doing diarization's job).
- It does **not** guess a missing attribution (e.g. if diarization has not
  covered a span, the speaker is honestly "unknown" — it is never borrowed from
  the nearest segment).
- Coverage gaps are represented faithfully on the time axis (the time code is
  continuous; missing pipeline data shows as absent/unknown, not filled in).
- Each pipeline owns its own time resolution: if finer-grained attribution is
  wanted, the pipeline emits finer-grained timed units; the comprehensive layer
  does not synthesize them.

## 2. Background and Problem

### 2.1 — Comprehensive Timeline

- The current comprehensive view is computed once at flush/end from a full
  snapshot, attributing each ASR token whole to a single speaker (max time-overlap).
  With coarse ASR segments and E2E diarization showing frequent speaker switches,
  a segment that spans several speakers is mis-attributed entirely to one.
- The view is produced only at flush/end and never revised, so an early ASR
  result aligned against incomplete diarization is never corrected.

### 2.2 — Time Base

- Each pipeline derives time codes from a local counter and divides by the sample
  rate independently. Alignment is by the coincidence of all starting at 0, not
  by a shared mechanism.
- There is no run-time check that a consumer's position equals its position on
  the common clock, and no end-point reconciliation against the stream total.
- Pipelines were communicating directly (VAD pushing segments to ASR via callback
  and atomic cursor), bypassing the comprehensive timeline as the sole coordination
  mechanism.

### 2.3 — Protocol Layer

- The current `ComprehensiveTimeline` exposes hard-coded methods:
  `AddVad()`, `UpsertText()`, `ReplaceSpeakers()`. Adding a fourth pipeline
  requires editing the timeline class.
- The WebSocket transport is implicitly coupled to three pipelines. The incoming
  audio stream is invisible in the protocol model.
- There is no schema registry. Subscription is event-based but not topic-based.
- The protocol has no concept of QoS, retained state, replay, or schema evolution.

## 3. Goals

- **G1 — Common time base as a first-class contract**: every emitted datum
  carries timestamps on one absolute time base derived from the cumulative
  ingested sample count.
- **G2 — Native stateful comprehensive timeline**: a single in-memory structure
  is updated incrementally as pipelines deposit results.
- **G3 — Revision in place**: when a pipeline updates a region it already
  reported, the affected comprehensive entries are recomputed and emitted as revisions.
- **G4 — Time-alignment attribution**: "who said what" is derived from time
  overlap between speaker segments and text segments on the common time base.
- **G5 — Revisions pushed to WS**: the consumer receives revision messages.
- **G6 — VAD as an independent pipeline**: the VAD detector runs as a third
  independent consumer, publishing speech segments onto the common time base.
- **G7 — Reusable TimeBase abstraction**: one `core::TimeBase` value type for
  absolute-sample↔seconds conversion, instantiable per session, derivable for
  sub-streams.
- **G8 — Wall clock anchor**: session entry/exit wall clock for downstream
  physical-time mapping, with drift detection.
- **G9 — Topic-based registration**: pipelines declare name, produces topics,
  consumes topics, and schema. The timeline routes by topic.
- **G10 — Audio input as a pipeline**: the WS binary stream is a registered
  pipeline (`ws_input`), producing `audio/raw`.
- **G11 — Schema registry**: the protocol timeline maintains a per-topic schema.
  Schema changes are versioned.
- **G12 — MQTT-style routing**: hierarchical topics with wildcards, fan-out
  subscriptions, and "no-local" flag.
- **G13 — Zero runtime dependencies**: pure C++17. No MQTT library, no JSON
  framework, no message-passing library.

## 4. Non-Goals

- **NG1** Per-word timestamps / forced alignment (model does not emit them).
- **NG2** Changing the diarization or ASR engines' numerics.
- **NG3** GPU scheduling (Spec 002).
- **~~NG4~~** *(Promoted to in-scope — see §5.5 Session Persistence)*
- **NG5** Wall-clock time inside the pipeline. Wall clock appears only at the
  WS protocol boundary for downstream consumers.
- **NG6** Engine numerics or buffer cursor rework.
- **NG7** Changing any emitted stream time value (origin-0 integration is identity).
- **NG8** Distributed messaging. The protocol is in-process only for Phase 1.
- **NG9** Replacing the GPU scheduler, shared audio buffer, or worker threads.
- **NG10** Changing WS transport framing (binary in, text out remains).

## 5. Functional Requirements

### 5.1 — Time Base System

- **FR1.1** `core::TimeBase` provides `SecondsAt(abs)`, `SampleAt(sec)`,
  `Duration(n)`, `valid()`, and accessors; default-constructed is invalid.
- **FR1.2** `Derive(anchor_abs)` returns a child base on the same clock; for it,
  `LocalSeconds(i) == SecondsAt(anchor + i)`.
- **FR1.3** `SharedAudioBuffer::time_base()` returns the session base, and a read
  can report the absolute start of the returned span.
- **FR1.4** Every registered pipeline obtains its time base from
  `SharedAudioBuffer::time_base()` (not from `params.sample_rate`), holds it
  as a member field, and derives all time codes through it.
- **FR1.5** `ReconcileExtent(processed, common_total)` returns the signed sample
  gap; the controller can log a mismatch under a debug flag.
- **FR1.6** `AuditoryStream` records `session_start_wall_sec` (system_clock epoch
  seconds) at `Reset()`. On `EmitTimeline(finalize=true)`, it reads the exit
  wall clock and validates drift: `|exit - entry| - audio_duration <= 1s`. The
  `wall_clock_ok` flag is set accordingly.
- **FR1.7** The WS protocol includes `session_start_wall_sec` in the `ready`
  message and both `session_start_wall_sec` and `wall_clock_ok` in the
  `timeline` message.

#### Three Consistency Principles (Constitution Article III §3.2)

Every registered pipeline MUST satisfy:

- **Origin consistency**: Every pipeline derives its time origin from the shared
  audio buffer's common time base. A pipeline MUST NOT set its own origin
  independently.
- **Process consistency**: During internal processing, a pipeline MUST compute
  all time codes through `SecondsAt`, `SampleAt`, or `Duration`. Ad hoc
  arithmetic (`sample / sample_rate`) is not permitted. Sub-streams anchored at
  an arbitrary absolute sample MUST use `Derive()` and `LocalSeconds()`.
- **Result consistency**: Every datum reported to the comprehensive timeline MUST
  carry absolute start and end times on the common time base. End-of-stream
  reconciliation MUST confirm the pipeline's processed extent equals the common
  clock total.

### 5.2 — Comprehensive Timeline

- **FR2.1 — Common time base metadata**: every result message (`diar`, `asr`,
  `vad`, `timeline`, and revisions) SHALL carry absolute start/end times in
  seconds on the shared base, and the protocol SHALL state the base (sample rate
  + absolute sample origin) so a consumer can align any pipeline's output.
- **FR2.2 — Native comprehensive timeline**: the controller SHALL maintain one
  stateful comprehensive timeline updated by `ReplaceSpeakers(segs)`,
  `UpsertText(id,start,end,text)`, and `AddVad(start,end)`; it SHALL NOT rebuild
  the view from a full raw snapshot on each update.
- **FR2.3 — Diarization-driven view split**: the comprehensive view's boundaries
  SHALL come from the DIARIZATION track. Each ASR text segment SHALL be placed
  onto the diarization speaker turns it overlaps, and a text segment crossing a
  diarization boundary SHALL be SPLIT at that boundary (its characters allocated
  to each turn proportionally by time). The view SHALL NOT re-segment text by
  ASR's own coarse segmentation. Diarization SHALL NOT map text; where no
  diarization covers a span the speaker is honestly "unknown" (never borrowed).
- **FR2.4 — Revision events**: when an upsert changes a region already emitted,
  the controller SHALL emit a revision message identifying the changed time range
  and the new entries; committed-and-unchanged entries SHALL NOT be re-emitted.
- **FR2.5 — WS revision push**: revision messages SHALL be sent to the WS consumer
  as they occur (subject to the existing emit serialization).
- **FR2.6 — VAD pipeline**: the VAD detector SHALL be a third buffer consumer on
  its own thread, depositing speech segments onto the timeline; the ASR worker
  SHALL consume the continuous audio independently. Its per-window detection
  compute SHALL run on the GPU in batches.
- **FR2.7 — Final timeline**: on flush/end the server SHALL still send one
  `{"type":"timeline",...}` document (tracks + comprehensive) consistent with the
  incremental revisions already sent. The VAD speech segments SHALL be a
  serialized part of this document.
- **FR2.8 — Pipeline independence**: Pipelines communicate only through the
  comprehensive timeline (`ComprehensiveTimeline`). Direct data exchange —
  callbacks, shared pointers, atomic flags — is not permitted.

### 5.3 — Protocol Layer: Pipeline Lifecycle

- **FR3.1 — Registration**: `PipelineRegistry::Register()` accepts a
  `PipelineDescriptor` containing: `name` (string), `version` (semver string),
  `produces` (list of topic strings), `consumes` (list of topic patterns),
  `schema` (per-topic schema map), `enabled` (boolean). Returns a
  `PipelineHandle` used for all subsequent operations.
- **FR3.2 — Discovery**: `PipelineRegistry::Describe()` returns all
  registered pipelines and their capabilities.
- **FR3.3 — Deregistration**: `PipelineRegistry::Unregister(handle)` removes
  the pipeline, clears its subscriptions, and publishes a `system/pipeline/
  offline` event.
- **FR3.4 — Health**: pipelines call `PipelineHandle::Heartbeat()` periodically.
  If heartbeat exceeds `timeout_sec`, the registry publishes `system/pipeline/
  unhealthy`.
- **FR3.5 — Disabled pipelines**: `enabled=false` in the descriptor skips
  subscription wiring. Config-driven: `ORATOR_ASR_DISABLE=1` maps to `enabled=false`.

### 5.4 — Protocol Layer: Data Type System

- **FR4.1 — Semantic topics**: hierarchical strings separated by `/`.
  Standard topic prefixes:

  | Prefix | Purpose |
  |---|---|
  | `audio/` | Raw and processed audio |
  | `vad/` | Voice activity detection |
  | `asr/` | Speech recognition |
  | `diar/` | Speaker diarization |
  | `system/` | Pipeline lifecycle, telemetry |

  Current standard topics:
  - `audio/raw` — raw PCM chunks (producer: `ws_input`)
  - `vad/speech_segment` — [start, end) intervals (producer: `vad`)
  - `asr/transcript` — timed text segments (producer: `asr`)
  - `asr/transcript_partial` — in-progress partial text (producer: `asr`)
  - `diar/speaker_segment` — who/when intervals (producer: `diar`)
  - `system/pipeline/online` — pipeline registered
  - `system/pipeline/offline` — pipeline deregistered
  - `system/gpu_telemetry` — GPU scheduling snapshot

- **FR4.2 — Schema definition**: each topic has a `TopicSchema` containing:
  topic string, version (uint32), and a `Schema` (field list). Each `Field`
  has: name, type enum (`STRING`, `DOUBLE`, `FLOAT`, `INT32`, `INT64`,
  `BOOL`, `BYTES`, `LIST`, `STRUCT`), optional (boolean), and default value.
- **FR4.3 — Static schema**: schemas are registered at pipeline registration.
  Runtime schema changes require a new schema version.
- **FR4.4 — Schema evolution**: if a publisher registers a schema with a
  different version than the existing topic schema, the registry records both.
  Subscribers declare their accepted version range. Messages carry their schema
  version. Consumers outside the range receive a `system/schema_mismatch` event.

### 5.5 — Protocol Layer: Topic Routing

- **FR5.1 — Hierarchical topics**: `/`-separated levels. Two wildcards:
  `+` matches one level, `#` matches remaining levels.
- **FR5.2 — Publish**: `PipelineHandle::Publish(topic, message, qos)` routes to
  all subscribers whose pattern matches the topic.
- **FR5.3 — Subscribe**: `PipelineHandle::Subscribe(pattern, qos, no_local)`
  adds a subscription. `no_local=true` prevents the subscriber from receiving
  its own published messages.
- **FR5.4 — Fan-out**: multiple subscribers to the same pattern all receive the
  message. Dispatch order is not guaranteed.
- **FR5.5 — Unsubscribe**: `PipelineHandle::Unsubscribe(sub_id)` removes the
  subscription.

### 5.6 — Protocol Layer: Time Base Binding

- **FR6.1 — Timestamped messages**: every message has a `timestamp_sec` field
  (stream seconds on the common time base). The protocol timeline rejects
  messages with `timestamp_sec < 0`.
- **FR6.2 — Time-base provenance**: REMOVED per owner feedback. `timebase_id`
  provides no anti-forgery value. Pipelines inherit time base from the shared
  audio buffer, which is sufficient for in-process communication. If IPC is
  needed in the future, a proper security model will be designed.
- **FR6.3 — Wall-clock metadata**: messages MAY carry `wall_clock_sec` (absolute
  physical time). This is optional and used only for post-hoc correlation.
- **FR6.4 — Time ordering**: messages within a topic are appended in
  timestamp order. The protocol timeline accepts out-of-order messages but
  stores them sorted. A `system/out_of_order` event is published if a message's
  timestamp is earlier than the last stored timestamp for the topic.

### 5.7 — Protocol Layer: QoS Semantics

- **FR7.1 — QoS 0 (at-most-once)**: fire-and-forget. Used for high-rate data.
- **FR7.2 — QoS 1 (at-least-once)**: ACK-based. Used for `asr/transcript`,
  `diar/speaker_segment`.
- **FR7.3 — QoS 2 (exactly-once)**: dedup window. Used for checkpoints.
- **FR7.4 — Per-subscription QoS**: QoS is negotiated at subscription time.
  The timeline routes at the MINIMUM of publisher-requested and
  subscriber-requested QoS.

### 5.8 — Protocol Layer: Retained / Replay / Storage

- **FR8.1 — Retained per topic**: each topic maintains a pointer to the last
  message published to it. When a new subscriber subscribes, the timeline sends
  the retained message first.
- **FR8.2 — Replay from timestamp**: `Replay(topic, from_sec)` returns all
  messages for the topic from `from_sec` onward.
- **FR8.3 — Subscriber cursor**: each subscription maintains an independent
  cursor (`last_consumed_sec`). Multiple subscribers have independent cursors.
- **FR8.4 — Automatic reclamation**: messages are reclaimed when ALL subscriber
  cursors have passed them AND they exceed the retention window.
- **FR8.5 — Storage backends**: MEMORY (128 MB ring buffer) and DISK (mmap files
  in configurable path). `audio/raw` writes directly to DISK. Small payloads
  (VAD, ASR, system) stay in MEMORY. No promotion policy in Phase 1 — data
  lands in the correct backend on first write.
- **FR8.6 — Configurable DISK path**: `ORATOR_STORAGE_DISK_PATH` environment
  variable or `--storage-disk-path` CLI flag. Default: `/tmp/orator/storage/`.
- **FR8.7 — Retention per topic**:

  | Topic | Retention | Backend |
  |---|---|---|
  | `audio/raw` | 30s–300s | DISK |
  | `vad/speech_segment` | 5s–30s | MEMORY |
  | `asr/transcript` | 30s–120s | MEMORY |
  | `asr/transcript_partial` | 5s–30s | MEMORY |
  | `diar/speaker_segment` | 30s–120s | MEMORY |
  | `system/*` | 0s | MEMORY |

### 5.9 — Protocol Layer: WS Serialization

- **FR9.1 — Topic-based envelope**: every WS JSON message uses the topic-based
  envelope with `topic`, `pipeline`, `pipeline_version`, `msg_id`, `ts`,
  `qos`, `schema_version`, `data`.
- **FR9.2 — Backward compatibility**: the WS handler recognizes both the legacy
  format (`{"type":"vad",...}`) and the new topic-based format. Legacy messages
  are translated to topic-based internally. The `ready` message includes
  `"protocol_version": 2`.
- **FR9.3 — Schema discovery**: `{"cmd":"describe"}` returns the full topic map,
  schemas, and registered pipelines.
- **FR9.4 — Control messages**: control commands (`reset`, `flush`, `end`)
  remain as text frames. They are not part of the topic protocol.

### 5.10 — Session Persistence (ex-NG4)

- **FR10.1 — Auto-save on reset**: when `AuditoryStream::Reset()` is called
  (client resets or session ends), the current timeline is serialized and saved
  to a session store on disk before any state is cleared.
- **FR10.2 — Session metadata**: each saved session records a unique session ID,
  wall-clock timestamp, audio duration, ASR segment count, diarization segment
  count, and the full timeline JSON.
- **FR10.3 — Session listing**: a client can request a list of historical
  sessions via `{"cmd":"sessions"}` and receive metadata for each (no timeline
  payload — lightweight).
- **FR10.4 — Session load**: a client can request a specific session's timeline
  via `{"cmd":"load_session","session_id":"<id>"}` and receive the full timeline
  as a `timeline`-type message.
- **FR10.5 — Configurable storage path**: the session store directory defaults
  to `$ORATOR_STORAGE_DISK_PATH/sessions/` and can be overridden via
  `ORATOR_SESSION_DIR` env var. When `ORATOR_STORAGE_DISK_PATH` is empty,
  session persistence is disabled.
- **FR10.6 — No data loss on unclean shutdown**: sessions are saved
  synchronously on Reset(). For unclean shutdown (SIGKILL, crash), the last
  session's data is lost — acceptable for Phase 1.

## 6. Acceptance Criteria

- **AC1** Streaming `test.mp3` produces a final timeline whose comprehensive view,
  on a multi-speaker span, attributes text to the time-overlapping speaker(s)
  correctly. (G4, FR2.3)
- **AC2** The comprehensive timeline is built incrementally: an update touches
  only the affected region, not a full rebuild. (G2, FR2.2)
- **AC3** A diarization or ASR update to an already-reported region produces a
  revision message with the changed time range. (G3, FR2.4)
- **AC4** Revision messages are delivered over the real WS path and a client can
  reconstruct the same final comprehensive view by applying them in order. (G5)
- **AC5** The VAD detector runs as a third independent consumer/thread and
  emits `vad` speech segments on the common time base. (G6, FR2.6)
- **AC6** Every result message carries common-time-base metadata sufficient to
  align it without reference to other messages. (G1, FR2.1)
- **AC7** Build clean under `-Wall -Wextra`; tests pass; threaded path race-free.
- **AC8** `test_time_base`: conversions exact; `Derive`/`LocalSeconds` map a
  sub-stream's local samples onto the common clock. (FR1.1, FR1.2)
- **AC9** 120 s + 600 s comprehensive output is identical before/after the
  consumer integration (origin-0 identity). (FR1.4)
- **AC10** With the reconciliation check enabled, a clean run reports zero gap.
  (FR1.5)
- **AC11** `ready` message contains `session_start_wall_sec`; `timeline` message
  contains `session_start_wall_sec` and `wall_clock_ok`. (FR1.6, FR1.7)
- **AC12** No direct data exchange between pipelines: no `set_vad_update`
  callback, no `vad_cursor_pos_` atomic, no `asr_worker_->vad_segments()`
  write from the VAD thread. (FR2.8)
- **AC13** Three existing pipelines (VAD, ASR, diarization) register with the
  protocol timeline using `PipelineDescriptor`, publish to their topics, and
  produce output identical to the current `ComprehensiveTimeline` on a 120 s
  streaming run. (FR3.1, FR5.2, FR9.1)
- **AC14** A new test pipeline registers, publishes to a custom topic, and is
  discoverable via `Describe()`. No changes to `ComprehensiveTimeline` are
  required. (FR3.1, FR3.2)
- **AC15** Topic wildcards work: subscribing to `vad/*` receives
  `vad/speech_segment`; subscribing to `system/#` receives all system events.
  (FR5.1)
- **AC16** Messages are time-ordered per topic. Out-of-order messages are stored
  sorted and a `system/out_of_order` event is published. (FR6.4)
- **AC17** Replay from timestamp returns correct messages. Retention window
  reclaims messages correctly. (FR8.2, FR8.4)
- **AC18** Full 1-hour streaming run produces output byte-identical to the
  current `ComprehensiveTimeline` final timeline. (FR9.2)
- **AC19** After a `reset`, the session store contains a non-empty session file
  with the timeline from the just-completed session. A subsequent `sessions`
  command lists that session, and `load_session` returns its timeline JSON.
  (FR10.1–10.4)

## 7. Constitution Check

- **Art. I (no deps)**: pure C++17. Topic routing, schema registry, ring
  buffers, time index, and QoS use standard containers only. No JSON library;
  existing `JsonEscape` is reused. No MQTT client.
- **Art. II (accuracy)**: no change to ASR, diarization, or VAD numerics.
  Protocol layer is transport.
- **Art. III (common time base + pipeline independence)**: reinforced.
  Origin/process/result consistency enforced. Pipelines communicate only through
  the protocol timeline (topic routing). No direct pipeline-to-pipeline paths.
- **Art. IV (streaming validation)**: validated through the real WS path.
  Legacy and new formats produce identical output.
- **Art. V (quality)**: strict layering. One responsibility per type.
  RAII handles. Thread-safe routing.
- **Art. VI (terminology)**: standard terms — topic, schema, QoS, retained,
  cursor, ring buffer, fan-out, no-local.
- **Art. VII (SDD)**: spec → plan → tasks before implementation.
- **Art. VIII (doc-code consistency)**: this spec describes new types and
  interfaces that do not yet exist (protocol layer). After implementation,
  status advances to `Implemented` with commit references.

## 8. Trade-off Analysis

### Topic-based vs. direct method calls
**Decision**: topic-based routing.
**Trade-off**: ~300 ns overhead per message vs. a direct virtual call.
Negligible compared to GPU latency (~5 ms per VAD batch, ~300 ms per ASR chunk).
The gain is extensibility: a new pipeline adds topics, not methods.

### Static schema at registration vs. dynamic schema
**Decision**: static schema at registration time.
**Trade-off**: dynamic schema would allow evolution without re-registration.
But schema validation at the boundary prevents silent data corruption.
Static schema with versioning provides evolution with safety.

### Centralized storage vs. per-topic buffers
**Decision**: centralized storage with time index.
**Trade-off**: `Publish()` does one extra storage write before dispatch.
Pipelines are storage-agnostic. Replay, retention, and backend selection
are managed by the timeline. Single source of truth.

### In-process protocol vs. IPC-ready
**Decision**: in-process only for Phase 1.
**Trade-off**: future IPC requires a transport abstraction layer.
The data model is already IPC-compatible. A socket layer can be swapped in later.

### DISK direct-write vs. MEMORY-first with promotion
**Decision**: direct-write to correct backend on first write.
**Trade-off**: no promotion policy complexity in Phase 1. `audio/raw` goes
straight to DISK. Small payloads stay in MEMORY. Promotion is a Phase 2 option.

### `timebase_id` removed
**Decision**: no per-message time-base provenance.
**Trade-off**: `timebase_id = Hash(sample_rate, origin_sample)` provides no
anti-forgery value (public hash, no secret key). Pipelines inherit time base
from the shared audio buffer, which is sufficient for in-process communication.

## 9. Open Questions

- **O1** Should the protocol timeline replace `ComprehensiveTimeline` entirely,
  or coexist during a transition period?
  **Owner decision**: one-shot refactoring. No coexistence. Replace `AddVad`,
  `UpsertText`, `ReplaceSpeakers` with protocol-based pipeline registration.

- **O2** How does `audio/raw` (binary PCM) coexist with JSON-over-WS?
  **Recommendation**: `audio/raw` is an internal protocol topic only. It does not
  cross the WS boundary. WS output remains JSON.

- **O3** Should the schema system support `BYTES` for binary data (future video,
  model weights)?
  **Recommendation**: yes. `BYTES` is a schema type. WS transport maps it to
  base64 for JSON or a separate binary frame.

---

## A. Merged Specs (deleted 2026-06-18)

- **Spec 005** (Reusable Common Time Base): content merged into Section 5.1. Directory deleted.
- **Spec 007** (Pipeline Protocol Layer): content merged into Sections 5.3–5.9. Directory deleted.
