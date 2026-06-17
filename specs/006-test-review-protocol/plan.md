# Plan 006 — Test Review Protocol Migration

## 1. Scope

Migrate the existing test review protocol from root-level
`TEST_REVIEW_SPEC.md` into spec-kit SDD artifacts, then reference it in project
prompting and state docs.

## 2. Design

### 2.1 SDD placement

Create:

- `specs/006-test-review-protocol/spec.md`
- `specs/006-test-review-protocol/plan.md`
- `specs/006-test-review-protocol/tasks.md`

### 2.2 Protocol content model

Keep the existing protocol semantics with explicit sections:

1. Required review inputs.
2. Segmented AI manual-style review method.
3. ASR semantic scoring rubric.
4. Diarization scoring rubric.
5. Final verdict structure.
6. Default rule hierarchy.

### 2.3 Prompt integration

Update `.github/copilot-instructions.md` to reference Spec 006 under active SDD
artifacts so future sessions automatically treat it as part of the governed
process.

### 2.4 State integration

Update `specs/PROJECT_STATE.md` SDD artifacts list to include Spec 006.

### 2.5 De-orphaning

Delete root `TEST_REVIEW_SPEC.md` once migration is complete.

## 3. Risks and Mitigations

- **Risk**: Drift between old file and new SDD docs.
  - **Mitigation**: Remove old file after migration; keep one source.
- **Risk**: Prompt misses new protocol.
  - **Mitigation**: Add explicit artifact reference in copilot instructions.
- **Risk**: Overlap with automated metrics interpretation.
  - **Mitigation**: Keep non-goals explicit (protocol does not replace ctest/
    oracle gates).

## 4. Validation

1. All Spec 006 files exist.
2. Prompt references Spec 006.
3. PROJECT_STATE includes Spec 006.
4. Root orphan file removed.
