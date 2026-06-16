# Tasks 004 — Common Time Base + Revisable Comprehensive Timeline

- **Feature**: `004-comprehensive-timeline`
- **Spec**: [spec.md](spec.md) · **Plan**: [plan.md](plan.md)
- **Status**: Implemented. Phases 1–6 done. Core + revisions + timebase
  (3159b75 step 1, 673f95d step 2); VAD pipeline completed in Phase 5 (GPU
  detector + serialized `vad` track + dead-code cleanup), validated on the
  real WS path including live ASR self-revision behavior.
- **Constitution**: v1.2.1

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
  reconcile clean; CER 16.2% (unchanged). GPU stayed busy (59.7%, mean 38%, max
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

## Traceability

| Requirement | Tasks |
|---|---|
| FR1 common-base metadata | T030 |
| FR2 native timeline | T010, T020 |
| FR3 time-alignment attribution | T010, T040 |
| FR4 revision events | T010, T021 |
| FR5 WS revision push | T021, T041 |
| FR6 endpoint pipeline | T031, T061, T062 |
| FR7 final timeline | T020, T063 |
| FR8 endpoint numeric gate | T060, T061 |
| FR9 dead-code removal | T064 |

| Acceptance | Tasks |
|---|---|
| AC1 multi-speaker correct | T011, T040 |
| AC2 incremental | T011 |
| AC3 revision on change | T011, T021 |
| AC4 WS replay | T041 |
| AC5 endpoint pipeline | T031 |
| AC6 common-base meta | T030 |
| AC7 build/tests/schema | T050 |
| AC8 GPU detector, no stall | T061, T065 |
| AC9 endpoints in final timeline | T063, T065 |
| AC10 GPU vs oracle numeric gate | T060, T061 |
| AC11 dead code removed | T064 |

## Definition of Done
Native stateful comprehensive timeline with time-alignment attribution and
in-place revision; revisions pushed to WS; common-time-base metadata exposed;
endpoint detector as an independent third pipeline; multi-speaker attribution
correct on test.mp3; build + tests green; schema additive; docs updated; commit.
