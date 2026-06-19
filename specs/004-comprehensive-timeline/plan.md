# Plan 004 — Common Time Base + Comprehensive Timeline + Protocol Layer

- **Feature**: `004-comprehensive-timeline`
- **Spec**: [spec.md](spec.md)
- **Status**: Implemented (Phases 1–12: time base, timeline, VAD, protocol layer).
- **Constitution**: v1.3.0

> HOW to satisfy [spec.md](spec.md). Standard terminology.
>
> Note: this plan is retained as implementation history. Where this file uses
> legacy wording such as "endpoint" markers or max-overlap-only projection,
> the authoritative runtime behavior is the current code + spec: `vad` speech
> segments, diarization-driven split in the comprehensive view, and ASR
> stable-`text_id` in-place revisions.

---

## 1. Data model (native, stateful)

A new `ComprehensiveTimeline` (pipeline layer) owns the comprehensive view as a
living structure on the common time base (seconds; origin = absolute sample 0).

State:
- `speakers_`: time-ordered speaker segments `{start, end, speaker, conf}`
  (who/when), from diarization. Overlaps allowed (E2E diarization can overlap).
- `texts_`: text segments `{id, start, end, text}` (what/when), from ASR, keyed
  by a stable `id` so a later update to the same utterance revises in place.
- `endpoints_`: sorted endpoint times (markers; informational).
- `view_`: the derived comprehensive entries `{start, end, speaker, text}`,
  ordered by time — the projection kept in sync with the inputs.
- `committed_until_`: time up to which `view_` has been emitted to the client
  (so revisions only cover changed regions).

## 2. Attribution by time alignment (G4, FR3)

Diarization gives who/when; ASR gives what/when; correspondence is time overlap.

`Project(text_seg)`:
- Find the speaker segment with the greatest time overlap with `text_seg`.
- Emit one comprehensive entry `{text_seg.start, text_seg.end, that_speaker,
  text_seg.text}`. If no overlap, speaker = "unknown" (nearest by midpoint, as
  today) until diarization covers it (then a revision fixes it).
- Consecutive entries with the same speaker are coalesced for the snapshot view.

Rationale: ASR text segments are produced at speech endpoints (Spec 003), so they
are mostly single-speaker; attribution is by time and is **revisable**, so an
early attribution against incomplete diarization is corrected when diarization
covers the span. No per-word timestamps are needed (NG1). A text segment that
genuinely spans multiple speakers is attributed to its max-overlap speaker
(default, R1); a future time-split projection can replace this without changing
the contract.

## 3. Incremental update + revision (G2, G3, FR2, FR4)

Each upsert returns the set of revisions to push.

`UpsertSpeaker(start,end,spk,conf)`:
1. Insert/merge into `speakers_`.
2. Affected range = `[start,end)`. Re-project only the `texts_` overlapping it.
3. For each re-projected text entry whose attributed speaker changed vs the
   stored `view_` entry, update `view_` and add it to the revision set.

`UpsertText(id,start,end,text)`:
1. Insert or replace the `texts_` entry with this `id`.
2. Re-project that one segment; if its `view_` entry is new or changed, add it to
   the revision set.

`MarkEndpoint(t)`: append to `endpoints_` (no view change by itself).

A revision is emitted only when the **attributed result** changes (not on every
raw update), bounding revision volume (R2). Out-of-order updates converge because
each upsert re-projects the affected region from current state (R3).

## 4. Wiring (controller)

`AuditoryStream`:
- Replace the one-shot `OverlapTimelineMerger` call in `Serialize()` with a
  snapshot of `ComprehensiveTimeline::view_`. Keep the `tracks` array as today
  (raw per-pipeline entries) plus the now-native `comprehensive`.
- Diarization worker → on each finalized speaker segment, call `UpsertSpeaker`
  and push returned revisions via the existing `EmitLocked`.
- ASR worker → on each committed text segment, call `UpsertText(id=...)` and push
  revisions. (The worker already emits `{"type":"asr",...}`; the comprehensive
  revision is additional.)
- **Endpoint pipeline (G6, FR6)**: a third `buffer_.AddConsumer()` + thread runs
  `AsrSileroVad` purely as an endpoint detector (continuous audio, no trim),
  calling `MarkEndpoint` and emitting `{"type":"endpoint","time":...}`. The ASR
  worker keeps its own reset for now (R4: the shared endpoint stream is
  informational; a later step can drive ASR reset from it to remove duplication).

## 5. WS protocol additions (G1, G5, FR1, FR5)

Additive only (schema stays compatible, AC7):
- A session-open metadata field `{"type":"meta","sample_rate":R,"time_base":
  "absolute_samples","origin_sample":0}` so a consumer knows the common base.
- `{"type":"endpoint","time":T}` markers.
- `{"type":"revision","dirty_start":S,"dirty_end":E,"entries":[{start,end,speaker,
  text}...]}` — replace the consumer's comprehensive entries in `[S,E)` with
  `entries`.
- All existing messages already carry `start`/`end` on the common base (FR1 met
  for them); confirm and document.

## 6. Validation

- **Unit (AC2, AC3)**: `test_comprehensive_timeline` — drive `UpsertSpeaker` /
  `UpsertText` out of order; assert the projection is correct, an update touches
  only the affected region, and a changed attribution yields one revision with
  the right dirty range; an unchanged update yields none.
- **Multi-speaker (AC1)**: a scripted case where a text segment overlaps two
  speaker segments asserts attribution to the max-overlap speaker, and a
  later-arriving speaker update flips it with a revision.
- **WS (AC4)**: stream `test.mp3`; collect revisions; a client applying them in
  order reproduces the final comprehensive view.
- **Endpoint (AC5)**: confirm the third thread emits endpoint markers and the
  other two pipelines complete if it is disabled.
- **Build/tests (AC7)**: `ctest` green, `-Wall -Wextra` clean, race-free.

## 7. Risks and Mitigations

- **R1 multi-speaker projection** → max-overlap + revisable default; time-split
  deferred (needs no word ts to swap in later).
- **R2 revision volume** → emit only when attributed result changes.
- **R3 out-of-order** → re-project affected region from current state each upsert.
- **R4 endpoint vs ASR reset** → endpoint stream informational first; unify later.

## 8. Out of Scope

Per-word timestamps, engine numerics, GPU scheduling, cross-session persistence.

---

## Protocol Layer (Phases 7–12)

> One-shot refactoring: replaces hard-coded `AddVad`/`UpsertText`/`ReplaceSpeakers`
> with protocol-based pipeline registration. No coexistence period (O1 decision).
>
> **Architecture**: new `include/protocol/` and `src/protocol/` directories.
> `ComprehensiveTimeline` is replaced by `ProtocolTimeline` which wraps the
> existing container logic behind a topic-based interface.

### Phase 7 — Data Type System (FR4.1–FR4.4)

**New files**: `include/protocol/schema.h`, `include/protocol/topic.h`

**`include/protocol/topic.h`**:
- `Topic` value type — hierarchical string (`std::string`) with level parsing
- `TopicPattern` — supports `+` (single-level) and `#` (multi-level) wildcards
- `TopicPattern::Matches(Topic const&)` — wildcard matching
- Standard topics as constexpr: `kAudioRaw`, `kVadSpeechSegment`, `kAsrTranscript`,
  `kAsrTranscriptPartial`, `kDiarSpeakerSegment`, `kSystemPipelineOnline`,
  `kSystemPipelineOffline`, `kSystemGpuTelemetry`

**`include/protocol/schema.h`**:
- `FieldType` enum: `STRING`, `DOUBLE`, `FLOAT`, `INT32`, `INT64`, `BOOL`, `BYTES`,
  `LIST`, `STRUCT`
- `Field` struct: `name`, `type`, `optional`, `default_value`
- `Schema` struct: `std::vector<Field> fields`
- `TopicSchema` struct: `topic`, `version` (uint32), `Schema schema`
- `SchemaRegistry` class: maps topic → `TopicSchema`; supports version tracking

### Phase 8 — Pipeline Registry (FR3.1–FR3.5)

**New files**: `include/protocol/pipeline_registry.h`, `src/protocol/pipeline_registry.cc`

**`PipelineDescriptor`**: `name`, `version`, `produces` (topic list), `consumes`
(topic pattern list), `schema` (topic → `TopicSchema` map), `enabled`

**`PipelineRegistry`**:
- `Register(descriptor) → PipelineHandle` — creates handle, wires subscriptions,
  publishes `system/pipeline/online`
- `Unregister(handle)` — removes pipeline, clears subscriptions, publishes
  `system/pipeline/offline`
- `Describe() → std::vector<PipelineDescriptor>` — discovery
- `Heartbeat(handle)` — updates last-seen timestamp
- `HealthCheck(timeout_sec)` — returns list of unhealthy pipelines
- Disabled pipelines (`enabled=false`) skip subscription wiring

**`PipelineHandle`**: RAII handle. Destructor auto-unregisters. Methods:
`Publish()`, `Subscribe()`, `Unsubscribe()`, `Heartbeat()`

### Phase 9 — Topic Routing Engine (FR5.1–FR5.5)

**New files**: `include/protocol/topic_router.h`, `src/protocol/topic_router.cc`

**`TopicRouter`**:
- Internal: `std::unordered_map<SubscriptionId, Subscription>` where `Subscription`
  contains `pattern`, `pipeline_handle`, `qos`, `no_local`
- `Subscribe(pattern, pipeline, qos, no_local) → SubscriptionId`
- `Unsubscribe(sub_id)`
- `Route(topic, publisher_handle, qos) → std::vector<Delivery>` — returns list of
  (subscription, effective_qos) pairs for all matching subscribers
- Wildcard matching: `+` matches one level, `#` matches remaining levels
- Fan-out: all matching subscriptions receive the message
- `no_local` filtering: publisher doesn't receive its own messages

### Phase 10 — Time Index + Storage Backends (FR6.1–FR6.4, FR8.1–FR8.7)

**New files**:
- `include/protocol/time_index.h`, `src/protocol/time_index.cc`
- `include/protocol/storage.h`, `src/protocol/storage.cc`
- `include/protocol/memory_backend.h`, `src/protocol/memory_backend.cc`
- `include/protocol/disk_backend.h`, `src/protocol/disk_backend.cc`

**`Message`** (in `storage.h`):
- `msg_id` (uint64), `topic`, `pipeline`, `pipeline_version`, `timestamp_sec`,
  `wall_clock_sec` (optional), `qos`, `schema_version`, `data` (std::string),
  `payload_bytes` (std::vector<uint8_t> for binary)

**`TimeIndex`**:
- `std::map<Topic, std::vector<IndexedMessage>>` where `IndexedMessage` contains
  `timestamp_sec`, `storage_ref` (backend + offset)
- `Append(topic, message) → void` — sorted insert by timestamp
- `Replay(topic, from_sec) → std::vector<Message>` — range query
- `Last(topic) → Message*` — retained message per topic
- Publishes `system/out_of_order` when timestamp < last stored for topic

**`StorageManager`**:
- Owns `MemoryBackend` + `DiskBackend`
- `Write(topic, message) → StorageRef` — routes to correct backend by topic config
- `Read(StorageRef) → Message`
- Per-topic retention config (from spec §5.8 table)

**`MemoryBackend`**:
- 128 MB ring buffer (`std::vector<uint8_t>` + head/tail pointers)
- `Write(data) → offset`, `Read(offset, size) → data`
- Automatic eviction on overflow (FIFO)

**`DiskBackend`**:
- mmap-backed file in configurable path (`ORATOR_STORAGE_DISK_PATH` or
  `--storage-disk-path`, default `/tmp/orator/storage/`)
- Per-session file: `{session_id}.dat`
- `Write(data) → file_offset`, `Read(offset, size) → data`
- Retention: time-based window per topic

### Phase 11 — ProtocolTimeline (integrates all layers)

**New files**: `include/protocol/protocol_timeline.h`, `src/protocol/protocol_timeline.cc`

**`ProtocolTimeline`** replaces `ComprehensiveTimeline` as the pipeline interface:
- Owns: `PipelineRegistry`, `TopicRouter`, `TimeIndex`, `StorageManager`
- `RegisterPipeline(descriptor) → PipelineHandle`
- `PipelineHandle::Publish(topic, message, qos)`:
  1. Validate timestamp >= 0
  2. Write to storage (via `StorageManager`)
  3. Update time index
  4. Route to subscribers
  5. Update retained message
- `PipelineHandle::Subscribe(pattern, qos, no_local) → SubscriptionId`
- `Replay(topic, from_sec) → std::vector<Message>`
- `Describe() → json` — full topic map, schemas, pipelines
- Internal subscriber: `ComprehensiveTimeline` subscribes to `audio/raw`,
  `vad/speech_segment`, `asr/transcript`, `asr/transcript_partial`,
  `diar/speaker_segment` — routes to existing container methods

**Migration path**: `AuditoryStream` creates a `ProtocolTimeline` instead of
`ComprehensiveTimeline`. Worker threads get `PipelineHandle`s. The existing
`ComprehensiveTimeline` container becomes an internal subscriber to the protocol
timeline — its methods are called from the routing callback.

### Phase 12 — WS Protocol v2 + One-Shot Refactor (FR9.1–FR9.4)

**Modified files**:
- `src/net/auditory_ws_handler.cc` — add topic-based envelope, backward compat
- `src/pipeline/auditory_stream.cc` — wire protocol timeline
- `src/pipeline/asr_worker.cc` — publish via `PipelineHandle`
- `src/pipeline/diarization_worker.cc` — publish via `PipelineHandle`
- `src/pipeline/auditory_stream.cc` (VAD thread) — publish via `PipelineHandle`
- `src/ws_main.cc` — add `--storage-disk-path` flag

**WS topic-based envelope** (FR9.1):
```json
{
  "topic": "asr/transcript",
  "pipeline": "asr",
  "pipeline_version": "1.0.0",
  "msg_id": 12345,
  "ts": 42.5,
  "qos": 1,
  "schema_version": 1,
  "data": { "start": 40.0, "end": 45.0, "text": "..." }
}
```

**Backward compatibility** (FR9.2):
- WS handler recognizes both legacy (`{"type":"vad",...}`) and new formats
- Legacy messages translated to topic-based internally
- `ready` message includes `"protocol_version": 2`
- AC18: full 1-hour run produces byte-identical final timeline

**`describe` command** (FR9.3):
- `{"cmd":"describe"}` returns full topic map, schemas, registered pipelines

**One-shot refactor**:
- Remove `AddVad()`, `UpsertText()`, `ReplaceSpeakers()` from public API
- Workers receive `PipelineHandle` at construction
- `ComprehensiveTimeline` becomes internal subscriber (not public pipeline interface)
- `StreamTimeline` removed (superseded by protocol timeline)

### Protocol Layer Risks

- **R5 one-shot refactoring risk** — replacing the public API in one shot means
  all workers must be updated simultaneously. Mitigation: the existing
  `ComprehensiveTimeline` container is preserved as an internal subscriber; only
  the wiring changes. Unit tests for the container remain valid.
- **R6 topic routing overhead** — ~300 ns per message vs. direct virtual call.
  Negligible vs. GPU latency (~5 ms VAD batch, ~300 ms ASR chunk).
- **R7 DISK backend complexity** — mmap on Jetson with configurable path.
  Mitigation: Phase 1 uses MEMORY backend only; DISK is Phase 2.
- **R8 WS backward compatibility** — legacy and new formats must coexist during
  client migration. Mitigation: WS handler translates legacy → topic-based
  internally; `protocol_version` in `ready` message signals support.

### Protocol Layer Out of Scope

- Distributed messaging (IPC, network transport) — in-process only (NG8)
- Dynamic schema evolution at runtime — static schema with versioning
- QoS 1/2 ACK/dedup — QoS 0 sufficient for in-process (FR7.1)
- Replacing GPU scheduler, shared audio buffer, or worker threads (NG9)
- Changing WS transport framing (NG10)
