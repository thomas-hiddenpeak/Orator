# Tasks: Evidence-First Comprehensive Timeline Fusion

- [x] T001 Enable forced alignment in `orator.toml` for all-pipeline evidence
  capture.
- [x] T002 Add an offline fusion/audit tool under `tools/verify/py/`.
- [x] T003 Run a short all-pipeline WebSocket smoke test and verify align track
  presence, count, and time-base sanity.
- [x] T004 Run a full-length all-pipeline evidence capture using `test.mp3`.
- [x] T005 Generate an align-first candidate comprehensive timeline from the
  frozen evidence package.
- [x] T006 Perform constitutional context-aware ASR/diar review of the candidate
  view against `test.txt`.
- [x] T007 Record verified findings in `specs/PROJECT_STATE.md` and this spec
  folder.
- [x] T008 Add an explicit speaker-business review method so final accuracy is
  judged on "who said what in context", not isolated diarization percentages.
- [x] T009 Generate business-turn fusion candidates from the frozen evidence
  package and audit their mechanical time-base consistency.
- [x] T010 Add a repeatable review-packet generator for side-by-side reference
  and candidate business-turn reading.
- [x] T011 Add a TOML-gated local-speaker drift epoch policy so one diarizer
  local slot can map to different speaker identities at different time ranges
  without rewriting earlier segments.
- [x] T012 Validate the drift epoch policy with unit tests, then run a
  full-length real WebSocket review before accepting it as an accuracy fix.
- [x] T013 Store resolved `speaker_id` per comprehensive timeline entry so a
  later local-speaker remap cannot rewrite historical comprehensive intervals.
- [x] T014 Add TOML-gated align-run splitting for diar boundaries near
  forced-alignment unit gaps, and validate it with a full-length WebSocket run.
- [x] T015 Run full-length refresh/cache and frozen-context fusion follow-up
  validation; reject `histctx 300/40/5`, `spkcache_refresh_rate=0`, and naive
  context low-support inheritance as accepted fixes. See
  `refresh0-context-review-2026-07-08.md`.
- [x] T016 Add a frame-level diar evidence probe and run tail-focused evidence
  review; reject reset-period, silence-profile, low-coverage, and gap-fill
  candidates as accepted fixes for 3270-3304 s. See
  `tail-evidence-review-2026-07-09.md`.
- [x] T017 Add runtime speaker-support diagnostics to comprehensive entries and
  the Web UI so weak diar evidence is visible without rewriting speaker
  attribution. See `speaker-support-diagnostics-2026-07-09.md`.
- [x] T018 Run a full-length real WebSocket support-diagnostics evaluation and
  perform context-aware speaker-business review. Reject the diagnostics-only
  change as an accepted speaker-accuracy recovery; keep it as uncertainty
  visibility. See `speaker-support-full-review-2026-07-10.md`.
- [ ] T019 Execute the speaker-recovery validation plan: decompose tail
  failures by pipeline layer, identify the first layer where speaker ownership
  becomes unreliable, and validate any candidate fix through full-session
  context-aware review. See `speaker-recovery-validation-plan-2026-07-10.md`.
- [x] T020 Add an explicit `speaker_uncertain` business-view field derived from
  speaker-support diagnostics, preserving raw speaker attribution while making
  weak or unsupported ownership machine-readable. See
  `speaker-uncertain-business-view-2026-07-10.md`.
- [x] T021 Run diar tail TOML experiments and NeMo full-length reference check
  for late-session wrong-speaker evidence. Reject strict onset, longer
  `min_dur_on`, left/right context variants, and their combination as accepted
  fixes. See `diar-tail-toml-experiments-2026-07-10.md`.
