# Spec 010 — Tasks (Speaker Identity)

Ordered, independently verifiable. `[ ]` todo · `[~]` in progress · `[x]` done.
Each task gates on build clean `-Wall -Wextra` + relevant test.

## Phase A — Voiceprint model + oracle
- [x] **A1** Acquired `titanet_large` (`nvidia/speakerverification_en_titanet_large`,
  NeMo .nemo 102 MB) → `models/speaker/titanet_large.safetensors` (108 tensors)
  via `tools/convert/convert_nemo_to_safetensors.py` (extended to auto-detect the
  torch-zip `archive/` prefix and tolerate a missing `byteorder` entry). Config
  at `models/speaker/titanet_large_config.yaml`. Full weight layout / forward
  blueprint recorded in plan.md §6.
- [x] **A2** NeMo Python oracle (`tools/reference/titanet_oracle.py`, runs in an
  isolated `tools/.venv-nemo` so it never touches the runtime tool venv): dumps
  per-span waveform / mel [80,F] / encoder [3072,F] / L2-normalized 192-d
  embedding to `models/reference/speaker/` (local, regenerated like other
  reference dumps). Sanity cosine matrix on test.mp3 spans 0/30/60 s:
  diag 1.0, span1–span2 0.54, span0 distinct — embeddings self-consistent.
- [x] **A3** `model::TitaNetEmbedder : core::ISpeakerEmbedder`
  (`include/model/titanet_embedder.h` + `src/model/titanet_embedder.cu`):
  time-major [T,C] forward — mel (reuse `MelSpectrogram` with the model's own
  `preprocessor.featurizer.{fb,window}` + per_feature norm) → 5-block ContextNet
  encoder (depthwise `DepthwiseConvKernel` + pointwise `LaunchSgemm` + BatchNorm
  (enc eps 1e-3) + ReLU + SqueezeExcite + residual) → attentive statistics
  pooling (global mean/std context → conv→ReLU→BN→tanh→conv → softmax-over-time
  → weighted mean⊕std, dec eps 1e-5) → BatchNorm + linear → L2-normalized 192-d.
  Weights F32 via `SafeTensorReader`/`UnifiedBuffer`, scratch via `DeviceScratch`.
  Builds clean (`-Wall -Wextra`, no warnings).
- [x] **A4** `test/unit/model/test_titanet.cc` (`test_titanet`): feeds the exact
  oracle waveforms, requires cosine(C++, NeMo) per span and reproduces the
  cross-span cosine matrix. **Measured: span cosine 1.000000 / 0.999999 /
  1.000000; cross-span matrix matches the oracle to 4 decimals.** ctest 46/46.

## Phase B — Identity stage
- [x] **B5** Clean-segment gate (`pipeline::SpeakerIdentityStage::IsClean`):
  duration >= min_embed_sec, diar mean-activity confidence >= cutoff,
  single-speaker (no other-local-speaker overlap beyond a tolerance), and
  VAD-confirmed (>= coverage fraction; skipped when VAD is unavailable). Also
  fixed `OnsetOffsetSegments` to compute `DiarSegment::confidence` as the mean
  per-frame activity over the span (was hard-coded 0.0), making the gate real.
- [x] **B6** Per-local-speaker embed + match/enroll: the longest fresh clean
  span per local speaker is read from a `RetainedAudioBuffer`, embedded with
  `TitaNetEmbedder`, blended into a per-local moving average, and matched
  against `SpeakerDatabase` (cosine >= tau => known global id, else auto-enroll
  `spk_<N>`; own enrollments are refined via `Update`).
- [x] **B7** Revisable local->global map: re-resolved each delivery from the
  refined embedding; the diar worker re-derives the full segment view every
  delivery so any id change propagates on the next publish. Wired into the diar
  pipeline behind a `DiarizationWorker` segment-processor hook + `[speaker]`
  config; **validated: deterministic `test_speaker_id_stage` (gate / enroll /
  cross-local re-id / overlap+low-conf gating) + real WS 120 s: 2 speakers
  auto-enrolled (spk_0 x21, spk_1 x5), diar confidence 0.50-0.97. ctest 47/47.**

## Phase C — Injection
- [x] **C8** Global id injected through the whole timeline: the published
  `diar/speaker_segment` message and the serialized diar track expose a
  `speaker_id` next to the integer local `speaker`; `ComprehensiveTimeline`
  threads it (`SpeakerInput`→`SpeakerSeg`, `SpeakerLabelIds()` label→id map) so
  the comprehensive speaker turns also carry `speaker_id` (revisable, backward
  compatible — integer `speaker` unchanged). Validated WS 120s: comprehensive
  turns carry spk_0×9 / spk_1×10, unknown spans correctly id-less.
- [x] **C9** Naming hook: `SpeakerDatabase::SetDisplayName(id, name)` /
  `DisplayName(id)`, persisted in a `<registry>.names` sidecar (binary registry
  format unchanged); the diar track + comprehensive view emit `speaker_name`
  when a name is set. Hook only (no UI / no command wired yet). Validated:
  `test_speaker_db` name + sidecar Save/Load round-trip.

## Phase D — Persistence + tuning + validation
- [x] **D10** `SpeakerDatabase::Save/Load` wired to `[speaker].registry_path`:
  loaded at `Start()` (per stream), saved in `StopWorkers()` after the diar
  thread joins (no enrollment race); an empty in-memory registry is never
  written so a speaker-less session cannot clobber a populated file. Validated:
  two consecutive server runs sharing one registry — run 1 enrolls, run 2
  **re-identifies the same speakers** (no new enrollment; registry stays at the
  same size; identical comprehensive id distribution across runs).
- [x] **D11** Real-WS 600 s validation + τ finding. **Fixed a real bug**: the
  embedding candidate was the longest clean span, which could be an early span
  already aged out of the audio retain window — re-picked every delivery,
  `ReadSpan` empty, blocking that local speaker forever; now only in-window
  spans (`start_sample >= RetainedAudioBuffer::base_sample()`) are candidates
  (1→3-4 local speakers embedded over 600 s). **τ finding** (measured): live
  diarized spans score same-speaker ~0.45-0.55 but up to ~0.45-0.48 *across*
  diarizer-local slots (boundary/crosstalk noise — much higher than the clean
  oracle's ~0.05), so the global speaker count sits in a τ-sensitive zone and,
  under the `rate=0` non-deterministic diar segmentation, varies run-to-run
  (2 vs 3 globals). Default τ kept at 0.45 (empirical operating point);
  definitive tuning needs deterministic input + ground-truth speaker labels
  (not available for test.mp3) and/or a stricter clean gate.
- [x] **D12** State docs synced (this change): `PROJECT_STATE.md` + this file.

## Phase E — Accuracy refinement + comprehensive-view presentation
Two-model division of labour (committed): Sortformer separates + flags high-
quality spans; TitaNet builds an accuracy-first centroid voiceprint; VAD dropped.
- [x] **E1** Centered-window embedding: trim `edge_margin_sec` (0.3 s) each side
  so the embed skips turn-boundary crosstalk. GT eval (≥4 s spans): 1:N 91.0 →
  **93.1%**, cross-speaker mean 0.465 → 0.439.
- [x] **E2** Enroll-confirm: enrol a NEW id only after `enroll_min_refs` (2) best
  spans agree (centroid), so one noisy span cannot spawn a spurious speaker;
  matching an EXISTING id uses τ. False-split guard.
- [x] **E3** Naming: enrol once, name via the `<registry>.names` sidecar; future
  sessions Load it and re-identified speakers carry the real name (no UI).
- [x] **E4** Comprehensive-view presentation: every speaker turn carries
  `speaker` (int local fallback) + `speaker_id` (global voiceprint) +
  `speaker_name` (real name when known); consumers display name → id → local.
  **Validated end-to-end**: enroll run, name sidecar, re-id run → comprehensive
  turns show (spk_0, 朱杰)×10, (spk_1, 徐子景)×5. ctest 47/47.

## Config (orator.toml `[speaker]`, added in D10)
- `enable`, `model_dir`, `registry_path`, `match_threshold`, `min_embed_sec`,
  `embedding_dim` (192), `max_ref_segs`.

## Phase F — Closing full-length review (test-review-protocol)
Full 60 min real-WS run (after the embed-window OOM fix + `--timeline-timeout
900`): server survives, registry saved, final timeline delivered.
- **ASR semantic**: ~88–92% (manual read vs test.txt — text tracks the
  reference closely, early and late).
- **Speaker / diarization**: end-to-end attribution **70.6%** dur-weighted
  (`tools/verify/py/speaker_attrib_eval.py`), 2/4 speakers enrolled, real
  speakers merged (朱杰+唐云峰 → one id at live cross-cosine 0.708).
- **Root cause (data-confirmed)**: Sortformer long-session degradation +
  local-slot drift — per-window diar ceiling (optimal local→name) decays
  90% (0–600 s) → 66% (1800–2400 s) → 65% (3000–3600 s); live diarized spans
  score ~0.7 across speakers (vs the clean oracle's ~0.05), so the embeddings
  are not separable and no τ reaches industrial accuracy (EER ~0.57). The
  speaker-id stage itself is oracle-validated at 91–93% on clean audio; it is
  capped by the diarizer's live segment quality, not by its own logic.
- **NOT a port bug (decisive reference)**: NeMo's own streaming Sortformer
  (`tools/reference/nemo_sortformer_ref.py`, same model + audio) shows the SAME
  per-window decay (88→86→84→**65**→71→**64**%) and the SAME slot drift. The
  C++ port tracks NeMo within ~2–5 pp per window — it is faithful. Reverting the
  streaming params to the model's trained values (chunk_len 188, rc 1,
  update_period 188, sil 3) did NOT change the decay (tested). The 65 % windows
  are genuine audio difficulty (heavy overlap / rapid interjections late in the
  meeting) under a strict per-frame single-speaker metric (harsher than DER).
- **Verdict: speaker pipeline FAIL (not industrial-grade on this audio); cannot
  close.** ASR passes. The diar port is faithful to NeMo; the limit is the
  audio's overlapped late segments, not a fixable code bug. Closing on this
  60 min 4-speaker meeting is bounded by the model+audio, not the port.
