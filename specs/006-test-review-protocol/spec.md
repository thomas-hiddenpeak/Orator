# Spec 006 — Test Review Protocol (AI Human-Style Evaluation)

- **Feature**: `006-test-review-protocol`
- **Status**: Revised
- **Owner intent**: Replace the root-level orphan `TEST_REVIEW_SPEC.md` with a
  spec-kit SDD artifact and make it part of the project source-of-truth set.

## 1. Context and Problem

Orator already has strong automated checks (ctest, oracle comparisons, runtime
metrics), but final go/no-go quality decisions for real streaming regressions
need a consistent AI-conducted manual-style review protocol:

- Read reference text + timeline/events/logs.
- Perform segmented, context-aware semantic comparison.
- Produce explicit ASR semantic and diarization judgments.
- Avoid "script number only" conclusions.

The protocol existed in a standalone root file (`TEST_REVIEW_SPEC.md`), which is
outside the SDD structure and therefore easy to drift from project governance.

## 2. Goals

1. Move the review protocol into spec-kit SDD artifacts.
2. Keep the protocol normative and reusable for real-WS and offline tests.
3. Make the project prompt explicitly reference this protocol.
4. Remove the root orphan file once migrated.

## 3. Non-Goals

1. Replace ctest/oracle metrics as engineering gates.
2. Introduce runtime dependencies or change production code paths.
3. Force human-in-the-loop review by default.

## 4. Requirements

### R1 — Inputs required for review

Each review MUST read:

1. Reference text (gold).
2. Model outputs (timeline JSON, ASR incremental events, diar track).
3. Run metadata (audio duration, wall time, pipeline RT/compute).
4. Key run logs (crash/abnormal warnings).

### R2 — AI-conducted segmented review

The reviewer MUST:

1. Perform actual context comparison (not just aggregate metrics).
2. Segment by natural speaker-turn/content changes.
3. Fill a per-segment table with ASR semantic and diar judgments.

### R3 — Two explicit core scores

Output MUST include:

1. ASR semantic accuracy range with rationale.
2. Diarization accuracy range with rationale.

### R4 — Final verdict block

Output MUST include:

1. ASR semantic range.
2. Diarization range.
3. Pass / Conditional pass / Fail.
4. Whether to proceed to next optimization round.

### R5 — Default rule hierarchy

Judgment order MUST be:

1. Stability first (crash/no result => fail).
2. Then realtime measurements (record, do not substitute accuracy).
3. Then ASR semantics.
4. Then diarization.
5. Then business usability summary.

### R6 — Template consistency

Use a stable report structure:

1. Test summary table.
2. Segmented review table.
3. Two core conclusions.
4. Final verdict.

## 5. Acceptance Criteria

- **AC1** Protocol is represented under `specs/006-test-review-protocol/` as
  `spec.md`, `plan.md`, `tasks.md`.
- **AC2** `.github/copilot-instructions.md` references this protocol as a
  project SDD artifact.
- **AC3** Root `TEST_REVIEW_SPEC.md` is removed.
- **AC4** `specs/PROJECT_STATE.md` includes Spec 006 in SDD artifacts.

## 6. Validation

Validation is documentation consistency:

1. File presence and links are correct.
2. Prompt reference is present.
3. Root orphan is absent.
4. No production-code behavior changes.
