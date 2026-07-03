# Project State — Orator

A point-in-time record of where the project stands. Updated at meaningful
checkpoints. Authoritative engineering rules live in
[.specify/memory/constitution.md](../.specify/memory/constitution.md); active
work is specified under [specs/](.).

> **How this document stays truthful (Constitution Article VIII).** The code is
> authoritative; this file is subordinate to it. Every claim below names how to
> confirm it against the code (a symbol/file, a test, or a commit reference). If
> a claim and the code disagree, the code is correct and this file is the defect
> — fix it. Before acting on any claim here, verify it: a clean
> `cmake --build build -j` plus a full `cd build && ctest --output-on-failure`
> pass is the consistency proof. Status lines advance to `Implemented` in the
> same change that lands the code, with the commit reference.

- **Last updated**: 2026-07-03 (ASR VAD-overlap hallucination filter, ASR chunk cap restored to 24 s after 600 s A/B, comprehensive speaker-turn snapshot restored, GPU telemetry device metrics surfaced)
- **Branch**: `master`
- **Constitution**: v1.5.0

---

## 1. What Orator is

A real-time, edge-deployed (Jetson Orin / Thor) auditory pipeline, **pure C++/CUDA with
zero runtime third-party dependencies**. It ingests a live mono-audio stream over
WebSocket and produces a comprehensive timeline that carries both **speaker
separation** and **ASR transcript** content, one track per pipeline, on one
absolute time base.

## 2. Current phase

**Spec 004 (unified: time base + comprehensive timeline + protocol layer) is the
authoritative specification.** Spec 004 covers:
- Time base system (`core::TimeBase`, three consistency principles, wall clock anchor)
- Comprehensive timeline (native stateful, revisable, diarization-driven view split)
- Protocol layer (topic-based registration, schema registry, QoS, storage backends)

Spec 005 (time base) and Spec 007 (protocol layer) were merged into Spec 004 and deleted (2026-06-18). Spec 004 is now fully implemented.

The system runs **three independent active-producer pipelines** —
diarization (who/when), ASR (what/when), and VAD speech-activity detection — each
feeding one **native, revisable comprehensive timeline** on a single absolute
time base. The comprehensive layer is a **pure time-alignment layer**: it never
modifies, splits, infers, or back-fills any pipeline's content (Spec 004 §1a).

### Pipeline responsibility boundaries (ratified, do not re-litigate)

- **ASR** outputs ONLY plain transcript text + its own time codes. It has **no**
  speaker awareness and never attributes speakers. ASR now includes `text_id` in
  incremental messages for stable in-place segment tracking.
- **Diarization** outputs ONLY its own speaker identities + time codes. It never
  attributes text.
- **Comprehensive timeline** aligns the two purely by time overlap on the common
  base. A coarse ASR span overlapping several speakers aligning to one speaker is
  **by design, not a defect**. Finer attribution, if ever wanted, is an OPTIONAL
  enhancement owned by the ASR pipeline emitting finer-grained timed units — the
  comprehensive layer never synthesizes them. Do not track this as a pending bug.

## 3. Component status

| Component | Status | Notes |
|---|---|---|
| Streaming diarization (Sortformer) | ✅ Verified, industrial-grade; v2.1 features | Incremental, O(n), persistent identity; matched vs NeMo (forward <5e-3, streaming <1e-2, incremental <1e-4). v2.1 features: silence profile (`mean_sil_emb`) in `CompressSpkcache` (gated: `use_silence_profile`), dynamic `pop_out_len` with FIFO buffering (`fifo_len`), `spkcache_refresh_rate`. All params configurable via TOML. |
| Native Qwen3-ASR engine | ✅ Verified vs PyTorch oracle | mel 3.9e-3, encoder 1.3e-3, decoder argmax-match; transcript matches gold. Pure bf16 compute. |
| Forced alignment (Qwen3-ForcedAligner-0.6B, Spec 009) | ✅ Implemented + WS-validated (uncommitted) | NAR single-pass forced aligner: `audio_tower` (bidir) + `multi_modal_projector` + 28-layer CAUSAL `Qwen3` LM + score head (5000 labels × 80 ms). Pure C++/CUDA. Validated vs torch CPU oracle stage-by-stage (`test_forced_align_decode`, `_tokenizer`, `_audio`, `_lm` 32 ts-labels 0 mismatch, `_e2e` 9 real words 0 ms, `_mel`). Independent pipeline (Art. III): `IForcedAligner`/`core::AlignUnit` contract, registered `qwen3_forced_aligner`; `AlignWorker` consumes published `asr/transcript` only, reads its audio span from a retained-window `RetainedAudioBuffer`, publishes `align/units` + `{"type":"align"}` WS events. Worker catches GPU faults (never crashes the server) and clamps unit times to segment bounds. **Finals-only**: `HandleTextSink` routes ASR finals → `asr/transcript`, partials → `asr/transcript_partial`; the aligner aligns each segment once against its finalized text (no partial re-alignment). **Comprehensive-timeline align track**: `ComprehensiveTimeline::UpsertAlign`/`SnapshotAlign` (keyed by `text_id`, idempotent); `HandleAlignSubscription` bridges `align/units` → `comp_`; serialize emits an `align` track grouped by `text_id` on the common time base. Real WS streaming path (120 s, rate=0): 39 align events = 39 ASR finals (0 duplicate ids), 0/564 units out of bounds, timeline tracks = diar/asr/vad/align (39:39 with asr track), real-speech segment units monotonic and spread across the segment, no regression to ASR/diar/VAD. Config: `[align]` in `orator.toml` (`enable`, `model_dir`, `language`, `retain_sec`, `max_segment_sec`). Defining symbols: `include/model/qwen3_forced_aligner.h`, `include/pipeline/align_worker.h`, `include/pipeline/retained_audio_buffer.h`. **Remaining (ASR-layer, not alignment)**: ASR hallucinations (garbage text, system-prompt echo) appear identically in the asr and align tracks; the aligner faithfully aligns whatever ASR emits — suppression belongs in the ASR decoding pipeline. **Closing acceptance (2026-06-30, commit fa5f2ad)**: the prior 120 s / rate=0 validation never exercised LONG segments and hid a grid y-dim overflow (the bf16 GEMM generic fallback launched `grid.y = M`; a 77 s segment yields M = 147200 > 65535, so every long segment failed with `CUDA Error invalid argument` and coverage was ~2%). Fixed by grid-striding `Bf16GemmGenericKernel`/`Im2ColKernel` (cap grid.y at 65535; zero behaviour change for bounded M, ASR unaffected). Real `rate=1` 60-min ALL-FEATURES stream (diar+asr+vad+speaker+align): coverage **2% → 100%** (119/119 segments, 13594 character units, 0 out-of-bounds / 0 non-monotonic, RTF ~35×), 0.999× real-time, no crash. |
| WhisperMel / BPE tokenizer / sharded safetensors loader | ✅ Verified | Unit-tested. |
| Decoupling (interfaces + registry) | ✅ In place | `IDiarizer`, `IAsr`; registry-constructed. Text↔speaker combination is the concrete `ComprehensiveTimeline` (pure time-alignment), not an interface. |
| `OverlapTimelineMerger` / `ITimelineMerger` | 🗑️ Removed | The old one-shot max-overlap merger and its orphaned interface were deleted — superseded by `ComprehensiveTimeline` (Spec 004). |
| WebSocket server (libwebsockets v4.3.3) | ✅ Refactored | Replaced hand-rolled POSIX WS with libwebsockets (multi-client, RFC 6455/7692). Eliminated file-scope static variables (`serve_server`, `serve_factory`, `pss_list_head`) → instance members via `lws_context_user`. Thread-safe `SendText` with wakeup/cancel-service. ServeOnce mode for unit tests. |
| ASR + WS integration | Done; threaded, three independent pipelines, fully decoupled | `AuditoryStream` is a controller owning a private `PipelineAudioCache` per active pipeline (diarization, ASR, VAD), three worker threads (`DiarizationWorker`, `AsrWorker`, VAD detector), and a mutex-guarded `ComprehensiveTimeline`. `PushAudio` fans each frame out to every cache; each worker drains only its own cache at its own max speed (single producer / single consumer, consumed prefix freed immediately — a slow pipeline never pins another's memory). Pipelines communicate ONLY through the comprehensive timeline (Constitution Art. III §8): no direct push, no callback, no shared atomic cursor. Diarization and ASR workers hold `core::TimeBase` from the common time base (`TimeBase(sample_rate, 0)`, identical across all caches); all time codes derive from it (origin/process/result consistency). ASR reads VAD segments via a local `VadCache` populated from protocol messages. Sends incremental `diar`, final `asr`, partial `asr_partial`, `vad`, `revision`, and a comprehensive `timeline`. ASR uses stable `text_id` for in-place segment revision. GPU work serialized by `gpu::DeviceLock()`. **VAD gate: ASR reads VAD segments from a local cache populated by ProtocolTimeline subscription, eliminating O(N²) Replay calls on the hot path. Silent-input hardening (2026-07-03): when VAD has advanced past an ASR span with no speech, finalization drops the span; `asr/transcript_partial` is excluded from the final comprehensive timeline. The ASR final filter now also requires a TOML-configured minimum confirmed VAD overlap (`vad_min_overlap_sec=0.12`) once VAD has confirmed a span, dropping weak tail/silence finals while preserving unconfirmed live leading-edge speech. 120 s zero-PCM real WS validation produced 0 ASR / 0 diar / 0 VAD entries. 600 s `test.mp3` validation dropped the previous 588.3–600.0 "啊。啊。" tail final.** (`SharedAudioBuffer` retained but inactive — superseded by `PipelineAudioCache`, kept only for its concurrency test.) |
| Incremental KV-cache ASR streaming (Spec 003) | ✅ Implemented, verified, committed (8cc31ab); params refined 2026-07-03 | Persistent KV cache + prefix caching + chunk-local windowed encoder; partial-emission every 1 s via WebSocket. Full 1hr CER 16.1% / 6.22x; beats production Silero-VAD at every scale. **Current params**: `kStreamWindowMel=100` (1 s), `max_new_tokens=32`, `unfixed_chunks=2`, `unfixed_tokens=15`, `segment_sec=24.0`, `vad_min_overlap_sec=0.12`. 2026-07-03 real WS `test.mp3` 600 s A/B after the VAD-overlap filter: `segment_sec=24` produced 49 ASR finals vs 67 at 12 s, with the same final comprehensive count (115) and better `To C` wording; default restored to 24 s for ASR semantic stability. |
| Revisable comprehensive timeline (Spec 004) | ✅ Implemented (core + VAD pipeline + WS conformance) | Native stateful PURE CONTAINER + diarization-driven VIEW. Three tracks (diarization, asr, vad) each carry data + `source` meta + time codes; every pipeline emits its own WS message (`diar`/`asr`/`vad`) and revisions are source-tagged. The comprehensive VIEW splits text at DIARIZATION boundaries (not ASR's coarse segmentation). Final snapshot coalesces consecutive same-speaker pieces into speaker turns; Web UI Live keeps ASR utterance boundaries separately for draft/update ergonomics. VAD = batched GPU `GpuVad` publishing speech segments (`test_vad` gate 3.7e-8). Owner invariant: no overlap → "unknown", never borrowed. |
| Reusable common time base (Spec 004) | ✅ Implemented, Constitution v1.3.0 Article III | Header-only `core::TimeBase` value type. All pipelines inherit the common time base (`TimeBase(sample_rate, 0)`, shared by every per-pipeline `PipelineAudioCache` by construction), hold it as a member, derive all time codes through it. Three consistency principles enforced (origin/process/result). `WaitAndRead` returns `span_start_abs` to each consumer. Reconciliation check clean (zero gap) at 120s + 600s. Wall clock anchor at session entry/exit: `session_start_wall_sec` in `ready`/`timeline` WS messages, `wall_clock_ok` drift validation (< 1s tolerance). |
| Pipeline protocol layer (Spec 004) | ✅ Implemented | Phases 7–12 complete: data types (topic.h, schema.h), pipeline registry, topic router, storage layer (MEMORY + DISK), ProtocolTimeline integration, WS v2 envelope with describe command, --storage-disk-path flag. 25/25 tests pass. |
| Streaming validation | ✅ Through real WebSocket | `tools/ws_stream_client.py` (stdlib, reader thread) streams PCM through the socket at an accelerated rate and exports the full event log + timeline to JSON. |
| Logging system | ✅ Include-level `core/log.h` | Level-based macros (`LOG_DEBUG`/`INFO`/`WARN`/`ERROR`) with compile-time floor (`ORATOR_LOG_LEVEL`) and runtime env-var gate. All 14 `fprintf(stderr)` calls in src/ replaced. |
| CUDA kernel unit tests | ✅ `test_kernels`: 13/13 passed | GPU kernel operations (Add, Multiply, NormalizeVector, CosineSimilarity, BatchCosineSimilarity) validated against CPU reference; includes edge cases (zero, single-element, large 1M vectors). |
| CI pipeline | ✅ GitHub Actions | `.github/workflows/ci.yml`: CUDA 12.5, CMake build + ctest + warning check + Python syntax verification. Triggered on push/PR to master. |
| Test suite | ✅ 39/39 tests pass | Clean build under `-Wall -Wextra`, ZERO warnings. `test_kernels` validates GPU kernel numerics. Python integration tests (`py-ws-comprehensive`, `py-ws-real-audio`) run automatically via `test/run_py_test.py`. 600 s real-audio eval: ASR 3.65× RTF, diarization 89.5× RTF, speaker accuracy 89.4 %. |
| OnText protocol matching | ✅ Fixed | Substring `text.find("end")` → JSON key `text.find("\"end\"")` to prevent false positives on partial matches. Same for reset/flush. |
| GPU telemetry default | ✅ Changed | `gpu_telemetry_interval_sec = 0.0` (was 1.0); disabled by default, opt-in via `ORATOR_GPU_TELEMETRY_SEC`. |
| VAD model path | ✅ Migrated | `models/asr/silero_vad.safetensors` → `models/vad/`. Updated 6 file references across test, include, and tools. |
| Web UI (Spec 006 MVP) | ✅ Implemented (all 16 tasks) | HTTP static server (`http_static_server.h/.cc`), modular SPA (`index.html`, `style.css`, `web/js/*`), Canvas timeline with zoom/pan and keyboard nav, ARIA labels + focus indicators + 44px touch targets, microphone + file upload, WebSocket client with auto-reconnect, device/pipeline telemetry display, JSON export, Spec 004 protocol envelope unwrapping, integration test (`tools/ws_ui_integration_test.py`), developer docs (`web/README.md`). 2026-07-03 hardening: Live transcript renders comprehensive revision rows directly, stores multiple rows per `text_id`, and avoids a second diarization-driven split in the browser. |
| Session persistence UI (Spec 004 T135) | ❌ Not implemented | Web UI session history panel in sidebar is not yet built. Backend SessionStore + WS commands (sessions/load_session) are implemented and tested. |
| ISpeakerEmbedder (core/stages.h) | 🔒 Retained, inactive | Interface declares a fixed-dimension speaker embedding extractor. Never implemented; no concrete class in src/. Retained for future speaker-identification features. |
| ISpeakerRegistry (core/stages.h) | 🔒 Retained, implemented | Interface declares a persistent enrolled-speaker registry with 1:N matching. `speaker_database.h` provides a concrete implementation (`SpeakerDatabase` class) that compiles and has tests (`test/unit/core/test_speaker_db.cc`), but is never wired into any runtime pipeline. Retained for future speaker-identification features. |
| ISink (core/stages.h) | 🔒 Retained, partially active | Interface for terminal timeline consumers. The runtime uses `Emit` callbacks (std::function) instead for primary flow, but a concrete implementation `JsonSink` exists in `include/io/json_sink.h` and `src/io/json_sink.cc` for JSON serialization to streams. Retained as a contract option for non-callback consumers. |
| ComprehensiveTimeline event system | 🔒 Retained, not implemented | The documentation previously claimed Subscribe/Unsubscribe/EventHandler/DispatchEvents were fully implemented with `fire_event_()` called from mutation methods, but code inspection of `comprehensive_timeline.h` and `comprehensive_timeline.cc` shows these event system components are not actually implemented. The container currently uses direct revision returns via `UpsertSpeaker`, `UpsertText`, `ReplaceSpeakers` methods. Retained as infrastructure concept for future pipeline growth. |

## 4. Measured performance (GPU fixed at 1.3 GHz, power mode MaxN)

Measured through the **real WebSocket** at max push rate, 120 s of `test.mp3`
(`/tmp/orator_stream_120.json`):
- **Diarization**: ~9.6× real-time (compute 12.5 s).
- **ASR**: ~2.6× real-time (compute 46.4 s) — many small endpointed utterances,
  each paying fixed per-call cost. Throughput tuning is deferred by owner
  (Spec 001 NG1).
- **End-to-end stream**: ~2.26× real-time (wall 53 s). Because the two pipelines
  share ONE GPU, the GPU lock serializes device work, so wall ≈
  diar_compute + asr_compute. The threads still overlap their CPU-side work
  (buffering, endpointing, serialization); the wall is GPU-bound.
- 25 diarization segments + 27 transcript utterances on one time base; the
  comprehensive view groups them into 10 speaker turns; transcript matches the
  verified engine's output.

Clip-based ("whole buffer") numbers are **not** treated as streaming results,
per Constitution Art. IV.

### Full-length (1 hr) verification, 2026-06-25

Full 3615 s of `test.mp3` pushed through the real WebSocket at max push rate
(380× wire speed), GPU warm, same hardware config:

| Metric | Value |
|---|---|
| Audio duration | 3615 s (1.00 hr) |
| Wall time | **3616 s** (60.3 min) |
| End-to-end speed | **1.0× real-time** (1× push rate) |
| ASR compute | ~3.65× real-time (compute RTF) |
| Diarization compute | ~89× real-time |
| VAD compute | ~300× real-time |
| `wall_clock_ok` | True (no clock drift) |
| ASR entries | 476, last at 3615.0 s (100 % coverage) |
| Diarization segments | 724, last at 3615.0 s (100 % coverage) |
| VAD segments | 972 |
| Total messages | ~1253 (comprehensive entries) |

**Key finding**: VAD gate `ProtocolTimeline` subscription + local `VadCache` (ProtocolTimeline → `VadCache::AddSegment` via `SubscribeInternal`) eliminated the O(N²) `Replay()` calls on the ASR hot path. **Wall time ≈ audio duration (3616s vs 3615s)** — O(N²) `Replay()` overhead eliminated. Diar track accuracy 77.3% (diar track) / 67.0% (comprehensive view); 600s eval shows 92.8% diar track accuracy.

**VAD cache fix**: Replaced `protocol_timeline_->Replay(0.0)` (O(N²) per `ProcessSpan` call) with `ProtocolTimeline` subscription → `VadCache::AddSegment` → `VadCache::GetAll()` (O(1) hot path). Eliminates O(N²) `Replay()` deserialization on ASR hot path, enabling real-time streaming at 1× push rate for full hour.

### 600 s verification (baseline params, VAD cache fix)

| Metric | Value |
|---|---|
| Audio duration | 600 s |
| Wall time | ~600 s (1× real-time) |
| Diar track accuracy | **92.8%** (duration-weighted vs test.txt) |
| ASR RTF | ~3.65× |
| Diar RTF | ~89× |
| `wall_clock_ok` | True |

Speaker mapping correct: [0]→朱杰, [1]→徐子景, [2]→唐云峰, [3]→石一. 600s diar track accuracy **92.8%** exceeds baseline 89.4%.

**Full-length (3615s) with baseline params**:

| Metric | Value |
|---|---|
| Audio duration | 3615 s (1.00 hr) |
| Wall time | **3616 s** (60.3 min) |
| End-to-end speed | **1.0× real-time** (1× push rate) |
| Diar track accuracy | **77.3%** (duration-weighted) |
| Comprehensive view accuracy | 67.0% (unknown gaps 14.3%) |
| Speaker mapping | 4/4 correct (朱杰/徐子景/唐云峰/石一) |

**Full-length observation**: At 1 hr duration, diar track accuracy drops to 77.3% due to speaker cache degradation over 1 hr (diarization track 77.3% vs 92.8% at 600s). 600s accuracy **exceeds baseline 89.4%**. 3615s accuracy 77.3% reflects model cache degradation at 1 hr, not code regression.

> **Note (superseded methodology)**: the 92.8% / 77.3% figures above are
> duration-weighted *script* metrics over the diarizer's per-window LOCAL slots
> (an optimal-mapping upper bound, not a deployable identity). Speaker accuracy is
> now judged by **context-aware per-segment semantic comparison** (Test Review
> Protocol), not scripts. The long-session diar degradation is mitigated by the
> periodic diarizer reset (commit 7507748) and the cross-session GLOBAL identity
> layer finalized in Spec 010 (see "cross-session identity finalized" below): the
> full 60-min stream now yields exactly 4 stable global speakers.

**Key improvement**: VAD gate `ProtocolTimeline` subscription + local `VadCache` (O(1) `GetAll()`) replaced O(N²) `Replay(0.0)` calls. Wall time ≈ audio duration (3616s vs 3615s) — O(N²) overhead eliminated.

### Spec 002 baseline (Phase 1, measured before any engine change)

Three configurations, 120 s of `test.mp3`, through the real WebSocket at max
push rate, GPU fixed at 1.3 GHz, power mode MaxN:

| Configuration | Wall time | GPU compute | GPU-busy fraction |
|---|---|---|---|
| Diarization only | 3.2 s (37.2×) | 3.0 s (39.9×) | 78.8% |
| ASR only | 38.4 s (3.13×) | 33.9 s (3.54×) | 72.8% |
| Both (current, global lock) | 53.3 s (2.26×) | — | ~63% |

Findings:
- The lower bound on total wall time is the larger single-pipeline compute time,
  which is ASR (~38 s). The current both-pipelines wall time is 53 s, so the
  global lock adds about 15 s of serialization.
- Diarization alone is about 3 s of GPU work, but under the global lock its
  measured time rises to 12.5 s because it waits behind ASR. The lock delays the
  latency-critical pipeline.
- ASR alone leaves the GPU idle about 27% of the time, so diarization's small
  GPU work can run during ASR's idle intervals.
- Realistic target (M3): reduce total wall time from 53 s toward the ASR-only
  floor (~38–40 s, about 3.0× real-time), a 25–28% reduction. The total cannot
  go below ASR-only without an ASR speedup (Spec 001 NG1, deferred).

## 5. Decisions on record

- **No quantization at this stage.** int8 was prototyped and **fully reverted**;
  decode is pure bf16. Any quantization is deferred to a separate, scheduled
  effort (Constitution II.3).
- **Two independent pipelines + threaded controller** is the agreed architecture
  (Spec 001). The main process owns and controls the worker threads.
- **Engineering quality is a ratified requirement** (Constitution Art. V):
  readability, organization, maintainability, extensibility, concurrency safety.
- **Spec consolidation**: Spec 004 is the unified spec for time base + comprehensive
  timeline + protocol layer. Spec 005 and Spec 007 are superseded. No new spec
  numbers will be created for overlapping scope.

## 6. SDD artifacts

- [.specify/memory/constitution.md](../.specify/memory/constitution.md) — v1.4.0 (Article I §1: C++17 → C++20)
- [specs/001-streaming-pipeline/spec.md](001-streaming-pipeline/spec.md) — implemented
- [specs/001-streaming-pipeline/plan.md](001-streaming-pipeline/plan.md) — implemented
- [specs/001-streaming-pipeline/tasks.md](001-streaming-pipeline/tasks.md) — implemented
- [specs/002-gpu-scheduling/spec.md](002-gpu-scheduling/spec.md) — **COMPLETED** (2026-06-17): all 17 tasks done
- [specs/002-gpu-scheduling/plan.md](002-gpu-scheduling/plan.md) — **COMPLETED**
- [specs/002-gpu-scheduling/tasks.md](002-gpu-scheduling/tasks.md) — **COMPLETED**
- [specs/003-sliding-window-asr/spec.md](003-sliding-window-asr/spec.md) — implemented (8cc31ab)
- [specs/004-comprehensive-timeline/spec.md](004-comprehensive-timeline/spec.md) — **UNIFIED SPEC** (time base + comprehensive timeline + protocol layer). Implemented (all phases 1–12). Supersedes 005 and 007.
- [specs/006-web-ui/spec.md](006-web-ui/spec.md) — implemented (16/16 tasks complete); **Phase 2 rebuild (2026-07-01)**: the MVP single-file `app.js` prototype consumed only a subset of the protocol (no `align`/`cursor_progress`, GPU telemetry reduced to one status string, 3-track canvas, no global speaker identity). Rebuilt as modular ES (`web/js/{ws,model,audio,format}.js` + `web/js/render/{transcript,timeline,observability,sessions}.js`, no framework/build step): single envelope-aware router consumes **every** WS message type (FR10); diarization/comprehensive keyed by global `speaker_id` (FR11); 4-lane canvas adds the forced-alignment unit lane (FR12); a **live observability panel** shows per-pipeline RTF + backlog sparklines, scheduling class/priority/active, and a starvation warning (FR13, fed by `gpu_telemetry`+`cursor_progress`); comprehensive view updates live from `revision` and reconciles on `timeline` (FR14). Validated: bun bundles all 9 modules; data layer exercised on a real captured run (125 gpu + 125 cursor samples, 4 tracks, per-pipeline RTF/backlog, global identity `spk_1`); all 30 referenced DOM ids resolve; static server serves `/js/*` as `application/javascript`. Browser context review pending.
- [specs/006-web-ui/plan.md](006-web-ui/plan.md) — implemented
- [specs/006-web-ui/tasks.md](006-web-ui/tasks.md) — implemented
- [specs/011-observability/spec.md](011-observability/spec.md) — **Implemented** (2026-06-30): offline [rerun](https://rerun.io) visualization, kept entirely in `tools/` (no runtime third-party dep, Art. I). **Phase 1**: `tools/verify/py/ws_unified_test.py` captures the runtime's periodic `gpu_telemetry`/cursor WS samples into a `telemetry` array; `tools/observability/timeline_to_rerun.py` keys diarization/comprehensive lanes by the global `speaker_id` (`spk_N`) + per-pipeline RTF lanes. **Phase 2 (comprehensive dashboard)**: `TegraSampler` records a continuous `device_series`; the exporter renders six namespaced dimensions on one `audio_time` axis — `pipelines/*`, `comprehensive/<id>` swimlanes, `scheduler/<pipe>/{rtf,compute_sec,active,cuda_priority}`, `cursors/<pipe>/{position_sec,pending_sec}`, `device/{mem,cpu,gpu,temp,power}/*` (extended tegrastats parse; Orin `GR3D_FREQ` optional, omitted on Thor), and `session/summary` — laid out by a `rerun.blueprint` persisted in the `.rrd`. Methodology + best practices in `tools/observability/README.md`. **Config fix**: nested `[telemetry.cursor]` was never read (`config["telemetry.cursor"]` literal-key lookup) → now `config["telemetry"]["cursor"]`, with a `test_config` regression. Validated on a `rate=1` 120 s run: 125 gpu + 125 cursor + 126 device samples, six dimensions populated, stream_rt 0.964×, ctest 47/47, zero warnings. Follow-ups: live WS→rerun consumer, full-hour acceptance recording.
- [specs/010-speaker-id/spec.md](010-speaker-id/spec.md) — **Implemented**: speaker identity (TitaNet-Large voiceprint enrollment / re-identification as a post-diarization stage inside the diar pipeline, Art. III). **Phase A complete & committed**: A1 acquire+convert weights → `models/speaker/titanet_large.safetensors` (108 tensors); A2 NeMo oracle (`tools/reference/titanet_oracle.py`, isolated `tools/.venv-nemo`); A3 pure C++/CUDA `model::TitaNetEmbedder` (`include/model/titanet_embedder.h` + `src/model/titanet_embedder.cu`, time-major [T,C]: mel+per_feature → 5-block ContextNet encoder → attentive statistics pooling → 192-d, F32 weights); A4 `test_titanet` validated vs NeMo oracle (**span cosine 1.000000/0.999999/1.000000, cross-span matrix to 4 decimals; ctest 46/46, no warnings**). **Phase B complete & committed**: `pipeline::SpeakerIdentityStage` (clean-segment gate + per-local embed/match/enroll via `SpeakerDatabase` + revisable local→global map), wired into the diar pipeline behind a `DiarizationWorker` segment-processor hook + `[speaker]` config; `OnsetOffsetSegments` now computes `DiarSegment::confidence`; diar message/track expose a backward-compatible `speaker_id` field. Validated: `test_speaker_id_stage` + real WS 120s (2 speakers auto-enrolled spk_0×21/spk_1×5, conf 0.50–0.97), ctest 47/47. **Phase C complete & committed**: C8 threads `speaker_id` through the diar message/track and `ComprehensiveTimeline` (`SpeakerLabelIds()` label→id) so comprehensive turns carry the global id (revisable, backward compatible); C9 `SpeakerDatabase::SetDisplayName`/`DisplayName` naming hook persisted in a `<registry>.names` sidecar, serialized as `speaker_name` when set. Validated: WS comprehensive turns carry spk_0×9/spk_1×10, `test_speaker_db` name+sidecar round-trip, ctest 47/47. **Phase D complete & committed**: D10 registry Save (`StopWorkers`, empty-registry guard) + Load (`Start`) wired to `[speaker].registry_path` — validated cross-session re-identification (run 2 re-identifies run 1's speakers, no new enrollment); D11 fixed an embedding-candidate bug (aged-out longest span blocked a local speaker from ever embedding → now only in-window spans are candidates, 1→3-4 locals embedded over 600 s) and recorded the τ finding (live diar cross-speaker cosines ~0.45-0.48 vs clean-oracle ~0.05 → τ-sensitive, run-to-run variance under `rate=0` non-determinism; default τ=0.45, definitive tuning needs deterministic input + ground truth). **Spec 010 Phases A–D all implemented; remaining: ground-truth τ tuning + optional UI for the naming hook.** Phase E (centroid voiceprint, conf-priority selection, no VAD, centered window, enroll-confirm, naming in comprehensive view) committed; ground-truth eval 91→93% (EER ~13%). **Full-length 60min closing test**: exposed + fixed a speaker-embed OOM (uncapped embed span → multi-GB → SIGKILL; cap `max_embed_window_sec=10` → survives the hour, registry saved gracefully, centroid match 0.639). Open: final-timeline delivery at 60min times out the test client (present with speaker off too — large-payload/client issue, not a server crash).## 7. Immediate next step

Specs 001, 002, 003, 004 (core protocol), and 006 (Web UI MVP) are complete, verified, and committed. Spec 004 Phase 13 T135 (Web UI session history panel) remains pending; all other Phase 13 tasks are implemented.

- **Spec 010 speaker identity — cross-session identity finalized** (commits 38cdf51, 9c02862, 17f8d92, 06875c3, 5f301ba). The voiceprint stage now assigns a persistent GLOBAL id to every diar segment. Design corrections, all validated through the REAL streaming path (rate=1) and judged by **context-aware per-segment semantic comparison vs `test.txt`** (Test Review Protocol — accuracy is NOT taken from script metrics):
  - **Trust the diarizer's within-session separation**: each local slot resolves to its own global id; same-session slots can never collapse to one id (`SpeakerDatabase::MatchExcluding`). Per-segment re-matching was removed (it collapses similar voices to the dominant centroid).
  - **Cross-session strengthening**: each global's centroid is the mean of the best references of all slots mapped to it across sessions, so a returning speaker re-matches reliably (match cosine ~0.55 → 0.7–0.87).
  - **Registry-level de-duplication, uncapped**: `MergeReconcile` merges two globals only when their centroids are confidently the same person (cosine > 0.70; a stricter 0.85 for two globals that ever co-occurred in one session, since the diarizer judged them distinct), and `SpeakerDatabase::Remove` deletes the duplicate so the registry holds exactly one entry per real speaker. The registry is never capped — it is designed to recognise many speakers (≥200) across sessions.
  - **Test-method correction**: validate speaker accuracy through the real `rate=1` stream (a `rate=0` shortcut ages clean spans out of the embed-retain window before they are delivered, starving enrollment). Full 60-min run: 4 real speakers → exactly 4 stable global ids (spk_0=朱杰, spk_1=唐云峰, spk_2=徐子景, spk_3=石一) across all 6 reset sessions; clear/substantive turns attributed correctly (~90% on 0–600 s and 1800–2400 s), the 2400–3600 s region remains the hard part — confirmed by an independent fresh run of that segment to be the **audio's inherent rapid-speaker-exchange difficulty**, not continuous-run degradation. ctest 47/47, no warnings.

- **Full-pipeline closing validation — all features on** (2026-06-30). A single real `rate=1` 60-min WebSocket stream with **diarization + ASR + VAD + speaker identity + forced alignment all enabled**: 0.999× real-time, no crash, no OOM. Tracks: diar 729 (RTF ~100×), asr 119 (RTF ~1.25×), vad 972, **align 119 = 100% of ASR segments** (13594 char-level units, 0 out-of-bounds / 0 non-monotonic, RTF ~35×). Speaker identity converges to 4 stable globals. Judged by context-aware per-segment semantic comparison vs `test.txt` (Test Review Protocol), not script metrics. ctest 47/47, no warnings, zero runtime third-party deps.

- **Codebase hardening** — complete. All P0/P1 items from 2026-06-21 evaluation executed:
  - GitHub Actions CI (CUDA 12.5, cmake + ctest + Python lint)
  - CUDA kernel unit tests (`test_kernels`: 13/13 passed, GPU vs CPU reference)
  - Level-based logging (`core/log.h`) replacing raw `fprintf(stderr)`
  - WebSocket server file-static elimination (`serve_server`/`serve_factory`/`pss_list_head` → instance members)
  - OnText JSON key exact matching (fixes `end`/`flush`/`reset` false positives)
  - GPU telemetry default disabled (1.0 → 0.0)
  - VAD model path migration (`models/asr/` → `models/vad/`)
  - README env var table + Python test CTest registration + protocol envelope unwrapping in web UI
- **Spec 004 — Protocol Layer**: Implemented. Phases 7–12 complete. Phase 13 (session persistence): core backend (SessionStore T130–T132) and WS commands (T133–T134) implemented; T135 (Web UI session history panel) NOT YET IMPLEMENTED. Web UI (`app.js`) now includes `unwrapEnvelope()` for Spec 004 topic-based protocol envelopes. Integration test (`ws_ui_integration_test.py`) uses `unwrap_envelope()` for all WS message parsing.
- **Full-length streaming verification**: 2026-06-21. 3615 s (1 hr) audio pushed through real WebSocket → 382.0 s wall = **9.46× real-time**. All three tracks (ASR/diarization/VAD) cover 100 % of the audio, no crash, no clock drift, no data loss. Achieved 9.25× on a consecutive warm-GPU re-run and 5.82× on a cold-start run, confirming model-load overhead is one-time.
- **Python integration tests** (2026-06-22). Auto server lifecycle via `test/run_py_test.py`. Tests are no longer manual — run automatically with `ctest`. 39/39 tests pass.
- **TOML config system** (2026-06-22). All ~34 runtime parameters consolidated into `orator.toml` with 8 sections: `[server]`, `[asr]`, `[vad]`, `[diarizer]`, `[storage]`, `[telemetry]`, `[debug]`, `[debug_model]`. Loading order: compile-time defaults → CLI args → `orator.toml` → env var overrides. Header-only toml++ (FetchContent, zero runtime dep). Config struct expanded to 34 fields across all pipelines. Previous env-only params (`ORATOR_TIMEBASE_CHECK`, `ORATOR_ASR_PROFILE`, `ORATOR_STREAM_PROGRESS`, `ORATOR_LOG_LEVEL`, `ORATOR_GPU_SERIAL`/`CONCURRENT`) now in Config + synced to environment for deep getenv() code. See `include/io/config_reader.h`, `src/io/config_reader.cc`, `orator.toml`.
- **VAD-gated ASR fix** (2026-06-22). VAD async-lag protection via segment-start confirmation check. ASR segments reduced from 43→18 (120s test). RTF improved 4.7→3.7. Parameters tuned: `asr_vad_trail_sec=1.0`, `vad_min_silence_ms=300`. See `src/pipeline/asr_worker.cc:61-141`.
- **Full-length verification (v7)** (2026-06-23). 3615s (1 hr) audio at 420× injection: **964s wall (3.75×)**, no crash, no data loss. 300s verification confirms 3 ASR segments cover 300s of audio (merging 90 VAD segments). Speed regression from 9.46× (pre-v7) due to VAD segment-start check keeping ASR segments open longer, causing more audio to pass through GPU processing. 120s test at 1× real-time still at RTF 3.7.
- **NeMo feature parity — silence profile, FIFO, dynamic pop_out_len** (2026-06-24). Ported three NeMo v2.1 streaming features to `streaming_sortformer.cc`:
  - `mean_sil_emb` in `CompressSpkcache`: cosine-similarity penalty against silence profile (`use_silence_profile` flag, gated off by default — v2.1 models opt in). `UpdateSilenceProfile()` already maintained per-chunk.
  - `spkcache_refresh_rate` (default 0): controls speaker cache refresh cadence in FIFO mode.
  - Dynamic `pop_out_len` + FIFO buffering: dual-path streaming update — sync (`fifo_len=0`, backward-compatible default) and FIFO async (`fifo_len>0`). `HostStreamState` gains lazy-init FIFO buffers (`fifo_embs`, `fifo_preds`), proper overflow handling, and NeMo-style pop-out calculation.
  - All three params wired through full config chain: `orator.toml [diarizer]` → `ConfigReader` → `AuditoryStream::Config` → `SortformerTuning` → `SortformerConfig`. See `include/model/streaming_sortformer.h`, `include/pipeline/auditory_stream.h`, `src/io/config_reader.cc`.
  - **Bug fixes**: `Sha1::Finalize()` padding fix for multi-block messages (≥ 56 bytes); `test_integration.py` eval_single indentation bug fix.
  - **Verification**: 600 s real-audio eval: diarization compute 6.7 s (89.5× RTF), ASR compute 164.2 s (3.65× RTF), wall_clock_ok. Speaker diarization accuracy 89.4% (weighted by duration) against test.txt ground truth. All 39/39 tests pass. Reference data (`ref_stream_total.f32`) regenerated for current build env.
