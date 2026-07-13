# Tasks 004 — Common Time Base + Comprehensive Timeline + Protocol Layer

- **Feature**: `004-comprehensive-timeline`
- **Spec**: [spec.md](spec.md) · **Plan**: [plan.md](plan.md)
- **Status**: Phases 1–6 Implemented. Phases 7–12 (protocol layer) Implemented. Phase 13 (session persistence): T130–T135 Implemented; browser reload revalidated under Spec 013 on 2026-07-13.
- **Constitution**: v1.3.0

> Ordered, independently verifiable steps. Each phase builds + tests before the
> next. Schema changes are additive only.

---

## Phase 1 — Native comprehensive timeline core (the foundation)
- [x] **T010** Add `ComprehensiveTimeline` (`include/pipeline/comprehensive_timeline.h`,
  `src/pipeline/comprehensive_timeline.cc`): state per plan §1; `UpsertSpeaker`,
  `UpsertText`, `MarkEndpoint`, `Snapshot`, returning a `Revision` set. Time-
  alignment attribution (plan §2), incremental re-projection of only the affected
  region (plan §3). No WS / no threads yet. *(Done 3159b75; compiles clean.)*
- [x] **T011** `test_comprehensive_timeline`: out-of-order upserts project
  correctly; an update touches only the affected region; a changed attribution
  yields one revision with the right dirty range; an unchanged update yields none;
  a multi-speaker text segment attributes to max-overlap and flips on a later
  speaker update. *(Done 3159b75; ctest green.)*

## Phase 2 — Controller wiring (replace one-shot merger)
- [x] **T020** `AuditoryStream` owns a `ComprehensiveTimeline`; diarization
  finalized segments call `UpsertSpeaker`, ASR committed segments call
  `UpsertText(id)`. `Serialize()` reads the native `view_` for the
  `comprehensive` array. *(Done 673f95d. The one-shot `OverlapTimelineMerger` was
  later fully removed in 2414128.)*
- [x] **T021** Push revisions: returned revisions are emitted via `EmitLocked` as
  `{"type":"revision",...}`. *(Done 673f95d; revisions appear on the WS path.)*

## Phase 3 — Common time base metadata + VAD pipeline
- [x] **T030** WS meta: on session open send the common-base declaration
  (`sample_rate`, `time_base`, `origin_sample`) in the `ready` message; all
  messages carry common-base start/end. *(Done 673f95d; AC6.)*
- [x] **T031** VAD pipeline: a third `buffer_.AddConsumer()` + thread runs
  the VAD detector (continuous, no trim), calls `AddVad`, emits
  `{"type":"vad","start","end"}`. ASR worker unchanged. *(Phase 3 wired the thread
  + emission (673f95d), AC5 met. Phase 5 completed it: the detector is now the
  batched GPU `GpuVad` (FR6) and endpoints are serialized into the timeline (FR7).)*

## Phase 4 — Validation
- [x] **T040** Multi-speaker attribution on test.mp3 (real diarizer + incremental
  ASR): a speaker-change span is attributed correctly, not wholly to one speaker.
  *(Done; AC1 end-to-end — head "unknown", spk0/1/2 present.)*
- [x] **T041** WS revision replay: stream test.mp3, collect revisions, apply in
  order, reproduce the final comprehensive view. *(Done; AC4 via ws_raw_capture.)*
- [x] **T050** Full build + `ctest` green under `-Wall -Wextra`; threaded path
  race-checked; schema additive-only. *(Done; AC7.)*
- [x] **T051** Update `/memories/repo/` + `PROJECT_STATE.md`; commit. *(Done.)*

## Phase 5 — Complete the VAD pipeline (GPU detector + serialized track) + cleanup
> Closes the FR6/FR7 gap left by T031 and folds in FR8/FR9. Each task builds +
> tests before the next; the final gate is the real-WS full-hour run.
- [x] **T060** Detector numeric reference. The CPU `AsrSileroVad` is the
  reference of record for the VAD detector (it produced every validated
  run's endpoints); a PyTorch silero-vad hub dump is not reproducible in this
  offline environment, so the gate is GPU-vs-CPU equivalence on identical fp32
  weights. Added `AsrSileroVad::DebugWindowProbs` exposing per-window
  probabilities. *(Done; reference established.)* (FR8)
- [x] **T061** GPU endpoint detector: ported the per-window compute (STFT via the
  fixed conv basis, the 4-layer conv encoder + ReLU, the LSTM cell step, the
  linear + sigmoid) to CUDA in `GpuVad` (`include/pipeline/gpu_vad.h`,
  `src/pipeline/gpu_vad.cu`), driven in BATCHES — one buffered read is processed
  as a single batched GPU pass over all ready windows; the LSTM recurrence is one
  scan over the batch in shared memory. Runs under `gpu::DeviceLock()`. *(Done;
  `test_vad` gate: GPU vs CPU max |dprob| = 3.7e-8, tol 2e-3; AC10.)* (FR6, FR8)
- [x] **T062** Wired the GPU detector into the VAD thread, replacing the
  CPU detector (`endpoint_vad_` is now a `GpuVad`; `DrainEndpoints` batches the
  buffered read). *(Done; endpoint markers still emitted on the common base.)* (FR6)
- [x] **T063** Serialized VAD speech segments into the timeline document: an
  additive `{"kind":"vad","source":"silero_gpu"}` track in `Serialize()` (pure data
  track — does NOT alter the ASR/diar tracks); replaced the write-only endpoint
  accessor with `SnapshotVad()`. *(Done; final timeline carries `vad` alongside
  diarization + asr tracks; AC9; schema additive.)* (FR7)
- [x] **T064** Dead-code cleanup (FR9): removed the stub models
  (`StubAsr`/`StubDiarizer`/`StubEmbedder` + headers + registrations + CMake) and
  SIX never-launched kernels (the audit caught 4 — `RelAttnKernel`,
  `FlattenLinearKernel`, `GeluKernel`/`Conv2dKernel` in `asr_audio_tower.cu` — plus
  `LinearKernel`/`LinearTiledKernel`/`PointwiseConvKernel` in `conformer_layer.cu`
  and `LinearKernel`/`LinearTiledKernel`/`AttnKernel` in `sortformer_decoder.cu`)
  and an unused variable in `asr_text_decoder.cu`; made `AsrWorker`'s Silero load
  lazy (default incremental path no longer loads it); fixed the stale
  `asr_worker.h` "energy VAD" comment. *(Done; build now ZERO warnings; 19/19
  ctest; AC11.)*
- [x] **T065** Real-WS full-hour validation: streamed the full 3615 s `test.mp3`
  through the production `orator_ws` path at the default config. ASR covered the
  whole hour (0 → 3615.1 s, 151 segments, ZERO discontinuities); the timeline
  carries the endpoint track (1454) with diarization (1083) + asr (151);
  reconcile clean; CER 16.2% (unchanged, measured on Jetson Orin). GPU stayed busy (59.7%, mean 38%, max
  100%, longest idle 30 s — no CPU-only stall). *(Done; AC8, AC9 on the real
  transport.)*
- [x] **T066** Updated `tasks.md` checkboxes, spec status → Implemented,
  `/memories/` and `PROJECT_STATE.md` with commit refs. *(Done; Constitution
  Art. VIII.)*

## Phase 6 — ASR self-revision convergence hardening
- [x] **T067** Stable `text_id` ownership moved to `AsrWorker` so in-segment ASR
  self-revisions upsert the SAME id and revise in place; id advances only when a
  segment closes. Removed obsolete controller-side id state.
- [x] **T068** Added interleaved ASR+diar convergence test
  (`test_comprehensive_timeline`) to assert order-independent final view under
  out-of-order diar updates and ASR in-segment text revisions; validated with
  real WS capture showing `asr_partial` + source-tagged `revision` events.
- [x] **T069** Milestone-gate full-hour real-WS revalidation on the stable-`text_id`
  code (2026-06-17): full 3615.1 s through `orator_ws` at the default config.
  ASR covered the whole hour (151 segments, zero discontinuities); ASR
  self-revision fired in place for 149/151 `text_id`s; revised text landed in
  the comprehensive view (151/151, 0 missing); timebase reconcile clean; GPU
  active-window busy 55.5% (longest idle 12 s, no CPU-only stall); CER 16.2% (measured on Jetson Orin)
  (unchanged — no accuracy regression). Evidence: `/tmp/fullhour_validate.py`,
  `/tmp/fullhour_timeline.json`.

## Traceability (Phases 1–6)

| Requirement | Tasks |
|---|---|
| FR1 common-base metadata | T030 |
| FR2 native timeline | T010, T020 |
| FR3 time-alignment attribution | T010, T040 |
| FR4 revision events | T010, T021 |
| FR5 WS revision push | T021, T041 |
| FR6 VAD pipeline | T031, T061, T062 |
| FR7 final timeline | T020, T063 |
| FR8 endpoint numeric gate | T060, T061 |
| FR9 dead-code removal | T064 |

| Acceptance | Tasks |
|---|---|
| AC1 multi-speaker correct | T011, T040 |
| AC2 incremental | T011 |
| AC3 revision on change | T011, T021 |
| AC4 WS replay | T041 |
| AC5 VAD pipeline | T031 |
| AC6 common-base meta | T030 |
| AC7 build/tests/schema | T050 |
| AC8 GPU detector, no stall | T061, T065 |
| AC9 VAD in final timeline | T063, T065 |
| AC10 GPU vs oracle numeric gate | T060, T061 |
| AC11 dead code removed | T064 |
| AC12 pipeline independence | T063, T068 |
| AC13–AC18 protocol layer | Phases 7–12 (see below) |

## Definition of Done (Phases 1–6)
Native stateful comprehensive timeline with time-alignment attribution and
in-place revision; revisions pushed to WS; common-time-base metadata exposed;
endpoint detector as an independent third pipeline; multi-speaker attribution
correct on test.mp3; build + tests green; schema additive; docs updated; commit.

---

## Phase 7 — Data Type System (FR4.1–FR4.4)
- [x] **T070** `include/protocol/topic.h`: `Topic` value type with level parsing;
  `TopicPattern` with `+`/`#` wildcard support; `TopicPattern::Matches(Topic)`;
  standard topic constexpr constants (`kAudioRaw`, `kVadSpeechSegment`, etc.).
  Header-only. *(Builds clean.)* (FR4.1)
- [x] **T071** `include/protocol/schema.h`: `FieldType` enum; `Field` struct;
  `Schema` struct; `TopicSchema` struct; `SchemaRegistry` class (topic →
  `TopicSchema` map, version tracking). Header-only. *(Builds clean.)* (FR4.2)
- [x] **T072** `test_protocol_types`: unit tests for topic parsing, wildcard
  matching (`+` single-level, `#` multi-level), schema registration, version
  conflict detection. *(ctest green.)* (FR4.1, FR4.3, FR4.4)

## Phase 8 — Pipeline Registry (FR3.1–FR3.5)
- [x] **T080** `include/protocol/pipeline_registry.h` +
  `src/protocol/pipeline_registry.cc`: `PipelineDescriptor`, `PipelineHandle`
  (RAII), `PipelineRegistry::Register()`/`Unregister()`/`Describe()`/
  `Heartbeat()`/`HealthCheck()`. `Register()` publishes `system/pipeline/online`;
  `Unregister()` publishes `system/pipeline/offline`. Disabled pipelines skip
  subscription wiring. Add sources to CMakeLists.txt. *(Builds clean.)* (FR3.1)
- [x] **T081** `test_pipeline_registry`: register/deregister lifecycle; `Describe()`
  returns all pipelines; heartbeat timeout detection; disabled pipeline skips
  wiring; RAII destructor auto-unregisters. *(ctest green.)* (FR3.1–FR3.5)

## Phase 9 — Topic Routing Engine (FR5.1–FR5.5)
- [x] **T090** `include/protocol/topic_router.h` + `src/protocol/topic_router.cc`:
  `TopicRouter::Subscribe()`/`Unsubscribe()`/`Route()`. Wildcard matching via
  `TopicPattern::Matches()`. Fan-out delivery. `no_local` filtering. Returns
  `std::vector<Delivery>` with effective QoS. Add to CMakeLists.txt.
  *(Builds clean.)* (FR5.1–FR5.5)
- [x] **T091** `test_topic_router`: wildcard matching (`vad/*` matches
  `vad/speech_segment`; `system/#` matches all system events); fan-out to
  multiple subscribers; `no_local` prevents self-receipt; unsubscribe removes
  subscription. *(ctest green.)* (FR5.1, FR5.3, FR5.4, FR5.5)

## Phase 10 — Time Index + Storage Backends (FR6.1–FR6.4, FR8.1–FR8.7)
- [x] **T100** `include/protocol/storage.h` + `src/protocol/storage.cc`:
  `Message` struct (msg_id, topic, pipeline, timestamp_sec, qos, schema_version,
  data); `StorageRef` (backend enum + offset); `StorageManager` with per-topic
  backend routing config. Add to CMakeLists.txt. *(Builds clean.)* (FR8.5)
- [x] **T101** `include/protocol/memory_backend.h` + `src/protocol/memory_backend.cc`:
  128 MB ring buffer; `Write()`/`Read()` with offset; FIFO eviction on overflow.
  Add to CMakeLists.txt. *(Builds clean.)* (FR8.5)
- [x] **T102** `include/protocol/disk_backend.h` + `src/protocol/disk_backend.cc`:
  mmap-backed file; configurable path (`ORATOR_STORAGE_DISK_PATH` env or
  `--storage-disk-path` CLI); per-session file naming; `Write()`/`Read()`.
  Add to CMakeLists.txt. *(Builds clean.)* (FR8.5, FR8.6)
- [x] **T103** `include/protocol/time_index.h` + `src/protocol/time_index.cc`:
  `TimeIndex::Append()` (sorted by timestamp); `Replay(topic, from_sec)`;
  `Last(topic)` for retained messages; `system/out_of_order` event on
  out-of-order insert. Add to CMakeLists.txt. *(Builds clean.)* (FR6.1, FR6.4)
- [x] **T104** `test_storage_backends`: memory ring buffer write/read/eviction;
  disk backend write/read with configurable path; time index sorted insert;
  replay from timestamp; retained message; out-of-order detection.
  *(ctest green.)* (FR6.1, FR6.4, FR8.2, FR8.5)

## Phase 11 — ProtocolTimeline (integration layer)
- [x] **T110** `include/protocol/protocol_timeline.h` +
  `src/protocol/protocol_timeline.cc`: `ProtocolTimeline` owns `PipelineRegistry`,
  `TopicRouter`, `TimeIndex`, `StorageManager`. `RegisterPipeline()` returns
  `PipelineHandle`. `Publish()` validates timestamp, writes storage, updates
  index, routes to subscribers, updates retained. `Replay()` and `Describe()`.
  Internal subscriber wires `ComprehensiveTimeline` methods to topic routing.
  Add to CMakeLists.txt. *(Builds clean.)* (FR3.1, FR5.2, FR6.1, FR8.1)
- [x] **T111** `test_protocol_timeline`: end-to-end: register pipeline → publish
  message → subscriber receives it; retained message on new subscribe; replay
  from timestamp; `Describe()` returns full state; out-of-order message stored
  sorted. *(ctest green.)* (FR3.1, FR5.2, FR8.1, FR8.2)

## Phase 12 — WS Protocol v2 + One-Shot Refactor (FR9.1–FR9.4)
- [x] **T120** Wire `ProtocolTimeline` into `AuditoryStream`: replace
  `ComprehensiveTimeline comp_` member with `ProtocolTimeline`; create
  `PipelineHandle`s for VAD, ASR, diarization pipelines at `Start()`; pass
  handles to workers. `ComprehensiveTimeline` becomes internal subscriber.
  *(Builds clean.)* (FR3.1, FR5.2)
- [x] **T121** Migrate workers to `PipelineHandle::Publish()`:
  - `AsrWorker`: `set_comprehensive_timeline()` → `set_pipeline_handle()`;
    publishes to `asr/transcript` and `asr/transcript_partial`
  - `DiarizationWorker`: publishes to `diar/speaker_segment`
  - VAD thread: publishes to `vad/speech_segment`
  - `ws_input` pipeline registered for `audio/raw`
  *(Builds clean.)* (FR3.1, FR4.1, FR5.2)
- [x] **T122** WS topic-based envelope (FR9.1): `auditory_ws_handler.cc` serializes
  messages with `topic`, `pipeline`, `pipeline_version`, `msg_id`, `ts`, `qos`,
  `schema_version`, `data`. `ready` message includes `"protocol_version": 2`.
  *(Builds clean.)* (FR9.1)
- [x] **T123** Backward compatibility (FR9.2): WS handler recognizes legacy
  `{"type":"vad",...}` format; translates to topic-based internally.
  `{"cmd":"describe"}` returns full topic map, schemas, pipelines.
  *(Builds clean.)* (FR9.2, FR9.3)
- [x] **T124** Remove legacy API: delete `AddVad()`, `UpsertText()`,
  `ReplaceSpeakers()` from public `ComprehensiveTimeline` interface (move to
  private/internal). Remove `StreamTimeline` (superseded). Clean up
  `auditory_stream.h`/`.cc`. Add `--storage-disk-path` to `ws_main.cc`.
  *(Builds clean, zero warnings.)* (FR9.4)
- [x] **T125** Regression validation: stream `test.mp3` (120 s) through WS;
  output byte-identical to pre-refactor final timeline. All 20+ ctests pass.
  *(ctest green, AC13, AC18.)* (AC13, AC18)
- [x] **T126** Full-hour validation: 3615 s `test.mp3` through `orator_ws`;
  timeline byte-identical; CER unchanged; GPU busy profile unchanged.
  *(AC18.)* (AC18)
- [x] **T127** Update `PROJECT_STATE.md`; commit. *(Constitution Art. VIII.)*

## Traceability (Protocol Layer)

| Requirement | Tasks |
|---|---|
| FR3.1 registration | T080, T110, T120 |
| FR3.2 discovery | T080, T110 |
| FR3.3 deregistration | T080, T081 |
| FR3.4 health | T080, T081 |
| FR3.5 disabled | T080, T081 |
| FR4.1 topics | T070, T072 |
| FR4.2 schema | T071, T072 |
| FR4.3 static schema | T071 |
| FR4.4 schema evolution | T071, T072 |
| FR5.1 wildcards | T090, T091 |
| FR5.2 publish | T090, T110 |
| FR5.3 subscribe | T090, T091 |
| FR5.4 fan-out | T090, T091 |
| FR5.5 unsubscribe | T090, T091 |
| FR6.1 timestamped | T103, T110 |
| FR6.4 time ordering | T103, T104 |
| FR7.1–FR7.4 QoS | T090, T100 |
| FR8.1 retained | T103, T104 |
| FR8.2 replay | T103, T104 |
| FR8.5 storage backends | T100, T101, T102, T104 |
| FR8.6 disk path | T102, T124 |
| FR8.7 retention | T100 |
| FR9.1 topic envelope | T122 |
| FR9.2 backward compat | T123 |
| FR9.3 describe | T123 |
| FR9.4 control messages | T124 |

| Acceptance | Tasks |
|---|---|
| AC13 pipeline registration | T110, T120, T121 |
| AC14 extensibility | T110, T111 |
| AC15 wildcards | T091, T111 |
| AC16 time ordering | T104, T111 |
| AC17 replay + retention | T104, T111 |
| AC18 byte-identical output | T125, T126 |
| AC19 session persistence | T130–T135 |

## Phase 13 — Session Persistence (ex-NG4)

Target: save timeline JSON on `Reset()`, expose session list/load over WS.

| ID | Task | Verification |
|----|------|-------------|
| T130 | Create `include/protocol/session_store.h` — `SessionStore` class with `Save(session_id, json)`, `List()`, `Load(session_id)`, configurable directory. File per session: `<dir>/<session_id>.json`. List returns JSON array of `{id, time, audio_sec}` lightweight metadata. | Unit test writes 3 sessions, lists, loads one, all match. |
| T131 | Implement `src/protocol/session_store.cc` — disk I/O via `<fstream>`, atomic write (write to `.tmp`, rename). Metadata extracted from timeline JSON's `session_start_wall_sec` and current `audio_sec` fields, with legacy `audio_duration` compatibility. | Same as T130. |
| T132 | Wire `AuditoryStream::Reset()` — call `Serialize()` before clearing state, pass to `SessionStore::Save()` with UUID-like session ID (`<wall_sec_hex>-<pid_hex>`). Guarded by optional `store_`: if unset (no disk path), skip. | After Reset, file appears in session dir. |
| T133 | Add WS command `{"cmd":"sessions"}` → returns `{"type":"sessions","list":[...]}`. Add `{"cmd":"load_session","session_id":"<id>"}` → returns full timeline message. Both handled in `ws_main.cc`. | WS client receives session list; load returns matching JSON. |
| T134 | Add `ORATOR_SESSION_DIR` env var to `ws_main.cc`. Default: `<ORATOR_STORAGE_DISK_PATH>/sessions/`. When empty, persistence disabled. | Env var controls directory; empty = no-op. |
| T135 | Web UI: session history panel in sidebar, list on load, click to restore + reset + load. | IMPLEMENTED. Real Chromium test finalized a 12 s session, found it by corrected metadata, and reloaded a byte-equivalent timeline on 2026-07-13. |

## Definition of Done (Phases 7–12)
Protocol layer replaces hard-coded pipeline interfaces; pipelines register via
`PipelineDescriptor`; topic-based routing with wildcard support; time-ordered
storage with MEMORY + DISK backends; WS messages use topic-based envelope with
backward compatibility; full 1-hour streaming run produces byte-identical output;
build + tests green; docs updated; commit. **DONE** (2026-06-19).

## VAD Gate O(N²) Fix — 2026-06-25
**Problem**: ASR Worker's VAD gate used `protocol_timeline_->Replay(0.0)` per `ProcessSpan` call (O(N²) — every call deserializes all VAD segments). At 3615s (972 VAD segments, ~400 ProcessSpan calls) → ~389K deserializations + mutex contention → wall time >> audio duration.

**Solution**: `ProtocolTimeline` subscription → local `VadCache` (O(1) `GetAll()`).
1. `VadCache` class in `AsrWorker` with thread-safe `AddSegment`/`GetAll`.
2. `AuditoryStream` owns `VadCache`, subscribes to `vad/speech_segment` via `ProtocolTimeline::SubscribeInternal`, cache populated by VAD thread's `Publish`.
3. ASR worker reads `vad_cache_->GetAll()` (O(1)) on hot path.
4. Result: Wall time 3615s → 3616s (1× real-time); O(N²) eliminated.

**Files changed**:
- `include/pipeline/asr_worker.h` — added `VadCache` class + `VadCache*` member
- `src/pipeline/asr_worker.cc` — `ProcessSpan` reads `vad_cache_->GetAll()`
- `src/pipeline/auditory_stream.cc` — `vad_cache_` member, `ProtocolTimeline::SubscribeInternal` in `StartWorkers`, `AsrWorker` receives `VadCache*`
- `include/pipeline/asr_worker.h` — `VadCache` class + `VadCache* vad_cache_` member

**VAD cache fix**: Replaced O(N²) `Replay(0.0)` (deserialize all VAD segments per `ProcessSpan`) with O(1) local cache read via ProtocolTimeline subscription. Result: 3615s wall time ≈ 3616s (1× real-time), O(N²) overhead eliminated.
