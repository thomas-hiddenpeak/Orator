# Spec 010 — Plan (Speaker Identity)

> HOW the speaker-identity layer is built: architecture, data flow, threading,
> phasing, and risks. Requirements/WHY are in `spec.md`.

## 1. Architecture (where it lives)

Speaker-id is a **post-diarization stage inside the diarization pipeline**
(Constitution Art. III decision, see spec R8). The diar pipeline becomes:

```
audio ─► Sortformer ─► local speaker segments (spk0..3, times, conf)
                              │
                              ▼
                  [SpeakerIdentityStage]   (new, this spec)
        clean-segment gate ─► TitaNet Embed ─► per-local-speaker voiceprint
                              │
                              ▼
              SpeakerDatabase 1:N cosine match (persistent)
                  match ≥ τ → known global id      (discover / identify)
                  match <  τ → Enroll new global id (auto-register)
                              │
                              ▼
              local-label → global-id map (revisable)
                              │
                              ▼
        publish diar segments carrying the GLOBAL id (protocol topic)
                              ▼
        ComprehensiveTimeline maps speaker turns to the global id
```

ASR / VAD pipelines are untouched and remain independent. The model runs on the
diar worker thread (diar rt ≈ 40–55×: ample headroom; the ASR critical path is
unaffected). GPU scheduling follows the diar pipeline's existing stream/lock.

## 2. Components

- **`model::TitaNetEmbedder` (`ISpeakerEmbedder`)** — pure C++/CUDA TitaNet-L:
  mel front-end (reuse `feature::MelSpectrogram`) → 1D depthwise-separable conv
  encoder (QuartzNet/ContextNet blocks) → attentive statistics pooling → linear
  → L2-normalized 192-d embedding. Weights bf16 (load-time upcast as elsewhere).
- **`model::SpeakerDatabase` (existing, `ISpeakerRegistry`)** — wired in:
  Enroll / Match / Update / Save / Load. Add a `id -> display_name` sidecar +
  a `SetDisplayName(id, name)` hook (R6); no UI.
- **`pipeline::SpeakerIdentityStage`** — drives clean-segment gating, embedding
  accumulation per local speaker, matching/enrolling, the revisable
  local→global map, and publishing the enriched diar segments.

## 3. Data flow / threading

- Runs on the **diarization worker thread**; uses the diar pipeline's CUDA
  stream. No new cross-pipeline coupling (Art. III): it consumes diar's own
  in-pipeline segment output + the shared audio buffer, and publishes the
  enriched segments through the existing diar protocol topic.
- **Streaming embedding**: accumulate a local speaker's CLEAN audio; embed once
  ≥ min-duration; refine (moving-average `Update`) as more accumulates. The
  local→global decision is **tentative early, revised later** → the comprehensive
  timeline receives a revision when an identity firms up or flips.

## 4. Phasing

- **Phase A — Voiceprint model + oracle (largest).**
  A1 Acquire `titanet_large` (NeMo) + convert weights → safetensors (tools/).
  A2 NeMo Python oracle: reference embeddings for test spans (tools/reference/).
  A3 Implement `TitaNetEmbedder` in C++/CUDA (encoder + ASP pooling + linear).
  A4 `test_titanet`: C++ embeddings vs oracle within tolerance.
- **Phase B — Identity stage.**
  B1 Clean-segment gate (VAD + single-speaker + duration + conf).
  B2 Per-local-speaker accumulation + embed + DB match/enroll.
  B3 Revisable local→global map.
- **Phase C — Injection.** Publish global ids on the diar topic; map them into
  the comprehensive view (revisable). Naming hook (R6).
- **Phase D — Persistence + tuning.** Save/Load wiring; threshold τ + min-dur
  tuning; 600 s real-WS validation (re-id across runs; auto-register).

Each phase gates on build clean + ctest + (A) oracle + (D) real-WS 600 s.

## 5. Risks / open details (refined while building)

- **Clean-segment definition** (R2): exact thresholds (min-dur, overlap rule,
  conf cutoff) tuned empirically in Phase B/D.
- **Threshold τ + enroll policy**: false-merge (two people → one id) vs
  false-split (one person → two ids); tune on 600 s; moving-average update.
- **Cold start**: first speaker of a session has no prior — always auto-register.
- **Overlapped speech**: excluded from embedding (R2) — may leave some turns
  with a tentative/low-confidence id.
- **TitaNet reimplementation effort**: depthwise-separable 1D conv + ASP is the
  main new kernel work; reuse existing conv/GEMM where possible.
- **Privacy** (non-goal here but noted): voiceprints are biometric; storage and
  consent are a later governance item.
