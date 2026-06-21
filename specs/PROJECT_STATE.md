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

- **Last updated**: 2026-06-21 (Spec 003 tasks.md audit complete; Spec 004 status corrected; Spec 006 Phase 3 status documented)
- **Branch**: `master`
- **Constitution**: v1.3.0

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
| Streaming diarization (Sortformer) | ✅ Verified, industrial-grade | Incremental, O(n), persistent identity; matched vs NeMo (forward <5e-3, streaming <1e-2, incremental <1e-4). |
| Native Qwen3-ASR engine | ✅ Verified vs PyTorch oracle | mel 3.9e-3, encoder 1.3e-3, decoder argmax-match; transcript matches gold. Pure bf16 compute. |
| WhisperMel / BPE tokenizer / sharded safetensors loader | ✅ Verified | Unit-tested. |
| Decoupling (interfaces + registry) | ✅ In place | `IDiarizer`, `IAsr`; registry-constructed. Text↔speaker combination is the concrete `ComprehensiveTimeline` (pure time-alignment), not an interface. |
| `OverlapTimelineMerger` / `ITimelineMerger` | 🗑️ Removed | The old one-shot max-overlap merger and its orphaned interface were deleted — superseded by `ComprehensiveTimeline` (Spec 004). |
| WebSocket server (libwebsockets v4.3.3) | ✅ Refactored | Replaced hand-rolled POSIX WS with libwebsockets (multi-client, RFC 6455/7692). Eliminated file-scope static variables (`serve_server`, `serve_factory`, `pss_list_head`) → instance members via `lws_context_user`. Thread-safe `SendText` with wakeup/cancel-service. ServeOnce mode for unit tests. |
| ASR + WS integration | Done; threaded, three independent pipelines, fully decoupled | `AuditoryStream` is a controller owning a `SharedAudioBuffer`, three worker threads (`DiarizationWorker`, `AsrWorker`, VAD detector), and a mutex-guarded `ComprehensiveTimeline`. Pipelines communicate ONLY through the comprehensive timeline (Constitution Art. III §8): no direct push, no callback, no shared atomic cursor. Diarization and ASR workers hold `core::TimeBase` inherited from `buffer_.time_base()`; all time codes derive from it (origin/process/result consistency). ASR reads VAD segments via `ComprehensiveTimeline::SnapshotVad()`. Sends incremental `diar`, `asr`/`asr_partial`, `vad`, `revision`, and a comprehensive `timeline`. ASR uses stable `text_id` for in-place segment revision. GPU work serialized by `gpu::DeviceLock()`. |
| Incremental KV-cache ASR streaming (Spec 003) | ✅ Implemented, verified, committed (8cc31ab); params refined 2026-06-17 | Persistent KV cache + prefix caching + chunk-local windowed encoder; partial-emission every 1 s via WebSocket. Full 1hr CER 16.1% / 6.22x; beats production Silero-VAD at every scale. **Current params**: `kStreamWindowMel=100` (1 s), `max_new_tokens=32`, `unfixed_chunks=2`, `unfixed_tokens=15`, `max_segment_sec=24.0`. |
| Revisable comprehensive timeline (Spec 004) | ✅ Implemented (core + VAD pipeline + WS conformance) | Native stateful PURE CONTAINER + diarization-driven VIEW. Three tracks (diarization, asr, vad) each carry data + `source` meta + time codes; every pipeline emits its own WS message (`diar`/`asr`/`vad`) and revisions are source-tagged. The comprehensive VIEW splits text at DIARIZATION boundaries (not ASR's coarse segmentation). VAD = batched GPU `GpuVad` publishing speech segments (`test_vad` gate 3.7e-8). Owner invariant: no overlap → "unknown", never borrowed. |
| Reusable common time base (Spec 004) | ✅ Implemented, Constitution v1.3.0 Article III | Header-only `core::TimeBase` value type. All pipelines inherit from `buffer_.time_base()`, hold it as a member, derive all time codes through it. Three consistency principles enforced (origin/process/result). `WaitAndRead` returns `span_start_abs` to all consumers. Reconciliation check clean (zero gap) at 120s + 600s. Wall clock anchor at session entry/exit: `session_start_wall_sec` in `ready`/`timeline` WS messages, `wall_clock_ok` drift validation (< 1s tolerance). |
| Pipeline protocol layer (Spec 004) | ✅ Implemented | Phases 7–12 complete: data types (topic.h, schema.h), pipeline registry, topic router, storage layer (MEMORY + DISK), ProtocolTimeline integration, WS v2 envelope with describe command, --storage-disk-path flag. 25/25 tests pass. |
| Streaming validation | ✅ Through real WebSocket | `tools/ws_stream_client.py` (stdlib, reader thread) streams PCM through the socket at an accelerated rate and exports the full event log + timeline to JSON. |
| Logging system | ✅ Include-level `core/log.h` | Level-based macros (`LOG_DEBUG`/`INFO`/`WARN`/`ERROR`) with compile-time floor (`ORATOR_LOG_LEVEL`) and runtime env-var gate. All 14 `fprintf(stderr)` calls in src/ replaced. |
| CUDA kernel unit tests | ✅ `test_kernels`: 13/13 passed | GPU kernel operations (Add, Multiply, NormalizeVector, CosineSimilarity, BatchCosineSimilarity) validated against CPU reference; includes edge cases (zero, single-element, large 1M vectors). |
| CI pipeline | ✅ GitHub Actions | `.github/workflows/ci.yml`: CUDA 12.5, CMake build + ctest + warning check + Python syntax verification. Triggered on push/PR to master. |
| Test suite | ✅ 26/26 C++ ctests pass | Clean build under `-Wall -Wextra`, ZERO warnings. `test_kernels` validates GPU kernel numerics. Python integration tests registered (`py-ws-comprehensive`, `py-ws-real-audio`) marked manual (require running server). |
| OnText protocol matching | ✅ Fixed | Substring `text.find("end")` → JSON key `text.find("\"end\"")` to prevent false positives on partial matches. Same for reset/flush. |
| GPU telemetry default | ✅ Changed | `gpu_telemetry_interval_sec = 0.0` (was 1.0); disabled by default, opt-in via `ORATOR_GPU_TELEMETRY_SEC`. |
| VAD model path | ✅ Migrated | `models/asr/silero_vad.safetensors` → `models/vad/`. Updated 6 file references across test, include, and tools. |
| Web UI (Spec 006 MVP) | ✅ Implemented (Phases 1+2) | HTTP static server (`http_static_server.h/.cc`), full SPA (`index.html`, `style.css`, `app.js`), Canvas timeline with zoom/pan (mouse wheel zoom toward cursor, click-drag pan, +/−/reset buttons, 3 tracks: diarization/ASR/VAD, adaptive time axis), microphone + file upload, WebSocket client with auto-reconnect, GPU telemetry display, JSON export, Spec 004 protocol envelope unwrapping (`unwrapEnvelope()`), protocol describe command on connect. Integration test: `tools/ws_ui_integration_test.py` with envelope-aware message parsing. |

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

### Full-length (1 hr) verification, 2026-06-21

Full 3615 s of `test.mp3` pushed through the real WebSocket at max push rate
(380× wire speed), GPU warm, same hardware config:

| Metric | Value |
|---|---|
| Audio duration | 3615 s (1.00 hr) |
| Wall time | **382.0 s** (6.4 min) |
| End-to-end speed | **9.46× real-time** |
| ASR compute | 381.7 s → RTF 9.47 |
| Diarization compute | 65.5 s → RTF 55.2 |
| VAD compute | 11.9 s → RTF 303.7 |
| `wall_clock_ok` | True (no clock drift) |
| ASR entries | 151, last at 3615.1 s (100 % coverage) |
| Diarization segments | 1063, last at 3614.6 s (100 % coverage) |
| VAD segments | 1454 |
| Total messages received | 171 (240 KB) |

**Key finding**: the pipeline processes a full hour of audio without any
degradation, crash, or clock drift. The 9.46× throughput is consistent with
ASR GPU compute dominating the wall (381.7 s of 382.0 s total). No mid-stream
buffer growth, no connection drop, no silent data loss. This validates the
three-pipeline architecture (Constitution Art. III) at production duration.

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

- [.specify/memory/constitution.md](../.specify/memory/constitution.md) — v1.3.0 (Article III expanded: common time base foundation, three consistency principles, pipeline decoupling via comprehensive timeline, wall clock anchor)
- [specs/001-streaming-pipeline/spec.md](001-streaming-pipeline/spec.md) — implemented
- [specs/001-streaming-pipeline/plan.md](001-streaming-pipeline/plan.md) — implemented
- [specs/001-streaming-pipeline/tasks.md](001-streaming-pipeline/tasks.md) — implemented
- [specs/002-gpu-scheduling/spec.md](002-gpu-scheduling/spec.md) — **COMPLETED** (2026-06-17): all 17 tasks done
- [specs/002-gpu-scheduling/plan.md](002-gpu-scheduling/plan.md) — **COMPLETED**
- [specs/002-gpu-scheduling/tasks.md](002-gpu-scheduling/tasks.md) — **COMPLETED**
- [specs/003-sliding-window-asr/spec.md](003-sliding-window-asr/spec.md) — implemented (8cc31ab)
- [specs/004-comprehensive-timeline/spec.md](004-comprehensive-timeline/spec.md) — **UNIFIED SPEC** (time base + comprehensive timeline + protocol layer). Implemented (all phases 1–12). Supersedes 005 and 007.
- [specs/006-web-ui/spec.md](006-web-ui/spec.md) — draft (WebSocket client UI, real-time visualization, Phase 1 MVP scope)
- [specs/006-web-ui/plan.md](006-web-ui/plan.md) — draft
- [specs/006-web-ui/tasks.md](006-web-ui/tasks.md) — draft

## 7. Immediate next step

Specs 001, 002, 003, 004, and 006 (Web UI MVP) are complete, verified, and committed.

- **Codebase hardening** — complete. All P0/P1 items from 2026-06-21 evaluation executed:
  - GitHub Actions CI (CUDA 12.5, cmake + ctest + Python lint)
  - CUDA kernel unit tests (`test_kernels`: 13/13 passed, GPU vs CPU reference)
  - Level-based logging (`core/log.h`) replacing raw `fprintf(stderr)`
  - WebSocket server file-static elimination (`serve_server`/`serve_factory`/`pss_list_head` → instance members)
  - OnText JSON key exact matching (fixes `end`/`flush`/`reset` false positives)
  - GPU telemetry default disabled (1.0 → 0.0)
  - VAD model path migration (`models/asr/` → `models/vad/`)
  - README env var table + Python test CTest registration + protocol envelope unwrapping in web UI
- **Spec 004 — Protocol Layer**: Implemented. Phases 7–12 complete. Web UI (`app.js`) now includes `unwrapEnvelope()` for Spec 004 topic-based protocol envelopes. Integration test (`ws_ui_integration_test.py`) uses `unwrap_envelope()` for all WS message parsing.
- **Full-length streaming verification**: 2026-06-21. 3615 s (1 hr) audio pushed through real WebSocket → 382.0 s wall = **9.46× real-time**. All three tracks (ASR/diarization/VAD) cover 100 % of the audio, no crash, no clock drift, no data loss. Achieved 9.25× on a consecutive warm-GPU re-run and 5.82× on a cold-start run, confirming model-load overhead is one-time.
