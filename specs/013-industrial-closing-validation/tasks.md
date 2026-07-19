# Tasks: Industrial Closing Validation

**Status**: In progress - T135 reconciled T111/T123/T133; FR31 rejected by
complete changed-context review; FR32 retained by frozen review and awaiting
real-WebSocket promotion; no full candidate passes all natural-turn gates;
T102/T084 remain open 2026-07-19

## Phase 0: Governance

- [x] T001 Review and approve Spec 013 requirements, thresholds, and terminology.
- [x] T002 Submit the common-clock/private-cache and supplemental-test
  Constitution amendment as a separate governance commit.
- [x] T002A Amend Constitution Article VI to prohibit every form of code-based
  result evaluation and require complete contextual semantic review, manual
  result derivation, and manual verification. Landed as Constitution 1.7.0 in
  governance commit `03bb22a`.
- [ ] T003 Correct all remaining closure and implementation claims in
  `PROJECT_STATE.md`, active specs, READMEs, and code comments.
- [x] T004 Define the reproducibility manifest and artifact naming convention.
- [x] T005 Freeze the current unaccepted baseline commit, TOML, registry state,
  model/data hashes, and full JSON package. The `ee0dd82` full package is
  retained as diagnostic evidence and explicitly rejected by its live/terminal
  ordering contract; see `baseline-2026-07-14.md`.

## Phase 1: Contract Compliance

- [x] T010 Create one session-owned immutable `TimeBase` and inject it into every
  audio store and worker.
- [x] T011 Make end-of-stream extent reconciliation cover every registered track
  and fail tests on any mismatch.
- [x] T012 Add typed incremental evidence reads/subscriptions to
  `ComprehensiveTimeline` without content inference.
- [x] T013 Replace the ASR `VadCache` pointer path with VAD evidence read through
  `ComprehensiveTimeline`.
- [x] T014 Replace forced aligner's direct protocol subscription with finalized
  ASR evidence read through `ComprehensiveTimeline`.
- [x] T015 Move speaker choice, text projection, and gap policy into a registered
  `business_speaker` pipeline and keep raw tracks immutable.
- [x] T016 Fix finalized ASR `text_id` allocation/emission order and serialize IDs
  explicitly in the ASR terminal track.
- [x] T017 Add ID-convergence tests across partial/final events, align groups,
  revisions, terminal tracks, export, reconnect, and Web UI state. Node model
  tests, the unified client, and the real Chromium flow cover the full chain.
- [x] T017A Replace the single active WebSocket output pointer with one audio
  producer plus multiple non-mutating observers. Extend the unified client gate
  to prove that observer connection lifecycle does not reset the producer and
  that producer/observer terminal timelines are identical. The registered
  12-second real-WebSocket gate covers an observer present before production, a
  rejected concurrent producer that disconnects during the stream, and a late
  observer. The producer and both retained observers received terminal SHA-256
  `9b1f2b3c...`; the early observer's 37 business events and nine telemetry
  events matched the producer exactly. A real Chromium observer independently
  converged to 2 ASR / 5 diar entries and exported the same parsed terminal
  document as the unified client.
- [x] T018 Correct configuration precedence and migrate/remove every behavioral
  environment-only switch. Precedence and timeline gap-fill are corrected;
  migrate ASR/align/Sortformer/GPU/transport controls through typed config and
  remove legacy GEMM kernel-forcing switches before marking complete.
- [x] T019 Emit or capture the complete resolved configuration in every run
  manifest. The terminal timeline must carry the canonical resolved object and
  the unified client must persist its hash in the artifact sidecar.
- [x] T020 Remove the misleading parameter-matrix mode from the unified test
  client and retain execution/capture responsibilities only.
- [x] T020A Remove the `closing_ledger.py summary` command, automated accuracy
  aggregation, threshold comparisons, acceptance booleans, and their test. Keep
  only initialization, unjudged evidence preparation, and structural/hash/
  signature validation under Constitution 1.7.0.

## Phase 2: Reference Ledger

- [x] T030 Define the immutable ledger schema and manual judgment categories.
- [x] T031 Confirm the human-listened `test.txt` as the immutable 556-turn
  reference and preserve its source speaker, text, timestamp, whole-second
  precision, and line order. The earlier claim that all rows still required
  audible adjudication was incorrect; it described an optional empty annotation
  JSON, not the authority of `test.txt`. See
  `reference-ledger-v21-2026-07-15.md`.
- [ ] T032 Classify critical turns before candidate-output review.
- [x] T033 Perform a second complete review in reversed 600-second block order.
  T111 completed this independently for accepted Run A and Run B after each
  complete chronological pass.
- [x] T034 Resolve every forward/reverse disagreement and retain duplicate,
  backward-timestamp, or ambiguous reference rows without deleting them. T111
  completed this for both accepted runs.
- [ ] T035 Independently verify the manual time, turn, speaker, criticality, and
  offset totals without code, formulas, queries, or automated aggregation.

## Phase 3: Reproducible Baseline

- [x] T040 Restore and register the real-WebSocket Python integration gate in
  CTest using the sole unified socket client plus a process-only runner.
- [x] T041 Add silence, endpoint, time-base, track-immutability, stable-ID, and
  Web UI model tests. The registered gate runs 12 s canonical speech and 30 s
  generated silence with isolated TOML storage.
- [x] T042 Run clean build, warning check, full CTest, JavaScript checks, and
  selected sanitizer/CUDA memory checks. Clean `ce388a7` passed the warning-free
  Release build and 64/64 CTest suite, 25/25 selected ASan/UBSan tests, full
  inherited-v2.1 memcheck/initcheck, and public-kernel/GEMM
  memcheck/racecheck/synccheck with zero errors. The instrumentation-limited
  full-model racecheck is explicitly unclaimed. See
  `engineering-gates-2026-07-15.md`.
- [x] T043 Run 120 s, 360 s, and 600 s real-WebSocket tests with committed TOML.
- [x] T044 Run one full v2.1 closing-baseline WebSocket capture with continuous
  `tegrastats` and browser evidence. Clean commit `3b40245` completed 3615.12 s
  in 3616.442 s at 1x with zero mechanical issues, 3,441 runtime and 3,606
  `tegrastats` samples, seven exact zero-gap extents, and exact producer /
  persisted-session / Chromium-rendered / browser-download terminal equality.
  See `closing-baseline-v21-2026-07-15.md`. This completes system evidence only;
  T045 supplied the product judgment.
- [x] T045 Complete the 556-row baseline context review without any code-inferred
  judgment or automated total. The exact clean 935-entry artifact received
  complete chronological and reverse-block manual contextual review: 443
  correct / 112 incorrect / 1 ambiguous (`79.6763%`). Tools only arranged
  evidence. The baseline was rejected at the natural-turn gate, so unused
  speaker-time, criticality, confidence, and source-time-offset breakdowns were
  not derived for that obsolete artifact. See
  `closing-baseline-v21-context-review-2026-07-15.md`.
- [x] T046 Canonicalize equal-start overlapping diarization records before the
  typed/live split and retain strict live/terminal equality validation. The
  full `ee0dd82` diagnostic package exposed three order-only mismatches;
  `test_typed_evidence_flow` now covers the contract.
- [x] T047 Designate streaming v2.1 `340/1/188/188` as the sole closing
  baseline. The compile-time default, checked-in TOML, generic weight-loader
  test, active async numerical gate, README, plan, and project state all select
  v2.1. The v2 weight and obsolete CTest are deleted, and `test_config` rejects
  their reappearance. Historical reports cannot satisfy a Phase 3-7 promotion
  or acceptance task.

## Phase 4: Existing-Model Upper Bound

- [x] T050 Export Sortformer posterior evidence per natural turn from the frozen
  package.
- [x] T051 Export TitaNet per-turn similarities independently of Sortformer
  bucket labels and validate them against NeMo.
- [x] T052 Export VAD and forced-alignment boundary evidence on the same time
  base.
- [x] T053 Build an auditable constrained speaker-decision candidate over frozen
  tracks without reference-specific runtime inputs.
- [x] T054 Screen the candidate by a complete 556-row chronological contextual
  pass without code, automated totals, ranking, or acceptance flags. That pass
  failed the natural-turn gate and was sufficient to reject the historical
  candidate; the reverse pass and other breakdowns were intentionally not spent
  on an already failed candidate.
- [x] T055 Reject the initial frozen candidate and stop tuning its threshold
  family. Its complete provisional text-context diagnostic is `378/556`
  (67.986 percent); the one ambiguous row remains a failure in the mandatory
  denominator. No policy was accepted from this profile.
- [x] T056 Add and execute a multi-chunk NeMo oracle for the exact asynchronous
  runtime model/profile and retain the FIFO mismatch as a regression test. The
  reference-free processed fixture now covers five chunks / 1502 output frames,
  and the actual v2 runtime checkpoint provenance is fixed at NVIDIA revision
  `a16aa88603f758b4e4788177c6345ba3594edef6`, source `.nemo` SHA-256
  `48bf1aee...`, and runtime safetensors SHA-256 `754e4468...`. The regenerated
  raw oracle retained SHA-256 `0fb3b6d0...`; C++ passed at 1502/1502 argmax,
  `max_abs=1.43051e-6`, and `mean_abs=9.48068e-8`.
- [x] T057 Correct asynchronous FIFO/model-input parity, pass the exact v2
  runtime oracle, and regenerate frozen Sortformer evidence before another
  runtime accuracy comparison. The corrected full-session diagnostic changed
  464/556 reference intervals but the corresponding frozen text-context
  candidate remained 378 correct / 177 incorrect / 1 ambiguous; numerical
  parity fixed the implementation defect without recovering business accuracy.
- [x] T058 Remove non-NeMo cache controls, make `spkcache_sil_frames` effective,
  and require the full TOML streaming profile before Sortformer initialization.
  Removed keys now fail configuration loading, late tuning is rejected, the
  rebuilt exact-profile oracle passes. The suite passed 61/61 at that step.
  See `sortformer-oracle-2026-07-15.md`.
- [x] T059 Evaluate the clean integrated-view upper bound without
  reference-specific runtime rules. The best provisional written-context
  diagnostic is `502/556` (90.288 percent), still below the 93 percent
  development gate and not a signed constitutional result.
- [x] T059A Reconstruct the abandoned streaming v2.1 context. The existing
  frozen-evidence first pass covers all 556 rows: async v2.1 recorded 377
  correct / 178 incorrect / 1 ambiguous, while checkpoint-native sync recorded
  357 / 198 / 1. These are historical written-context diagnostics, not a
  real-WebSocket constitutional result.
- [x] T059B Regenerate the five-chunk v2.1 asynchronous NeMo oracle using the
  active `188/340/188`, FIFO `188`, context `1+1` profile. The regenerated raw
  fixture is byte-identical to `ref_stream_async_v21_long.f32` at SHA-256
  `2635b090...`; bind the runtime numerical gate to v2.1 before streaming.
- [x] T059C Run the v2.1 candidate through the clean build and numerical suite,
  then a 120-second real-WebSocket promotion run with continuous telemetry. The
  clean suite passed 63/63 and the transport/telemetry contracts passed, but the
  contextual promotion gate failed: v2.1 lost diar evidence from 4.48-27.20 s
  and reused local slots across several 65-84 s short turns. Continue the full
  run only as the owner-requested diagnostic, not as a promoted candidate.
- [x] T059D Run full `test.mp3` at 1x through the real WebSocket path using the
  committed v2.1 TOML and retain the source-stable manifest, terminal timeline,
  events, logs, browser evidence, and `tegrastats` series. The 3615.12-second
  run completed in 3616.559 seconds with zero mechanical-contract issues;
  terminal artifact SHA-256 is `653e366b...`.
- [x] T059E Complete the full 556-row chronological and reversed-block
  contextual speaker review for the v2.1 runtime. The reconciled diagnostic is
  413 correct / 142 incorrect / 1 ambiguous (`74.2806%`), with the worst fixed
  block at 2400-3000 seconds (`62.02%`). The retained corrected-v2 result is
  reported only as a different-class frozen diagnostic; no paired acceptance
  comparison or exact speaker-time score is claimed.
- [x] T059H Validate the two NVIDIA-published v2.1 streaming profiles with exact
  multi-chunk NeMo/C++ fixtures: high latency `340/40/40/300` and low latency
  `6/7/188/144` (chunk/right/FIFO/update). Keep the legacy
  `340/1/188/188` result only as a same-profile checkpoint diagnostic. High
  latency passed 1502 frames at `max_abs=1.07288e-6`; low latency passed at
  `max_abs=1.54972e-6`, both with 1502/1502 argmax agreement. The low-latency
  gate exposed and fixed an undersized Conformer scratch buffer when `T<64`;
  the corrected first-block path also passes `compute-sanitizer` with zero
  errors.
- [x] T059I Export full-length native diar evidence for both official profiles,
  preserve the common time base, and complete the 556-row contextual comparison
  before selecting a profile for another real-WebSocket run. High latency
  recorded 385 correct / 170 incorrect / 1 ambiguous (`69.2446%`); low latency
  recorded 377 / 178 / 1 (`67.8058%`). Both fail the 90 percent screening gate,
  so neither is selected for an integrated run. The inherited v2.1 profile
  remains active at its previously measured 413 / 142 / 1 real-WebSocket
  diagnostic.
- [x] T059F Trace the current v2.1 incorrect rows through local Sortformer
  channels, global speaker-identity assignments, and the `business_speaker`
  comprehensive view. The first loss is usually in the local Sortformer slot:
  after 2400 seconds its reference-localization ceiling falls to roughly
  62-67 percent in several fixed windows, while the identity layer mostly
  propagates stable local epochs. Native TitaNet evidence independently repairs
  some complete Zhu Jie turns but is ambiguous or wrong at 1039, 2136, and
  3270-3298 seconds, so a direct whole-turn override is ineligible.
- [x] T059G Build a reference-free multi-scale rolling TitaNet evidence track
  for the captured v2.1 session. The 3/5-second, 1-second-step TOML policy
  produced 1,166 dual-scale points and 239 runs from 7,224 native spans. It
  changed nine of 936 business entries. Full contextual review of all 11
  affected reference rows found five repairs and no regression, yielding 418 /
  137 / 1 (`75.1799%`). This remains below the 93 percent implementation gate,
  so policy tuning stops and the frozen candidate is not integrated. See
  `speaker-sliding-v21-2026-07-15.md`.
- [x] T059J Generate one full v2.1 Sortformer candidate with a TOML-defined
  90-second state-rotation period, session-qualified local slots, and the exact
  inherited `340/1/188/188` execution profile. Retain frame posterior, segment,
  source hash, and common-time-base evidence without reading reference labels.
  The run produced 45,189 frames, 768 segments, and 41 sessions over 3615.12 s.
- [x] T059K Independently map every eligible session-qualified local slot to the
  frozen global registry with TitaNet audio evidence. Build one auditable
  business-speaker candidate using VAD/forced-alignment boundaries, with no
  transcript phrase, speaker name, known timestamp, or correctness input. Four
  focused tests cover session qualification, cannot-link completion, mapped
  diar priority, and conservative no-diar behavior.
- [ ] T059L Complete chronological and reversed-block contextual semantic review
  over all 556 rows. Manually derive and independently verify every result; no
  code may label rows, aggregate accuracy, rank the candidate, or issue the
  promotion verdict. Stop this candidate family if the 93 percent development
  gate is not established. The candidate stopped after the complete 0-120 s
  promotion context: multiple short interjections retained the wrong local
  Sortformer slot despite correct session mapping, so no full-session verdict
  is claimed and the rotation family receives no retuning.
- [x] T059M Generate one continuous-v2.1 short-phrase TitaNet candidate from the
  frozen business/alignment intervals using the dedicated TOML, existing strong
  identity gates, and active-registry filtering. The generator must not read
  references or emit correctness fields. The frozen 935-entry candidate and
  complete changed-context worksheet were generated; 16 reference
  contributions display an assignment change.
- [ ] T059N Complete chronological and reversed-block contextual semantic review
  of all 556 rows and manually verify the development gate before any runtime
  implementation. Stop the short-phrase family on failure; do not sweep its
  minimum duration, edge margin, or identity thresholds against the reference.
  The strong-only view is not promoted: complete review of every changed
  context shows useful repairs but insufficient coverage and one whole-interval
  mixed-speaker rewrite risk. No full-session result is claimed.
- [x] T059O Generate one continuous-v2.1 direct-evidence fusion candidate using
  the unchanged TOML candidate gates. Permit current business-turn and
  overlap-weighted diar-segment voiceprint evidence only; reject conflicts,
  prohibit neighbour/local-slot propagation, and preserve the baseline when no
  eligible direct evidence exists. The generator must not read references or
  emit correctness fields. Four focused tests pass and the 935-entry candidate
  was frozen with a complete 89-reference changed-context worksheet.
- [x] T059P Complete the 0-120-second contextual promotion review, followed only
  on success by chronological and reversed-block review of all 556 rows. All
  judgments and totals are manual; tools may arrange evidence but may not label
  correctness, aggregate accuracy, rank the candidate, or issue the verdict.
  The candidate failed the first gate: it did not recover the existing early
  short-turn failures and regressed Zhu Jie's "没说完" to Tang Yunfeng. No
  full-session result is claimed.
- [x] T059Q Generate one full continuous-v2.1 candidate with the exact pinned
  NeMo postprocessing defaults `0.5/0.5/0/0`, changing no other producer or
  fusion parameter. Retain frame posterior, segment, TitaNet mapping, source
  hashes, and common-time-base evidence without reading reference labels. The
  full run retained 45,189 frames, 1,370 native segments, 909 eligible segment
  voiceprints, and one four-slot acoustic mapping.
- [x] T059R Complete the 0-120-second contextual promotion review, followed only
  on success by chronological and reversed-block review of all 556 rows. The
  candidate must be rejected without retuning if the model-native contract does
  not establish the development gate. The preliminary old-grid projection is
  not a valid boundary-candidate verdict because it cannot split an inherited
  business interval. Production replay was reviewed over all 18 contributions
  in the gate: 15 are contextually correct and three remain incorrect, so no
  full-session result is claimed.
- [x] T059S Replay the frozen native diar segments, unchanged ASR, and unchanged
  forced-alignment tracks through the production `ComprehensiveTimeline` and
  `BusinessSpeakerPipeline`. Retain typed replay inputs, source hashes, and the
  rebuilt business view; then use that view for T059R. The replay consumed
  1,370 diar segments, 287 ASR finals, and 287 alignment groups and produced
  1,233 business intervals.
- [x] T059T Generate one replay-interval direct TitaNet candidate using the
  dedicated duration-aware TOML gates. Every interval must be embedded from its
  current audio only; no local-slot or neighbour identity propagation and no
  reference input are permitted. Both initial- and final-registry evidence was
  frozen; the final registry differs because it contains the current-session
  refreshed centroids.
- [x] T059U Complete the 0-120-second contextual promotion review, followed only
  on success by chronological and reversed-block review of all 556 rows. Do not
  sweep the short-duration boundary, score, or margin against the reference.
  The final-registry view repairs most of the long opening contribution but
  leaves the 72-second Shi Yi and 78-second Tang Yunfeng contributions wrong,
  and the 82-second Tang Yunfeng start is still wrong. The topology is rejected
  without a full-session result.
- [x] T059V Generate one final-registry duration-calibrated candidate from
  `speaker-v21-duration-calibrated.toml`. Below 1.5 seconds use only the existing
  `0.04` active-identity margin; at and above 1.5 seconds reuse the production
  `0.55` match threshold and existing `0.04` candidate margin. Do not read the
  reference, propagate neighbours/local slots, or sweep any value.
- [x] T059W Complete the 0-120-second contextual promotion review. Only on
  success, review all 556 rows chronologically and in reversed fixed blocks and
  derive every label and total manually. No code may assign correctness,
  aggregate accuracy, rank the candidate, or issue a verdict. The early gate
  passed, then complete chronological and reversed-block review manually
  recorded 469 correct / 86 incorrect / 1 ambiguous (`84.35%`). The topology
  fails the industrial gate; no threshold is retuned.
- [x] T059X Generate one aligned-phrase reaggregation candidate from
  `speaker-v21-aligned-phrase.toml`. Preserve every eligible current-interval
  direct voiceprint anchor. Fill only a sub-1.5-second low-evidence bridge whose
  immediate two anchors independently select the same active identity inside
  one ASR `text_id` and one forced-alignment run. Preserve every conflict,
  one-sided case, overlong bridge, pause crossing, and direct anchor. The
  generator may not read references or emit correctness fields. Nine legal
  bridges changed ten entries; all direct anchors and source text are retained.
- [x] T059Y Run the complete 0-120-second contextual promotion review. The
  candidate leaves this range unchanged and therefore preserves the accepted
  duration-calibrated early context. Stop the topology as an insufficient-
  coverage evidence probe without widening any parameter; no new full-session
  accuracy result is claimed.
- [x] T059Z Generate punctuation-bounded phrase spans from finalized ASR and
  forced alignment using `speaker-v21-punctuation-phrase.toml`, run each span's
  own audio through TitaNet against the final four-identity session registry,
  and freeze the complete score audit. No reference input or correctness field
  is permitted. All 1,397 frozen spans embedded successfully.
- [x] T059AA Build one phrase-overlay business candidate. Permit a phrase to
  fill low-evidence characters only when all overlapping eligible direct
  voiceprint anchors agree with its identity; reject every conflict and never
  rewrite an eligible direct anchor. Prove exact per-`text_id` ASR source-text
  preservation and common-clock bounds mechanically, then perform the complete
  0-120-second contextual promotion review. Only on success, review all 556
  rows chronologically and in reversed fixed blocks and manually derive and
  check every result. No code, script, formula, query, notebook, metric, or
  algorithm may label correctness, aggregate accuracy, rank/select a candidate,
  or issue a verdict. The 984-overlay candidate fails the 0-120-second manual
  gate: an unanchored phrase regresses Zhu Jie's `81.6-82.16` "然后呢" to Xu
  Zijing and the 96-second split remains unresolved. Stop without full review.
- [x] T059AB Rebuild from the same frozen phrase evidence using
  `speaker-v21-anchored-punctuation-phrase.toml`. Require at least one eligible
  direct interval anchor and unanimous identity agreement across every anchor;
  zero-anchor phrases become audit-only. Do not change any numeric gate. The
  frozen candidate accepts 802 phrases and produces 1,754 business-view turns.
- [x] T059AC Complete the 0-120-second contextual promotion review. Only on
  success, review all 556 rows chronologically and in reversed fixed blocks and
  manually derive and check every result. No code, script, formula, query,
  notebook, metric, or algorithm may label correctness, aggregate accuracy,
  rank/select a candidate, or issue a verdict. The candidate passes the early
  gate, then the complete two-direction manual review records 469 correct, 86
  incorrect, and one ambiguous row (approximately 84.35 percent). It repairs
  `ref-0033` but regresses `ref-0121` by propagating a one-sided Tang Yunfeng
  phrase anchor across Shi Yi's short interjection. Stop this topology without
  changing a numeric gate.
- [x] T059AD Freeze and generate one posterior-bounded aligned-phrase candidate
  from `speaker-v21-posterior-bounded-phrase.toml`. Split punctuation phrases
  at sustained active v2.1 raw-posterior local-slot transitions, embed every
  resulting piece independently with TitaNet, and permit a rewrite only when
  the piece identity agrees with the frozen local/global slot mapping. Preserve
  every regular direct anchor and every unresolved conflict. Mechanically prove
  exact source-text preservation, common-clock bounds, source hashes, and the
  absence of reference/correctness inputs or outputs. The frozen run produces
  1,376 bounded pieces, embeds 1,372 of them, accepts 982 consensus pieces, and
  emits a 1,406-turn terminal candidate; these are mechanical provenance facts,
  not product-accuracy measurements.
- [x] T059AE Complete the 0-120-second contextual promotion review. Only on a
  pass, review all 556 rows chronologically and again in reversed fixed blocks.
  Every semantic judgment, count, percentage, candidate comparison, and gate
  verdict must be performed manually from full context. No code, script,
  formula, query, notebook, metric, or algorithm may label correctness,
  aggregate accuracy, rank/select the candidate, or issue a verdict. The early
  gate passes. The complete two-direction manual review records 472 correct,
  83 incorrect, and one ambiguous row (approximately 84.9 percent), repairing
  `ref-0033`, `ref-0156`, and `ref-0459` with no semantic regression. This
  remains below the industrial gate, so stop this topology without integration
  or numeric retuning. See
  `posterior-bounded-phrase-review-2026-07-15.md`.
- [x] T059AF Freeze and generate one multiresolution phrase candidate from
  `speaker-v21-multiresolution-phrase.toml`. Reuse every posterior-bounded piece
  and TitaNet score without rerunning or changing a gate. Add one-frame raw
  micro pieces, enclosing-phrase TitaNet evidence, global active-run extent,
  three-view regular-anchor splitting, strong dual-voiceprint correction, and
  isolated unanchored short correction exactly as FR16K defines. Prove exact
  source-text preservation, common-clock bounds, deterministic overlay priority,
  source hashes, and reference-free provenance with focused tests. Mechanical
  counts are evidence inventory only and may not be treated as accuracy. The
  one-shot candidate contains 1,376 inherited posterior pieces, 170 one-frame
  micro pieces, 24 accepted corrections, and 1,456 output turns; focused tests
  pass and no reference data enters generation.
- [x] T059AG Complete the multiresolution candidate's full-context gate. Review
  the complete 0-120-second prefix manually; only on a pass, review all 556
  reference contexts chronologically and again in reversed fixed blocks. Every
  correctness judgment, result count, percentage, comparison, configuration
  decision, and verdict is manual. No code, script, formula, query, notebook,
  metric, or algorithm may evaluate, aggregate, rank, select, promote, or reject
  the result. The complete manual review records `475 correct / 80 incorrect /
  1 ambiguous`, approximately 85.4 percent. It repairs `ref-0035`, `ref-0037`,
  and `ref-0109` without a contextual regression, but remains below the
  industrial gate and is not integrated. See
  `multiresolution-phrase-review-2026-07-15.md`.
- [x] T059AH Implement FR16L in `SpeakerIdentityStage` with a TOML-owned
  candidate confirmation count. Preserve all existing score, margin, clean-
  span, overlap, epoch-age, and backfill gates. Add deterministic unit tests for
  one-candidate preservation, repeated same-identity confirmation, conflicting
  candidate replacement, strong active-epoch cancellation, and a strong return
  split. Update resolved-config serialization and configuration tests. The
  focused state-machine and configuration tests pass; they also expose and fix
  a prior backfill bug that allowed a return split to rewrite clean evidence
  already committed to the current epoch.
- [x] T059AI Add a C++ frozen identity-stage replay probe that reads the native
  v2.1 segment CSV, original audio, initial registry, and one TOML; streams them
  on the common time base through the production stage and exports only
  evidence-bearing diarization identities. Replay those identities through the
  production business-speaker view and freeze source hashes. No reference or
  correctness input/output is permitted. The probe preserves all 1,370 segment
  intervals and, with unknown-identity drift disabled in the candidate TOML,
  retains exactly the established four active identities.
- [x] T059AJ Complete the repeated-candidate epoch full-context gate. Manually
  review the complete 0-120-second prefix; only on a pass, manually review all
  556 contexts chronologically and again in reversed fixed blocks. No code,
  script, formula, query, notebook, metric, or algorithm may evaluate,
  aggregate, rank, select, promote, or reject the result. Integrate only after
  the manual industrial gate is met without a critical-turn regression. The
  0-120-second prefix is unchanged, but manual review of all 20 changed
  full-session contexts rejects the topology: one long local-slot epoch mixes
  rapidly alternating Tang Yunfeng and Zhu Jie turns and introduces multiple
  clear Zhu-to-Tang regressions.
- [x] T059AK Implement FR16M as a reference-free multi-prototype TitaNet
  evidence reducer. Require identical evidence IDs, intervals, statuses, and
  complete score galleries; combine only scores sharing one registry ID; emit
  both source hashes and no correctness field. Add focused mechanical tests.
  The reducer and five focused tests are complete.
- [x] T059AL Build one frozen duration-calibrated direct candidate from the
  pre-session and session-refreshed registries. Apply the unchanged TOML score,
  margin, and duration gates. Manually complete the 0-120-second contextual
  gate and, only on a pass, all changed contexts plus the full chronological
  and reversed-block review. No executable automation may evaluate, aggregate,
  rank, select, promote, or reject the result. Manual review of all 38 changed
  contexts rejects the candidate: it repairs reference contexts 127 and 500
  but clearly regresses 66, 405, 407, and 478.
- [x] T059AM If T059AL passes without regression, apply the same frozen multi-
  prototype gallery to posterior-bounded pieces and punctuation phrases,
  rebuild the terminal multiresolution candidate, and complete the same manual
  full-context review before any production integration. T059AL did not pass,
  so this conditional branch is closed without generation.
- [ ] T059AN Implement FR16N as a reference-free initial-prototype challenger
  over the accepted multiresolution baseline. Require an eligible initial
  direct identity, disagreement with the terminal direct identity, exact raw
  local-slot agreement on the forced-aligned source range, and no eligible
  conflicting terminal piece or punctuation-phrase identity. Freeze every
  enabled contract in TOML and add focused mechanical tests.
- [ ] T059AO Generate one frozen FR16N candidate from the existing native v2.1
  evidence. Verify source hashes, direct-track parity, active-ID completeness,
  exact text reconstruction, and common-clock projection. Do not read or emit
  reference labels or correctness judgments.
- [ ] T059AP Manually judge the complete 0-120-second prefix. Only on a pass,
  manually review every changed context and then all 556 contexts in
  chronological and reversed fixed-block order. No code, script, formula,
  query, notebook, metric, or algorithm may evaluate, aggregate, rank, select,
  promote, or reject the candidate.
- [ ] T059AQ Implement FR16O clean-gallery span selection. Reuse production
  clean-span gates, `confidence * duration` ordering, and `max_ref_segs`; require
  raw-local, pre-session, and session-refreshed identity agreement. Freeze the
  contract in TOML and add focused mechanical tests.
- [ ] T059AR Add a native C++ TitaNet embedding probe that emits normalized
  current-audio embeddings for arbitrary absolute-clock spans without reading
  diar labels, transcript meaning, real names, or reference judgments. Verify
  dimensions, source intervals, and failure statuses.
- [ ] T059AS Build one reference-free direct clean-gallery candidate by maximum
  reduction within each stable ID and unchanged duration-aware cross-identity
  gates. Verify complete galleries, hashes, interval parity, and exact source
  reconstruction.
- [ ] T059AT Apply the direct gallery only through the FR16N raw-local and
  current piece/phrase veto topology. Manually review 0-120 seconds, all changed
  contexts, and then all 556 chronological and reversed-block contexts. No
  executable automation may evaluate, aggregate, rank, select, promote, or
  reject the result.
- [ ] T059AU Implement FR16P clean-gallery/current-audio consensus. Require at
  least one agreeing eligible posterior-piece or punctuation-phrase identity,
  reject every eligible conflict, freeze the switch in TOML, preserve exact
  source intersections, and add focused mechanical tests.
- [ ] T059AV Generate one FR16P candidate from frozen evidence and complete the
  same manual prefix, changed-context, chronological, and reversed-block review
  before integration. No executable automation may assign correctness,
  aggregate accuracy, rank/select a candidate or parameter, or issue a verdict.
- [ ] T059AW Implement FR16Q four-view non-top-channel consensus. Reuse the
  frozen 0.5 native-channel activity gate and one-frame micro floor, require
  clean-gallery direct/piece/phrase agreement, reject every current or gallery
  conflict, preserve exact source ranges, and add focused mechanical tests.
- [ ] T059AX Generate one FR16Q candidate and perform the complete manual
  prefix, changed-context, chronological, and reversed-block semantic review.
  No executable automation may evaluate, aggregate, rank, select, promote, or
  reject the result.
- [ ] T059AY Implement FR16R gallery multiscale/channel consensus. Require
  clean-gallery exact-piece and enclosing-phrase agreement, the unchanged
  raw-local identity, native-channel activity/run contract, and no eligible
  session-refreshed conflict. Freeze the switch in TOML, preserve exact aligned source ranges,
  and add focused mechanical tests.
- [ ] T059AZ Generate one FR16R candidate and manually review every changed
  context before any full chronological/reversed-block review. No executable
  automation may evaluate, aggregate accuracy, rank/select a candidate or
  parameter, or issue a promotion/rejection verdict.
- [ ] T059BA Implement FR16S single-gallery-scale/channel consensus. Require
  exactly one eligible gallery scale, raw-local and native-channel agreement,
  no current-audio conflict, unchanged gates, exact projection, TOML freezing,
  and focused mechanical tests.
- [ ] T059BB Generate one FR16S candidate and manually review every changed
  context before chronological/reversed-block review. Executable automation
  remains prohibited from assigning correctness or a product verdict.
- [ ] T059BC Implement FR16T current multiscale/channel consensus with exact
  piece/phrase/raw-local/native-channel agreement, clean-gallery conflict veto,
  baseline-disagreement guard, exact projection, TOML freezing, and focused
  mechanical tests.
- [ ] T059BD Generate one FR16T candidate and manually review every changed
  context before any full review; tools may arrange evidence but may not assign
  correctness, rank candidates, or issue a verdict.
- [ ] T059BE Implement FR16U current single-scale/channel consensus with exact
  one-scale eligibility, raw-local/native-channel agreement, clean-gallery
  positive support and conflict veto, unchanged gates, exact projection, and
  focused tests.
- [ ] T059BF Generate one FR16U candidate and manually review every changed
  context before any full review; executable result evaluation remains
  prohibited.
- [ ] T059BG Implement FR16V sustained raw-local restoration with the inherited
  FR16K activity and sustained-run gates, all-voiceprint conflict veto,
  baseline-disagreement guard, exact projection, TOML consistency check, and
  focused tests.
- [ ] T059BH Generate one FR16V candidate and manually review every changed
  context before any full review; no executable accuracy or verdict is allowed.
- [ ] T059BI Implement FR16W dual-registry multiscale voiceprint consensus with
  four eligible agreeing views, baseline-disagreement guard, exact projection,
  TOML switch, unchanged gates, and focused tests.
- [ ] T059BJ Generate one FR16W candidate and manually review every changed
  context before any full review; tools may arrange but never judge results.
- [ ] T059BK Implement FR16X dominant raw-micro restoration with exact frame
  count/range validation, sole-active/top-1/local-slot agreement, overlay
  conflict rejection, exact projection, TOML switch, and focused tests.
- [ ] T059BL Generate one FR16X candidate and manually review every changed
  context before any full review; no executable result evaluation is allowed.
- [x] T059BM Implement FR16Y raw-authoritative fusion with TOML-owned allowed
  reasons, immutable text/identity/source parity, exact forced-alignment
  projection, source hashes, audit records, and focused mechanical tests.
- [x] T059BN Generate the `sole_diar_support` FR16Y candidate and manually
  review every one of its 52 changed reference contexts. The candidate is
  rejected because it clearly regresses multiple previously correct speaker
  turns across the session; no full candidate result is claimed. See
  `raw-authoritative-review-2026-07-15.md`. No executable result judgment was
  used.
- [x] T059BO Implement FR16Z exact-phrase dual-gallery consensus using the
  existing session-refreshed and clean multi-prototype phrase decisions,
  unchanged duration/score/margin gates, competing-direct-anchor veto, exact
  source projection, conflict rejection, a TOML boolean, and focused mechanical
  tests. The generator may not read references or emit correctness fields.
- [x] T059BP Generate one FR16Z candidate and manually review all 37 changed
  contexts. Reject the candidate because shared-model gallery agreement causes
  clear speaker regressions, including a phrase spanning the real change at
  `ref-0121`. Do not perform a complete candidate review. See
  `dual-gallery-phrase-review-2026-07-15.md`. No executable result judgment was
  used.
- [x] T059BQ Implement FR16ZA native-channel guards. Require the agreed target
  channel to become native frame top-1 inside the exact phrase and reject any
  different channel with an active top-1 run meeting the unchanged FR16K
  `0.5` activity and `0.4 s` sustained-run contract. Verify exact TOML parity,
  common-clock frame bounds, exact projection, and focused mechanical tests.
- [x] T059BR Generate one FR16ZA candidate and manually review all 20 changed
  contexts. The guard removes the cross-speaker phrase regression but still
  produces an unsafe confident fill over a pure-unknown phrase and expands a
  short-only direct anchor. Do not perform a complete review. See
  `dual-gallery-native-review-2026-07-15.md`; no executable result judgment was
  used.
- [x] T059BS Implement FR16ZB known-conflict and regular-anchor guards. Require
  at least one different known baseline identity in the exact phrase. If any
  direct anchor exists, require at least one agreeing regular direct anchor and
  reject short-only anchor support. Use only existing categorical reasons,
  retain every FR16ZA guard, and add focused mechanical tests.
- [x] T059BT Generate one FR16ZB candidate and manually review all 11 changed
  contexts. No new whole-turn regression is found, but only one previously
  incorrect reference contribution is clearly repaired, so stop before a
  complete review as insufficient coverage. See
  `dual-gallery-final-review-2026-07-15.md`. Executable result evaluation was not
  used.
- [x] T059BU Implement FR16ZC robust clean-gallery evidence from frozen query
  and prototype embeddings. Require complete normalized galleries and
  `top_half_mean` aggregation from TOML, preserve all query metadata/source
  hashes, reuse unchanged downstream gates, and add focused mechanical tests.
  The focused suite covers complete-gallery enforcement, normalization,
  deterministic top-half aggregation, and metadata preservation.
- [x] T059BV Regenerate only the clean-gallery punctuation-phrase evidence,
  substitute it into FR16ZB, and manually review every changed context. No code,
  script, formula, query, notebook, metric, or algorithm may assign product
  correctness, aggregate accuracy, rank/select the candidate, choose a result
  parameter, or issue the verdict. The guarded path is retained after complete
  changed-context review; a separate broad R/T/U substitution is rejected after
  both of its changed contexts regress real speaker boundaries. See
  `robust-gallery-review-2026-07-15.md`.
- [x] T059BW Implement FR16ZD VAD-bounded relative-top-1 phrase evidence.
  Require frozen VAD speech support, one continuous native top-1 channel for
  the unchanged FR16J run floor, exact forced-alignment projection, agreement
  between session-refreshed TitaNet, robust clean-gallery TitaNet, and the
  local-slot mapping, a different known baseline identity, TOML parity, source
  hashes, and focused mechanical tests.
- [x] T059BX Generate one FR16ZD candidate on top of the retained robust FR16ZB
  path and manually review every changed context before any complete review.
  Tools may execute models, verify contracts, and arrange unjudged evidence;
  no code, script, formula, query, notebook, metric, or algorithm may assign
  correctness, aggregate accuracy, rank/select a candidate or parameter, or
  issue the verdict.
- [x] T059BY Implement FR16ZE complete local-channel island evidence. Require a
  VAD-continuous `A-B-A` top-1 sequence with all three runs meeting the
  unchanged FR16J floor, query TitaNet on the complete B run, project only
  wholly contained forced-alignment units, require session-registry, robust-
  gallery, and frozen local-map agreement, preserve exact text/time contracts,
  freeze all switches in TOML, and add focused mechanical tests.
- [x] T059BZ Generate one FR16ZE candidate on top of the retained robust FR16ZB
  path and manually review every changed context before any complete review.
  Tools may arrange evidence only; no executable result judgment, aggregation,
  ranking, candidate selection, parameter selection, or verdict is permitted.
- [x] T059CA Implement FR16ZF in `GpuVad::DrainSegments`: apply the TOML-owned
  sample padding at emission, clamp to the processed horizon, update the stale
  interface contract, and add a paired zero-padding/configured-padding real-
  audio mechanical test that proves count and exact-boundary behavior without
  assigning product correctness.
- [x] T059CB Regenerate frozen VAD evidence through the native runtime after
  FR16ZF, inspect all changed endpoint contexts manually, and rerun speaker
  evidence only if endpoint review shows no semantic regression. No executable
  product evaluation, result aggregation, ranking, parameter selection, or
  verdict is permitted.
- [x] T059CC Implement FR16ZG complete short-VAD-utterance evidence. Require a
  padded VAD interval in the inherited FR16J/TitaNet short-duration bounds,
  one native top-1 channel throughout, complete-interval acoustic queries,
  session-registry, robust-gallery, and frozen local-map agreement, exact
  wholly-contained forced-alignment projection, TOML parity, source hashes,
  and focused mechanical tests.
- [x] T059CD Generate one FR16ZG candidate on top of the retained guarded
  robust-gallery path and manually review every changed context before any
  complete review. Tools may execute models, verify contracts, and arrange
  unjudged evidence only; no executable correctness judgment, result
  aggregation, ranking, candidate/parameter selection, or verdict is permitted.
  The complete two-context manual review rejects the unrestricted candidate:
  one context regresses a continuous contribution and the other is only a
  partial repair. See `vad-utterance-review-2026-07-15.md`.
- [x] T059CE Implement FR16ZH padded-VAD edge-run evidence. Require a
  multi-run VAD interval, first/last-run scope, inherited sustained floors for
  the candidate and adjacent run, complete-query parity with the TOML maximum
  embedding window, dual TitaNet/local-map agreement, exact wholly-contained
  alignment projection, known baseline conflict, source hashes, and focused
  mechanical tests.
- [x] T059CF Generate one FR16ZH candidate on top of the retained guarded
  robust-gallery path and manually review every changed context. Tools may
  execute models, check contracts, and arrange unjudged context only; no code,
  script, formula, query, notebook, metric, or algorithm may assign product
  correctness, aggregate accuracy, rank/select candidates or parameters, or
  issue the verdict.
  The complete nine-context manual review retains one repair but rejects the
  unrestricted candidate because it also trades an adjacent speaker boundary
  and produces incomplete character-only changes. See
  `vad-edge-run-review-2026-07-15.md`.
- [x] T059CG Implement FR16ZI by requiring every eligible FR16ZH edge-run frame
  to remain below the unchanged TOML/FR16J activity threshold. Add exact-parity
  and threshold-boundary mechanical tests; retain every other FR16ZH contract.
- [x] T059CH Generate one FR16ZI candidate and manually review every displayed
  changed context. No executable product judgment, aggregation, ranking,
  candidate/parameter selection, or verdict is permitted.
  The single displayed change completely repairs `ref-0146`; no other
  assignment changes. Retain the guarded candidate as the next baseline. See
  `vad-edge-run-review-2026-07-15.md`.
- [x] T059CI Implement FR16ZJ active VAD-edge handoff evidence. Require every
  frame in both edge and adjacent runs to meet the unchanged FR16J activity
  threshold, both runs to meet the inherited duration floor, complete-query
  window parity, exact wholly-contained alignment projection, a frozen local
  mapping and known baseline conflict, plus an unchanged dual-TitaNet agreed-
  different-identity veto. Add focused mechanical tests and source hashes.
- [x] T059CJ Generate one FR16ZJ candidate on top of retained FR16ZI and
  manually review every displayed changed context before broader use. Tools
  may execute and arrange evidence only; no executable correctness judgment,
  aggregation, ranking, candidate/parameter selection, or verdict is allowed.
  The complete 24-context manual review rejects the unrestricted candidate:
  one complete repair does not offset multiple semantic-boundary regressions.
  FR16ZI remains the retained baseline. See
  `vad-active-edge-review-2026-07-15.md`.
- [x] T059CK Implement FR16ZK complete local-phrase evidence. Require one
  finalized punctuation phrase wholly inside one padded-VAD interval, exact
  parity with the inherited sustained floor and TitaNet embedding window, one
  native top-1 local channel for every phrase frame, a frozen local mapping,
  one different known baseline identity across the complete source range, and
  an independent different-identity veto from either unchanged TitaNet view.
  Also preserve the uniform baseline when either raw TitaNet ranking places it
  first, even if the duration-aware gate abstains; never use a sub-gate rank as
  positive rewrite evidence.
  Project only the exact complete phrase and add focused mechanical tests and
  source hashes.
- [x] T059CL Generate one FR16ZK candidate on top of retained FR16ZI and
  manually review every displayed changed context before broader use. Tools
  may execute models, verify contracts, and arrange unjudged context only; no
  code, script, formula, query, notebook, metric, or algorithm may assign
  product correctness, aggregate accuracy, rank/select a candidate or
  parameter, or issue the verdict.
  The guarded candidate retains two complete phrases. Manual review of all
  four displayed contexts finds one residual contribution repair, one safe
  strengthening, and no regression. Retain it as the next composition
  baseline. See `complete-local-phrase-review-2026-07-15.md`.
- [x] T059CM Implement FR16ZL bracketed unknown-phrase evidence. Reuse FR16ZK
  complete-phrase eligibility; require unknown-only baseline coverage, known
  immediately adjacent source fragments with one identity, frozen local-map
  agreement, and a different-identity veto from either unchanged TitaNet view.
  Preserve exact source/time contracts and add focused tests and source hashes.
- [x] T059CN Generate one FR16ZL candidate on top of retained FR16ZK and
  manually review every displayed changed context. Tools may generate and
  arrange evidence but no executable mechanism may assign correctness,
  aggregate accuracy, rank/select candidates or parameters, or issue a verdict.
  The frozen candidate has zero accepted phrases because the sole unknown-only
  complete phrase lacks exact known brackets. Close this tested no-op without
  relaxing source adjacency.
- [x] T059CO Implement FR16ZM bracketed known-conflict phrase evidence. Reuse
  FR16ZK complete-phrase eligibility; require a uniform different known phrase
  identity, immediately adjacent finalized phrases in the same ASR source with
  one uniform mapped stable identity, and an eligible conflict veto from either
  unchanged TitaNet view.
  Preserve exact projection and add focused tests and source hashes.
- [x] T059CP Generate one FR16ZM candidate on top of retained FR16ZK and
  manually review every displayed changed context before broader use. No
  executable product judgment, aggregation, ranking, candidate/parameter
  selection, or verdict is permitted.
  The frozen candidate has zero accepted phrases. All 27 mechanically valid
  same-identity phrase brackets have a native local mapping that supports the
  middle phrase identity instead of the bracket identity. Close this tested
  no-op without weakening the local-map or immediate-phrase contracts.
- [x] T059CQ Implement FR16ZN expanded-local-run phrase evidence. Reuse FR16ZK
  complete-phrase eligibility, expand only the acoustic query to the maximal
  contiguous same-top-1 run inside the containing padded VAD, require the full
  run to fit unchanged TOML duration bounds, and require eligible agreement
  among session registry, robust gallery, and frozen local mapping. Preserve
  exact phrase-only projection and add focused tests and source hashes.
- [x] T059CR Generate one FR16ZN candidate on top of retained FR16ZK and
  manually review every displayed changed context before broader use. Tools may
  execute models, verify contracts, and arrange unjudged evidence only; no
  executable product judgment, aggregation, ranking, candidate/parameter
  selection, or verdict is permitted.
  The complete two-context manual review rejects the sole rewrite: the phrase
  belongs to Zhu Jie but is assigned to Xu Zijing. Retain guarded FR16ZK and
  use expanded-run voiceprint only as counter-evidence. See
  `expanded-local-run-phrase-review-2026-07-15.md`.
- [x] T059CS Implement FR16ZO bounded local-run voiceprint override evidence.
  Reuse FR16ZK complete-phrase eligibility, derive a deterministic query of at
  most the existing TOML embedding window from the containing same-top-1 run,
  and require eligible session/robust agreement on an identity different from
  both the uniform baseline and frozen local mapping. Require the selected
  identity's mapped native channel to have the unchanged FR16J sustained
  support inside the query. Preserve exact phrase-only projection and add
  focused tests and source hashes.
- [x] T059CT Generate one FR16ZO candidate on top of retained FR16ZK and
  manually review every displayed changed context before broader use. Tools may
  execute models, verify contracts, and arrange unjudged evidence only; no
  executable product judgment, aggregation, ranking, candidate/parameter
  selection, or verdict is permitted.
  The unguarded four-context review finds two regressions where the selected
  voiceprint identity has no native channel support. The inherited sustained-
  channel guard leaves only `ref-0258`; manual context review repairs its main
  Tang Yunfeng contribution with one approximately `0.4 s` repeated-fragment
  boundary residual. Retain the guarded candidate. See
  `bounded-local-run-voiceprint-review-2026-07-15.md`.
- [x] T059CU Implement FR16ZQ temporally stratified clean-gallery selection.
  Retain all FR16O eligibility gates and the complete TOML gallery size, divide
  each identity's common-clock-ordered eligible set into that many contiguous
  rank strata, and use the existing quality maximum within each stratum. Add
  focused deterministic/completeness tests and source hashes.
- [x] T059CV Generate the FR16ZQ prototype embeddings and robust scores, first
  substitute them only into retained FR16ZO, and manually review every changed
  context. No executable product judgment, aggregation, ranking,
  candidate/parameter selection, or verdict is permitted.
  The substitution reproduces guarded FR16ZO without a new speaker assignment;
  guarded exact-phrase output is also unchanged. Close as component evidence.
- [x] T059CW Freeze FR16ZR at the committed production `10.0 s` speaker
  embedding window while retaining every guarded FR16ZO decision and projection
  contract. Allow selected-identity raw support through either the inherited
  sustained active channel or unanimous native top-2 across the complete
  query. Add policy parity and categorical-boundary coverage; do not change any
  score or margin gate.
- [x] T059CX Generate one FR16ZR candidate on top of guarded FR16ZO and manually
  review every changed context before broader use. Tools may execute models,
  verify contracts, and arrange unjudged evidence only; no executable product
  judgment, aggregation, ranking, candidate/parameter selection, or verdict is
  permitted.
  The sole changed context, `ref-0194`, moves only the indexed prefix to Xu
  Zijing and leaves the unindexed suffix on Tang Yunfeng. Manual review records
  this as a partial repair and does not retain FR16ZR alone. See
  `production-window-complete-source-review-2026-07-15.md`.
- [x] T059CY Implement FR16ZS complete-source projection. Require exactly one
  indexed phrase in the ASR source, complete forced alignment of every non-
  separator character inside the inherited FR16ZR query, one uniform known
  source identity, and the unchanged FR16ZR identity decision. Add focused
  alignment/source-boundary tests and hashes.
- [x] T059CZ Generate one FR16ZS candidate on top of guarded FR16ZO and manually
  review every changed context. No executable product judgment, aggregation,
  ranking, candidate/parameter selection, or verdict is permitted.
  The sole changed context remains `ref-0194`; complete contextual review
  confirms that the whole contribution belongs to Xu Zijing and that no other
  speaker sequence changes. Retain FR16ZS as the next composition baseline.
  See `production-window-complete-source-review-2026-07-15.md`.
- [x] T059DA Implement FR16ZT complete edge-contribution evidence. Enumerate
  punctuation clauses without the phrase character floor, require complete
  in-run forced alignment for every non-separator character, require the
  terminal edge run, merge only
  source-adjacent eligible clauses, require one uniform known baseline
  identity, and reuse the inherited active-edge local mapping and dual-
  voiceprint veto. Add focused completeness, adjacency, and abstention tests.
- [x] T059DB Generate one FR16ZT candidate on top of retained FR16ZS and
  manually review every changed context before retention. Tools may execute
  models, verify contracts, and arrange unjudged evidence only. No executable
  product judgment, accuracy aggregation, candidate/parameter ranking or
  selection, or verdict is permitted.
  Manual review rejects all three initial-edge changes and retains the four
  terminal-edge groups. Three complete business contributions are repaired at
  `ref-0162`, `ref-0221`, and `ref-0404`; the fourth change is attribution-
  neutral and no reviewed context regresses. See
  `complete-edge-contribution-review-2026-07-15.md`.
- [x] T059DC Implement FR16ZU secondary-channel complete-phrase evidence. Keep
  the frozen punctuation and TitaNet query bounds, require both voiceprint
  views to have one top-ranked active identity with the existing duration
  margin, and require that identity's mapped raw channel to satisfy the
  inherited `0.5`/`0.4 s` sustained-activity contract while being non-top-1 on
  at least one phrase frame. Project only one uniform-conflict complete phrase.
- [x] T059DD Generate FR16ZU once on retained FR16ZT and manually review every
  changed context. Tools may check numerical/source contracts and arrange
  unjudged evidence only; no executable product judgment, accuracy aggregation,
  candidate/parameter ranking or selection, or verdict is permitted.
  The frozen generation enumerates 1195 structurally eligible phrase contexts
  and produces zero timeline writes because none satisfies every independent
  evidence gate. Zero writes is a mechanical no-op, not an accuracy judgment;
  no changed context exists for manual semantic review and retained FR16ZT is
  unchanged. See `secondary-channel-phrase-review-2026-07-15.md`.
- [x] T059DE Implement FR16ZV bracketed local-churn contribution evidence.
  Derive native top-1 runs from the frozen frame table, require same-channel
  stable outer runs and a bounded different-channel interior, construct one
  deterministic production-window query, require unchanged regular-gate
  agreement from session and robust TitaNet, and project only fully aligned
  complete punctuation phrases. Preserve a dual-phrase-voiceprint veto for a
  single-channel interior. After complete review of the unrestricted candidate,
  require every projected phrase to have one uniform known conflicting baseline
  identity and require both phrase-level galleries to rank the outer identity
  first. Reject mixed phrases, phrase-rank disagreement, and conflicting
  overlays. Add a complete-clause-group path only when every intervening run
  has lower mean native margin than both outer runs and no phrase has dual-
  gallery agreement on another top-ranked identity. Require one active native
  channel on every intervening frame so overlap cannot be absorbed. Record
  source hashes and add focused mechanical tests.
- [x] T059DF Generate FR16ZV once on retained FR16ZT and manually review every
  changed conversational context before retention. Tools may execute models,
  verify numerical/source/time contracts, and arrange unjudged evidence only.
  No code, script, formula, query, notebook, metric, or algorithm may assign
  correctness, aggregate accuracy, rank or select a candidate or parameter, or
  issue the verdict.
  Complete review rejects the unrestricted 22-piece candidate. The frozen
  phrase and overlap guards leave three changed contexts: `ref-0441` becomes a
  complete Shi Yi contribution, while `ref-0452` and `ref-0554` receive
  attribution-consistent cleanup. No reviewed context regresses; retain the
  final guarded candidate. See
  `bracketed-local-churn-review-2026-07-15.md`.
- [x] T059DG Implement FR16ZW maximal multi-slot envelope evidence. Choose the
  farthest same-channel stable closure that fits the inherited production
  query capacity, require at least two foreign local channels and greater
  combined outer support, maximize actual outer audio deterministically,
  require unchanged regular-gate agreement from both TitaNet galleries, veto
  dual phrase-top support for the duration-dominant foreign channel, and
  project only complete aligned clauses per ASR source wholly contained in one
  frozen comprehensive-timeline VAD segment. Reject VAD-split projections and
  conflicting envelopes, preserve source hashes, and add focused mechanical
  tests.
- [x] T059DH Generate FR16ZW once on retained FR16ZV and manually review every
  changed conversational context before retention. Tools may execute models,
  verify numerical/source/time contracts, and arrange unjudged evidence only.
  No code, script, formula, query, notebook, metric, or algorithm may assign
  correctness, aggregate accuracy, rank or select a candidate or parameter, or
  issue the verdict. Complete review rejects the unrestricted 12-accepted-query
  candidate because five classes bridge independent VAD endpoints. The frozen
  comprehensive-timeline VAD guard leaves three changed contexts; all three
  are contextual repairs with no reviewed regression. Retain the guarded
  candidate. See `maximal-multislot-envelope-review-2026-07-15.md`.
- [x] T059DI Implement FR16ZX VAD-complete contribution evidence. Require one
  frozen VAD segment within the production window, at least two inherited
  stable native channels, one strict stable-duration majority, unchanged
  session/robust TitaNet agreement with that channel's mapping, a dual-phrase
  different-identity veto, and adjacent complete forced-alignment clauses
  wholly contained by the VAD. Require every written clause to independently
  pass both unchanged TitaNet galleries with the VAD identity, contain an
  existing selected-identity anchor, and receive the same dual identity on
  every exact conflicting baseline fragment. Project only actual baseline
  conflicts, reject conflicting overlays, preserve all source hashes, and add
  focused tests.
- [x] T059DJ Generate FR16ZX once on retained FR16ZW and manually review every
  changed conversational context before retention. Tools may execute models,
  verify numerical/source/time contracts, and arrange unjudged evidence only.
  No code, script, formula, query, notebook, metric, or algorithm may assign
  correctness, aggregate accuracy, rank or select a candidate or parameter, or
  issue the verdict. Complete review rejects the unrestricted VAD and clause-
  only projections because long crops absorb real short replies. Requiring an
  existing selected anchor and dual agreement on every exact conflicting
  fragment leaves zero writes. Close FR16ZX as a safe no-op and preserve
  retained FR16ZW. See `vad-complete-contribution-review-2026-07-15.md`.
- [x] T059DK Implement FR16ZY complete-source unanimous-phrase evidence. Use a
  fully aligned production-window ASR source only as query context, require
  dual source identity agreement, inherited stable support from that identity's
  mapped native channel, frozen VAD containment for every indexed phrase, and
  unanimous dual-gallery phrase top rank. Require the selected identity at both
  baseline phrase boundaries and project only internal complete indexed phrase
  conflicts. Reject conflicting overlays, record hashes, and add focused tests.
- [x] T059DL Generate FR16ZY once on retained FR16ZW and manually review every
  changed conversational context before retention. Tools may execute models,
  verify numerical/source/time contracts, and arrange unjudged evidence only.
  No code, script, formula, query, notebook, metric, or algorithm may assign
  correctness, aggregate accuracy, rank or select a candidate or parameter, or
  issue the verdict. The initial candidate changes one visible context and
  merges a real source-edge handoff. Requiring the selected identity at both
  phrase boundaries leaves zero writes. Close FR16ZY as a safe no-op and
  preserve FR16ZW. See
  `complete-source-unanimous-phrase-review-2026-07-15.md`.
- [x] T059DM Implement FR16ZZ typed equal-overlap arbitration. Add the
  `timeline.speaker_overlap_tie_policy` TOML field, preserve `shorter_span` as
  the default, implement `higher_confidence` as confidence-first with segment
  length as the secondary deterministic tie-break, serialize the resolved
  value, and add focused configuration and business-pipeline tests. Do not
  alter overlap priority, boundaries, model evidence, or raw tracks.
- [x] T059DN Replay the frozen full-session raw tracks once with the FR16ZZ
  experiment TOML, mechanically verify deterministic/source/time contracts,
  arrange every changed context, and read every displayed conversation
  manually before retention. No code, script, formula, query, notebook,
  metric, or algorithm may assign correctness, aggregate accuracy, rank/select
  the policy, or issue the verdict. Both replays are byte-identical. Manual
  reading stops after the first 20 of 188 displayed changed contexts because
  genuine short replies are absorbed; the candidate is rejected and the old
  default remains. See `confidence-overlap-review-2026-07-15.md`.
- [ ] T059DO Implement FR16AAA primary top-1 replay evidence. Read frozen native
  frames, VAD spans, local-slot mapping, and TOML; emit only VAD-contained,
  `0.5`-active, same-top-1 runs meeting the inherited `0.4 s` sustained floor.
  Short runs abstain without neighbour fill. Record source hashes and add
  focused boundary, duration, mapping, and deterministic tests.
- [ ] T059DP Replay the primary track through the production C++ business
  pipeline, then compose only exact source ranges whose reasons are present in
  the frozen TOML allowlist of previously completed manual voiceprint reviews.
  Verify immutable source/time contracts mechanically, read every changed
  context manually, and only then perform complete chronological and reverse-
  block contextual semantic review. No code, script, formula, query, notebook,
  metric, or algorithm may assign correctness, aggregate accuracy, rank/select
  a candidate or parameter, or issue the verdict.
- [ ] T059DQ Implement FR16AAB typed primary-speaker arbitration. Add a
  separate comprehensive-timeline producer track, preserve raw activity
  diarization, allow primary evidence only for an exact maximum-overlap tie,
  retain the existing activity policy as fallback, expose the selected reason,
  and add focused isolation, unique-winner, matching-tie, ambiguous-primary,
  and configuration tests.
- [ ] T059DR Replay frozen activity and primary tracks twice through the
  production C++ business projector, verify deterministic/source/time/track
  contracts mechanically, arrange every changed context, and read every one
  manually. Only a no-regression contextual result may proceed to complete
  chronological and reverse-block review. No executable mechanism may assign
  correctness, aggregate accuracy, select the policy, or issue the verdict.
- [x] T059DS Implement the FR16AAC primary-aligned-island evidence composer.
  Require one VAD-bounded sustained primary run, same-identity production
  activity support, robust clean-gallery TitaNet agreement under existing
  duration-class gates, complete forced-alignment units, and one conflicting
  known baseline identity. Add focused abstention, conflict, source, time, and
  deterministic contract tests without reading any reference result.
- [x] T059DT Generate the full-session FR16AAC candidate twice, verify only its
  mechanical hashes and immutable source/time contracts, arrange every changed
  context, and read every context manually. A retained candidate must then pass
  complete chronological and reverse-block contextual semantic review. No code,
  script, formula, query, notebook, metric, or algorithm may assign correctness,
  aggregate accuracy, rank/select a candidate or parameter, or issue a verdict.
  The two business tracks are identical. All 36 displayed changed contexts are
  read manually; genuine cross-speaker boundary regressions remain, so the
  candidate is rejected before full-session review. See
  `primary-aligned-island-review-2026-07-16.md`.
- [x] T059DU Implement the FR16AAD complete-phrase cross-prototype composer.
  Require frozen accepted challenge provenance, exact punctuation-phrase source
  equality, initial/source/local identity parity, a different terminal override,
  one conflicting known current-baseline identity, protected-overlay abstention,
  source/time preservation, non-overlap, and deterministic focused tests.
- [x] T059DV Generate the full-session FR16AAD candidate twice, verify only its
  mechanical contracts and hashes, arrange every changed context, and read all
  changes manually before complete chronological and reverse-block review. No
  executable mechanism may judge correctness, aggregate accuracy, rank/select,
  or issue a product verdict.
  Both generated candidates are identical and contain no overlay because the
  retained input already carries every eligible accepted prototype decision.
  The path is recorded as a deterministic no-op; no product result is inferred.
  See `complete-phrase-cross-prototype-review-2026-07-16.md`.
- [x] T059DW Implement FR16AAE relative-top-1 complete-phrase expansion. Reuse
  only accepted FR16ZD identity decisions, require exact enclosing phrase
  containment, dual phrase-view top-rank and inherited margin vetoes, all-known
  conflicting baseline characters, protected-overlay abstention, immutable
  source/time, and focused deterministic tests.
- [ ] T059DX Generate FR16AAE twice on the retained full candidate, verify only
  mechanical contracts and hashes, arrange and manually read every changed
  context, then include it only in a complete chronological and reverse-block
  contextual semantic review. Executable result judgment and aggregation are
  prohibited.
- [x] T059DY Implement FR16AAF complete-VAD-phrase voiceprint challenges. Reuse
  frozen short-VAD and punctuation-phrase evidence, require four-view identity
  agreement under inherited gates, challenge one differing raw local mapping
  and one uniform known baseline identity, derive one-frame boundary tolerance
  from metadata, protect retained overlay reasons, and add focused deterministic
  source/time/abstention tests.
- [ ] T059DZ Generate FR16AAF twice on the retained full candidate, verify only
  hashes and immutable source/time contracts, arrange every changed context,
  and read every context manually before retention. A retained candidate must
  then enter complete chronological and reverse-block contextual semantic
  review. No executable mechanism may judge correctness, aggregate accuracy,
  rank/select a candidate or parameter, or issue the product verdict.
  Two guarded generations are byte-identical and the sole retained change was
  manually accepted; complete two-pass review remains pending. See
  `complete-vad-phrase-challenge-review-2026-07-16.md`.
- [x] T059EA Implement FR16AAG contextual VAD phrase challenges. Require one
  containing VAD interval and one covering primary raw run, four-view identical
  top rank under inherited margins, at least one outer VAD view at the inherited
  regular-score floor, one conflicting raw mapping, one uniform known baseline
  identity, one-frame-derived boundary tolerance, protected-overlay abstention,
  and focused deterministic source/time tests.
- [ ] T059EB Generate FR16AAG twice on the retained FR16AAF candidate, verify
  only hashes and immutable contracts, arrange every changed context, and read
  all changes manually before full two-pass review. No executable mechanism may
  judge correctness, aggregate accuracy, rank/select a candidate or parameter,
  or issue the product verdict.
  Two generations are byte-identical and all four displayed changed contexts
  were manually accepted without a changed-context regression; complete two-
  pass review remains pending. See
  `contextual-vad-phrase-challenge-review-2026-07-16.md`.
- [x] T059EC Implement FR16AAH edge-anchor-trimmed phrase challenges. Require
  one contiguous competing direct-anchor prefix or suffix, one derived native
  frame of forced-alignment separation, dual complete-phrase registry decisions
  under unchanged gates, all-known conflicting remainder labels, protected-
  overlay abstention, exact source/time projection, TOML ownership, and focused
  deterministic mechanical tests.
- [ ] T059ED Generate FR16AAH twice on the retained FR16AAG candidate, verify
  only hashes and immutable contracts, arrange every changed context, and read
  all changes manually before retention and full two-pass review. No executable
  mechanism may judge correctness, aggregate accuracy, rank/select a candidate
  or parameter, or issue the product verdict.
  Two generations are byte-identical and the sole changed context was manually
  accepted; complete two-pass review remains pending. See
  `edge-anchor-trimmed-phrase-review-2026-07-16.md`.
- [x] T059EE Implement FR16AAI adjacent-subminimum-clause envelopes. Enumerate
  exactly two adjacent complete clauses whose individual aligned durations are
  below the inherited phrase minimum and whose combined span satisfies the
  inherited phrase bounds; require dual registry decisions under unchanged
  gates, a uniform known conflicting baseline, protected-overlay and overlap
  abstention, exact source/time projection, TOML ownership, and focused tests.
- [x] T059EF Export all FR16AAI spans, run both frozen TitaNet evidence views,
  generate the candidate twice on retained FR16AAH, verify only model/numerical/
  source/time/hash contracts, and manually read every changed context before
  retention and full two-pass review. Executable result judgment, aggregation,
  ranking, and parameter or candidate selection are prohibited.
  Both generations are byte-identical. Manual reading rejects all six changed
  contexts because the two-clause envelope crosses real handoffs; the two
  motivating residuals also abstain under the frozen dual-view gates. FR16AAI
  is not retained and does not enter full review. See
  `adjacent-subminimum-clause-review-2026-07-16.md`.
- [x] T059EG Implement FR16AAJ two-phrase primary-run anchor expansion. Require
  exactly two source-adjacent complete phrases in one primary run, one mapped-
  identity-containing anchor confirmed by both frozen phrase views, one all-
  known combined range with exactly one competing identity, no dual eligible non-mapped
  voiceprint result, one-frame-derived tolerance, protected-overlay and overlap
  abstention, exact two-phrase closure, TOML ownership, and focused mechanical
  tests.
- [ ] T059EH Generate FR16AAJ twice on retained FR16AAH, verify only source/
  time/model/config/hash contracts, arrange every changed context, and read all
  changes manually before retention and full two-pass review. No executable
  mechanism may judge correctness, aggregate accuracy, rank/select a candidate
  or parameter, or issue the product verdict.
  Two generations are byte-identical and all five displayed changed contexts
  were manually accepted without regression; the complete two-pass review
  remains pending. See `primary-run-phrase-anchor-review-2026-07-16.md`.
- [x] T059EI Implement FR16AAK phrase-led outer-abstention challenges. Require
  one containing VAD and one covering primary run, a uniform mapped baseline
  with TOML-listed unconfirmed provenance, dual outer top-rank agreement on the
  mapped identity with both views abstaining under unchanged gates, dual exact-
  phrase top-rank agreement on one challenge identity with exactly one eligible
  view and one margin-only abstention, one-frame tolerance, protected-overlay
  and overlap abstention, exact source projection, and focused tests.
- [x] T059EJ Generate FR16AAK twice on retained FR16AAJ, verify only model,
  source/time/config/hash and determinism contracts, arrange every changed
  context, and manually read every change before retention. No executable
  mechanism may judge correctness, aggregate accuracy, rank or select a
  candidate or parameter, or issue the verdict. Complete forward and reverse-
  block review remains mandatory after policy freeze.
  Both generations are byte-identical. Manual contextual reading retains both
  exact-phrase changes without a changed-context regression; the final full
  two-pass review remains a separate pending gate. See
  `phrase-led-outer-abstention-review-2026-07-16.md`.
- [x] T059EK Implement FR16AAL secondary-channel single-unit edge closures.
  Require exactly one direct-anchor target prefix and one competing suffix,
  exactly one positive-duration suffix alignment unit, one containing VAD,
  dual exact-phrase margin-rank agreement, inherited sustained target-channel
  activity, simultaneous target activity under a mapped competing top-1 tail
  frame, exact source projection, provenance/protected/overlap abstention, TOML
  ownership, and focused mechanical tests.
- [x] T059EL Generate FR16AAL twice on retained FR16AAK, verify only model,
  numerical/source/time/config/hash and determinism contracts, arrange every
  changed context, and manually read every change before retention. No
  executable mechanism may judge correctness, aggregate accuracy, rank or
  select a candidate or parameter, or issue the verdict.
  Both generations are byte-identical. The sole changed context was read
  manually and retained without expanding across the following real handoff.
  See `secondary-channel-edge-closure-review-2026-07-16.md`.
- [ ] T059EM Implement FR16AAM in the production C++ business projector. Reuse
  the TOML `speaker_fusion.min_embed_sec` and existing duration-class voiceprint
  gates; require complete same-identity primary coverage, same-identity activity
  support, and session/robust phrase agreement before overriding a conflicting
  coarse direct interval. Add focused agreement and abstention tests.
- [ ] T059EN Export the same real-WebSocket timeline's typed tracks with source
  hashes, replay FR16AAM twice, verify only deterministic/source/time/track
  contracts, and manually read every changed context. No code, script, formula,
  query, notebook, metric, or algorithm may judge correctness, aggregate
  accuracy, rank/select a policy or parameter, or issue the verdict. Promote
  only through a new real-WebSocket run and complete chronological plus reverse-
  block contextual semantic review.
- [ ] T059EO Implement FR16AAN overlap-contained primary refinement in the
  production C++ projector. Add primary boundaries only inside simultaneous
  multi-identity activity, require full unambiguous primary coverage by an
  already-active identity, keep support activity-only, expose a distinct audit
  reason, and add focused refinement plus unique-winner abstention tests.
- [ ] T059EP Replay the frozen 600-second typed tracks twice, verify only
  deterministic/source/time/track contracts, manually read every changed
  context, and then manually reread the complete block in chronological and
  reverse order. No executable mechanism may assign correctness, aggregate
  accuracy, rank/select a candidate or parameter, or issue the verdict.
- [x] T059EQ Implement FR16AAO in the runtime speaker-evidence producer. Split
  only leading/trailing business ranges that partially intersect a complete
  punctuation phrase, retain complete interior phrase context, derive every
  query from forced-alignment character times, and add focused edge-isolation,
  wholly-contained, and source-reconstruction tests.
- [x] T059ER Re-run frozen replay and a clean 600-second real-WebSocket stream,
  verify mechanical source/time/hash/telemetry contracts, read every changed
  context manually, and repeat complete chronological plus reverse-block
  semantic review. No executable mechanism may judge correctness, aggregate
  accuracy, rank/select a candidate or parameter, or issue the verdict.
- [x] T059ES Implement FR16AAP in the production business projector. Normalize
  only adjacent overlapping source-ordered projection spans, require both sides
  to retain at least one sample, calculate entries from the normalized times,
  and fall back to the reconstructing diarization baseline when infeasible.
  Add a focused reversed-alignment-time identity-boundary regression test.
- [x] T059ET Rebuild and rerun the clean 600-second real-WebSocket acceptance.
  Require byte-exact ASR/business reconstruction, latest-revision/terminal
  equality, common-time-base, source/config/binary stability, and telemetry
  contracts before any manual semantic judgment. Then repeat complete
  chronological and reverse-block contextual review; no executable mechanism
  may judge correctness, aggregate accuracy, rank/select, or issue a verdict.
- [x] T059EU Implement FR16AAQ in the production C++ business projector. Derive
  one immutable initial identity per local Sortformer slot; require complete,
  uncontested activity coverage, a uniform conflicting current phrase, dual
  phrase-gallery top-rank agreement under TOML duration/margin gates, exact
  forced-alignment projection, and focused agreement plus abstention tests.
- [x] T059EV Replay the frozen Run A typed tracks twice with FR16AAQ, verify only
  deterministic/source/time/config/track contracts, arrange every changed
  context, and read all changes manually before any retention decision. No
  code, script, formula, query, notebook, metric, or algorithm may assign
  correctness, aggregate accuracy, rank/select a candidate or parameter, or
  issue the product verdict.
- [x] T059EW Implement FR16AAR across live revisions and terminal timeline JSON,
  add focused one-sample serialization and source-order replay tests, and verify
  that the transport precision change leaves typed in-memory evidence unchanged.
- [x] T059EX Implement FR16AAS in the production C++ projector using the typed
  VAD and phrase voiceprint evidence. Require four-view top-rank identity,
  exactly three unchanged-gate decisions, one score-passing margin-only
  abstention, unique VAD/primary containment, the TOML minimum aligned-unit
  count, uniform conflicting current identity, exact phrase projection, and
  focused agreement/abstention tests.
- [x] T059EY Replay FR16AAS twice on the retained FR16AAQ frozen candidate,
  verify only hashes and immutable source/time/config/track contracts, arrange
  every changed context, and read all changes manually before retention or full
  review. No executable mechanism may judge correctness, aggregate accuracy,
  rank/select a candidate or parameter, or issue the verdict.
- [x] T059EZ Implement FR16AAT in the authoritative raw WebSocket acceptance
  client. Serialize writes per connection, respond to server PING with a masked
  same-payload PONG, exclude control frames from captured application events,
  and add focused socket-level coverage. Do not change the server validity
  policy, event contents, producer ownership, or typed tracks.
- [x] T059FA Repeat the real 360-second producer plus early/late observer gate.
  Require all connections to remain open, producer/observer ordered live events
  and terminal timeline to match, and existing time/hash/telemetry contracts to
  pass before resuming speaker contextual review. This task performs no product
  result evaluation.
- [x] T059FB Diagnose the independent 360-second semantic failure from typed
  production evidence. Record that all worker extents reached the common clock
  while the voiceprint track remained empty because three observed stable
  identities did not satisfy the configured four-identity gallery gate. Define
  FR16AAU before changing the profile; do not alter score, margin, duration,
  phrase, primary, alignment, or robust-gallery rules.
- [x] T059FC Set only TOML `speaker_fusion.minimum_gallery_size` to three, then
  repeat independently terminated 120-second, 360-second, and 600-second real
  WebSocket promotion runs with producer plus observers. Verify mechanical
  contracts separately, then manually read every chronological context and all
  reverse blocks. Stop on the first semantic failure. No executable mechanism
  may judge correctness, count or aggregate results, rank the profile, or issue
  a verdict.
- [x] T059FD Implement FR16AAV in the production business projector. Require a
  short business-interval or phrase to cross at least two typed VAD intervals
  without one containing VAD, then require a uniform current source identity,
  complete uncontested activity and primary coverage, and the existing TOML
  duration boundaries before ordinary voiceprint evidence must abstain.
  Preserve the earlier FR16AAQ and FR16AAS specialized challenges, raw tracks,
  source text, time base, and all existing score/margin gates. Add focused
  fragmented-VAD, containing-VAD, and competing-native tests.
- [ ] T059FE Export the frozen full Run B typed tracks, replay FR16AAV twice,
  verify only deterministic/source/time/config/track contracts, and arrange
  every changed context. Read every change manually with complete surrounding
  conversation before retaining the candidate, then repeat the complete
  chronological and reverse-block contextual review. No code, script,
  formula, query, notebook, metric, or algorithm may assign correctness,
  aggregate an accuracy result, rank/select a candidate or parameter, or issue
  the product verdict.
- [x] T059FF Implement FR16AAW for generic aligned-unit voiceprint writes. Keep
  a conflicting exact-range primary tie-break or overlap-refinement label,
  while preserving aligned evidence that agrees, sole-activity behavior, all
  explicit multi-view challenges, raw tracks, source text, time base, and TOML
  gates. Add focused conflicting-primary and agreeing-primary tests.
- [x] T059FG Replay the combined FR16AAV/FR16AAW production projector twice on
  the frozen Run B tracks, verify only deterministic/source/time/config/track
  contracts, arrange all changed contexts, and read each change manually in
  full conversation before retaining or rejecting FR16AAW. No code, script,
  formula, query, notebook, metric, or algorithm may assign correctness,
  aggregate accuracy, rank/select, or issue the product verdict.
- [x] T059FH Implement FR16AAX in the production business projector. Record only
  same-stable-identity activity and primary starts within the existing TOML
  alignment-boundary tolerance, after each view has an existing-TOML-duration
  native gap and while each new segment remains uncontested for the existing
  TOML minimum embedding duration. Require a different nearest prior identity,
  then allow a continuous forced-alignment run to break after a complete
  aligned unit that straddles that corroborated native transition. Preserve
  activity-owned business boundaries, midpoint assignment of the crossing
  unit, byte-exact source reconstruction, unmatched/short/contested/conflicting
  transition abstention, and evidence-arrival convergence. Add focused tests.
- [x] T059FI Replay the combined FR16AAV/FR16AAW/FR16AAX projector twice on the
  frozen Run B tracks, verify only determinism and source/time/config/immutable-
  track contracts, arrange every changed context, and manually read each one in
  complete surrounding conversation before retaining or rejecting FR16AAX. No
  code, script, formula, query, notebook, metric, or algorithm may judge
  correctness, aggregate accuracy, rank/select, or issue the product verdict.
- [x] T059FJ Implement FR16AAY for a short, exact punctuation phrase whose
  primary-arbitrated current local slot has a different immutable initial
  stable identity. Require existing TOML duration/aligned-unit gates, one
  containing robust VAD whose two galleries pass for the initial identity,
  phrase robust margin-only support for the initial identity, and phrase
  session margin-only support for the current identity. Preserve every other
  topology and add focused positive plus abstention tests without new values.
- [x] T059FK Replay the combined retained projector with FR16AAY twice on frozen
  Run B, verify only determinism and source/time/config/immutable-track
  contracts, arrange every changed context, and manually read each in complete
  conversation before retention or rejection. No executable mechanism may
  judge correctness, aggregate accuracy, rank/select, or issue the verdict.
- [x] T059FL Implement FR16AAZ for one short business interval immediately after
  a dual-gallery-passing punctuation phrase. Require exact source/time
  adjacency, the anchor identity as unique runner-up in both margin-only target
  galleries, continuous activity coverage through the existing TOML minimum
  duration, one covering current primary run, and the existing aligned-unit
  count. Write only the target with a direct-anchor reason and add positive plus
  weak/non-runner-up/short/missing-primary/alignment abstention tests.
- [x] T059FM Replay the combined retained projector with FR16AAZ twice on frozen
  Run B, verify only determinism and source/time/config/immutable-track
  contracts, arrange every changed context, and manually read each in complete
  conversation before retention or rejection. No executable mechanism may
  judge correctness, aggregate accuracy, rank/select, or issue the verdict.
- [x] T059FN Implement FR16ABA for an exact short punctuation phrase whose two
  galleries top-rank one initial local-slot identity but abstain only on margin
  against a uniform coarse `voiceprint_direct_*` current attribution. Require
  one uncontested covering current local slot, one current primary run, and the
  existing TOML duration/alignment gates. Add positive and all specified
  abstention tests without changing any threshold.
- [x] T059FO Replay the combined retained projector with FR16ABA twice on frozen
  Run B, verify only determinism and source/time/config/immutable-track
  contracts, arrange every changed context, and manually read each in complete
  conversation before retention or rejection. No executable mechanism may
  judge correctness, aggregate accuracy, rank/select, or issue the verdict.
- [x] T059FP Implement FR16ABB as a postpass for one positive but subminimum
  no-embedding aligned unit. Require the existing TOML pause to the nearest
  aligned unit on both sides, uniform non-voiceprint current labels, one
  uncontested covering current local slot with a different initial identity,
  one uncontested covering current primary run, and one containing robust VAD
  whose two galleries pass existing gates for that initial identity. Write only
  the exact unit and add the specified positive and abstention tests.
- [x] T059FQ Replay the combined retained projector with FR16ABB twice on frozen
  Run B, verify only determinism and source/time/config/immutable-track
  contracts, arrange every changed context, and read each manually in complete
  surrounding conversation before retention or rejection. No executable
  mechanism may judge correctness, aggregate accuracy, rank/select, or issue
  the verdict.
- [x] T059FR Implement FR16ABC for one subminimum no-embedding aligned unit
  completely contained by a different primary micro-run that is immediately
  and gaplessly bracketed by the same uniform current identity. Require typed direct current
  labels, sole uncontested current activity, no candidate activity, and the
  nearest aligned neighbors outside the micro-run. Write only the exact unit
  and add the specified positive and abstention tests without new thresholds.
- [x] T059FS Replay the combined retained projector with FR16ABC twice on frozen
  Run B, verify only determinism and source/time/config/immutable-track
  contracts, arrange every changed context, and read each manually in complete
  surrounding conversation before retention or rejection. No executable
  mechanism may judge correctness, aggregate accuracy, rank/select, or issue
  the verdict.
- [x] T059FT Implement FR16ABD for the maximal contiguous forced-aligned source
  range wholly contained by one robust-complete VAD interval. Require existing-
  gate dual-gallery agreement, typed VAD isolation on both sides, one text ID,
  contiguous alignment, uniform direct current labels, one uncontested current
  activity slot and primary run, and no candidate activity. Write only the
  aligned source range and add all specified positive and abstention tests.
- [x] T059FU Replay the combined retained projector with FR16ABD twice on frozen
  Run B, verify only determinism and source/time/config/immutable-track
  contracts, arrange every changed context, and read each manually in complete
  surrounding conversation before retention or rejection. No executable
  mechanism may judge correctness, aggregate accuracy, rank/select, or issue
  the verdict.
- [x] T059FV Implement FR16ABE for one short primary run at a separately bounded
  VAD onset. Require a configured pause after the preceding current primary,
  gapless same-current recovery, one candidate activity slot, no third activity
  identity, a mixed VAD continuing through the existing minimum duration of the
  restored current identity, and a one-text aligned run with exactly one
  internal source discontinuity wholly inside the primary run. Preserve the
  prefix, write only the aligned suffix, and add all specified positive and
  abstention tests.
- [x] T059FW Replay the combined retained projector with FR16ABE twice on frozen
  Run B, verify only determinism and source/time/config/immutable-track
  contracts, arrange every changed context, and read each manually in complete
  surrounding conversation before retention or rejection. No executable
  mechanism may judge correctness, aggregate accuracy, rank/select, or issue
  the verdict.
- [x] T059FX Implement FR16ABF for one no-embedding subminimum business interval
  isolated on both sides by the existing TOML pause in typed VAD and positive-
  duration alignment views. Require one uncontested current local slot in both
  activity and primary, a different immutable initial identity, native current
  labels, no candidate activity, and the existing aligned-unit count. Write
  only the exact business-interval source range and add all abstention tests.
- [x] T059FY Replay the combined retained projector with FR16ABF twice on frozen
  Run B, verify only determinism and source/time/config/immutable-track
  contracts, arrange every changed context, and read each manually in complete
  surrounding conversation before retention or rejection. No executable
  mechanism may judge correctness, aggregate accuracy, rank/select, or issue
  the verdict.
- [x] T059FZ Implement FR16ABG for one exact short punctuation phrase whose
  phrase-session, phrase-robust, VAD-session, and VAD-robust rankings all
  abstain on the existing margin while exposing one consistent top-two pair.
  Require the initial local-slot identity to rank first once and second three
  times, exclude the current identity from every pair, require uncontested
  current activity/primary coverage and the existing aligned-unit count, and
  write only the exact phrase.
- [x] T059GA Replay the combined retained projector with FR16ABG twice on frozen
  Run B, verify only determinism and source/time/config/immutable-track
  contracts, arrange every changed context, and read each manually in complete
  surrounding conversation before retention or rejection. No executable
  mechanism may judge correctness, aggregate accuracy, rank/select, or issue
  the verdict.
  The focused C++ policy test passes. Both frozen replays produced SHA-256
  `d60af2f079dcadf771418913021dac093e003f5a89b8b958aa18af3ac81a0aa0`.
  Mechanical comparison arranged one changed reference context only. Complete
  manual reading of `ref-0500` retained the change: Tang Yunfeng's statement
  that an adult child is possible follows Shi Yi's conclusion that spouses are
  financially indistinguishable, and Shi Yi then responds by asking about
  waiting for Lili to become an adult. The candidate restores only that exact
  phrase to Tang Yunfeng; no other reference context changed.
- [x] T059GB Implement FR16ABH for one short punctuation phrase whose two
  phrase views top-rank one candidate under score-pass/margin-abstention while
  the two containing-VAD views top-rank the uniform direct current identity
  under score-and-margin abstention. Require the same two identities as every
  top-two pair, uncontested current activity and primary coverage, no candidate
  activity, one robust-complete containing VAD, the existing aligned-unit
  count, and exact phrase-only projection. Add positive and all specified
  abstention tests without changing a TOML threshold.
- [x] T059GC Replay the combined retained projector with FR16ABH twice on
  frozen Run B, verify only deterministic/source/time/config/immutable-track
  contracts, arrange every changed context, and read each manually in complete
  surrounding conversation before retention or rejection. No executable
  mechanism may judge correctness, aggregate accuracy, rank/select, or issue
  the verdict.
  The focused C++ policy test passes. Both frozen replays produced SHA-256
  `8e0fa49382a252bbe291894447f2b9b0d44073ed00e9389b20f8b727ec492ace`.
  Mechanical comparison arranged one changed coarse reference context. Full
  manual reading retained the exact phrase: Zhu Jie asks whether to split,
  Tang Yunfeng answers `不一定啊`, and Shi Yi then says `这不是老板吗`. The
  source timestamp places Tang's audible reply just inside the next coarse
  reference interval, but the semantic turn order is unambiguous and the rule
  leaves Shi Yi's following phrase untouched.
- [x] T059GD Implement FR16ABI for one exact short business interval whose two
  galleries make the same eligible candidate decision while a uniform primary
  attribution conflicts. Require the current and one competing activity slot
  to cover the interval, exclude candidate activity, require one covering
  primary run, and require both robust-complete containing-VAD views to abstain
  while exposing the candidate and activity competitor as the same top-two pair
  in opposite order. Add positive and all specified abstention tests without
  changing a TOML threshold.
- [x] T059GE Replay the combined retained projector with FR16ABI twice on
  frozen Run B, verify only deterministic/source/time/config/immutable-track
  contracts, arrange every changed context, and read each manually in complete
  surrounding conversation before retention or rejection. No executable
  mechanism may judge correctness, aggregate accuracy, rank/select, or issue
  the verdict.
  The focused C++ policy test passes. Both frozen replays produced SHA-256
  `cc757ccdb0ff1a17b6b99a4b0b7ff0d3efa7d81acdd0cffd0fd1ef33f4d050a9`.
  Mechanical comparison arranged one changed reference context only. Complete
  manual reading retained the exact interval: Zhu Jie finishes that the product
  is now international, Tang Yunfeng answers `是啊`, and Shi Yi then begins the
  COB internationalization point. The candidate restores only Tang Yunfeng's
  exact short reply and leaves both surrounding turns unchanged.
- [x] T059GF Implement FR16ABJ as an exact subminimum business-interval
  postpass. Require uniform phrase-session provenance, exactly two covering
  activity slots, one matching the sole covering primary and one different
  competitor, no current-candidate activity, one containing
  phrase whose session is eligible and robust view margin-abstains for the
  current identity, one containing VAD whose two margin-abstaining views rank
  the native identity first and current identity second, and exactly one
  contained positive-duration aligned unit. Add all specified abstention tests
  without changing a TOML threshold.
- [x] T059GG Replay the combined retained projector with FR16ABJ twice on frozen
  Run B, verify only deterministic/source/time/config/immutable-track contracts,
  arrange every changed context, and read each manually in complete surrounding
  conversation before retention or rejection. No executable mechanism may
  judge correctness, aggregate accuracy, rank/select, or issue the verdict.
  The focused C++ policy test passes, including a cross-text source guard added
  after the first full replay exposed the missing bound check. Both corrected
  frozen replays produced SHA-256
  `851cc707471d79e8e9187b832f2174a19ad08186cd98285c901b6108efc3b03a`.
  Mechanical comparison displayed two coarse reference contexts around one
  exact source split. Complete manual reading retained the change: Shi Yi asks
  whether the three rounds include the current one, Tang Yunfeng answers
  `不含`, and only that exact response is restored to Tang. The following `哦`
  remains unchanged, so the already accepted next contribution does not
  regress.
- [x] T059GH Implement FR16ABK aligned complete-source VAD closure in the typed
  C++ projector. Reuse only existing TOML thresholds; require an exact typed
  business-interval source partition with per-interval alignment anchors,
  dual-gallery short eligibility plus regular-score-only
  abstention, one agreeing containing VAD, one margin-abstaining phrase, bounded
  current-label edge topology, and exact two-identity activity/primary closure.
  Add positive and specified abstention tests. Keep per-text postpasses bounded
  to that text's typed source evidence.
- [x] T059GI Replay the combined retained projector with FR16ABK twice on frozen
  Run B, verify only deterministic/source/time/config/immutable-track contracts,
  arrange every changed context, and read each manually in complete surrounding
  conversation before retention or rejection. No executable mechanism may
  judge correctness, aggregate accuracy, rank/select, or issue the verdict.
  The focused C++ policy test passes with partition, alignment, gate, topology,
  VAD, phrase, activity, and primary abstention cases. Both frozen replays
  produced SHA-256
  `ca6c506e4d2a3101d85720776bab51cf8d4f73559c68c7f90cf2e44e4eab4fca`.
  Mechanical comparison arranged one changed reference context only. Complete
  manual reading retained the atomic source: Xu Zijing comments that Tang has
  too little after Shi Yi asks for opinions, and Tang enters only on the next
  contribution. The rule leaves that following turn unchanged.
- [x] T059GJ Implement FR16ABL adjacent-business-pair evidence and the exact
  source-initial subminimum-prefix challenge. Enumerate every qualifying pair
  from typed source/common-clock bounds, reuse only existing TOML gates, require
  dual-gallery, primary, activity, alignment, and following-handoff closure,
  and add positive plus all specified abstention tests.
- [x] T059GK Replay the combined retained projector with FR16ABL twice on
  frozen Run B. Verify only raw-evidence, deterministic, source/time/config,
  and immutable-track contracts; arrange every changed context and read each
  manually in complete surrounding conversation before retention or rejection.
  No executable mechanism may judge correctness, aggregate accuracy,
  rank/select a candidate, or issue the verdict.
  The evidence stage enumerated 67 typed queries. The focused C++ tests pass,
  including source/time adjacency, duration, dual-gallery, component,
  provenance, primary, activity-confidence, alignment, and following-handoff
  abstention cases. The augmented frozen voiceprint input has SHA-256
  `252642472ffd2cf1b8cc10308f56914bc698bd31413d4886b597cd5a4082d357`;
  both C++ replays produced SHA-256
  `281a74b57803ab32af6a36a0c75ed9dbf19f23c02b15904babe6858950998b1b`.
  Mechanical comparison displayed one changed entry only. Complete forward and
  reverse context reading retained the change: Shi Yi's continuous finance-role
  question now has one identity, while Xu Zijing's following reply remains
  unchanged. The manually maintained frozen development ledger is now 518
  accepted and 38 incorrect contributions out of 556, approximately 93.2
  percent. See `adjacent-primary-prefix-review-2026-07-16.md`; real runtime
  acceptance remains pending.
- [x] T059GL Complete the frozen FR16ABL full-session contextual semantic gate.
  Read all 556 contributions chronologically and then reread the complete
  `0-3615.120`-second session in reverse fixed blocks. The two manual passes
  agree on 518 accepted and 38 incorrect contributions, approximately 93.2
  percent. No program assigned a correctness label, total, percentage, rank,
  candidate decision, or verdict. The repaired `ref-0459` remains correct;
  `ref-0396` is a natural Tang-to-Shi handoff and `ref-0548` is an ASR wording
  error on the correct Shi speaker interval. See
  `final-full-context-review-2026-07-16.md`.

## Phase 5: Model Escalation If Required

- [x] T060 Close the non-deployable model branch. The historical reference-free ERes2NetV2
  turn-voiceprint candidate reached a provisional text-context `400/556`
  (71.942 percent). Forced-alignment phrase boundaries plus independent phrase
  embeddings reached `409/556` (73.561 percent), with 9 repaired rows, no
  correct-to-incorrect row transitions, and 2 accurate-to-mostly-accurate
  downgrades. A pinned WeSpeaker ResNet293-LM audit found no additional rewrite
  that passed the strict reference-free conflict rule. These are written-context
  diagnostics, not constitutional audible scores, and cannot enter the 93
  percent model-selection gate without a native streaming deployment path.
- [x] T060A Remove the exploratory offline Sortformer/Pyannote tooling, tests,
  and report. They duplicate the role of the manually adjudicated `test.txt`
  and cannot produce a deployed runtime candidate. Retain only NeMo fixtures
  for same-checkpoint v2.1 numerical parity.
- [ ] T061 Select a model only after complete contextual semantic review manually
  establishes that its frozen-evidence result exceeds the 93 percent gate
  without a fixed-block or critical-turn regression. No code-derived ranking or
  gate decision is permitted.
- [ ] T062 Write model-specific SDD artifacts and trusted-oracle tolerances before
  any C++/CUDA port.
- [ ] T063 Port and validate each selected model stage numerically before runtime
  integration.

## Phase 6: v2.1 Runtime Candidate

- [x] T070 Implement the frozen speaker-evidence and fusion contracts behind
  typed TOML fields.
- [x] T071 Add complete audit evidence to each business-speaker decision. The
  attribution-neutral increment records structured source/projection/reason,
  all selected and rejected diar candidates, overlap/coverage/confidence, and
  overlap/confidence margins; live, terminal, alias, and browser representations
  converge exactly. See `speaker-decision-audit-2026-07-15.md`.
- [x] T071A Reconstruct FR21 decisions for legacy frozen timelines using only
  immutable raw/business tracks, fail on structural or declared confidence-
  and time-envelope mismatch, preserve round-trip confidence in new artifacts,
  and emit a source-hashed full-session decision-evidence package without
  correctness labels. See `speaker-decision-audit-2026-07-15.md`.
- [x] T072 Validate that raw ASR, diarization, VAD, align, and voiceprint tracks
  remain unchanged by fusion revisions.
- [x] T073 Complete Web UI live/final convergence, microphone, reconnect,
  telemetry, and export validation in a real browser.
- [x] T074 Run all model oracle gates for the exact accepted v2.1 TOML profile.
- [x] T075 Pass 120 s, then 360 s, then 600 s promotion gates; stop on the first
  regression.

## Phase 7: Full v2.1 Acceptance

- [x] T080 Run full v2.1 acceptance A with an empty isolated speaker registry.
  The complete 3615.120-second real-WebSocket producer plus early/late observer
  current-source run passed all manifest-enforced structural contracts and
  froze the generated registry with SHA256
  `66461a77755984a08231d06306da7ce9e1eeac07be1927e91f8a772fc54c7b3f`.
- [x] T081 Complete the full 556-row context and semantic review for run A.
  Every row was reconciled chronologically and then reread in reverse fixed
  blocks. The manually established current result is 515 accepted and 41
  incorrect contributions, approximately 92.63 percent; see
  `final-full-context-review-2026-07-16.md`. No executable mechanism judged or
  aggregated the product result.
- [x] T082 Restart the process and run full v2.1 acceptance B with the frozen
  enrolled-registry fixture. The complete 3615.120-second current-source run
  passed all manifest-enforced structural contracts and preserved the frozen
  registry hash. The client did not record the `flush` and `end` terminal waits
  independently, so the direct 30-second latency gate remains open.
- [x] T083 Complete the full 556-row context and semantic review for run B.
  The forward and reverse reviews manually established 513 accepted and 43
  incorrect contributions, approximately 92.27 percent. No executable
  mechanism assigned correctness or produced the verdict.
- [ ] T084 Verify every Spec 013 accuracy and business-result gate manually from
  complete contextual semantics, and verify mechanical boundary, alignment,
  latency, telemetry, stability, and repeatability evidence separately for both
  runs. No code may aggregate these into a product verdict. The 2026-07-17
  audit confirmed this remained open. The 2026-07-18 direct-end A/B recapture
  now signs the terminal-latency gate mechanically and again passes the natural-
  turn gate under complete contextual semantic review. Speaker-time, fixed
  blocks, per-speaker recall, criticality, confidence, and source-time offsets
  remain unsigned.
- [ ] T085 Execute the supplemental locked holdout suite after the Constitution
  amendment if an industrial-readiness claim is requested.
- [ ] T086 Write the final closing report with manifests, hashes, complete
  signed ledger, limitations, and exact product claim after T084. The current
  report is an interim clean-commit evidence seal, not a final closing report.
- [x] T087 Synchronize specs, tasks, `PROJECT_STATE.md`, and README with the
  current evidence boundary.
- [ ] T088 Review the final report, close all priority-zero/priority-one defects,
  and create the release tag only after sign-off.

## Phase 8: Accepted Speaker-Policy Maintainability

- [x] T089 Freeze the accepted production projector before refactoring. Replay
  the retained full-session typed diar/ASR/align/primary/voiceprint tracks using
  the checked-in TOML and record SHA-256
  `04ba82a844a14edb08b3cce60a543e831dfd6bb1e1368d58440303f6f2251db9`.
  This is a mechanical behavior baseline and does not evaluate correctness.
- [x] T090 Extract the accepted voiceprint fusion implementation from
  `BusinessSpeakerPipeline` into one internal `SpeakerFusionPolicy` owner while
  preserving the public API, read-only state access, policy order, reason/source
  strings, TOML surface, and timeline ownership.
- [x] T091 Preserve or strengthen focused tests for every accepted fusion guard
  and abstention path without retaining an alternate runtime implementation.
- [x] T092 Replay the same frozen full-session typed inputs after extraction and
  require byte identity with T089. No program may convert this equivalence check
  into an accuracy or acceptance judgment.
- [x] T093 Pass a warning-clean build, all registered CTest entries, and a real-
  WebSocket smoke with source/config/binary/time/observer/telemetry contracts.
- [x] T094 Synchronize `PROJECT_STATE.md`, Spec 013, plan, tasks, and final
  maintainability evidence in the same commit.
- [x] T095 In a separate exact-equivalence change, consolidate duplicated policy
  guards and move rejected one-off candidate tools/configurations out of the
  active build surface. Do not remove production regression coverage or alter
  the accepted output.
- [x] T095A Freeze the current full retained typed replay at commit `52b3b22`.
  The 1,775-entry output has SHA-256
  `04ba82a844a14edb08b3cce60a543e831dfd6bb1e1368d58440303f6f2251db9`.
  This is a mechanical behavior baseline only.
- [x] T095B Consolidate the duplicated ranked-top-two and minimum-aligned-unit
  guards without changing challenge order, thresholds, reasons, sources, or
  abstention behavior.
- [x] T095C Move standalone candidate generators, their integration tests, and
  non-production speaker TOMLs to an explicitly inactive historical archive;
  remove all archived candidate registrations from CTest.
- [x] T095D Remove legacy runnable scripts that calculate speaker-attribution or
  speaker-identification accuracy, plus the compiled reference-projection
  harness. Preserve their history only through Git.
- [x] T095E Require byte identity with T095A, a warning-clean build, all active
  CTest entries, and a new-binary real-WebSocket observer/telemetry smoke.
- [x] T095F Synchronize Spec 013, `PROJECT_STATE.md`, archive documentation, and
  final maintainability evidence in the same commit.

## Phase 9: Current-Commit Acceptance Seal

- [x] T096 Build commit `6dbc600e4eb5` warning-clean and pass all 68 registered
  CTest entries. Freeze binary, TOML, audio, reference, and test-log hashes.
- [x] T097 Run the complete `3615.120`-second empty-registry A fixture through
  the production real-WebSocket path at 1.0x, preserve producer and observer
  evidence, and freeze the generated registry at SHA-256
  `66461a77755984a08231d06306da7ce9e1eeac07be1927e91f8a772fc54c7b3f`.
- [x] T098 Restart the server and run the complete frozen-registry B fixture.
  Preserve the registry hash and verify source, configuration, binary, common-
  clock, observer, telemetry, and stability contracts mechanically.
- [x] T099 Reconcile all 556 natural contributions for both current runs under
  complete forward and reverse conversational context. The reviewer manually
  establishes Run A at 514 accepted / 42 incorrect (approximately 92.45
  percent) and Run B at 513 accepted / 43 incorrect (approximately 92.27
  percent). See `current-commit-full-review-2026-07-17.md`.
- [x] T100 Audit every T084 conjunctive gate without converting mechanical
  evidence into a product verdict. Record that natural-turn attribution passes
  while the remaining unsigned gates keep T084 and Spec 013 open.
- [x] T101 Record `flush` and `end` terminal waits independently in the evidence
  manifest, add focused structural tests, and recapture the affected real-
  WebSocket latency evidence without changing runtime TOML policy. The first
  full-length direct-end A recapture at `7721024ceb60` recorded `66.915 s` and
  remains a mechanical failure artifact. After FR26, clean commit
  `588bfbe63555` completed full A/B direct-end runs at `25.597 s` and
  `26.305 s`, with no priming `flush`, stable source/config/binary identity,
  exact producer/observer terminal convergence, complete common-clock extents,
  and required telemetry coverage. Complete forward/reverse contextual review
  manually records 512/556 for Run A and 509/556 for Run B. See
  `direct-end-full-review-2026-07-18.md`. Timing and structural checks are
  mechanical evidence only; the speaker result comes only from that review.
- [x] T101A Add the FR26 typed speaker-evidence snapshot, common-sample-span
  embedding cache, session-owned periodic precompute worker, TOML cadence, and
  phase timing diagnostics. Route TitaNet through a scheduler-owned
  lowest-priority CUDA stream and warm maximum scratch before live audio.
  Gate live extraction on a mature active gallery and diar/ASR/VAD common-head
  catch-up, bound each cycle through typed TOML, and leave the final drain
  unlimited. Precomputation may extract acoustics only; final mature-gallery
  scoring and policy order remain authoritative.
- [x] T101B Replace repeated full voiceprint scans with stable kind indexes,
  prove exact cached/uncached output identity and lifecycle reset behavior,
  then pass warning-clean build, all CTest entries, short and 600-second real-
  WebSocket direct-end checks before spending another full A/B run. The clean
  build and `68/68` CTest pass; cached and uncached track payloads are exactly
  equal at both durations. The 120-second track hash is
  `7b291c32fdd5d40679f5ea1f944e3cc2f508061b1dbeebe1ee5bdf3e3a97634f`,
  and the 600-second track hash is
  `a6a7ea95299ea7568977b220715e5b1e6b3ad3c4317cf6c4a8b4d019124aa11b`.
  Exact equality is a mechanical invariant, not a product-accuracy verdict.
- [ ] T102 Reuse the completed 556-row forward/reverse contextual judgments
  against the human-audited `test.txt` reference and manually sign the remaining
  speaker-time, fixed-block, per-speaker, critical-turn, confident-wrong, and
  source-time-offset breakdowns for Run A and Run B independently. Do not demand
  a duplicate audio transcription or invent sub-second reference boundaries.
  The 2026-07-18 breakdown review signs fixed-block and per-speaker turn recall
  as passed, but signs critical-speaker and confident-wrong attribution as
  failed. Speaker-time, per-speaker time, and source-time-offset totals remain
  open; see `speaker-gate-breakdown-review-2026-07-18.md`.

## Phase 10: Critical Handoff Evidence

- [x] T103 Trace the first-block critical speaker residuals through the final
  business view, forced alignment, activity diarization, primary top-1, VAD,
  and voiceprint evidence without using a reference-driven executable
  comparison. `ref-0045`, `ref-0058`, and `ref-0071` retain useful local
  Sortformer evidence that is erased or displaced by phrase-scale projection;
  `ref-0049` remains evidence-poor; `ref-0090` exposes a separate forced-
  alignment placement problem and is not folded into the phrase fix.
- [x] T104 Implement FR16ABM in the production fusion policy and add focused
  C++ regression coverage for a dual-gallery phrase spanning a native
  two-identity handoff. Preserve homogeneous phrase behavior, source text,
  time-base ownership, policy order, and all checked-in TOML values. The final
  guard requires exactly two source-ordered base identity runs and existing
  minimum aligned duration; the complete build and `68/68` CTest pass.
- [x] T105 Replay the frozen direct-end A and B typed tracks twice with the
  unchanged TOML. Verify only deterministic and structural contracts by code,
  then manually read every changed context forward and in reverse before
  retaining or rejecting the candidate. Both replays are byte-stable and the
  complete changed-context review retains the bounded rule; see
  `native-handoff-guard-review-2026-07-18.md`.
- [x] T106 If T105 is retained by complete contextual semantic review, pass the
  warning-clean build, full CTest suite, 120-second and 600-second real-
  WebSocket promotion runs, then recapture full A/B and perform the complete
  556-contribution forward/reverse semantic review. Clean commit `1a475e6`
  passed the warning-clean build and `68/68` CTest, then passed 120-second,
  600-second, and full direct-end A/B mechanical contracts. Complete manual
  forward/reverse contextual review establishes Run A at 514 accepted / 42
  incorrect (approximately 92.45 percent) and Run B at 515 / 41
  (approximately 92.63 percent). No executable mechanism assigned or
  aggregated the result. See
  `native-handoff-full-promotion-review-2026-07-18.md`.
- [x] T107 Investigate the independent forced-alignment/VAD placement defect
  exposed around `ref-0090` using source-free common-clock evidence. Specify a
  separate bounded rule before implementation; do not compensate through the
  phrase-handoff guard or a reference-fitted parameter. The raw A/B topology is
  identical: the common clock is stable, forced alignment delays a subminimum
  response across a `1.84 s` internal hole, and containing TitaNet evidence is
  dominated by following speech. FR16ABN specifies a source-free A-B-A native
  island plus typed-VAD-gap recovery with no new parameter; see
  `alignment-lag-root-cause-2026-07-18.md`.
- [x] T108 Implement FR16ABN in the production fusion policy, pass the existing
  TOML punctuation set into the business projector without changing its value,
  and add focused C++ positive/abstention/source-order coverage. Build warning-
  clean and pass the full CTest suite before generating a candidate. The
  warning-clean build and all 68 CTest entries pass; the focused matrix covers
  the positive topology and ten bounded abstention/source-order conditions.
- [x] T109 Export the promoted full A/B typed tracks, replay FR16ABN twice per
  run, verify only deterministic structural contracts by code, and manually
  read every changed context forward and reverse. Reject or retain only by the
  complete contextual semantic review; do not start another real-WebSocket
  full run until that review establishes the frozen upper bound. Three replays
  per path are byte-stable and change only the same short response group.
  Complete chronological/reverse context review retains that repair and the
  following substantive turn on both paths; see
  `delayed-alignment-clause-review-2026-07-18.md`.
- [x] T110 Commit and push the retained FR16ABN candidate as a transitional
  experiment, then pass warning-clean build, full CTest, and 120-second and
  600-second direct-end real-WebSocket promotion with unchanged TOML values.
  Verify only mechanical contracts by code and manually review every changed
  conversational context before retention. Commit `6b1cb79fa4f5` is pushed on
  `master`; its build is warning-clean and `68/68` CTest pass. The 120-second
  and 600-second runs passed all mechanical contracts with direct waits of
  `0.803 s` and `4.607 s`. Complete forward/reverse contextual review found no
  changed sequence at 120 seconds and retained only the intended `ref-0090`
  repair at 600 seconds.
- [x] T111 If T110 is retained, recapture full empty-registry Run A and
  restarted frozen-registry Run B through the production WebSocket path. Read
  all 556 contributions chronologically and in reverse fixed windows for each
  run before manually deriving any result or advancing the baseline. Run A and
  the retained Run B retry completed all `3615.120` seconds at `0.993x`, with
  direct waits of `25.849 s` and `25.585 s`, exact common-clock extents,
  observer convergence, and accepted telemetry coverage. Complete forward and
  reverse review, followed by T102 reconciliation of the `ref-0160` source-label
  conflict and the `ref-0182` boundary-only judgment, originally established
  `519/556` for both runs. T135 supersedes that incomplete ledger with
  `514/556`. The first Run B
  artifact is preserved but excluded because runtime telemetry cadence was
  `94.965%`; the controlled retry passed at `95.214%` with unchanged behavioral
  values. See `delayed-alignment-full-promotion-review-2026-07-18.md`.
- [x] T112 Replace relative-delay GPU telemetry sampling with monotonic
  absolute-deadline scheduling so probe latency cannot accumulate into cadence
  loss. Preserve the TOML interval and telemetry payload, add focused timing
  coverage, and verify the mechanical cadence contract through the real
  WebSocket path. Transitional experimental commit `d610de36ed13` is pushed on
  `master`, builds warning-clean, and passes all `69/69` CTest entries. Its
  clean 120-second 1.0x real-WebSocket run records 119 runtime samples
  (`99.167%` cadence), 120 tegrastats samples, exact one-second runtime steps,
  and 100 percent required-field coverage. This task does not evaluate speaker
  correctness or reopen the frozen T111 contextual result; see
  `gpu-telemetry-deadline-review-2026-07-18.md`.

## Phase 11: Future-Epoch Corroboration

- [x] T113 Reconcile the T111 speaker ledger against complete conversational
  context and trace every remaining critical failure through final business,
  forced alignment, typed VAD, diarization, primary top-1, both TitaNet
  galleries, and identity epochs. Correct the boundary-only `ref-0182`
  judgment without scoring ASR wording, retain `ref-0249` as a critical mixed-
  identity failure, and reject a global 20-second identity backfill because it
  rewrites unrelated epoch boundaries. This task originally signed 519
  accepted and 37 incorrect contributions for each frozen run; T135's complete
  uniform reread supersedes it with 514 accepted and 42 incorrect; see
  `speaker-gate-breakdown-review-2026-07-18.md`.
- [x] T114 Implement FR16ABO with an explicit zero-disabled
  `[speaker_fusion].future_epoch_lookahead_sec` typed configuration field.
  Preserve raw identity epochs and every producer track. Add focused C++
  positive and abstention coverage, config/runtime serialization coverage, and
  pass a warning-clean build plus all registered CTest entries. The candidate
  passes all `69/69` entries; see
  `future-epoch-phrase-review-2026-07-18.md`.
- [x] T115 Replay the frozen T111 A/B typed tracks at least twice per path.
  Verify only deterministic and structural contracts by code, arrange every
  changed context, and read all changed conversational contexts manually in
  forward and reverse order against the human-listened `test.txt`. Retain or
  reject FR16ABO only from that complete semantic review; no script, metric,
  formula, query, test, or algorithm may assign correctness or select it. Both
  paths are byte-stable across two replays. Complete semantic review retains
  one real repair and one identity-neutral activation without a changed-context
  regression; see `future-epoch-phrase-review-2026-07-18.md`.
- [x] T116 If T115 retains FR16ABO, commit the transitional experiment and pass
  the warning-clean build, full CTest suite, and 120/600-second real-WebSocket
  ladder before deciding whether a new full A/B capture is justified. A full
  capture must receive complete 556-contribution forward/reverse contextual
  review before any gate or baseline advances. Clean commit `f49a8278e0d8`
  passed all engineering and mechanical gates and completed full A/B. Complete
  chronological and tail-to-start semantic review against the human-listened
  `test.txt` manually records `518/556` for each run, one below T111, with
  different A/B error sets. Promotion is rejected and the default TOML switch
  returns to zero; see
  `future-epoch-full-promotion-review-2026-07-18.md`.

## Phase 12: Cross-Pipeline Evidence Stability

- [x] T117 Freeze and export both T116 full typed-track packages, prove
  same-input business replay byte stability, and mechanically locate the first
  A/B divergence in each raw producer track and in final projection. Tools may
  display structural evidence only. Determine the business meaning of every
  implicated context solely by complete forward/reverse semantic comparison
  with `test.txt`; do not add another runtime rule or start another full audio
  run until the producer-versus-projection source is established. Both frozen
  packages replay byte-stably. Diarization and primary speaker are identical;
  ASR first differs at `text_id=49` around `658.7/658.8 s`, followed by
  alignment, voiceprint, and business projection. Forward/reverse review of
  `ref-0098`-`ref-0103` confirms a real `ref-0102` speaker divergence. See
  `vad-gated-asr-stability-review-2026-07-18.md`.
- [x] T118 Specify and implement FR28's typed VAD state snapshot. Publish the
  active padded onset and stable active/silence decision frontiers through the
  canonical `ComprehensiveTimeline`; preserve worker independence and all
  existing TOML endpoint values. `VadStateResult`, `GpuVad`, and
  `ComprehensiveTimeline` now carry the typed frontiers without a direct worker
  dependency.
- [x] T119 Replace scheduling-sensitive unconfirmed ASR feeding with a
  worker-owned pending buffer, restore TOML `asr.vad_lead_ms`, add TOML
  `asr.vad_gate_chunk_ms`, preserve trailing-group behavior, and finalize only
  after the terminal VAD snapshot is frozen. The pending buffer and fixed gate
  quanta are owned by `AsrWorker`; terminal VAD is joined and frozen before ASR
  drains.
- [x] T120 Add focused publication-order, chunk-boundary, lead-buffer,
  confirmed-silence, short-gap, endpoint, reset, and terminal-tail tests. Pass a
  warning-clean build, the VAD numerical oracle gate, and every registered
  CTest. Exact equality is mechanical determinism evidence only. The
  warning-clean build, VAD oracle, and all `69/69` CTest entries pass.
- [x] T121 Run two independent 120-second production real-WebSocket captures
  with unchanged behavioral TOML except the explicit gate chunk, isolated
  registries, direct `end`, telemetry, and complete manifests. Mechanically
  compare typed tracks and locate differences without assigning correctness.
  Read every changed conversational context forward and reverse against
  `test.txt` before retaining, revising, or rejecting FR28 and before any
  600-second or full-length promotion. All seven canonical product tracks are
  identical across the two runs. Complete `ref-0001`-`ref-0018` forward and
  reverse review finds no FR28 speaker regression and retains it for the next
  gate; see `vad-gated-asr-stability-review-2026-07-18.md`.
- [x] T122 Commit the retained FR28 implementation as a transitional
  experiment, then run one clean 600-second production real-WebSocket capture
  from that exact commit with direct `end`, telemetry, an isolated registry,
  and the checked-in behavioral TOML. Pass all mechanical contracts and review
  every in-scope `test.txt` contribution forward and reverse before permitting
  a full capture. Automation may arrange evidence only. Commit `1d511a9`
  passes the mechanical contracts, but complete forward/reverse review rejects
  promotion: `ref-0037` and `ref-0073` are new contextual speaker regressions.
- [x] T124 Correct FR28's retained-gap handling without restoring the
  scheduling race. Feed every source sample between speech regions separated
  by no more than TOML `asr.vad_trail_sec`; retain the confirmed trailing
  source-clock bound for a closing region; preserve the next TOML lead when
  the two context windows overlap; continue skipping all remaining confirmed
  silence. Add exact publication-order, short-gap, long-gap, terminal-tail,
  and silence-only tests before implementation is considered complete. The
  warning/error scan is empty and all `69/69` CTest entries pass; real-stream
  promotion remains T125/T126.
- [x] T125 Pass warning-clean build, VAD oracle, all CTest entries, three
  independent blank-audio real-WebSocket checks, and two independent
  120-second real-WebSocket captures from one clean corrected commit. Compare
  typed tracks mechanically and review every in-scope contribution forward and
  reverse against `test.txt`; automation may not assign correctness. Commit
  `7579bc25411c` passes the build and all `69/69` CTest entries. All three
  silence runs contain zero product records. The two 120-second runs are
  byte-identical across all seven tracks, and complete forward/reverse review
  finds no new natural-turn regression.
- [x] T126 Run a new clean 600-second real-WebSocket gate with direct `end`,
  telemetry, isolated registry, and checked-in behavioral TOML. Review every
  in-scope contribution chronologically and in reverse, explicitly confirming
  the `ref-0037` and `ref-0073` contexts before permitting a full capture. The
  mechanical run passes, and complete review of all 93 contributions confirms
  that `ref-0037` is substantively restored. Promotion is rejected because
  `ref-0073` remains split Tang/Shi/Tang instead of Shi Yi's complete response.
- [x] T127 Specify and implement FR29 without changing TOML or producer tracks.
  Add focused base-projection and fusion-policy positive/abstention tests, pass
  warning-clean build and all CTest entries, export the T126 typed evidence,
  and replay it at least twice. Automation may report only structural changes;
  read every changed context chronologically and in reverse against `test.txt`
  before retaining or rejecting the correction. Three frozen replays are
  byte-identical. Complete chronological/reverse review retains the restored
  `ref-0073` response and confirms that the following Tang Yunfeng contribution
  remains separate; all `69/69` CTest entries pass. See
  `cross-view-handoff-review-2026-07-18.md`.
- [x] T128 If T127 is retained, commit and push the transitional experiment,
  repeat two independent 120-second production WebSocket runs, and run one new
  clean 600-second direct-end gate. Verify mechanical contracts by automation
  and review every in-scope `test.txt` contribution chronologically and in
  reverse before any full capture. Commit `2ce4a12b7973` passes both 120-second
  runs with byte-identical seven-track entries and complete `ref-0001` through
  `ref-0018` review. Its 600-second run closes all tracks at `9,600,000`
  samples; complete forward/reverse review of all 93 contributions restores
  the substantive `ref-0073` Shi Yi response, keeps `ref-0074` with Tang
  Yunfeng, and finds no new contextual speaker regression. Code inspection
  corrects the stale assumption that business-interval voiceprint queries are
  upstream records: the final evidence stage intentionally derives them from
  the revised base business view. The resulting one-record partition is
  confined to the retained handoff and is covered by a focused unit contract.
  No behavioral TOML value changes.
- [x] T123 If T128 passes, run independent full-length empty-registry Run A and
  restarted frozen-registry Run B from the same clean commit. Freeze complete
  manifests and typed tracks, mechanically verify time-base, transport,
  telemetry, terminal-latency, and repeatability contracts, then manually read
  all 556 contributions chronologically and in reversed fixed blocks against
  `test.txt`. No code may label, aggregate, rank, promote, or reject either
  result. Do not alter the T111 baseline until both complete reviews pass all
  applicable gates. Clean commit `2ff9ce3655b2a12e90a5d0def25c0a30f171f2d9`
  completes both full paths at `0.995x` with direct-end waits of `16.540 s` and
  `17.499 s`, exact seven-track extents, observer convergence, complete
  telemetry, and identical normalized seven-track entries. Complete independent
  chronological and reverse-block semantic review originally records
  `506/556`; T135 adds the omitted `ref-0099` error and corrects each run to
  `505/556`. Two fixed blocks, two canonical speakers, critical attribution,
  confident-wrong attribution, and the development margin fail, so FR29 is not
  promoted and T111 remains the best frozen comparison, not a closing result.
  The final documentation
  synchronization builds warning-clean and passes all `69/69` CTest entries;
  these are mechanical checks only. See
  `cross-view-handoff-full-promotion-review-2026-07-18.md`.
- [x] T129 Diagnose the T123 regression from frozen T111/T123 typed tracks
  without rerunning audio. Prove only mechanical causality: identical
  Sortformer/primary inputs, zero current-projector speaker-sequence changes on
  T111 inputs, T123 replay reproduction, and stable VAD gaps at manually
  reviewed lost-utterance contexts. Probe TOML thresholds `0.4` and `0.3` one
  at a time and retain no product verdict. Threshold `0.3` exposes the two
  reviewed low-energy regions and emits zero VAD segments on the frozen
  30-second silence fixture.
- [x] T130 Implement FR30 by changing only checked-in TOML `vad.threshold` from
  `0.5` to `0.3`. Keep every model, code path, and other behavioral value
  unchanged. Pass the VAD oracle, warning-clean build, all CTest entries, and
  the frozen silence probe, then commit and push the candidate explicitly as a
  transitional experiment. The full-audio production VAD export is byte-
  identical to the temporary threshold-0.3 probe, the frozen-silence export is
  empty, `test_vad` passes, the clean warning/error scan is empty, and all
  `69/69` CTest entries pass. See
  `vad-sensitivity-diagnosis-2026-07-19.md`.
- [x] T131 From the clean T130 commit, run three independent real-WebSocket
  silence sessions and two independent 120-second production captures with
  isolated registries, direct `end`, telemetry, and complete manifests. Verify
  structural and repeatability contracts mechanically, then read every
  in-scope `test.txt` contribution chronologically and in reverse. Automation
  may not assign correctness or acceptance. Commit `5046bccf7ea2` produces
  zero product records in all three silence sessions and identical normalized
  seven-track entries in the two 120-second sessions. Complete independent
  forward and reverse review of all 18 contributions finds no new natural-turn
  regression from T125. See
  `vad-sensitivity-120-context-review-2026-07-19.md`.
- [x] T132 Only if T131 is manually retained, run one clean 600-second
  production WebSocket capture and perform complete chronological and reverse
  contextual review of all 93 contributions. Do not authorize a full A/B run
  or alter T111 unless this gate passes. Clean commit `30162d1c844d`
  completes the real stream in `603.342 s`, closes every pipeline at
  `9,600,000` samples without a gap, converges all observer terminal hashes,
  and has complete required telemetry. Complete forward and reverse reading of
  all 93 contributions retains `ref-0037` and `ref-0073`, and complete reading
  of all ten T128 sequence changes finds no new natural-turn regression. See
  `vad-sensitivity-600-context-review-2026-07-19.md`.
- [x] T133 Commit the retained T132 evidence, then use that exact clean
  revision for one full-length empty-registry production WebSocket capture and
  one independently restarted full-length capture using Run A's frozen
  registry. Both runs use checked-in behavioral TOML, 100 ms frames, 1.0x
  pacing, direct `end`, observers, telemetry, isolated protocol storage, and
  complete manifests. Automation may verify only mechanical contracts and
  repeatability. Clean commit `a96e278ea340` completes valid Run A and Run B
  at `0.996x`; both pass direct-end, common-clock, provenance, observer,
  telemetry, and exact seven-track repeatability contracts. The first Run B
  attempt is excluded because its copied registry was not writable.
- [x] T134 Read all 556 `test.txt` contributions for each full artifact in
  chronological order and again in reverse 600-second blocks. Manually derive
  every required speaker-business gate, compare the two complete judgments to
  T111 and the rejected T123 result, and then retain or reject FR30. No code,
  script, query, formula, metric, or model score may assign any result or
  promotion verdict. Complete independent forward and reverse review originally
  records `498/556`; T135 adds the omitted `ref-0099` error and corrects both
  runs to `497/556`. Two fixed blocks, three canonical speakers,
  the full 90-percent floor, critical attribution, and confident-wrong
  attribution fail. FR30 is rejected and checked-in `vad.threshold` returns to
  `0.5`; see `vad-sensitivity-full-promotion-review-2026-07-19.md`.
- [x] T135 Reaudit the frozen T111 Run A and Run B artifacts under the same
  complete-context material-fragment rule used for T123 and T133. Read all 556
  contributions chronologically and in reverse 600-second blocks for each run,
  reconcile every ambiguous mixed-identity natural turn, then manually compare
  the three full ledgers without code-based labels or aggregation. Add
  `ref-0099` to all three ledgers and add `ref-0239`, `ref-0426`, `ref-0503`,
  and `ref-0518` to T111. Corrected results are T111 `514/556`, T123 `505/556`,
  and T133 `497/556`. T111 remains the best frozen comparison but fails the
  3000-3600 block, 朱杰 recall, critical attribution, and confident-wrong
  gates. The documentation synchronization builds warning-clean and passes all
  `69/69` CTest entries as engineering verification only. See
  `speaker-baseline-reconciliation-2026-07-19.md`.
- [x] T136 Split every T123-only speaker regression from the completed uniform
  T111/T123 contextual comparison into missing upstream contribution evidence
  versus present contribution with changed identity or boundary. Display the
  immutable activity, primary, alignment, voiceprint, and final-decision
  evidence without executable correctness judgment. Three contributions are
  absent before alignment and thirteen remain present. Several present-text
  contexts expose corroborated short primary returns, while others have no
  two-view native support or no aligned source character in the return. See
  `corroborated-primary-return-diagnosis-2026-07-19.md`.
- [x] T137 Implement FR31 only in the final speaker-fusion policy. Preserve an
  existing aligned `primary_speaker_*` B character from an ordinary phrase or
  complete-source A repaint only for a short primary A-B-A return fully covered
  by activity B. Reuse existing TOML duration bounds, preserve specialized
  challenge precedence and every producer track, and add positive plus all
  specified abstention tests. The focused engineering test passed; T138's
  product review rejected the rule, so this implementation is removed.
- [x] T138 Replay frozen T111 and T123 typed packages repeatedly through the
  production C++ projector. Verify only determinism, byte-exact source
  reconstruction, monotonic time, configuration, and immutable-track
  contracts. Arrange every changed context, then read each complete surrounding
  conversation chronologically and in reverse against `test.txt` before
  retaining or rejecting FR31. No executable mechanism may assign correctness,
  aggregate a result, rank/select the candidate, or issue the verdict. T123
  replays were byte-identical at SHA-256
  `23f92e6bc26b925d07666e9969889251c3202e51bab744db3cf632b232e9f58f`;
  T111 replays were byte-identical at SHA-256
  `c15ccd6988fe3c6d4bca9afa9e3d1c37c56bab6402504146c99f609801ba272b`.
  Complete forward and reverse review rejects FR31 because restored short
  returns are accompanied by new boundary-leakage attribution errors. See
  `corroborated-primary-return-review-2026-07-19.md`.
- [x] T139 Close the conditional FR31 promotion step without a commit or audio
  run because T138 rejected the candidate. No silence, 120-second, 600-second,
  or full capture is attributed to FR31.
- [x] T140 Implement FR32 exact cross-scale primary-return precedence in the
  final fusion policy. Require one exact primary-run business interval whose
  session and robust galleries both pass existing gates and select the primary
  B identity, complete activity B coverage, no third activity identity, aligned
  source, and A-B-A primary brackets. Add the specified abstention tests without
  adding a TOML value or changing producer evidence. The focused policy test,
  warning-clean full build, and all 69 CTest entries pass as engineering gates.
- [x] T141 Replay frozen T111 and T123 typed packages twice through the
  production C++ projector. Verify only mechanical contracts, arrange every
  changed context, and complete forward plus reverse contextual semantic review
  against `test.txt` before retaining or rejecting FR32. No executable
  mechanism may assign correctness, aggregate a result, rank/select the
  candidate, or issue the verdict. T123 repeats are byte-identical at SHA-256
  `8fb70821df483cf40b28c701b88d38713404472dcb179a2bad10c14d4fd72ef2`
  and change only `text_id=84`; T111 repeats are byte-identical at SHA-256
  `646ea91b357cafaf8af82c4f45e5cc771c622a501e71b12f2d1aa1555fb055f2`
  and leave the business view unchanged. Complete forward and reverse review
  retains the Tang Yunfeng `不含` repair with no changed neighbor. See
  `exact-cross-scale-primary-return-review-2026-07-19.md`.
- [x] T142 Only if T141 retains FR32, commit and push the transitional
  experiment, then repeat the silence, independent 120-second, complete
  600-second, and full empty/frozen-registry real-WebSocket promotion ladder.
  Each accuracy gate is decided only by complete forward and reverse contextual
  semantic review; mechanical tooling remains limited to transport, timing,
  provenance, telemetry, determinism, and evidence arrangement. Clean commit
  `72d81c8084757b4c4210ba90ac14b5d1c1155e89` passes the lower ladder and
  completes both full paths at `0.995x` with direct-end waits of `16.768 s`
  and `16.684 s`. The full normalized seven-track entries are byte-identical.
  Independent complete forward and reverse contextual reviews each manually
  record `506/556`: FR32 repairs `ref-0154` without a new regression, but two
  fixed blocks, 朱杰 recall, critical attribution, and confident-wrong
  attribution fail. FR32 remains a bounded transitional repair; closure stays
  open. See
  `exact-cross-scale-primary-return-full-promotion-review-2026-07-19.md`.
- [x] T143 Diagnose the remaining frozen full-session speaker errors by
  displaying each pipeline's typed evidence on the common time base. Specify
  FR33 only for the partition-invariant `ref-0517` topology: uniform activity
  and primary current identity, session-only phrase challenge, opposite
  containing-VAD abstention, broader same-text and complete-source support for
  the current identity, complete galleries, and existing alignment/TOML gates.
  Add no fitted value or reference-specific input; document why each condition
  is independently available at runtime and why specialized challenges keep
  precedence. Frozen `text_id=289` confirms uniform Tang activity/primary,
  a session-only Zhu phrase challenge, a robust VAD reversal to Tang, and
  session/robust broad Tang support that fails only the existing regular score
  gate. FR33 is bounded to that exact typed topology; see
  `partition-invariant-cross-scale-abstention-diagnosis-2026-07-19.md`.
- [x] T144 Implement FR33 in the final fusion policy with focused positive and
  abstention tests. Pass a warning-clean build and all CTest entries, replay
  frozen T111 and T123 packages at least twice, verify only mechanical
  contracts, and arrange all changed evidence. Read every changed complete
  conversation forward and reverse against `test.txt` before retaining or
  rejecting the candidate. No executable mechanism may label correctness,
  aggregate accuracy, rank/select the candidate, or issue the verdict. Do not
  run new audio until this frozen review passes. Repeated T123 replays are
  byte-identical at SHA-256
  `c1b3622c36daa34537984ed8036e45a40199a4612ba3a4590dc2f02a3d7e172e`
  and change only `text_id=289`; repeated T111 replays remain byte-identical
  to FR32 at SHA-256
  `646ea91b357cafaf8af82c4f45e5cc771c622a501e71b12f2d1aa1555fb055f2`.
  Complete forward and reverse reading of `ref-0508` through `ref-0525`
  retains the Tang Yunfeng `ref-0517` repair with no neighboring regression.
  The frozen candidate is manually reconciled at `507/556`, but all remaining
  conjunctive failures keep closure open. See
  `partition-invariant-cross-scale-abstention-review-2026-07-19.md`.
- [x] T145 Commit and push FR33 as a transitional experimental checkpoint only
  after the warning-clean full build and all CTest entries pass. The complete
  build emits no warning/error line and all `69/69` CTest entries pass; these
  are engineering checks only. The checkpoint explicitly leaves speaker
  closing open and attributes no new real-WebSocket result to FR33.
- [x] T146 Specify FR34 independently on frozen evidence for the exact phrase/
  unique VAD versus coarse-direct topology exposed at `ref-0406`. The current
  typed package contains one regular coarse interval whose two galleries write
  Zhu Jie, one exact short phrase and one unique containing VAD whose four
  gallery views select Tang Yunfeng, complete Tang activity coverage, and one
  complete Xu Zijing activity/primary conflict. FR34 requires these three
  identities to remain pairwise different, reuses only existing TOML gates,
  writes only the exact phrase, and does not inherit the archived FR16AAG
  verdict. See
  `exact-phrase-vad-direct-conflict-diagnosis-2026-07-19.md`.
- [x] T147 Implement FR34 in the final speaker-fusion policy with focused
  positive and abstention tests for current-label provenance, business/VAD
  uniqueness, every gallery gate and identity agreement, alignment, candidate
  and primary activity coverage, primary uniqueness, and extra identities.
  Pass a warning-clean build and all CTest entries. Replay frozen T111 and T123
  at least twice, verify only mechanical contracts, and arrange all changed
  complete contexts without assigning correctness or a product verdict. The
  clean build emits no warning/error line and `69/69` CTest entries pass.
  Repeated T123 outputs are byte-identical at SHA-256
  `11d30935c940f155fb4b2089134b03b7ce08edb659cb1ae924122e46eea3919b`
  and split only `text_id=236`; repeated T111 outputs remain byte-identical to
  FR33 at SHA-256
  `646ea91b357cafaf8af82c4f45e5cc771c622a501e71b12f2d1aa1555fb055f2`.
- [x] T148 Read every FR34 changed complete conversation chronologically and in
  reverse against `test.txt`, then retain or remove the implementation. Only
  this contextual semantic review may update the manually reconciled ledger.
  If retained, synchronize the spec, plan, tasks, and `PROJECT_STATE.md`, then
  commit and push a clearly labeled transitional experiment. Do not attribute
  a new real-WebSocket result or closing claim to the frozen replay. Complete
  forward and reverse reading of `44:28-46:57` retains the exact Tang Yunfeng
  answer at `ref-0406`, records the preceding `对，` as a `0.160 s` boundary
  residual, and finds no neighboring change. The manually reconciled frozen
  ledger is `508/556`; every remaining conjunctive failure keeps closure open.
  See `exact-phrase-vad-direct-conflict-review-2026-07-19.md`.
