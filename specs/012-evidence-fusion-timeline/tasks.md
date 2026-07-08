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
