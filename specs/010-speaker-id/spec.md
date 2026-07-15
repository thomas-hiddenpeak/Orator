# Spec 010 — Speaker Identity (Voiceprint Enrollment / Re-identification)

- **Feature**: `010-speaker-id`
- **Status**: Implemented (Phases A–E + cross-session identity finalized).
  Phase H added a TOML-gated conservative identity policy, but the first
  2026-07-06 full-length real WebSocket candidate was NOT accepted as an
  accuracy fix: it reduced spurious new global ids into local-only labels, but
  did not restore full-session diar attribution quality. The follow-up
  2026-07-06 local-diar review restored the accepted async/no-reset operating
  profile to Sortformer runtime tuning (`188/1/3`) in TOML; the full run
  produced four stable global ids with no local-only gaps, while leaving
  rapid-turn tail fragmentation and one ASR repeat burst as residual issues.
  Lower-level `SortformerConfig` defaults stay tied to the existing NeMo oracle
  fixture and are not the runtime operating profile. TitaNet
  embedder oracle-validated; speaker-identity stage in the diar pipeline assigns a
  persistent GLOBAL id to every segment. Cross-session stitching: per-slot
  voiceprint centroids strengthen across sessions; the diarizer's within-session
  separation is trusted (same-session slots never collapse to one id, via
  `SpeakerDatabase::MatchExcluding`); confident duplicates are de-duplicated at the
  registry level (`MergeReconcile` + `SpeakerDatabase::Remove`), session-aware so two
  co-session speakers never merge. The registry is uncapped (designed to recognise
  many speakers, ≥200). Validated on the full 60-min real WebSocket stream (rate=1):
  4 real speakers → exactly 4 stable global ids across all 6 reset sessions, judged
  by context-aware per-segment semantic comparison vs `test.txt` (Test Review
  Protocol — no code or automated metric may derive accuracy, rank a candidate,
  or issue a verdict). Commits 38cdf51, 9c02862,
  17f8d92, 06875c3, 5f301ba.
- **Created**: 2026-06-28
- **Owner**: project owner
- **Constitution**: v1.7.0

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
- **R9 Conservative cross-session policy**: reset-session local slots MAY be
  configured to require multiple clean references before matching a global id,
  and unmatched later-session slots MAY stay local-only instead of auto-enrolling
  a new global id. This is a TOML-controlled accuracy experiment: unresolved
  local labels are preferable to stable but wrong global speaker ids.

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
- Phase H acceptance requires a full-length real WebSocket run with `test.mp3`,
  `tegrastats` observation, and context-aware per-segment comparison against
  `test.txt`. No code, script, test, metric, formula, query, or algorithm may
  assign correctness, calculate speaker accuracy, rank/select a candidate, or
  issue the acceptance conclusion. Historical code-derived percentages are
  retained as non-authoritative mechanical diagnostics only.
- Local-diar recovery acceptance requires the same full-length real WebSocket
  path and must prove stable global identity coverage without sacrificing the
  Test Review Protocol's context-aware speaker-turn judgment. Structural JQ
  diagnostics may display evidence but may not select evaluation windows;
  every in-scope turn must be reviewed in full context.
