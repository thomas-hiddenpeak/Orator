# Spec 012 Speaker-Recovery Validation Plan - 2026-07-10

> **Evaluation governance:** Under Constitution 1.7.0, no code or executable
> automation may assign correctness, calculate accuracy, rank/select a
> candidate, or issue a verdict. Automation may display unjudged evidence only;
> product results require complete contextual semantic review and manual result
> verification.

## Purpose

The current full-session comprehensive timeline exposes weak speaker-support
evidence, but speaker-business accuracy has not recovered to closing-grade
quality. This plan defines the next validation sequence.

The goal is not to optimize one isolated score. The goal is to find where the
final business view first loses reliable speaker ownership, then apply the
smallest justified fix and validate it through the real WebSocket path and
context-aware review.

## Constraints

- Use `orator.toml` for runtime parameters. Command-line flags may capture
  audio and choose output files, but accepted runtime behavior must be
  represented in TOML and code.
- Code may package and display unjudged evidence and report mechanical
  consistency. It may not select evaluation windows, assign judgments, total
  accuracy, rank candidates, or decide acceptance.
- Accuracy acceptance requires review against `/home/rm01/test/test.txt` in
  context, using `speaker-business-method.md` and
  `.specify/test-review-protocol.md`.
- The comprehensive timeline is the reviewed business output. Standalone diar
  percentages are historical mechanical diagnostics only and cannot evaluate
  the result.
- Existing pipeline tracks remain immutable evidence. Any business-view rewrite
  must preserve raw track data and record the reason for uncertainty or
  attribution.

## Evidence Layers

For each target window, inspect the following layers in order:

| Layer | Question | Evidence |
|---|---|---|
| Reference context | Who actually held the business position? | `test.txt` natural speaker turns |
| ASR + forced alignment | Is the text and phrase timing good enough to anchor the turn? | `asr` and `align` tracks |
| VAD | Does speech support the span and endpoint? | `vad` track |
| Diar local slot | Which local speaker owns the raw acoustic interval? | `diarization` track |
| Speaker identity | Does local slot to global id remain stable? | `speaker_id`, `speaker_name`, drift epoch behavior |
| Comprehensive view | What does the business timeline actually attribute? | `timeline.comprehensive` |
| Support diagnostics | Is the attribution strong, weak, or unsupported? | `speaker_support`, coverage, gap, islands |

The first layer that contradicts the reference context becomes the primary
candidate for repair.

## Mandatory Windows

Review these windows on every candidate:

| Window | Reason |
|---|---|
| 0-600 s | Early-session control window; speaker roles are mostly usable. |
| 1200-1320 s | Regression guard for context inheritance and short-turn handling. |
| 1800-2400 s | Mid-session transition where local diar ceiling previously drops. |
| 2400-3000 s | Start of late-session degradation. |
| 3000-3068 s | Business content remains readable but attribution is fragile. |
| 3270-3304 s | Known hard failure: Zhu Jie / Tang Yunfeng exchange attributed to the wrong speaker. |
| 3350-3615 s | Tail stability guard after the known hard failure. |

## Validation Phases

### Phase 1 - Frozen Evidence Decomposition

Input:

- `/tmp/orator_support_diag_full_20260710.json`
- `/tmp/orator_support_diag_review_packet_20260710.md`
- `/home/rm01/test/test.txt`

Actions:

1. Run existing mechanical audits only to locate mismatched windows and verify
   time-base consistency.
2. Read the review packet against the reference for each mandatory window.
3. Record whether the first contradiction appears in diar local slot, speaker
   identity, comprehensive attribution, or support classification.

Acceptance for this phase:

- A written layer-by-layer finding exists for each mandatory window.
- Any proposed fix names the layer it targets and the evidence justifying that
  layer.

### Phase 2 - Candidate Policy on Frozen Evidence

Only after Phase 1 identifies a target layer, test the smallest candidate policy
on the frozen package.

Allowed short-term candidate class:

- business-view uncertainty policy: preserve raw selected speaker, but mark
  weak or unsupported ownership as uncertain in the final view/export.

Rejected without new evidence:

- acoustic-only override for 3270-3304 s;
- context inheritance across weak evidence;
- command-line-only runtime parameter changes;
- changes that improve one hard window while degrading early-session controls.

Acceptance for this phase:

- Mechanical audit has no time-base or monotonicity issues.
- Review packet shows no regression in the control windows.
- The candidate improves business safety by reducing confidently wrong
  attribution, or improves speaker ownership without introducing comparable
  new errors.

### Phase 3 - Runtime Implementation

If a frozen-evidence candidate is accepted, implement it in the runtime path:

- configuration in `orator.toml`;
- code in the comprehensive timeline or serialization layer, depending on the
  target layer;
- focused unit tests for the policy;
- Web UI propagation when the business view displays the new state.

### Phase 4 - Full-Length WebSocket Acceptance

Run the full 3615 s real WebSocket test with `test.mp3`, then produce a
context-aware review using the mandatory windows.

Acceptance requires:

- successful full-length real WebSocket run;
- device metrics observed through `tegrastats` when performing the run;
- no build or CTest regression;
- final review explicitly separates mechanical code diagnostics from contextual
  speaker-business judgment.

## Current Working Hypothesis

The known hard failure at 3270-3304 s originates in bottom diarization evidence,
but the broader tail degradation is probably multi-layer:

- local diar evidence weakens in late windows;
- global speaker identity and comprehensive attribution can preserve or amplify
  that evidence;
- support diagnostics correctly identifies weak ownership but does not recover
  the speaker.

This remains a hypothesis until Phase 1 produces per-window layer findings.
