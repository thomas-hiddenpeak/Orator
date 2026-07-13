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

- **Last updated**: 2026-07-13 (Spec 013 contract and product-surface gates established)
- **Branch**: `master`
- **Constitution**: v1.6.0
- **Product closure**: **OPEN / NOT ACCEPTED**. No current artifact proves the
  Spec 013 full-session 90 percent business-accuracy gates. Historical
  "closing" runs remain component stability and numerical-fidelity evidence,
  not product acceptance.

---

## 1. What Orator is

A real-time, edge-deployed (Jetson Orin / Thor) auditory pipeline, **pure C++/CUDA with
zero runtime third-party dependencies**. It ingests a live mono-audio stream over
WebSocket and produces a comprehensive timeline that carries both **speaker
separation** and **ASR transcript** content, one track per pipeline, on one
absolute time base.

## 2. Current phase

**Spec 013 is the active approved closing-validation work.** It does not replace the
implemented feature contracts in Specs 001-012; it defines the architecture
corrections, complete reference review, and conjunctive evidence required before
the combined product can be accepted. Spec 004 remains the feature specification
for time base, comprehensive timeline, and protocol behavior, but its claimed
completion is under a code-compliance review because the current production path
does not satisfy all Article III details.

Spec 004 covers:
- Time base system (`core::TimeBase`, three consistency principles, wall clock anchor)
- Comprehensive timeline (native stateful, revisable, diarization-driven view split)
- Protocol layer (topic-based registration, schema registry, QoS, storage backends)

Spec 005 (time base) and Spec 007 (protocol layer) were merged into Spec 004 and
deleted (2026-06-18). Their historical task status does not override the current
code findings recorded below.

The system runs three active producer pipelines —
diarization (who/when), ASR (what/when), and VAD speech-activity detection — each
feeding one **native, revisable comprehensive timeline** on a single absolute
seconds scale. `AuditoryStream` now owns one immutable session `TimeBase` and
injects it into each private cache, worker, and retained audio store. Pipeline
records now commit as typed `ComprehensiveTimeline` evidence before protocol
serialization: ASR reads VAD snapshots there, and forced alignment subscribes
to finalized ASR records there. A registered `BusinessSpeakerPipeline` consumes
raw typed evidence, owns speaker choice/text projection/gap policy, and writes a
separate revisable `business_speaker` track. `ComprehensiveTimeline` is now a
pure thread-safe typed store; raw ASR and alignment records are append-once and
reject conflicting same-ID deposits.

### Target pipeline responsibility boundaries

- **ASR** outputs ONLY plain transcript text + its own time codes. It has **no**
  speaker awareness and never attributes speakers. ASR now includes `text_id` in
  incremental messages for stable in-place segment tracking.
- **Diarization** outputs ONLY its own speaker identities + time codes. It never
  attributes text.
- **Comprehensive timeline** stores and aligns typed tracks on the common base.
  The registered business-speaker fusion pipeline derives the user-facing
  attribution track; the container itself does not choose a speaker or fill
  missing evidence.

**2026-07-13 Phase 1 verification**: the configured CTest suite passed 51/51.
A 120 s, rate=1 real-WebSocket run with committed `orator.toml`
(`/tmp/orator_spec013_t011_120s.json`) produced 25 diarization, 10 ASR, 39 VAD,
10 alignment, and 25 business-speaker entries. Every alignment/business
`text_id` resolved to one raw ASR record, business spans stayed inside their
source span, the business track exactly matched the compatibility
`comprehensive` view, and each source text was reconstructed byte-for-byte from
its business slices. All seven active extents ended at 1,920,000 samples with
zero gap; `timebase_reconciled`, `timebase_ok`, and `wall_clock_ok` were true.
The single-reader client captured 120 continuous `tegrastats` samples and
finished in 121.214 s (0.990x). This is architecture/transport evidence, not
contextual accuracy evidence.

**2026-07-13 contract/UI verification**: CMake now registers 53 tests (51 C++,
one Node browser-model test, and one real-WebSocket integration test). The
registered WS gate uses an isolated generated TOML and the sole unified socket
client to run 12 s of canonical speech plus 30 s of generated zero PCM; both
had no mechanical contract errors, and silence produced no live or terminal
ASR/business text. A real Chromium 12 s file run passed live rows, flush/end,
terminal ID/extent checks, exact JSON download, persisted-session lookup and
exact reload, deliberate server restart with clean reconnect, fake-device
microphone capture, and desktop/mobile screenshots with no unexpected browser
errors. This is short-path product-contract evidence, not the required physical
microphone, full-session context review, or accuracy result.

## 3. Component status

| Component | Status | Notes |
|---|---|---|
| Streaming diarization (Sortformer) | Numerical oracle verified; business accuracy open | Incremental engine features match the stored NeMo fixtures at the recorded tolerances. The accepted runtime TOML profile and full-session final business attribution do not yet have a closing-grade oracle/context result. Full reviews record major late-session attribution failures, so `industrial-grade` is not a supported current claim. |
| Native Qwen3-ASR engine | Numerical oracle verified; semantic closure open | Stored stage fixtures report mel 3.9e-3, encoder 1.3e-3, and decoder argmax parity. These numerical gates do not establish the Spec 013 full contextual semantic or silence-hallucination gates. Pure bf16 compute. |
| Forced alignment (Qwen3-ForcedAligner-0.6B, Spec 009) | Implemented; numerical and prior WS evidence recorded | The registered `AlignWorker` consumes typed finalized ASR records from `ComprehensiveTimeline`, reads the matching retained audio span, deposits a typed alignment group, then mirrors `align/units` to protocol and WebSocket. Partials are never aligned. Stage-level torch-oracle checks and the 2026-06-30 60-minute real-WebSocket run reported 119/119 segment coverage, 13,594 units, no bounds/monotonicity failures, and no crash after the CUDA grid-stride fix. These historical results establish the aligner implementation, not current Spec 013 product closure; repeatable full-session acceptance remains open. |
| WhisperMel / BPE tokenizer / sharded safetensors loader | ✅ Verified | Unit-tested. |
| Decoupling (interfaces + registry) | Contract corrected; product acceptance open | Model interfaces and registry construction are in place. VAD→ASR, ASR→forced-align, and raw evidence→business speaker now flow only through typed `ComprehensiveTimeline` reads/subscriptions. The registered `business_speaker` pipeline owns fusion policy and writes its own track; protocol topics mirror committed records for persistence and transport. Full product validation remains open under Spec 013. |
| `OverlapTimelineMerger` / `ITimelineMerger` | 🗑️ Removed | The old one-shot max-overlap merger and its orphaned interface were deleted — superseded by `ComprehensiveTimeline` (Spec 004). |
| WebSocket server (libwebsockets v4.3.3) | ✅ Refactored | Replaced hand-rolled POSIX WS with libwebsockets (multi-client, RFC 6455/7692). Eliminated file-scope static variables (`serve_server`, `serve_factory`, `pss_list_head`) → instance members via `lws_context_user`. Thread-safe `SendText` with wakeup/cancel-service. ServeOnce mode for unit tests. |
| ASR + WS integration | Implemented; full-session acceptance open | `AuditoryStream` owns one private `PipelineAudioCache` per active producer and uses separate worker threads. One session-owned `TimeBase` is injected into all active stores and workers. Final ASR live emission and its typed sink reuse one `text_id`; partial rejection emits a matching retract, and the terminal ASR track serializes the ID. ASR reads immutable VAD evidence snapshots from `ComprehensiveTimeline`; forced alignment consumes finalized ASR records there. Registered WS/Node tests and a real Chromium run verify short-path revision/export/reconnect/UI convergence. Full repeatability and contextual accuracy remain open. |
| Incremental KV-cache ASR streaming (Spec 003) | ✅ Implemented, verified, committed (8cc31ab); params refined 2026-07-03 | Persistent KV cache + prefix caching + chunk-local windowed encoder; partial-emission every 1 s via WebSocket. Full 1hr CER 16.1% / 6.22x; beats production Silero-VAD at every scale. **Current params**: `kStreamWindowMel=100` (1 s), `max_new_tokens=32`, `unfixed_chunks=2`, `unfixed_tokens=15`, `segment_sec=24.0`, `vad_min_overlap_sec=0.12`. 2026-07-03 real WS `test.mp3` 600 s A/B after the VAD-overlap filter: `segment_sec=24` produced 49 ASR finals vs 67 at 12 s, with the same final comprehensive count (115) and better `To C` wording; default restored to 24 s for ASR semantic stability. |
| Revisable comprehensive timeline (Spec 004) | Container/fusion ownership corrected; acceptance open | `ComprehensiveTimeline` stores typed diarization, ASR, VAD, alignment, and business tracks and publishes immutable snapshots/typed updates. `BusinessSpeakerPipeline` owns align-aware projection, gap fill, and speaker-support diagnostics. The terminal `comprehensive` field is a compatibility alias serialized from the exact stored `business_speaker` entries. Full contextual acceptance remains open. |
| Reusable common time base (Spec 004) | Session ownership and final reconciliation implemented; acceptance open | `AuditoryStream` owns one immutable `TimeBase` and injects it into every active private cache, worker, and retained audio store. Finalization reconciles exact sample extents for input, diarization, speaker identity, ASR, VAD, alignment, and business speaker; focused tests and the 2026-07-13 120 s real-WebSocket run reported zero gaps. Full-session repeatability remains open under Spec 013. |
| Pipeline protocol layer (Spec 004) | ✅ Implemented | Phases 7–12 complete: data types (topic.h, schema.h), pipeline registry, topic router, storage layer (MEMORY + DISK), ProtocolTimeline integration, WS v2 envelope with describe command, --storage-disk-path flag. 25/25 tests pass. |
| Streaming validation | Registered short-path gate; closing runs open | `tools/verify/py/ws_unified_test.py` has one socket reader, correlates command responses without competing reads, captures continuous `tegrastats`, and rejects mechanical live/final, typed-track, ID, alignment, VAD/diar, and extent violations. `test_ws_contract` now starts the real server with isolated TOML storage and runs canonical speech plus generated silence through that sole client. Structural checks never assign semantic accuracy; 120/360/600/full contextual gates remain open. |
| Logging system | ✅ Include-level `core/log.h` | Level-based macros (`LOG_DEBUG`/`INFO`/`WARN`/`ERROR`) with compile-time floor (`ORATOR_LOG_LEVEL`) and runtime env-var gate. All 14 `fprintf(stderr)` calls in src/ replaced. |
| CUDA kernel unit tests | ✅ `test_kernels`: 13/13 passed | GPU kernel operations (Add, Multiply, NormalizeVector, CosineSimilarity, BatchCosineSimilarity) validated against CPU reference; includes edge cases (zero, single-element, large 1M vectors). |
| CI pipeline | ✅ GitHub Actions | `.github/workflows/ci.yml`: CUDA 12.5, CMake build + ctest + warning check + Python syntax verification. Triggered on push/PR to master. |
| Test suite | 53 configured CTest entries | The suite contains 51 C++ tests plus `test_ws_contract` and `test_web_model`. It covers canonical time-base ownership, exact extents, append-once raw records, raw/business isolation, stable IDs, typed ordering, immutable/active VAD evidence, endpoint/retract behavior, protocol mirrors, real transport speech/silence, and deterministic browser-state convergence. Playwright and physical-microphone acceptance remain outside CTest, and no automated result substitutes for the 556-row review. |
| Diar tail parameter experiments | ❌ No accepted fix | 2026-07-10 TOML experiments used `diar_evidence_probe` on full `test.mp3` for strict onset/offset, `min_dur_on=1.2`, `min_dur_on=2.0`, `chunk_left_context=2`, `chunk_right_context=0`, and `left2_right0`. Threshold/min-duration changes deleted evidence without recovering the correct speaker; context variants did not solve 3270-3304 s and some removed the small local-2 hint at 3299.76 s. NeMo full-length reference on the same audio produced the same hard-window spk3 bias (`3270-3304.5`: spk3 313/431 frames; `3240-3360`: spk3 1013/1500 frames). `test_diar_stream` still passes against the stored NeMo oracle sample (`max_abs=0`, `mean_abs=0`). See Spec 012 `diar-tail-toml-experiments-2026-07-10.md`. |
| TitaNet tail voiceprint review | ❌ No accepted override | 2026-07-10 orthogonal speaker-embedding review used `speaker_embedding_probe` on full `test.mp3` with 600 s, 60 s, and 30 s buckets. The hard-window `L3@3270-3300` bucket remains closest to historical L3 (`L3@3300-3330=0.762`, historical L3 up to 0.724) while best non-L3 alternatives are lower (`L0=0.440`, `L1=0.424`, `L2=0.321`). This rejects direct TitaNet override for 3270-3304 s. See Spec 012 `titanet-tail-evidence-2026-07-10.md`. |
| OnText protocol matching | ✅ Fixed | Substring `text.find("end")` → JSON key `text.find("\"end\"")` to prevent false positives on partial matches. Same for reset/flush. |
| GPU telemetry default | ✅ Changed | `gpu_telemetry_interval_sec = 0.0` (was 1.0); disabled by default, opt-in via `ORATOR_GPU_TELEMETRY_SEC`. |
| VAD model path | ✅ Migrated | `models/asr/silero_vad.safetensors` → `models/vad/`. Updated 6 file references across test, include, and tools. |
| Web UI (Spec 006) | Contract-hardened; graphical timeline and final acceptance open | Static serving, modular state/router, exact PCM file framing, microphone capture, live transcript/evidence, telemetry, developer status, speaker naming, saved sessions, reconnect, authoritative terminal/load rebuild, and exact JSON export are implemented. Node tests and a real Chromium run verify the short path. The main timeline is currently formatted JSON, not the previously documented four-lane Canvas; graphical time-axis controls, physical microphone evidence, and Firefox/Safari evidence remain open. |
| Configuration consistency | Precedence corrected; lower-level switches open | Startup now applies defaults, TOML, environment, then CLI, with focused precedence coverage. Timeline gap-fill moved from `ORATOR_TIMELINE_NO_GAPFILL` to typed `[timeline].gap_fill_enabled`. Several lower-level behavioral switches still call `getenv()` below the typed config layer, and resolved-configuration capture remains open. |
| Session persistence UI (Spec 004 T135) | Implemented and browser-verified | `SessionStore`, sessions/load RPCs, and the Web UI history panel are active. Metadata parsing now handles the current `audio_sec` field plus legacy `audio_duration`; a real Chromium run finalized, listed, and reloaded an exact 12 s terminal document. |
| ISpeakerEmbedder (core/stages.h) | ✅ Active in Spec 010 | Interface declares a fixed-dimension speaker embedding extractor. Runtime implementation: `model::TitaNetEmbedder`, wired into the diarization pipeline by `SpeakerIdentityStage` when `[speaker].enable=true` and `model_dir` is set. |
| ISpeakerRegistry (core/stages.h) | ✅ Active in Spec 010 | Interface declares a persistent enrolled-speaker registry with 1:N matching. Runtime implementation: `model::SpeakerDatabase`, loaded/saved through `[speaker].registry_path` and used by `SpeakerIdentityStage` for global speaker ids. |
| ISink (core/stages.h) | 🔒 Retained, partially active | Interface for terminal timeline consumers. The runtime uses `Emit` callbacks (std::function) instead for primary flow, but a concrete implementation `JsonSink` exists in `include/io/json_sink.h` and `src/io/json_sink.cc` for JSON serialization to streams. Retained as a contract option for non-callback consumers. |
| ComprehensiveTimeline typed subscriptions | Implemented; acceptance open | The container commits typed records under lock, then emits typed evidence updates after commit. Readers obtain immutable snapshots or ID-keyed records. Focused tests cover callback ordering, unsubscribe behavior, VAD snapshot immutability, append-once conflicts, reset, and raw/business isolation. Runtime transport and full-session convergence remain Spec 013 gates. |

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
- Historical run: 25 diarization segments + 27 transcript utterances on one
  time base; transcript matches the verified engine's output. Current
  comprehensive snapshots preserve ASR `text_id` boundaries and split them
  through diarization ownership rather than grouping them into speaker turns.

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

**Historical finding (2026-06-25)**: the then-current protocol subscription plus local VAD cache eliminated the O(N²) `Replay()` calls on the ASR hot path. **Wall time ≈ audio duration (3616s vs 3615s)**. Diar track accuracy was 77.3% (diar track) / 67.0% (comprehensive view); the 600 s evaluation reported 92.8% diar track accuracy. This describes the measured implementation at that date, not the current evidence-flow architecture.

**Current implementation**: ASR reads an immutable typed VAD snapshot and monotonic processed horizon from `ComprehensiveTimeline`. Protocol messages mirror the committed VAD evidence and are not a private runtime data bus.

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

**Historical performance note**: the 2026-06-25 VAD cache change replaced O(N²) `Replay(0.0)` calls and produced wall time near audio duration (3616 s vs 3615 s). The current implementation preserves the O(1) immutable-snapshot read through `ComprehensiveTimeline` without a protocol subscription.

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

- [.specify/memory/constitution.md](../.specify/memory/constitution.md) — v1.6.0 (one canonical session clock; supplemental test provenance)
- [specs/001-streaming-pipeline/spec.md](001-streaming-pipeline/spec.md) — implemented
- [specs/001-streaming-pipeline/plan.md](001-streaming-pipeline/plan.md) — implemented
- [specs/001-streaming-pipeline/tasks.md](001-streaming-pipeline/tasks.md) — implemented
- [specs/002-gpu-scheduling/spec.md](002-gpu-scheduling/spec.md) — **COMPLETED** (2026-06-17): all 17 tasks done
- [specs/002-gpu-scheduling/plan.md](002-gpu-scheduling/plan.md) — **COMPLETED**
- [specs/002-gpu-scheduling/tasks.md](002-gpu-scheduling/tasks.md) — **COMPLETED**
- [specs/003-sliding-window-asr/spec.md](003-sliding-window-asr/spec.md) — implemented (8cc31ab)
- [specs/004-comprehensive-timeline/spec.md](004-comprehensive-timeline/spec.md) — **UNIFIED SPEC** (time base + comprehensive timeline + protocol layer). Implemented (all phases 1–12). Supersedes 005 and 007.
- [specs/006-web-ui/spec.md](006-web-ui/spec.md) — **In progress; contract hardening browser-verified 2026-07-13**. The modular ES client routes all known protocol/RPC types, maintains stable-ID live state, treats terminal/loaded documents as authoritative, shows device/pipeline telemetry and developer status, supports file/microphone input, speaker naming, saved sessions, reconnect, and exact export. Dependency-free Node tests and a real Chromium 12 s flow passed. The main timeline currently displays formatted authoritative JSON; the graphical multi-track time axis, physical microphone evidence, and Firefox/Safari evidence remain open, so the historical 16/16 and four-lane Canvas claims are withdrawn.
- [specs/006-web-ui/plan.md](006-web-ui/plan.md) — implemented
- [specs/006-web-ui/tasks.md](006-web-ui/tasks.md) — implemented
- [specs/011-observability/spec.md](011-observability/spec.md) — **Implemented** (2026-06-30): offline [rerun](https://rerun.io) visualization, kept entirely in `tools/` (no runtime third-party dep, Art. I). **Phase 1**: `tools/verify/py/ws_unified_test.py` captures the runtime's periodic `gpu_telemetry`/cursor WS samples into a `telemetry` array; `tools/observability/timeline_to_rerun.py` keys diarization/comprehensive lanes by the global `speaker_id` (`spk_N`) + per-pipeline RTF lanes. **Phase 2 (comprehensive dashboard)**: `TegraSampler` records a continuous `device_series`; the exporter renders six namespaced dimensions on one `audio_time` axis — `pipelines/*`, `comprehensive/<id>` swimlanes, `scheduler/<pipe>/{rtf,compute_sec,active,cuda_priority}`, `cursors/<pipe>/{position_sec,pending_sec}`, `device/{mem,cpu,gpu,temp,power}/*` (extended tegrastats parse; Orin `GR3D_FREQ` optional, omitted on Thor), and `session/summary` — laid out by a `rerun.blueprint` persisted in the `.rrd`. Methodology + best practices in `tools/observability/README.md`. **Config fix**: nested `[telemetry.cursor]` was never read (`config["telemetry.cursor"]` literal-key lookup) → now `config["telemetry"]["cursor"]`, with a `test_config` regression. Validated on a `rate=1` 120 s run: 125 gpu + 125 cursor + 126 device samples, six dimensions populated, stream_rt 0.964×, ctest 47/47, zero warnings. Follow-ups: live WS→rerun consumer, full-hour acceptance recording.
- [specs/010-speaker-id/spec.md](010-speaker-id/spec.md) — **Implemented, with Phase H experiment not accepted as accuracy fix; local-diar operating profile restored**: speaker identity (TitaNet-Large voiceprint enrollment / re-identification as a post-diarization stage inside the diar pipeline, Art. III). **Phase A complete & committed**: A1 acquire+convert weights → `models/speaker/titanet_large.safetensors` (108 tensors); A2 NeMo oracle (`tools/reference/titanet_oracle.py`, isolated `tools/.venv-nemo`); A3 pure C++/CUDA `model::TitaNetEmbedder` (`include/model/titanet_embedder.h` + `src/model/titanet_embedder.cu`, time-major [T,C]: mel+per_feature → 5-block ContextNet encoder → attentive statistics pooling → 192-d, F32 weights); A4 `test_titanet` validated vs NeMo oracle (**span cosine 1.000000/0.999999/1.000000, cross-span matrix to 4 decimals; ctest 46/46, no warnings**). **Phase B complete & committed**: `pipeline::SpeakerIdentityStage` (clean-segment gate + per-local embed/match/enroll via `SpeakerDatabase` + revisable local→global map), wired into the diar pipeline behind a `DiarizationWorker` segment-processor hook + `[speaker]` config; diar message/track expose a backward-compatible `speaker_id` field. **2026-07-06 validation**: Phase H conservative cross-session candidate (`/tmp/orator_phaseh_full.json`) was rejected by context review [local-diar-review-2026-07-06.md](010-speaker-id/local-diar-review-2026-07-06.md): it reduced wrong late globals into local-only gaps but did not restore attribution. Follow-up restored Sortformer local-diar runtime tuning to the async/no-reset profile (`spkcache_update_period=188`, `chunk_right_context=1`, `spkcache_sil_frames=3`) in `orator.toml`; lower-level `SortformerConfig` defaults remain tied to the existing NeMo oracle fixture. Full-length real WS `/tmp/orator_full_async_default_20260706.json`: 3615 s audio, 3618.487 s wall, stream RT 0.999x, diar 773, ASR 288, VAD 972, 3611 tegrastats samples, stable 4 global ids and no local-only gaps; context review [local-diar-default-188-review-2026-07-06.md](010-speaker-id/local-diar-default-188-review-2026-07-06.md) accepts the stable operating profile but records residual rapid-turn fragmentation in 3000-3615 s and an ASR repeat burst at 1927-1944 s.
- [specs/012-evidence-fusion-timeline/spec.md](012-evidence-fusion-timeline/spec.md) — **Runtime candidate validated (2026-07-08); tail evidence reviewed and support diagnostics added (2026-07-09)**: evidence-first comprehensive timeline fusion plus TOML-gated runtime adoption. `tools/verify/py/fusion_audit.py` and `speaker_business_review_packet.py` read frozen `ws_unified_test.py` JSON packages, audit ASR/diar/VAD/align consistency, and emit candidate/business-turn views without mutating captured tracks. After the 2026-07-07 review showed forced alignment alone did not recover speaker-business accuracy, 2026-07-08 fixes added local-speaker drift/competing-identity split and backfill, per-entry comprehensive `speaker_id`, and `[timeline]` align-run split parameters. Full-length real WS run `/tmp/orator_timelinefusion_full_20260708.json`: 3615.0 s audio, 3618.74 s wall, stream RT 0.999x, diar 773, ASR 288, align 288/288. Fusion audit `/tmp/orator_timelinefusion_full_20260708_fusion_bt_timeline.json`: business_turns=728, unknown 171.860 s (4.75%), no mechanical audit issues. Context-aware review [drift-epoch-review-2026-07-08.md](012-evidence-fusion-timeline/drift-epoch-review-2026-07-08.md) follows [speaker-business-method.md](012-evidence-fusion-timeline/speaker-business-method.md): major known full-length speaker-business regressions are materially recovered, with residual short-boundary artifacts and conservative unknown spans. Follow-up full-length candidates [refresh0-context-review-2026-07-08.md](012-evidence-fusion-timeline/refresh0-context-review-2026-07-08.md) rejected `spkcache_refresh_rate=0`, `histctx 300/40/5`, and a context low-support speaker inheritance heuristic: refresh-0 did not materially improve the tail, and the inheritance rule fixed 3270-3304 s while regressing 1200-1320 s. Tail evidence review [tail-evidence-review-2026-07-09.md](012-evidence-fusion-timeline/tail-evidence-review-2026-07-09.md) added `diar_evidence_probe` and confirmed that 3270-3304 s is a bottom-diarization hard spot already present in older closing packages; `reset_period_sec=600/120/60`, `use_silence_profile=true`, coverage-to-unknown, and gap-fill limits are not accepted fixes. Follow-up [speaker-support-diagnostics-2026-07-09.md](012-evidence-fusion-timeline/speaker-support-diagnostics-2026-07-09.md) implements the recommended uncertainty-aware direction by surfacing per-entry speaker-support metrics in runtime JSON and Web UI without changing attribution. Follow-up [titanet-tail-evidence-2026-07-10.md](012-evidence-fusion-timeline/titanet-tail-evidence-2026-07-10.md) tested orthogonal TitaNet voiceprint evidence and rejected a direct override for 3270-3304 s because 600 s, 60 s, and 30 s bucket views all keep the target L3 spans closest to historical L3. Diar-only or script-derived percentages remain diagnostics, not acceptance results.
- [specs/013-industrial-closing-validation/spec.md](013-industrial-closing-validation/spec.md) — **In progress, approved 2026-07-13**: defines the 90 percent full business-view gates, complete 556-turn reference ledger, architecture/configuration remediation, frozen-evidence upper-bound decision, two repeat full runs, and the distinction between canonical-scene acceptance and a broader industrial-readiness claim. No closing status is implied until all tasks and gates are completed.

## 7. Immediate next step

Continue [Spec 013](013-industrial-closing-validation/spec.md) by completing the
remaining configuration migration and reproducibility manifest, then freeze and
run the 120/360/600/full baseline packages. Build the complete 556-row reference
ledger and perform the mandatory item-by-item contextual review before any
accuracy claim or candidate selection.
Do not resume parameter tuning or make a product-closure claim before the
architecture, reference-ledger, and reproducibility gates pass. The bullets
below are historical implementation and measurement records; none independently
satisfies Spec 013 acceptance.

- **Spec 012 speaker-business recovery — historical evidence line** (2026-07-09).
  The latest runtime candidate fixes the known full-length regression windows by
  combining local drift epoch handling, per-entry comprehensive `speaker_id`, and
  align-run splitting near diarization boundaries. Continue from
  [drift-epoch-review-2026-07-08.md](012-evidence-fusion-timeline/drift-epoch-review-2026-07-08.md):
  residual work is limited to short-boundary artifacts, conservative `unknown`
  spans, and broader context-aware review, not a return to diar-only script
  percentages. The follow-up
  [refresh0-context-review-2026-07-08.md](012-evidence-fusion-timeline/refresh0-context-review-2026-07-08.md)
  rejects further cache-refresh tuning and naive context inheritance for this
  round; the remaining 3270-3304 s failure originates in sparse bottom diar
  evidence, not Web UI rendering or business-turn serialization. The
  follow-up support-diagnostics change exposes this weakness in live/final
  comprehensive entries but is not yet an accepted accuracy fix; acceptance
  still requires a full-length real WebSocket run and context-aware review under
  `speaker-business-method.md`.
- **Spec 010 speaker identity — cross-session identity finalized** (commits 38cdf51, 9c02862, 17f8d92, 06875c3, 5f301ba). The voiceprint stage now assigns a persistent GLOBAL id to every diar segment. Design corrections, all validated through the REAL streaming path (rate=1) and judged by **context-aware per-segment semantic comparison vs `test.txt`** (Test Review Protocol — accuracy is NOT taken from script metrics):
  - **Trust the diarizer's within-session separation**: each local slot resolves to its own global id; same-session slots can never collapse to one id (`SpeakerDatabase::MatchExcluding`). Per-segment re-matching was removed (it collapses similar voices to the dominant centroid).
  - **Cross-session strengthening**: each global's centroid is the mean of the best references of all slots mapped to it across sessions, so a returning speaker re-matches reliably (match cosine ~0.55 → 0.7–0.87).
  - **Registry-level de-duplication, uncapped**: `MergeReconcile` merges two globals only when their centroids are confidently the same person (cosine > 0.70; a stricter 0.85 for two globals that ever co-occurred in one session, since the diarizer judged them distinct), and `SpeakerDatabase::Remove` deletes the duplicate so the registry holds exactly one entry per real speaker. The registry is never capped — it is designed to recognise many speakers (≥200) across sessions.
  - **Test-method correction**: validate speaker accuracy through the real `rate=1` stream (a `rate=0` shortcut ages clean spans out of the embed-retain window before they are delivered, starving enrollment). Full 60-min run: 4 real speakers → exactly 4 stable global ids (spk_0=朱杰, spk_1=唐云峰, spk_2=徐子景, spk_3=石一) across all 6 reset sessions; clear/substantive turns attributed correctly (~90% on 0–600 s and 1800–2400 s), the 2400–3600 s region remains the hard part — confirmed by an independent fresh run of that segment to be the **audio's inherent rapid-speaker-exchange difficulty**, not continuous-run degradation. ctest 47/47, no warnings.
- **Spec 010 Phase H — conservative cross-session identity experiment** (2026-07-06). Implemented but **not accepted** as an accuracy improvement. All new thresholds are in `[speaker]` TOML; defaults preserve current behaviour. The opt-in conservative profile requires multiple clean references before reset-session re-identification and can keep unmatched later-session slots local-only. Full-length real WebSocket candidate completed with tegrastats (`/tmp/orator_phaseh_full.json`, 0.999x real time), but context-aware review [local-diar-review-2026-07-06.md](010-speaker-id/local-diar-review-2026-07-06.md) found that it only turned some wrong late global ids into local-only labels; it did not fix diarizer local-slot fragmentation/attribution in 600-1800 or 3000-3615. Next work must isolate local diar segmentation quality before global identity stitching.
- **Spec 010 local-diar operating profile restore** (2026-07-06). The accepted runtime profile now uses async/no-reset with Sortformer tuning (`188/1/3`) explicitly in TOML. Full-length real WS validation (`/tmp/orator_full_async_default_20260706.json`) succeeded at 0.999x real time with 3611 tegrastats samples and stable 4 global ids. Context-aware review accepts this as the current operating profile, but not as a complete diar quality fix: short-turn/tail fragmentation remains in 3000-3615 s, and ASR still has a repeat burst around 1927-1944 s. A direct compile-time default change was rejected because it broke `test_diar_stream` NeMo oracle equivalence; runtime parameters stay in TOML.

- **Historical full-pipeline stability validation — all features on** (2026-06-30). A single real `rate=1` 60-min WebSocket stream with **diarization + ASR + VAD + speaker identity + forced alignment all enabled**: 0.999× real-time, no crash, no OOM. Tracks: diar 729 (RTF ~100×), asr 119 (RTF ~1.25×), vad 972, **align 119 = 100% of ASR segments** (13594 char-level units, 0 out-of-bounds / 0 non-monotonic, RTF ~35×). Speaker identity converged to 4 global IDs. This proves stability and alignment coverage for that commit; it did not perform the Spec 013 556-turn accuracy review and is not a product closing result.

- **Codebase hardening** — complete. All P0/P1 items from 2026-06-21 evaluation executed:
  - GitHub Actions CI (CUDA 12.5, cmake + ctest + Python lint)
  - CUDA kernel unit tests (`test_kernels`: 13/13 passed, GPU vs CPU reference)
  - Level-based logging (`core/log.h`) replacing raw `fprintf(stderr)`
  - WebSocket server file-static elimination (`serve_server`/`serve_factory`/`pss_list_head` → instance members)
  - OnText JSON key exact matching (fixes `end`/`flush`/`reset` false positives)
  - GPU telemetry default disabled (1.0 → 0.0)
  - VAD model path migration (`models/asr/` → `models/vad/`)
  - README env var table + Python test CTest registration + protocol envelope unwrapping in web UI
- **Spec 004 — Protocol Layer**: Implemented. Phases 7–12 and Phase 13 session persistence are complete. `SessionStore` metadata, sessions/load RPCs, and the Web UI history/reload path are unit- and browser-verified; current `audio_sec` plus legacy `audio_duration` metadata are supported.
- **Full-length streaming verification**: 2026-06-21. 3615 s (1 hr) audio pushed through real WebSocket → 382.0 s wall = **9.46× real-time**. All three tracks (ASR/diarization/VAD) cover 100 % of the audio, no crash, no clock drift, no data loss. Achieved 9.25× on a consecutive warm-GPU re-run and 5.82× on a cold-start run, confirming model-load overhead is one-time.
- **Python integration-test correction** (2026-07-13). The obsolete `test/run_py_test.py` claim is superseded by registered CTest `test_ws_contract`. Its process-only runner starts `orator_ws` with isolated TOML storage and invokes the sole `tools/verify/py/ws_unified_test.py` client for canonical speech and generated silence.
- **TOML config system** (2026-06-22; precedence corrected 2026-07-13). Runtime fields are broadly represented in `orator.toml`, and `ws_main.cc` now applies defaults → TOML → environment → CLI. Several lower-level environment-only switches remain, so configuration consistency is still open under Spec 013. See `include/io/config_reader.h`, `src/io/config_reader.cc`, `src/net/ws_main.cc`, and `orator.toml`.
- **VAD-gated ASR fix** (2026-06-22). VAD async-lag protection via segment-start confirmation check. ASR segments reduced from 43→18 (120s test). RTF improved 4.7→3.7. Parameters tuned: `asr_vad_trail_sec=1.0`, `vad_min_silence_ms=300`. See `src/pipeline/asr_worker.cc:61-141`.
- **Full-length verification (v7)** (2026-06-23). 3615s (1 hr) audio at 420× injection: **964s wall (3.75×)**, no crash, no data loss. 300s verification confirms 3 ASR segments cover 300s of audio (merging 90 VAD segments). Speed regression from 9.46× (pre-v7) due to VAD segment-start check keeping ASR segments open longer, causing more audio to pass through GPU processing. 120s test at 1× real-time still at RTF 3.7.
- **NeMo feature parity — silence profile, FIFO, dynamic pop_out_len** (2026-06-24). Ported three NeMo v2.1 streaming features to `streaming_sortformer.cc`:
  - `mean_sil_emb` in `CompressSpkcache`: cosine-similarity penalty against silence profile (`use_silence_profile` flag, gated off by default — v2.1 models opt in). `UpdateSilenceProfile()` already maintained per-chunk.
  - `spkcache_refresh_rate` (default 0): controls speaker cache refresh cadence in FIFO mode.
  - Dynamic `pop_out_len` + FIFO buffering: dual-path streaming update — sync (`fifo_len=0`, backward-compatible default) and FIFO async (`fifo_len>0`). `HostStreamState` gains lazy-init FIFO buffers (`fifo_embs`, `fifo_preds`), proper overflow handling, and NeMo-style pop-out calculation.
  - All three params wired through full config chain: `orator.toml [diarizer]` → `ConfigReader` → `AuditoryStream::Config` → `SortformerTuning` → `SortformerConfig`. See `include/model/streaming_sortformer.h`, `include/pipeline/auditory_stream.h`, `src/io/config_reader.cc`.
  - **Bug fixes**: `Sha1::Finalize()` padding fix for multi-block messages (≥ 56 bytes); `test_integration.py` eval_single indentation bug fix.
  - **Verification**: 600 s real-audio eval: diarization compute 6.7 s (89.5× RTF), ASR compute 164.2 s (3.65× RTF), wall_clock_ok. Speaker diarization accuracy 89.4% (weighted by duration) against test.txt ground truth. All 39/39 tests pass. Reference data (`ref_stream_total.f32`) regenerated for current build env.
