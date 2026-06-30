# Changelog

All notable changes to Orator are documented here.

The format follows [Keep a Changelog](https://keepachangelog.com/) conventions,
and this project adheres to [Semantic Versioning](https://semver.org/).

Dates in `YYYY-MM-DD` format.

---

## [Unreleased]

### Added
- Observability dashboard (Spec 011 Phase 2) — the offline rerun module now
  presents a full **engineered dashboard** across six dimensions on one
  `audio_time` axis: pipelines (diar/asr/vad/align), comprehensive timeline
  (per-speaker swimlanes by global `speaker_id`), scheduler health
  (`scheduler/<pipe>/{rtf,compute_sec,active,cuda_priority}`), pipeline backlog
  (`cursors/<pipe>/{position_sec,pending_sec}`), continuous hardware telemetry
  (`device/{mem,cpu,gpu,temp,power}/*` from continuous `tegrastats`), and a
  session-summary document — laid out by a `rerun.blueprint` (persisted in the
  `.rrd`; `--no-blueprint` to opt out). `ws_unified_test.py` gained a
  `TegraSampler` that records a continuous `device_series`; `timeline_to_rerun.py`
  was restructured into namespaced entities with `_log_cursors`,
  `_log_device_series` (extended tegrastats parsing: SWAP/CPU/power rails/temps;
  Orin `GR3D_FREQ` optional, gracefully omitted on Thor), and
  `_log_session_summary`. A system-observation methodology + best practices were
  added to `tools/observability/README.md`. Validated on a `rate=1` 120 s run
  (125 gpu + 125 cursor + 126 device samples; six dimensions populated;
  stream_rt 0.964×).

### Fixed
- ASR segment finalization / decoder crash: in the VAD-gated path the segment
  was bounded only by the ~115 s KV-cache token cap, because the silence-
  confirmed close rarely fires in steady real-time ingest (the ASR processing
  head stays ahead of the lagging VAD progress horizon). A single utterance then
  grew until it approached the text decoder's ~1800-token illegal-access
  threshold, aborting the server (`asr_text_decoder.cu` GqaDecodeAttnKernel,
  illegal memory access) and dropping every WebSocket client. Re-introduced a
  time-based segment cap (`segment_sec`, default 24 s) in `EmitIncrementalChunk`
  so segments finalize regularly and the KV-cache never nears the crash
  threshold. Validated on a real `rate=1` 150 s WS run: 7 finals at 24 s
  cadence, 7 alignments, server stable (was: 0 finals, unbounded growth, crash).
- Config: the nested `[telemetry.cursor]` table was never read — `config_reader`
  used `config["telemetry.cursor"]` (a literal-key lookup that cannot match a
  TOML sub-table), so cursor progress telemetry could never be enabled. Now
  reads `config["telemetry"]["cursor"]`, with a `test_config` regression
  assertion.

### Added (Phase 1)
  [rerun](https://rerun.io) visualization, kept entirely in `tools/` with **no**
  runtime third-party dependency. `ws_unified_test.py` now captures the runtime's
  periodic `gpu_telemetry` (and cursor) WS samples into a `telemetry` array in
  the run JSON; `timeline_to_rerun.py` keys diarization/comprehensive lanes by
  the global `speaker_id` (`spk_N`), adds per-pipeline RTF / scheduling lanes
  (`gpu/<name>/rtf`, `gpu/<name>/active`) and per-track RTF summaries. New
  `tools/observability/README.md` documents the one-command workflow; telemetry
  intervals stay OFF by default and are enabled per-run via `[telemetry]` /
  `[telemetry.cursor]` in `orator.toml`. Validated on a `rate=1` 120 s stream
  (125 gpu_telemetry samples, stream_rt 0.964×, exported `.rrd`).

### Changed
- Speaker identity (Spec 010) — cross-session GLOBAL identity finalized. The
  voiceprint stage now assigns a persistent global id to every diarization
  segment, validated through the real `rate=1` WebSocket stream and judged by
  context-aware per-segment semantic comparison vs `test.txt` (Test Review
  Protocol). The diarizer's within-session separation is trusted (same-session
  slots never collapse to one id, `SpeakerDatabase::MatchExcluding`); per-segment
  re-matching was removed. Each global's centroid strengthens across sessions, so
  returning speakers re-match reliably. Full 60-min run: 4 real speakers → exactly
  4 stable global ids across all 6 reset sessions.
- VAD-gated ASR: async-lag protection via segment-start confirmation check.
  ASR segments reduced from 43→18 in 120s real-time test. RTF improved 4.7→3.7.
- `asr_vad_trail_sec` default: 1.5 → 1.0
- `vad_min_silence_ms` default: 120 → 300

### Fixed
- Forced alignment (Spec 009) now covers the FULL transcript. A bf16 GEMM generic
  fallback launched `grid.y = M` (rows); the audio tower feeds a whole alignment
  segment at once, so long segments (e.g. 77 s -> M = 147200) exceeded the CUDA
  grid y-dimension limit of 65535 and every long segment failed with
  `CUDA Error ... invalid argument` (the ASR never hit this — it streams bounded
  windows). `Bf16GemmGenericKernel` and `Im2ColKernel` now grid-stride over rows
  with grid.y capped at 65535 (zero behaviour change for bounded M). Real rate=1
  60-min stream: alignment coverage 2% -> 100% (119/119 ASR segments, 13594
  character units, 0 out-of-bounds / 0 non-monotonic).
- Speaker registry de-duplication (Spec 010): `MergeReconcile` merges two global
  ids only when their centroids are confidently the same person (cosine > 0.70;
  stricter 0.85 for two ids that ever co-occurred in one session, since the
  diarizer judged them distinct), and `SpeakerDatabase::Remove` deletes the
  duplicate so the registry holds exactly one entry per real speaker. The registry
  is never capped — designed to recognise many speakers (≥200) across sessions.

### Added
- TOML config system (`orator.toml`). All ~35 runtime parameters consolidated into
  8 sections with documented defaults. Loading order: compile-time defaults →
  CLI args → `orator.toml` → environment variables.
- `include/io/config_reader.h` / `src/io/config_reader.cc`: TOML config reader
  using toml++ (header-only, FetchContent, zero runtime deps).
- `AuditoryStream::Config` expanded from 15→35 fields, covering: server ports,
  ASR (VAD gating, KV-cache cap, ban steps, decode batch, profile),
  VAD (threshold, min speech/silence, padding, stream toggle),
  diarizer (max speakers, thresholds, delivery interval),
  storage, telemetry, debug (log level, timebase check, progress, GPU scheduling).
- All previously env-only params (`ORATOR_TIMEBASE_CHECK`, `ORATOR_ASR_PROFILE`,
  `ORATOR_STREAM_PROGRESS`, `ORATOR_LOG_LEVEL`, `ORATOR_GPU_SERIAL/CONCURRENT`)
  now settable via `orator.toml` and synced to environment for deep getenv() code.
- `max_audio_tokens` field in `AuditoryStream::Config` with propagation to
  `AsrWorker::Params` (was hardcoded).
- `vad_speech_pad_ms` propagation from Config to `GpuVad::Params`.
- `diar_deliver_interval_sec` propagation from Config to `DiarizationWorker::Params`.

### Fixed
- Protocol layer: `vad_state/event` subscription restored (was using polling path).
- IDLE skip: only skips confirmed VAD silence gaps (between known segments);
  past-last-segment audio now falls through to incremental processing.
- Code style: brace consistency for single-line if statements in `qwen3_asr.cc`.

---

## [Spec 004 — Protocol Layer + Timeline Consolidation] — 2026-06-21

### Added
- Protocol layer (Phases 7–12): topic-based pub/sub, schema registry, QoS levels,
  storage backends (memory + DISK), time index for replay, WS envelope.
- Session persistence (Spec 004 Phase 13): timeline JSON saved to DISK on Reset().
- Canvas timeline with zoom/pan (Spec 006 Phase 2).
- GitHub Actions CI: CUDA 12.5, cmake + ctest + Python lint.
- GPU kernel unit tests (`test_kernels`: 13/13, GPU vs CPU reference).
- Level-based logging (`core/log.h`) replacing raw `fprintf(stderr)`.
- WebSocket consecutive-process hazard fix (server_factory → instance members).

### Changed
- Pipeline consolidated onto `ComprehensiveTimeline`; `StreamTimeline` removed.
- Web UI timeline: canvas-based with shared time axis, overlap-aware row grouping,
  text wrapping, auto-inferred WS port.

### Fixed
- ASR incremental text diff check prevents redundant partial events.
- VAD model path migration (`models/asr/` → `models/vad/`).
- Diarization delivery via internal storage (not external StreamTimeline).
- WebSocket OnText JSON key exact matching (fixes `end`/`flush`/`reset` false positives).

### Performance
- Full-length (1 hr) streaming verification: **9.46× real-time**, no crash,
  no clock drift, no data loss. All three tracks cover 100% of audio.

---

## [Spec 003 — Sliding-Window Real-Time ASR] — 2026-06-18

### Added
- Qwen3-ASR incremental KV-cache streaming engine.
- VAD-gated segment boundaries with trailing window.
- `asr_stream_incremental_probe` for per-step cost measurement.
- `asr_encoder_chunk_probe` verifying windowed-encoder chunk equivalence.
- ASR VAD subscription: O(1) per-segment replay (was O(n)).
- KV-cache safety cap (`max_audio_tokens=1500`) preventing crash above ~1768 positions.
- Web UI MVP (Spec 006, T001–T008): microphone input, auto-flush, transcript display,
  timeline visualization.

### Changed
- ASR max_new_tokens: 32 → 32 (confirmed via probe).
- Decode batch: 32 → 4 (stops near EOS, improved streaming RTF 3.53×→4.99×).

### Fixed
- Remove 24s hard segment cap (VAD gate now primary segment delimiter).

---

## [Spec 002 — GPU Scheduling + Concurrency] — 2026-06-16

### Added
- GPU priority-index registry (foreground diarization, medium ASR, background VAD).
- Lock-free ASR on dedicated CUDA stream (production default).
- Full concurrency mode for all three pipelines (`ORATOR_GPU_CONCURRENT`).
- Periodic GPU scheduling telemetry (`gpu_telemetry` WS message).
- Mel spectrogram stream parameterization for Sortformer.
- GpuVad device-memory conversion (no managed memory).
- Periodically published VAD speaker-state events for UI LED indicator.

### Changed
- Default concurrency: serial → ASR-concurrent (diar blocked ~40→~31s wall).
- Diarization sortformer kernel stream routing (pre-encode, conformer, decoder).
- ASR worker + auditory stream GPU stream registration.

### Fixed
- Managed-memory SIGSEGV on Tegra (concurrentManagedAccess=0).
- CUDA graph capture under concurrency (disabled when streams diverge).

---

## [Spec 001 — Streaming Dual Pipeline] — 2026-06-10

### Added
- Initial pure C++/CUDA project with real-time speaker diarization.
- Threaded streaming dual-pipeline architecture (diarization + ASR).
- Native Qwen3-ASR engine with streaming KV-cache decode.
- Comprehensive timeline with per-pipeline tracks.
- Speaker-turn view (who said what when).
- Native GPU VAD using Silero model.
- LFS tracking for `*.safetensors`, `*.npz`, `*.npy` model weights.
- AGPL-3.0-or-later licensing.
- Constitution v1.0.0 + SDD workflow (constitution → spec → plan → tasks).
- Specification artifacts for Spec 001 (streaming pipeline), Spec 002 (GPU scheduling),
  Spec 003 (sliding-window ASR), Spec 004 (unified timeline + protocol).

### Changed
- OverlapTimelineMerger removed in favor of native ComprehensiveTimeline.
- Terminal VAD → Silero GPU VAD.

### Fixed
- WebSocket buffer overflow guard with high-rate injection stress tool.
- Tegra managed-memory segfault in streaming ASR.
- Lossless incremental window transitions for long-form ASR.
