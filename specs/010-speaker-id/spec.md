# Spec 010 — Speaker Identity (Voiceprint Enrollment / Re-identification)

- **Feature**: `010-speaker-id`
- **Status**: Implemented (Phases A–D landed; TitaNet embedder oracle-validated, speaker-identity stage in the diar pipeline, timeline injection + naming hook, persistence + cross-session re-id; τ tuning is an ongoing data-dependent refinement)
- **Created**: 2026-06-28
- **Owner**: project owner
- **Constitution**: v1.5.0

> WHAT a speaker-identity layer must provide and WHY. Architecture, data flow,
> threading, and phasing are in `plan.md`; ordered work is in `tasks.md`.
> This is a living document: details are refined as Phase A–D are built.

---

## 1. Summary

Diarization (Sortformer) answers "who spoke when" with **per-session LOCAL
labels** (`speaker_0..3`) that are not stable across sessions and carry no
identity. This feature adds a **speaker-identity layer** that turns a local
label into a **persistent GLOBAL identity** by matching a voiceprint
(speaker embedding) against a persistent registry: it **auto-registers** an
unseen speaker, **discovers** a previously enrolled one, **identifies** the
match, and thereby **distinguishes** speakers across sessions. The resolved
global id is injected into the comprehensive timeline (revisably), so the
"who said what" view carries stable identities instead of throwaway labels.

## 2. WHY

- The comprehensive view's `speaker_0/1/2` are session-local and meaningless to
  a human or across recordings. A stable identity ("the same person as last
  meeting", later nameable) is required for any real "who said what" product.
- The registry (`SpeakerDatabase`/`ISpeakerRegistry`) already exists with GPU
  1:N cosine matching + persistence, but is unwired and has **no embedding
  model** to feed it. The single missing capability is the voiceprint model.

## 3. Requirements (testable)

- **R1 Voiceprint model**: a concrete `ISpeakerEmbedder` producing a fixed-dim
  speaker embedding from an audio span. Model = **NeMo TitaNet-Large**
  (`titanet_large`, 192-d), reimplemented in pure C++/CUDA (Art. I), validated
  against the NeMo Python oracle within bf16/fp32 tolerance (Art. II).
- **R2 Clean-segment gating**: only embed a span that is (a) VAD-confirmed
  speech, (b) a single speaker (no overlap with another diar segment),
  (c) of sufficient duration (≥ ~1.5–3 s), (d) high diar confidence. Overlapped
  / too-short spans are skipped (never contaminate a voiceprint).
- **R3 Auto-register**: a query whose best cosine match is below threshold is
  enrolled as a NEW global speaker with a generated anonymous id.
- **R4 Discover / identify**: a query matching an enrolled speaker (cosine ≥
  threshold) resolves to that global id; the registry is loaded from disk at
  startup so identities persist across sessions.
- **R5 Persistence**: the registry is saved to / loaded from disk
  (`SpeakerDatabase::Save/Load`, already implemented) so enrollments survive
  restarts. Persistence is a hard requirement.
- **R6 Naming interface (hook only)**: a stable interface to attach a human
  display name to a global id (`id -> name` sidecar), but NO naming UI/flow in
  this spec — only the extension point.
- **R7 Injection**: the resolved global id replaces the local label in the
  comprehensive timeline, **revisably** (early tentative when little audio,
  refined as audio accumulates), consistent with the revisable-timeline model.
- **R8 Decoupling**: the layer runs INSIDE the diarization pipeline as a
  post-diarization stage (Art. III): ASR/diar/VAD remain the three independent
  pipelines; speaker-id is an internal enrichment of the diar pipeline, not a
  4th pipeline.

## 4. Non-goals (this spec)

- Naming UI / speaker-management UX (only the R6 hook).
- Cross-device identity sync, cloud registry, or consent/retention policy
  (voiceprints are biometric data — flagged for a later governance pass).
- Beating TitaNet's accuracy (we faithfully reimplement it; Art. II ceiling).

## 5. Acceptance gates

- TitaNet C++/CUDA embeddings match the NeMo oracle within tolerance (R1).
- Real WS 600 s path: known speakers are re-identified across two runs; unseen
  speakers auto-register; the comprehensive view carries stable global ids;
  server stable; ASR critical path unaffected (diar headroom absorbs the model).
- Persistence: a registry saved in one run is loaded and re-matched in the next.
- Build clean `-Wall -Wextra`; `ctest` green.
