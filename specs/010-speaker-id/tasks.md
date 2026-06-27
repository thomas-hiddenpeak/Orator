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
- [ ] **A2** NeMo Python oracle: dump reference 192-d embeddings (and key
  intermediate activations) for a few clean spans of `test.mp3` to
  `models/reference/speaker/`. Tool: `tools/reference/titanet_oracle.py`.
- [ ] **A3** Implement `model::TitaNetEmbedder : core::ISpeakerEmbedder`
  (`include/model/titanet_embedder.h` + `src/model/titanet_embedder.cu`):
  mel (reuse `MelSpectrogram`) → SE/depthwise-separable 1D conv encoder →
  attentive statistics pooling → linear → L2-normalized 192-d. bf16 weights,
  fp32 accumulate, stream-explicit, RAII (`DeviceScratch`).
- [ ] **A4** `test/unit/model/test_titanet.cc` (`test_titanet`): C++ embedding
  and cosine vs the A2 oracle within tolerance; register in CMake + ctest.

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
