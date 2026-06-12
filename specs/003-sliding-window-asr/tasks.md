# Tasks 003 — Bounded Sliding-Window Real-Time ASR

- **Feature**: `003-sliding-window-asr`
- **Spec**: [spec.md](spec.md) · **Plan**: [plan.md](plan.md)
- **Status**: In progress (owner approved: "直接开始做")
- **Constitution**: v1.1.0

> Ordered, independently verifiable steps. The parameter defaults are chosen
> from the sweep (T040), not assumed.

---

## Phase 1 — CER measurement first (so accuracy is quantified before/after)
- [ ] **T010** Add `tools/cer.py` (standard library only): normalize (keep CJK +
  alphanumerics, drop punctuation/whitespace) and compute character error rate
  between a hypothesis transcript and `asrTest2Final.txt` over the same span.
  *(Verify: runs on the current energy-VAD streamed JSON and prints a CER.)*
- [ ] **T011** Record the baseline CER of the current energy-VAD method on 120 s
  of `test.mp3`. *(Verify: number recorded; this is the bar AC3 must not exceed.)*

## Phase 2 — Bounded sliding-window worker
- [ ] **T020** Rewrite `AsrWorker` internals to the bounded sliding window
  (plan §1): window of `window_sec`, decode every `step_sec`, commit audio
  leaving the window with a `commit_margin_sec` unfixed tail, supply
  `committed_text_` as the prefix via `TranscribeWindow`. Keep the public surface
  unchanged. New `Params`; remove energy-VAD params. *(Verify: builds clean;
  produces a continuous transcript on a short stream.)*
- [ ] **T021** Append committed spans to the timeline with absolute times; keep
  the comprehensive view working. *(Verify: timeline document has both tracks +
  comprehensive; times monotonic.)*
- [ ] **T022** Finalize commits the remaining window tail. *(Verify: no audio
  left untranscribed at end of stream.)*

## Phase 3 — Validation and sweep
- [ ] **T030** Per-step decode-time stability: confirm last-third vs first-third
  step time stays bounded (not linearly growing). *(Verify: AC1.)*
- [ ] **T040** Sweep `window_sec` ∈ {8,12,16,24}, `step_sec` ∈ {1,2},
  `commit_margin_sec` ∈ {1,2} on 120 s; record RTF and CER per setting; choose
  defaults (smallest window with no CER regression and RTF ≥ target). *(Verify:
  sweep table recorded; defaults set.)*
- [ ] **T041** With chosen defaults: RTF ≥ current 4.99x (AC2) and CER ≤ the
  energy-VAD baseline (AC3); committed text deterministic across two runs (AC4);
  VmRSS bounded over a long run (AC5). *(Verify: AC2–AC5.)*

## Phase 4 — Integration and cleanup
- [ ] **T050** Full build + `ctest` green under `-Wall -Wextra`; threaded path
  race-checked. *(Verify: AC7.)*
- [ ] **T051** Update `/memories/repo/` and `PROJECT_STATE.md` with the chosen
  parameters, RTF, CER, and the cost-stability result. Remove the now-unused
  energy-VAD code paths. *(Verify: no dead code; docs match reality.)*
- [ ] **T052** Commit.

## Traceability (requirement → task)

| Requirement | Tasks |
|---|---|
| FR1 bounded window | T020 |
| FR2 periodic decode | T020 |
| FR3 commit + prefix | T020 |
| FR4 timeline output | T021 |
| FR5 finalize | T022 |
| FR6 stable cost | T030 |
| FR7 CER measurement | T010, T011 |

| Acceptance | Tasks |
|---|---|
| AC1 cost stable | T030 |
| AC2 RTF ≥ 4.99x | T040, T041 |
| AC3 CER ≤ baseline | T011, T040, T041 |
| AC4 deterministic commit | T041 |
| AC5 bounded memory | T041 |
| AC6 output contract unchanged | T021 |
| AC7 clean build/tests | T050 |

## Definition of Done
Bounded sliding window implemented; per-step cost stable; RTF ≥ 4.99x and CER ≤
the energy-VAD baseline with chosen defaults; deterministic committed text;
bounded memory; output contract unchanged; build + tests green; energy-VAD code
removed; docs updated; committed.
