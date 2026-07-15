# Spec 012: Evidence-First Comprehensive Timeline Fusion

**Status**: Runtime candidate validated; accuracy review still open
**Created**: 2026-07-07
**Scope**: Offline fusion strategy over independently captured pipeline tracks,
then TOML-gated runtime comprehensive-view adoption after validation.

## 1. Objective

Build a repeatable workflow that captures each pipeline's independent evidence
once, then searches for a higher-accuracy comprehensive timeline view offline
without repeatedly re-running the models.

The final comprehensive timeline remains a derived view on the common time base.
It must use each pipeline for the evidence it is best suited to provide:

- ASR: finalized `text_id`, text content, segment span.
- Forced alignment: per-unit text timing inside each ASR segment.
- VAD: speech/pause evidence and endpoint support.
- Diarization: speaker ownership and speaker identity evidence.

## 2. Requirements

- **FR1 — All-pipeline evidence capture**: one real WebSocket run shall capture
  ASR, diarization, VAD, forced-alignment, speaker identity, device metrics, and
  the current comprehensive output into one JSON evidence package.
- **FR2 — Common time base preservation**: every fusion input and generated
  output shall use the same absolute seconds clock derived from sample 0. No
  wall-clock timestamps or local counters may participate in fusion decisions.
- **FR3 — Offline fusion**: fusion strategy iteration shall run over frozen
  evidence packages. It may generate candidate comprehensive views, but it shall
  not mutate the captured pipeline tracks.
- **FR4 — Align-first text placement**: when forced-alignment units are present,
  text splitting shall use unit timestamps before falling back to proportional
  ASR-span splitting.
- **FR5 — Boundary evidence**: VAD pauses and alignment-unit gaps are strong
  boundaries. Diarization boundaries are speaker evidence and shall be treated
  as soft boundaries unless supported by pause/unit timing evidence.
- **FR6 — Evaluation governance**: code may generate reference-free candidate
  views and organize unjudged evidence, but no compiled program, script, test,
  notebook, formula, query, metric, or algorithm may assign correctness,
  calculate accuracy, rank/select a candidate, or issue a verdict. ASR and
  speaker results are evaluated only by complete context-semantic reading
  against `test/data/reference/test.txt` under
  `.specify/test-review-protocol.md`.
- **FR7 — Speaker business view**: speaker accuracy shall be judged on the final
  business-facing view ("who said what in context") using all pipeline evidence
  together. Isolated diarization-track percentages are diagnostics only.
- **FR8 — Runtime parity**: once a fusion rule is accepted, the runtime
  comprehensive view shall apply the same time-base and speaker-identity rules
  under explicit `orator.toml` parameters.

## 3. Non-goals

- Do not change ASR, diarization, VAD, or forced-alignment model math in this
  spec.
- Do not use character error rate, diarization error rate, timestamp overlap,
  duration mapping, embedding similarity, or any code-derived score to evaluate
  a result or make an acceptance decision.
- Do not bypass the real WebSocket path for the evidence package capture.

## 4. Acceptance

- A real WebSocket all-pipeline evidence package exists for `test.mp3`.
- The package includes an `align` track whose entries use the same time base as
  ASR, diarization, and VAD.
- An offline fusion tool can produce at least one candidate comprehensive view
  from the frozen tracks and an audit report explaining which evidence supports
  each text span.
- The candidate view is reviewed using the constitutional test-review protocol
  before any runtime fusion change is considered complete.
- Speaker-business accuracy is reviewed using
  `speaker-business-method.md`, not a diarization-only metric.
- Runtime adoption is accepted only after a full-length real WebSocket run,
  `ctest`, clean warning check, and context-aware review of known regression
  windows.
