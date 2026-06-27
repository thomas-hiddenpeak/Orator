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
- [ ] **B5** Clean-segment gate: VAD-confirmed + single-speaker (no diar
  overlap) + duration ≥ min + diar conf ≥ cutoff.
- [ ] **B6** Per-local-speaker audio accumulation → embed → `SpeakerDatabase`
  Match/Enroll; moving-average `Update` of an enrolled embedding.
- [ ] **B7** Revisable local→global map: tentative early, revised as audio
  accumulates; emit a revision on firm-up/flip.

## Phase C — Injection
- [ ] **C8** Publish diar segments carrying the resolved GLOBAL id on the diar
  protocol topic; `ComprehensiveTimeline` speaker turns use the global id
  (revisable). Backward-compatible with the local-label path when speaker-id is
  disabled.
- [ ] **C9** Naming hook: `SetDisplayName(global_id, name)` + `id -> name`
  sidecar persisted alongside the registry; serialized into the timeline view.
  No UI.

## Phase D — Persistence + tuning + validation
- [ ] **D10** Wire `SpeakerDatabase::Save/Load` to a configured path
  (`orator.toml [speaker]`); load at startup, save on shutdown/checkpoint.
- [ ] **D11** Tune threshold τ + min-duration on real WS 600 s: re-identify
  known speakers across two runs; auto-register unseen; report false-merge /
  false-split. Compare attributed speakers to `test.txt` item-by-item (Art. VI).
- [ ] **D12** Update `PROJECT_STATE.md` + spec status to Implemented with commit
  references (Art. VIII).

## Config (orator.toml `[speaker]`, added in D10)
- `enable`, `model_dir`, `registry_path`, `match_threshold`, `min_embed_sec`,
  `embedding_dim` (192).
