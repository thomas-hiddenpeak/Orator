# Changelog

All notable changes to Orator are documented here.

The format follows [Keep a Changelog](https://keepachangelog.com/) conventions,
and this project adheres to [Semantic Versioning](https://semver.org/).

Dates in `YYYY-MM-DD` format.

---

## [Unreleased]

### Changed
- VAD-gated ASR: async-lag protection via segment-start confirmation check.
  ASR segments reduced from 43→18 in 120s real-time test. RTF improved 4.7→3.7.
- `asr_vad_trail_sec` default: 1.5 → 1.0
- `vad_min_silence_ms` default: 120 → 300

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
