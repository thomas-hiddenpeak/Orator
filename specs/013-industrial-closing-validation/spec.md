# Spec 013: Industrial Closing Validation

**Status**: FR16ABN full real-WebSocket natural-turn gate and T112 telemetry
cadence passed; FR16ABO full promotion rejected; FR28 scheduling stability and
its deterministic trailing-context correction pass the silence and repeated
120-second gates, but the corrected 600-second contextual gate fails at
`ref-0073`; FR29 passes repeated 120-second and complete 600-second real-stream
review but its full T123 A/B promotion is rejected; FR30 VAD-sensitivity
passes T131 silence/repeatability and T132 complete 600-second contextual
review, but its full T133/T134 A/B promotion is rejected at corrected
`497/556` and the checked-in threshold returns to `0.5`; T135 reconciles the
T111/T123/T133 ledgers and withdraws the prior all-natural-turn-gates-passed
claim; FR31 deterministic frozen replay passes but complete changed-context
review rejects its broad primary-return guard; FR32 exact cross-scale
primary-return precedence passes deterministic frozen replay and the complete
real-WebSocket promotion ladder; independent full Run A and Run B contextual
reviews each record `506/556`, but two fixed blocks, 朱杰 recall, critical
turns, and confident-wrong attribution fail; FR33 deterministic T111/T123
replay and complete changed-context review retain a frozen `ref-0517` repair
at `507/556`; FR34 independently retains an exact `ref-0406` phrase repair on
frozen evidence at `508/556`; FR35 retains the partition-invariant
`ref-0420` isolated response on frozen evidence at `509/556`; FR36 retains the
regular same-slot six-view `ref-0350` repair at `510/556` and passes the
2400-3000 fixed block; FR37 retains the bracketed-primary `ref-0478` response
at `511/556`, with no new real-path claim; FR38 retains the cross-VAD
`ref-0504` phrase tail at `512/556` and raises the 3000-3600 natural-turn block
to `79/87`, with no new real-path claim; FR39 diagnosis isolates the
partition-stable `ref-0518` defect to a source-leading exact phrase whose
separate aligned tail reverses wider evidence; frozen replay and complete
context review retain the exact phrase repair at `513/556` and raise the
3000-3600 block to `80/87`, with no new real-path claim; FR40 retains the
partition-invariant two-unit primary handoff for `ref-0024`/`ref-0025`, moving
the current frozen candidate to `514/556` without a new real-path claim; FR41
retains the single-unit source partition of the same primary-onset aligned-
island rule at `ref-0268`, moving the current frozen candidate to `515/556`
and passing all four per-speaker natural-turn floors without a new real-path
claim; FR42 diagnosis isolates the critical `ref-0432` T111/T123 regression to
one zero-duration character inside an otherwise retained isolated-VAD aligned
island; deterministic replay and complete forward/reverse contextual review
retain the bounded repair at `516/556` without a new real-path claim; FR43
diagnosis bounds the critical `ref-0194` T111/T123 regression to one
zero-duration aligned character plus a nonlocal phrase top identity;
deterministic replay and complete forward/reverse contextual review retain the
complete-source repair at `517/556` without a new real-path claim; FR44 bounds
the critical `ref-0071` overwrite to one regular session-only phrase whose
base source is Shi-Tang-Shi across two VAD records; deterministic replay and
complete forward/reverse contextual review retain the phrase abstention at
`518/556` without a new real-path claim; capture-faithful exact-PCM snapshot
replay separates the source-absent `ref-0066` from the source-present
`ref-0192` and reproduces every captured T123 identity value; strict FR45
frozen replay and complete forward/reverse contextual review retain the bounded
`ref-0192` repair at `519/556` without a new real-path claim; FR46 stops after
complete orthogonal-evidence diagnosis; source-bounded FR47 deterministic
replay and complete 556-contribution forward/reverse contextual review retain
`ref-0507` and `ref-0509`; commit `70f1186` then completes the
restarted 120/600/full A/B real-WebSocket ladder, and separate complete
forward/reverse contextual review of each full artifact initially transcribes
`521/556` without long-term identity drift; FR48 repeats both complete reviews
under the speaker-only boundary, corrects the ASR-only `ref-0375` row to
speaker-correct, and initially records `522/556` with 20 critical residuals;
its proposed hierarchical-consensus guard stops before implementation because
only one material context has the complete topology; FR49's complete residual
reread corrects the prior omission of `ref-0121`, implements one bounded
source-leading primary-prefix topology shared with `ref-0061`; clean commit
`1f09052` completes its restarted 120/600/full A/B real-WebSocket ladder, and
four independent complete full-artifact contextual readings retain a manually
signed `523/556` without a new attribution regression or accumulating late
drift; all 20 critical residuals remain; FR50 T229-T231 implement one
false-default, TOML-enabled right-bounded short-primary aligned-unit policy,
complete deterministic frozen FR49 A/B replay, and retain `ref-0327` and
`ref-0417` through four independent complete candidate readings; the manually
reconciled frozen candidate is `525/556` with 19 critical residuals, but no
FR50 live run has occurred and FR49 remains the `523/556` real-path baseline;
critical attribution,
confident-wrong attribution, T102, T084, full canonical closure, release sign-
off, and industrial readiness remain open
**Created**: 2026-07-13
**Scope**: Re-establish a truthful product baseline, recover full-session business
accuracy, and define the evidence required before Orator may be declared closed.
**Constitution**: v1.7.0

## 1. Objective

Orator's v2.1 speaker-business pipeline has repeatable full-session evidence,
but no frozen run passes every natural-business-turn speaker-attribution gate.
The FR16ABN Run A and Run B were each executed through the real WebSocket path,
and
all 556 reference contributions were reconciled under complete forward and
reverse conversational context. The T102 breakdown reread reconciles the
`ref-0160` source-label conflict and the `ref-0182` boundary-only judgment, and
the T135 complete A/B reread corrects five omitted errors. Each run has 514
accepted and 42 incorrect contributions, approximately 92.45 percent. Their
direct-end runs also satisfy the terminal-latency gate mechanically. The
3000-3600 fixed block, 朱杰 natural-turn recall, critical-turn, and
confident-wrong gates fail. Speaker-time, per-speaker time,
source-time-offset, ASR, release, and independent-holdout gates remain open.

The later FR16ABO full promotion does not replace that baseline. Clean
transitional commit `f49a8278e0d8` passed the 120/600-second and full A/B
mechanical ladder, but complete 556-contribution forward/reverse semantic
review against `test.txt` manually records `518/556` for each run. The two
error sets also differ. That historical total predates T135's uniform
material-fragment reconciliation and is not used for cross-version ranking.
FR16ABO is disabled in the checked-in TOML, and T111 remains only the best
fully reconciled frozen comparison baseline.

FR29 also does not replace T111. Clean commit `2ff9ce3655b2a12e90a5d0def25c0a30f171f2d9`
completed independent empty-registry and restarted frozen-registry full runs.
All mechanical, terminal-latency, telemetry, observer, provenance, and
repeatability contracts pass, and the seven product-track entry bundles are
identical between runs. Complete chronological and reverse-block contextual
review, corrected by T135 for `ref-0099`, manually records `505/556` for each
run. The full average remains above
90 percent, but two fixed blocks, 朱杰 and 唐云峰 turn recall, critical
attribution, confident-wrong attribution, and the 93-percent development margin
fail. FR29 therefore remains transitional and T111 remains the best frozen
comparison baseline, not an accepted closing result.

Frozen T111/T123 diagnosis isolates the full-session regression upstream of
the deterministic business projector. Sortformer diarization and primary-
speaker tracks are identical. The current projector reproduces every T111
reference-interval speaker sequence when given T111 typed inputs, while the
T123 typed inputs reproduce the T123 view. At manually reviewed low-energy
utterances, the checked-in `vad.threshold = 0.5` leaves stable VAD gaps that
FR28 must now skip; T111 had consumed the same audio only because its ASR worker
ran ahead of the VAD frontier. T135 establishes that the strongest
`ref-0503` speaker error was already present in T111, so this upstream evidence
difference is not itself a speaker-attribution repair. A one-variable FR30 TOML
candidate lowers only
the production VAD threshold to `0.3`. It passes three independent
real-WebSocket silence sessions, exact seven-track repeatability across two
independent 120-second runs, and complete forward and reverse review of all 18
in-scope contributions without a new natural-turn regression. Its clean
600-second run then passes every mechanical contract, and complete forward and
reverse review of all 93 contributions plus all ten T128 sequence changes
finds no new natural-turn regression. The subsequently authorized full
empty-registry and frozen-registry paths are mechanically repeatable, but
complete independent forward and reverse review, corrected by T135 for
`ref-0099`, manually records `497/556`
for each. The full 90-percent floor, two fixed blocks, three canonical
speakers, critical attribution, and confident-wrong attribution fail. FR30 is
therefore rejected, its checked-in threshold returns to `0.5`, and T111
remains the best frozen comparison baseline without satisfying closing.

T117-T121 subsequently prove that the T116 A/B producer difference begins in
scheduling-sensitive VAD-gated ASR rather than Sortformer or the deterministic
business projector. FR28 publishes stable typed VAD frontiers and buffers
undecided ASR audio. It passes the warning-clean build, VAD oracle, all 69 CTest
entries, and two independent 120-second production WebSocket runs whose seven
canonical product tracks are identical. Complete forward/reverse review of all
18 in-scope reference contributions finds no new speaker regression. This
permitted a 600-second promotion run only. That clean run passed every
mechanical contract, but complete forward/reverse review found new speaker
regressions at `ref-0037` and `ref-0073`. FR28 therefore did not advance to a
full run. Its successor preserves short-gap decoder audio and terminal
source-clock context while retaining publication-order determinism. Three
silence runs and two byte-identical 120-second runs pass, and a new 600-second
run restores the substantive `ref-0037` contribution. Complete chronological
and reverse review still rejects promotion because `ref-0073` remains split
across Tang Yunfeng, Shi Yi, and Tang Yunfeng. At that point both Sortformer
views support Shi Yi, but one anomalously extended forced-alignment unit crosses
their corroborated handoff into Tang Yunfeng and allows a containing voiceprint
interval to erase the useful native boundary. The next correction is therefore
confined to the business projection; it does not alter FR28, any producer
track, or the full T111 result.

This spec defines two separate claims:

1. **Canonical-scene acceptance**: the complete product passes the required
   full-length evaluation on `test/data/audio/test.mp3` against
   `test/data/reference/test.txt`.
2. **Industrial readiness**: the same accepted configuration also passes a
   locked, independently recorded holdout set that was not used for tuning.

Passing the canonical scene is mandatory and cannot be replaced by a simpler
recording. It is necessary, but by itself it is not sufficient evidence for a
general industrial-readiness claim.

### 1.1 Closing baseline decision

All remaining work in this spec uses streaming Sortformer v2.1 with the
checked-in `340/1/188/188` profile (chunk/right-context/FIFO/cache-update) as
the sole closing baseline. The v2 checkpoint and its CTest gate have been
removed; only prior reports and hashes remain as historical evidence. A v2
artifact cannot be selected for a new candidate, used for an acceptance run, or
satisfy any Phase 3-7 gate. This decision fixed the model line on which accuracy
recovery and formal validation proceeded. The current-source A/B evidence
accepts only its natural-business-turn gate within the claim boundary above.

## 2. Definitions

- **Final business view**: the terminal user-facing track that states who said
  what and when. Raw ASR, diarization, voiceprint, voice activity detection
  (VAD), and forced-alignment tracks are evidence, not the acceptance target.
- **Reference turn ledger**: the immutable 556-turn human-audited reference in
  `test.txt`, plus review annotations that do not rewrite it. The source speaker,
  text, timestamp, line order, and their recorded precision are authoritative.
  Duplicate or backward timestamps remain in source order and are interpreted
  from the surrounding conversation; an auxiliary JSON or worksheet only
  mirrors this reference and arranges system evidence.
- **Natural business turn**: one speaker's contextually complete contribution,
  including short replies and interruptions when they change or confirm a
  business position.
- **Critical turn**: a turn containing a decision, commitment, number,
  percentage, negation, ownership position, responsibility, or other content
  whose speaker identity changes the business interpretation.
- **Correct speaker time**: human-reviewed source time blocks in `test.txt`
  attributed to the correct real speaker in the final business view. Review uses
  the recorded one-second source precision and surrounding line context.
  Higher-resolution runtime boundaries may expose an offset but cannot create
  finer reference truth than `test.txt` provides.
- **Uncertain output**: an output marked `unknown`, `speaker_uncertain`, or an
  equivalent state. It is safer than a confident wrong attribution, but it does
  not count as correct for the 90 percent gate.
- **Closing baseline**: the exact v2.1 model file and `340/1/188/188` streaming
  profile resolved from the checked-in `orator.toml`. Experiments use separate
  TOML files and do not change the closing baseline until they pass every
  promotion gate.

## 3. Verified Baseline Defects and Remediation Status

The following code-level defects were verified when this spec was approved.
Items marked resolved have implementation and focused-test evidence but remain
subject to the complete acceptance gates in this spec.

1. **Resolved in Phase 1**: `AuditoryStream` now owns one immutable `TimeBase`
   and injects it into private caches, workers, and retained audio stores. At
   finalization it reports and verifies exact sample extents for every active
   input, evidence, identity, alignment, and business-speaker track.
2. **Resolved in Phase 1**: ASR reads an immutable typed VAD snapshot from
   `ComprehensiveTimeline`, while forced alignment receives finalized typed ASR
   records through a `ComprehensiveTimeline` subscription. Protocol messages
   are emitted only after typed track commit.
3. **Resolved in Phase 1**: `ComprehensiveTimeline` is now a pure typed store.
   A registered `business_speaker` pipeline owns speaker selection, gap policy,
   align-aware text projection, and support diagnostics, and writes only its
   own revisable track. Raw ASR and alignment records reject conflicting
   same-ID deposits.
4. **Resolved for the runtime contract**: timeline gap-fill and every retained
   model/GPU/transport behavior are typed fields; startup applies defaults,
   TOML, environment, then CLI. Model, GPU, and transport layers no longer read
   process environment, and terminal packages carry the resolved configuration.
5. **Resolved in Phase 1**: finalized ASR allocates one `text_id` and reuses it
   for the typed sink and live event; the terminal ASR track serializes that ID.
   The registered real-WebSocket and Node gates plus a real Chromium run verify
   partial/final/retract, alignment, business revision, terminal track, exact
   export/load, and reconnect convergence. Full-session repeatability remains
   open.
6. **Expanded through 2026-07-17**: the configured CTest suite contains 68
   active entries. The complete suite passes, including the real-WebSocket
   contract, official v2.1 numerical profiles, focused speaker-evidence and
   production-policy coverage, and the dependency-free browser-model gate.
   Full browser acceptance remains manual because Playwright is a tools-only
   dependency.
7. **Historical defect-isolation evidence only**: a pinned NVIDIA v2 NeMo oracle
   crosses five runtime chunks and repeated FIFO/cache updates. The
   regenerated 1502-frame fixture matches C++ at `max_abs=1.43051e-6` and
   1502/1502 argmax. This proved the FIFO implementation correction but cannot
   satisfy a current model or acceptance gate. Its model file and obsolete
   CTest were removed after v2.1 became the sole closing baseline.
8. **v2.1 selected as the sole closing baseline; product gate still open**: the
   compile-time default and checked-in TOML select streaming v2.1. Its inherited
   asynchronous profile passed the exact multi-chunk NeMo gate. An earlier
   936-entry full real-WebSocket artifact recorded a cut-oriented written-context
   diagnostic of 413 / 142 / 1 natural turns. NVIDIA's official high- and
   low-latency profiles also passed separate numerical gates, but historical
   full contextual diagnostics recorded only 385 / 170 / 1 and 377 / 178 / 1.
   Neither official profile proceeds to transport acceptance. The inherited
   v2.1 profile is the fixed starting point for closing work and remains below
   the 90 percent product gate.
9. Previous full-length reviews used coarse 10-minute summaries and selected
   regression windows. They did not adjudicate and sign off every reference
   turn, so they cannot supply the denominator for a 90 percent claim.
10. **Full closing-baseline system capture complete; accuracy review open**:
    clean commit `3b40245` completed the 3615.12-second v2.1 stream in 3616.442
    seconds with exact seven-pipeline extents, zero mechanical issues,
    continuous telemetry, and exact producer/persisted/Web UI/download terminal
    equality. This closes the capture requirement in T044. At that checkpoint an
    optional annotation JSON was still described as unsigned; the corrected
    contract recognizes `test.txt` itself as the human-audited reference. T045
    remained open because that run had not yet received complete result review.
    See
    `closing-baseline-v21-2026-07-15.md`.
11. **Clean closing-baseline contextual review complete; detailed breakdowns
    open**: the exact 935-entry clean package received a full chronological
    manual pass and a reverse fixed-block manual pass. The reconciled result is
    443 correct / 112 incorrect / 1 ambiguous (`79.6763%`). Tools only arranged
    evidence and did not assign correctness. The result fails the full-session
    gate and five fixed 600-second block gates. Speaker-time, source-time
    offsets, criticality, and independent totals remain unsigned,
    so this is not a constitutional closing score. See
    `closing-baseline-v21-context-review-2026-07-15.md`.
12. **Current clean-commit natural-turn gate passed; T084 open**: commit
    `6dbc600e4eb5` completed empty-registry Run A and restarted frozen-registry
    Run B over the full real-WebSocket path. Complete contextual semantic review
    manually records `514/556` for Run A and `513/556` for Run B. The complete
    T084 audit leaves speaker-time, fixed-block, per-speaker, criticality,
    confidence, source-time-offset, and terminal-latency gates unsigned. See
    `current-commit-full-review-2026-07-17.md`.
13. **FR26 direct-end full A/B seal complete; T084 open**: clean commit
    `588bfbe63555` completed the empty-registry and restarted frozen-registry
    `3615.120`-second runs with direct `end` waits of `25.597 s` and
    `26.305 s`. Both runs satisfy the terminal-latency contract mechanically.
    Complete forward/reverse contextual semantic review manually records
    `512/556` for Run A and `509/556` for Run B, so both retain the natural-turn
    gate. Speaker-time, fixed-block, per-speaker, criticality, confidence, and
    source-time-offset gates remain unsigned. See
    `direct-end-full-review-2026-07-18.md`.
14. **FR16ABM full real-path promotion complete; T102/T084 open**: clean commit
    `1a475e6b7473` passed the warning-clean build, `68/68` CTest, 120-second and
    600-second real-WebSocket promotion, and full direct-end A/B recapture.
    Complete 556-contribution forward and reverse contextual semantic review
    manually records `514/556` for empty-registry Run A and `515/556` for
    frozen-registry Run B. This includes the repaired sustained handoff at
    `ref-0071` and a context-based interpretation of the whole-second source-time
    edge at `ref-0250`; no code assigned or aggregated the result. Speaker-time,
    fixed blocks, per-speaker review, criticality, confidence, and source-time
    offsets remain unsigned. See
    `native-handoff-full-promotion-review-2026-07-18.md`.
15. **FR16ABN frozen A/B replay retained; real-path promotion completed later**:
    a bounded delayed-clause rule reuses existing TOML punctuation and
    thresholds to
    recover one subminimum response only when activity, primary, typed VAD, and
    forced alignment form the specified A-B-A topology. The warning-clean build
    and `68/68` CTest pass. Three byte-stable replays per promoted A/B track
    change only the same short response group. Complete chronological/reverse
    context review retains that repair while preserving the following
    substantive turn. This frozen checkpoint was not a production-run result;
    its successor promotion is recorded below. See
    `delayed-alignment-clause-review-2026-07-18.md`.
16. **FR16ABN full real-path captured; T135 corrects the gate result**:
    transitional experimental commit `6b1cb79fa4f5` passed the warning-clean
    build, `68/68` CTest, and 120-second and 600-second direct-end production
    WebSocket promotion. Empty-registry Run A and restarted frozen-registry Run
    B then completed the full `3615.120` seconds at `0.993x`, with direct waits
    of `25.849 s` and `25.585 s`, exact seven-track extents, observer
    convergence, and accepted telemetry coverage. One earlier Run B artifact
    is explicitly excluded because runtime telemetry cadence was `94.965%`;
    the controlled retry passed at `95.214%` without changing behavioral TOML
    values. Complete forward/reverse contextual semantic review, followed by the
    T102 `ref-0160` and `ref-0182` context reconciliations, originally recorded
    `519/556`. T135's complete A/B reread corrects five omitted errors and
    supersedes that total with `514/556` for both frozen runs. FR16ABN repairs `ref-0090` on both
    paths; other run-specific repairs are not attributed to that rule.
    The 3000-3600 fixed block, 朱杰 recall, critical, and confident-wrong gates
    fail. Speaker-time, per-speaker time, and source-time-offset
    results remain unsigned. See
    `delayed-alignment-full-promotion-review-2026-07-18.md` and
    `speaker-gate-breakdown-review-2026-07-18.md` and
    `speaker-baseline-reconciliation-2026-07-19.md`.
17. **GPU telemetry absolute cadence verified; T102/T084 unchanged**:
    transitional experimental commit `d610de36ed13` replaces relative waiting
    with monotonic absolute deadlines while preserving the one-second TOML
    interval and telemetry payload. The warning-clean build and all `69/69`
    CTest entries pass. A clean 120-second 1.0x real-WebSocket run records 119
    runtime GPU samples (`99.167%` cadence), 120 continuous tegrastats samples,
    exact one-second runtime steps, and complete required-field coverage. This
    is mechanical evidence only and does not reopen or extend the T111 product
    result. See `gpu-telemetry-deadline-review-2026-07-18.md`.
18. **FR16ABO full real-path promotion rejected; T102/T084 unchanged**:
    clean transitional commit `f49a8278e0d8` passed the warning-clean build,
    all `69/69` CTest entries, 120/600-second direct-end promotion, and full
    empty/frozen-registry A/B capture. Both full runs satisfy common-clock,
    terminal-latency, observer, provenance, and telemetry contracts. Complete
    review of all 556 contributions in chronological and tail-to-start order
    against the human-listened `test.txt` historically records `518/556` for
    each run, with different A/B error sets. That total predates T135's uniform
    material-fragment reconciliation and is no longer used to rank it against
    the corrected T111 baseline.
    No code assigned or aggregated the result. The default TOML lookahead is
    restored to zero. See
    `future-epoch-full-promotion-review-2026-07-18.md`.
19. **FR29 full real-path promotion rejected; T102/T084 unchanged**: clean
    commit `2ff9ce3655b2a12e90a5d0def25c0a30f171f2d9` completed full empty-registry
    Run A and restarted frozen-registry Run B. Both runs finish `3615.120`
    seconds at `0.995x`, receive their direct-end terminal packages in
    `16.540 s` and `17.499 s`, close all seven tracks at `57,841,920` samples,
    and satisfy observer, provenance, and telemetry contracts. Their seven
    normalized product-track entry bundles are identical. Complete independent
    chronological and reverse-block review of all 556 `test.txt` contributions
    originally recorded `506/556`; T135 adds the omitted `ref-0099` error and
    corrects each run to `505/556`. The 2400-3000 and 3000-3600 fixed
    blocks fail, as do 朱杰 and 唐云峰 turn recall, critical attribution, and
    confident-wrong attribution. Final synchronization builds warning-clean and
    passes `69/69` CTest as mechanical verification. T111 remains the best
    frozen comparison baseline, not a closing result; see
    `cross-view-handoff-full-promotion-review-2026-07-18.md`.
20. **T123 regression boundary isolated without a new audio run**: frozen
    T111 and T123 Run A have byte-identical Sortformer diarization and primary-
    speaker exports. Replaying T111 typed tracks through the current projector
    changes zero reference-interval speaker sequences, while replaying T123
    typed tracks reproduces its 1,707-entry final view apart from sub-microsecond
    text-split serialization. At the manually reviewed `2752-2754 s` and
    `3278-3284 s` contexts, production VAD at TOML threshold `0.5` leaves the
    speech outside stable evidence, so FR28 deterministically omits it from ASR.
    T135 later establishes that T111 already misattributes the sustained
    `ref-0503` turn despite retaining more words there; this observation is an
    upstream evidence diagnosis, not a speaker-correctness explanation.
    Production `GpuVad` probes changing only the TOML threshold to `0.4` and
    then `0.3` expose additional raw speech intervals; `0.3` also retains zero
    VAD segments on the frozen 30-second silence fixture. The checked-in `0.3`
    candidate then passes the VAD numerical test, a warning-clean build, and
    all `69/69` CTest entries. These are causal and mechanical observations
    only, not candidate accuracy or acceptance. See
    `vad-sensitivity-diagnosis-2026-07-19.md`.
21. **T111/T123/T133 natural-turn ledgers uniformly reconciled**: both frozen
    T111 A/B artifacts receive a new complete 556-contribution chronological
    read and reverse-block read. The material-fragment rule adds `ref-0099` to
    all three ledgers and adds `ref-0239`, `ref-0426`, `ref-0503`, and
    `ref-0518` to the previously incomplete T111 ledger. Corrected results are
    T111 `514/556`, T123 `505/556`, and T133 `497/556`. T111 remains the best
    frozen comparison, but its 3000-3600 block, 朱杰 recall, critical
    attribution, and confident-wrong gates fail. See
    `speaker-baseline-reconciliation-2026-07-19.md`.

## 4. Requirements

### 4.1 Governance and reproducibility

- **FR1**: The project state must explicitly say that closure is open until all
  gates in this spec pass in one release candidate.
- **FR2**: Every run must record commit ID, dirty-worktree state, `orator.toml`
  hash, resolved configuration, model/data hashes, device identity, power mode,
  clock state, speaker-registry fixture hash, test-client version, and output
  artifact hash.
- **FR3**: Runtime tuning may be changed only in `orator.toml`. Test-client
  duration, pacing, and output path are test-harness controls, not runtime model
  parameters.
- **FR4**: A result obtained with an unrecorded environment or CLI override is
  invalid for acceptance.

### 4.2 Architecture contracts

- **FR5**: One immutable session time base must be created once by the audio
  ingest owner and passed to every pipeline and audio store. No pipeline or
  cache may construct an independent origin.
- **FR6**: Pipelines may consume another pipeline's evidence only through typed
  tracks in `ComprehensiveTimeline`. The protocol layer may serialize, retain,
  and transport track updates, but it may not be the private pipeline data bus.
- **FR7**: Evidence combination must be an explicit registered fusion pipeline
  that writes its own revisable `business_speaker` track. Raw tracks remain
  immutable. `ComprehensiveTimeline` remains a container/alignment layer.
- **FR8**: ASR final, forced-alignment group, revision, terminal ASR track, final
  business view, and Web UI state must use the same stable `text_id`.
- **FR21**: Every `business_speaker` entry must carry a reference-free,
  structured speaker-decision audit. The audit records the speaker evidence
  source, text-projection source, decision reason, selected candidate, every
  rejected overlapping candidate, candidate overlap/coverage/confidence, and
  selected-versus-best-alternative overlap and confidence margins. Live
  revisions, terminal tracks, the compatibility alias, and Web UI state must
  preserve the same audit object. Adding or revising audit evidence must not
  change a raw track or silently change the selected speaker.
- **FR22**: Frozen legacy timeline packages that predate FR21 may reconstruct
  the same audit object from their immutable diarization and business tracks.
  The reconstruction tool must not read reference annotations, alter an input
  track, or assign correctness. Discrete fields must match exactly. Legacy
  three-decimal confidence and millisecond timeline fields may use only their
  declared quantization envelopes; any runtime value outside those bounds must
  fail. New artifacts must retain round-trip raw confidence precision, while
  millisecond time-derived fields remain explicitly bounded.
- **FR23**: Closing includes an engineering-maintainability gate for the accepted
  speaker-business implementation. Evidence collection, speaker-fusion policy,
  and business-track orchestration MUST have separate ownership boundaries.
  Refactoring MUST preserve the public `BusinessSpeakerPipeline` contract, typed
  timeline tracks, TOML fields and defaults, policy execution order, decision
  reasons and sources, projected text/time ranges, and serialized output. It
  MUST add no model, threshold, runtime dependency, reference-specific value, or
  alternate pipeline data path. Before and after outputs from one frozen full-
  session typed input MUST be byte-identical; a mismatch aborts the refactor and
  cannot be interpreted as an accuracy change or improvement.
- **FR24**: The active build and validation surface MUST distinguish production
  regression coverage from historical candidate experiments. The checked-in
  root `orator.toml` is the sole production speaker configuration. Accepted
  runtime fusion behavior MUST remain covered by focused C++ tests; evidence
  capture, provenance, and review-packet tools may remain active only when they
  do not assign correctness or select a result. One-off candidate generators,
  their tests, and non-production TOML profiles MUST live in an explicitly
  inactive historical archive and MUST NOT be registered by CMake/CTest.
  Legacy programs that calculate product speaker accuracy or choose a result
  MUST be removed rather than archived as runnable tools. Git history remains
  the source for their historical implementation.

### 4.3 Reference and evaluation

- **FR9**: `test.txt` is the authoritative human-listened reference and the
  reference turn ledger must cover all 556 timestamped turns without asking for
  a second transcription or a new audible-boundary pass. Its source speaker,
  text, timestamp, precision, and line order are immutable. Duplicate and
  backward timestamps are interpreted manually from complete context and are
  never repaired by code. No sampling, selected-window substitution, or
  code-inferred correctness is permitted.
- **FR10**: Accuracy judgments must be made item by item in conversational
  context. No compiled program, C++/CUDA test, Python/shell/JavaScript script,
  notebook, spreadsheet formula, query, metric, algorithm, or temporary command
  may assign correctness, infer semantic equivalence, aggregate accuracy,
  rank/select a candidate, or issue a promotion/acceptance verdict. Tools may
  capture, verify mechanical contracts, index, and present unjudged evidence
  only.
- **FR11**: Every reported percentage must be manually derived and manually
  cross-checked from signed contextual ledger rows. Automated counting,
  percentage calculation, threshold comparison, and acceptance booleans are
  prohibited. Source-time offsets and overlapping speech must remain visible at
  the precision supported by `test.txt`; no sub-second reference statistic may
  be claimed from whole-second source timestamps.
- **FR12**: Diagnostic model metrics, including local-slot mapping scores,
  diarization error rate, character error rate, unknown duration, and embedding
  similarity, must not replace the final business-view gates.

### 4.4 Model and pipeline validation

- **FR13**: Every model stage affected by a candidate must pass its trusted
  oracle gate. The exact accepted Sortformer model file and runtime profile must
  be compared with NeMo; the source checkpoint revision, source and converted
  model hashes, TOML hash, and oracle-fixture hash are part of that gate.
  TitaNet and ASR/align changes must be compared with their PyTorch or NeMo
  references. An asynchronous Sortformer profile must exercise at least three
  chunks, include output produced after cache compression, and compare FIFO
  inclusion, overflow transfer, cache compression, and frame probabilities. A
  synchronous `fifo_len = 0` fixture or a different Sortformer checkpoint does
  not validate an asynchronous runtime profile. Every Phase 3-7 closing run
  must resolve the v2.1 model and `340/1/188/188` baseline from TOML; a v2 run
  is historical evidence and is ineligible for promotion.
- **FR14**: Before another full model run, frozen evidence must be used to
  expose the best attainable speaker decision from independent Sortformer,
  TitaNet, VAD, ASR, and forced-alignment evidence. Reference labels may be read
  only during complete manual contextual semantic review and may never enter
  runtime decisions or any executable correctness, score, accuracy,
  aggregation, ranking, candidate selection, parameter selection, or verdict.
- **FR15**: If the frozen-evidence development candidate cannot reach 93 percent
  on both speaker-time and natural-turn measures, parameter tuning stops. Every
  locally available, deployable streaming model revision must be evaluated under
  the same runtime profile. Offline-only diarization models are excluded from
  candidate selection because the manually adjudicated `test.txt` already
  supplies business truth and an offline result cannot become the deployed
  runtime. This does not remove the same-checkpoint NeMo numerical oracle
  required by FR13.
- **FR16**: Silence, endpoint, and hallucination behavior must be tested through
  the real WebSocket path. No substantive final ASR text may be emitted for
  confirmed silence.
- **FR16A**: The v2.1 state-rotation candidate must derive its rotation period
  from the checkpoint's declared 90-second training-session contract. Every
  rotated Sortformer slot must receive a session-qualified local identity, and
  TitaNet must stitch those local identities using audio evidence only. The
  candidate may not read transcript phrases, reference speakers, known failure
  timestamps, or correctness judgments. VAD and forced alignment may constrain
  legal rewrite boundaries but may not supply a speaker identity.
- **FR16B**: A continuous-v2.1 direct-evidence fusion candidate may override a
  frozen business-speaker identity only from TitaNet evidence extracted from
  that same business interval or its overlapping diar intervals. The existing
  TOML strong and candidate score/margin gates apply. Conflicting evidence may
  not override, and local-slot proximity, neighbouring identities, transcript
  content, reference speakers, or known failure timestamps may not manufacture
  an identity. When direct evidence is absent or ineligible, the candidate must
  preserve the frozen v2.1 business attribution and uncertainty state.
- **FR16C**: The v2.1 native-postprocessing candidate must use the NeMo
  `PostProcessingParams` defaults shipped in the pinned local validation
  environment: onset `0.5`, offset `0.5`, minimum on duration `0`, and minimum
  off duration `0`. It must keep the accepted continuous streaming profile,
  model weights, session clock, and all non-diar tracks unchanged. These four
  values form one model-contract candidate and may not be swept against the
  reference.
- **FR16D**: Any frozen candidate that changes diarization boundaries must be
  replayed through the production `ComprehensiveTimeline` and
  `BusinessSpeakerPipeline`. Reassigning identities on the old business grid is
  evidence display only and cannot promote or reject that boundary candidate.
  Replay inputs may be mechanically exported, but the exporter and replay
  probe may not read references or emit correctness fields.
- **FR16E**: The replay-interval voiceprint candidate must embed each rebuilt
  business interval independently and may use only its current audio. Intervals
  below the configured short-duration boundary use a separate TOML score and
  margin gate because absolute TitaNet cosine decreases with available speech;
  longer intervals retain the existing candidate gate. No diar local identity,
  neighbouring turn, transcript content, reference label, or known timestamp
  may supply or propagate the selected identity.
- **FR16F**: A duration-calibrated replay candidate may use the final
  session-refreshed registry because the comprehensive timeline is a rewritable
  end-of-session view. For intervals shorter than 1.5 seconds, absolute cosine
  may not gate identity because the production `0.55` match threshold is
  calibrated for approximately 3-4 seconds of speech; the top-two margin over
  the four session-active identities remains mandatory. Regular intervals must
  reuse the production `match_threshold` and configured candidate margin. The
  duration boundary, margins, and regular threshold are frozen in TOML before
  contextual review and may not be swept against the reference.
- **FR16G**: A phrase-reaggregation candidate may bridge a low-evidence replay
  fragment only when two eligible current-audio TitaNet anchors on its
  immediate sides select the same active session identity. The anchors and
  fragment must share one ASR `text_id` and one forced-alignment run whose unit
  gaps do not exceed the TOML pause boundary. The complete bridge must remain
  below the frozen short-duration boundary. Eligible anchors are immutable,
  conflicting or one-sided evidence never propagates, and no bridge may cross
  an ASR segment, forced-alignment pause, or configured duration/gap limit.
  Reference text, reference identities, names, known timestamps, and prior
  judgments are forbidden inputs. This topology must be generated once from a
  frozen TOML and must pass complete manual contextual review without a value
  sweep.
- **FR16H**: A punctuation-phrase candidate may use finalized ASR punctuation
  only to define natural phrase boundaries. Every phrase timestamp must come
  from the matching forced-alignment characters on the common session clock,
  and every proposed identity must come from TitaNet applied to that phrase's
  own audio against only the four active session identities. The phrase may
  fill low-evidence text only when every eligible direct-interval voiceprint
  anchor it overlaps agrees with the phrase identity. Any conflicting anchor
  rejects the complete phrase overlay, and an eligible direct anchor is never
  changed. The phrase duration, punctuation set, score/margin gates, and
  projection policy must be frozen in TOML before evidence generation and may
  not be swept against the reference.
- **FR16I**: An anchored punctuation-phrase candidate strengthens FR16H by
  requiring at least one overlapping eligible direct-interval voiceprint
  anchor. A phrase with no direct anchor is evidence-only and may not change
  the business view. Every present anchor must still agree with the phrase
  identity, and all score, margin, duration, punctuation, clock, and immutable-
  anchor rules from FR16H remain unchanged.
- **FR16J**: A posterior-bounded phrase candidate must split every aligned ASR
  phrase at sustained v2.1 raw-posterior local-speaker transitions before
  identity fusion. Each resulting piece keeps one absolute session interval,
  one source-character range, one dominant Sortformer local slot, and its own
  TitaNet embedding against the four active session identities. A piece may
  rewrite low-evidence text only when the local-slot global mapping and the
  piece TitaNet identity agree. Regular direct-interval voiceprint anchors are
  immutable. A conflicting short direct anchor may be replaced only by a
  regular-duration piece with that two-model agreement; otherwise every
  conflict preserves the duration-calibrated input. Frame activity, sustained
  run, phrase duration, identity gates, and overwrite policy must be frozen in
  TOML before generation. The generator may verify time, source-text, and
  evidence-provenance contracts, but may not read the reference ledger, emit a
  correctness field, score a result, or select a parameter.
- **FR16K**: A multiresolution phrase candidate may add two bounded evidence
  paths without changing any identity score, margin, or duration gate. First,
  an active raw-posterior run shorter than the TitaNet embedding floor may
  project its mapped identity only when the enclosing aligned punctuation
  phrase's TitaNet identity agrees; a conflicting sole direct anchor blocks the
  projection. Second, a regular direct anchor may be split inside an aligned
  posterior-bounded piece only when the mapped raw local identity, that piece's
  TitaNet identity, and its enclosing punctuation phrase's TitaNet identity all
  agree. When piece and phrase TitaNet agree against the mapped local identity,
  they may override it only if the piece reuses the existing regular score gate
  and the phrase does not contain competing direct-anchor identities, or if the
  conflicting active local run is isolated within the existing short-duration
  boundary and no direct anchor exists. Every write is limited to the forced-
  aligned source-character range and absolute raw-frame interval that supplied
  the evidence. The micro-run floor, enabled paths, mixed-anchor rule, and
  existing gate reuse must be frozen in TOML. The generator may arrange and
  verify mechanical evidence only; no code, script, formula, query, notebook,
  metric, or algorithm may assign correctness, aggregate accuracy, rank/select
  a candidate or parameter, or issue a verdict.
- **FR16L**: The production speaker-identity stage may confirm a local-slot
  identity epoch change from repeated candidate-strength current-audio
  evidence without lowering any score or margin gate. Confirmation requires a
  TOML-defined number of non-overlapping clean spans from the same local slot,
  all naming the same competing enrolled identity, all independently passing
  the existing candidate score and own-epoch margin gates, and all falling
  inside the existing TOML backfill window. One candidate span remains
  provisional. A different competing identity replaces the provisional state,
  and a later clean span that strongly supports the active epoch clears it.
  Confirmation may backfill only through the existing same-local gap contract;
  a later strong return to the prior identity must start a new epoch rather
  than rewriting the confirmed interval. The mechanism may not read transcript
  text, real speaker names, reference labels, known timestamps, or correctness
  judgments. Its TOML count and inherited gates must be frozen before candidate
  generation and may not be swept against the reference.
- **FR16M**: A multi-prototype TitaNet candidate may retain both the persistent
  pre-session registry prototype and the session-refreshed terminal prototype
  for each enrolled identity. For one current-audio query, each identity's
  score is the maximum cosine across its own prototypes; the resulting
  per-identity scores compete under the unchanged duration-aware absolute and
  top-two margin gates. Scores may be combined only for the same stable
  registry ID, and only the four acoustically established active session IDs
  may enter the business decision. Missing evidence, mismatched intervals, or
  an incomplete identity gallery rejects the fused record. The aggregation
  mode, active-ID source, identity gates, and duration boundary must be frozen
  in TOML before generation. Tools may verify provenance and mechanical
  contracts but may not read reference labels, emit correctness, calculate or
  aggregate accuracy, rank candidates, select parameters, or issue a verdict.
- **FR16N**: A bounded prototype/local-veto candidate may rewrite the terminal
  multiresolution business view through either of two independent mechanical
  paths. First, the pre-session prototype may supply an eligible direct
  challenger under the existing duration-aware gates. Second, a terminal
  direct override must be withdrawn when maximum-within-identity prototype
  reduction leaves the terminal identity first and the source baseline
  identity second but their top-two margin no longer passes the unchanged
  gate. Both paths additionally require the forced-aligned v2.1 raw-posterior
  piece covering the rewritten source characters to map to the restored or
  challenged identity. Every eligible session-refreshed TitaNet identity for
  that exact piece and its enclosing punctuation phrase must either agree or
  abstain; an eligible conflict vetoes the write. Score- or margin-ineligible
  piece/phrase evidence is an abstention, while a cross-prototype margin failure
  is used only to withdraw the terminal direct override that created it. Every
  accepted write is limited to the intersection of the direct interval and the
  raw-posterior source-character range on the common absolute clock. The
  topology reuses all frozen FR16K gates and may not average prototypes, lower
  a threshold, infer a long local-slot epoch, read transcript meaning, real
  speaker names, known timestamps, reference labels, or correctness judgments.
  Its enabled contracts must be frozen in TOML before generation. Tools may
  verify provenance and mechanical contracts, but no code, script, formula,
  query, notebook, metric, or algorithm may assign correctness, aggregate
  accuracy, rank/select a candidate or parameter, or issue a verdict.
- **FR16O**: A clean session-gallery candidate may retain the existing
  `max_ref_segs` individual TitaNet embeddings for each stable identity instead
  of collapsing them into one centroid. A native v2.1 diar segment is eligible
  only when it passes the existing clean-span duration, confidence, overlap,
  score, and margin gates; its raw local mapping, pre-session registry match,
  and session-refreshed registry match must all name the same active stable ID.
  Eligible segments are retained by the production `confidence * duration`
  quality order with deterministic time/evidence tie breaking. Each current-
  audio query is scored against every retained prototype, reduced by maximum
  only within one stable ID, and then subjected to the unchanged duration-aware
  cross-identity score and top-two margin gates. An incomplete identity gallery,
  source interval mismatch, non-normalized embedding, overlap-contaminated
  prototype, or provenance mismatch rejects generation. The clean-span gates,
  gallery cap, within-ID reduction, and query gates must be frozen in TOML and
  must reuse production values rather than reference-derived tuning. Tools may
  extract embeddings, verify numerical and provenance contracts, and generate
  a reference-free candidate, but no code, script, formula, query, notebook,
  metric, or algorithm may read reference labels, assign correctness,
  calculate or aggregate accuracy, rank/select result candidates or parameters,
  or issue a verdict.
- **FR16P**: A clean-gallery direct identity may override a disagreeing raw
  local mapping only when at least one eligible session-refreshed current-audio
  identity for the exact posterior-bounded piece or enclosing punctuation
  phrase independently selects the same stable ID, and every other eligible
  piece/phrase identity agrees. If both current-audio views abstain, the clean-
  gallery direct identity must still equal the raw local mapping as required by
  FR16N. Any eligible piece/phrase conflict preserves the multiresolution
  baseline. The write remains limited to the forced-aligned intersection of
  the direct interval and raw-posterior piece; it may not propagate through a
  neighbouring turn, an unaligned pause, a long local-slot epoch, or transcript
  semantics. The enable switch must be frozen in TOML. Tools may verify the
  evidence contract and arrange unjudged contexts only; no executable method
  may assign correctness, aggregate accuracy, rank/select a result or parameter,
  or issue a verdict.
- **FR16Q**: When FR16P has no eligible session-refreshed identity but the clean
  gallery's direct interval, exact posterior-bounded piece, and enclosing
  punctuation phrase all select the same stable ID, that identity may override
  top-1 local mapping only if its own native Sortformer output channel is active
  at or above the existing posterior activity threshold for at least the frozen
  one-frame micro-run floor inside the exact piece interval. Any eligible
  session-refreshed piece/phrase conflict, clean-gallery identity conflict,
  missing channel mapping, or insufficient active run preserves the baseline.
  The activity threshold and run floor must be copied from the frozen FR16K
  TOML values; they may not be tuned against reference results. Projection is
  limited to the forced-aligned source range supported by the qualifying piece.
  Tools may verify frame continuity and arrange contexts, but no executable
  method may assign correctness, aggregate accuracy, rank/select a result or
  parameter, or issue a verdict.
- **FR16R**: A gallery multiscale/channel candidate may act without an eligible
  direct-interval identity only when the clean gallery independently selects
  the same stable ID for both the exact posterior-bounded piece and its
  enclosing punctuation phrase, the raw local mapping selects that same ID,
  and that identity's mapped native Sortformer
  channel satisfies the unchanged FR16Q activity/run contract inside the exact
  piece, and every eligible session-refreshed piece/phrase identity either
  agrees or abstains. A clean-gallery scale disagreement, current-audio
  identity conflict, raw-local disagreement, missing channel mapping, or
  insufficient active run preserves the baseline. The write is limited to the
  exact forced-aligned
  piece source range. The switch must be frozen in TOML; no transcript meaning,
  real name, known failure interval, or reference judgment may enter the
  decision. Tools may verify the evidence contract and arrange all changed
  contexts, but no executable method may assign correctness, aggregate
  accuracy, rank/select a result or parameter, or issue a verdict.
- **FR16S**: When exactly one clean-gallery temporal scale selects an eligible
  stable ID and the other scale abstains, a gallery single-scale/channel
  candidate may use that identity only when the raw local mapping agrees, the
  mapped native Sortformer channel satisfies the unchanged FR16Q activity/run
  contract inside the exact posterior piece, and every eligible session-
  refreshed piece/phrase identity agrees or abstains. If both clean-gallery
  scales are eligible they must follow FR16R; a disagreement between them is a
  veto, not an abstention. Raw-local disagreement, any current-audio conflict,
  missing channel mapping, or insufficient activity preserves the baseline.
  Projection, TOML freezing, source independence, and the prohibition on
  executable correctness judgment are identical to FR16R.
- **FR16T**: A current multiscale/channel candidate may override an exact
  posterior-piece source range when the raw local mapping, eligible session-
  refreshed piece identity, eligible session-refreshed enclosing-phrase
  identity, and mapped native Sortformer channel all select the same stable ID.
  Every eligible clean-gallery piece/phrase identity must agree or abstain; a
  clean-gallery conflict vetoes the write. The unchanged FR16Q activity/run
  contract applies, and the baseline must actually differ in the projected
  range. No direct-interval eligibility is required because two current-audio
  voiceprint resolutions and the orthogonal diar channel already agree.
  Projection is exact, the switch is TOML-frozen, no threshold is added, and
  executable product-result judgment remains prohibited.
- **FR16U**: When exactly one session-refreshed temporal scale selects an
  eligible stable ID and the other current scale abstains, a current single-
  scale/channel candidate may use that identity only if the raw local mapping
  and mapped native Sortformer channel agree, at least one clean-gallery
  piece/phrase identity independently selects that same ID, and every other
  eligible clean-gallery identity agrees or abstains. If both current scales are eligible
  they must follow FR16T; disagreement is a veto. All FR16T activity,
  baseline-disagreement, exact-projection, TOML, threshold, and evaluation-
  governance constraints remain unchanged.
- **FR16V**: A sustained raw-local candidate may restore the mapped stable ID
  on an exact posterior-bounded piece when that identity's native Sortformer
  channel remains above the unchanged activity threshold for at least FR16K's
  unchanged `minimum_sustained_run_sec`, the baseline differs, and every
  eligible session-refreshed or clean-gallery piece/phrase identity agrees or
  abstains. Any eligible voiceprint conflict, missing channel mapping, or short
  channel run preserves the baseline. The run floor must be copied into and
  frozen by TOML and mechanically checked against the FR16K policy; it may not
  be tuned from review results. Projection is exact and executable result
  evaluation remains prohibited.
- **FR16W**: A dual-registry multiscale voiceprint candidate may override an
  exact posterior-piece range without raw-local agreement only when the
  session-refreshed exact piece, session-refreshed enclosing phrase, clean-
  gallery exact piece, and clean-gallery enclosing phrase all independently
  pass their unchanged duration-aware gates and select the same stable ID. Any
  abstention or identity disagreement among the four voiceprint views preserves
  the baseline. No channel-probability threshold is lowered or added. The
  baseline must differ, projection is exact, the switch is TOML-frozen, and no
  executable method may judge product correctness or select the result.
- **FR16X**: A dominant raw-micro candidate may restore a mapped stable ID on
  an FR16K forced-aligned micro piece only when every native frame in that
  piece has exactly one channel at or above the unchanged activity threshold,
  that sole channel is also frame top-1, and it equals the micro piece's raw
  local slot. The declared frame range/count must match the frozen frame table,
  and the baseline must differ. Any multi-channel frame, top-1 disagreement,
  inactive frame, mapping gap, frame-contract mismatch, or overlap with an
  accepted different-identity overlay preserves the baseline. No voiceprint
  window is fabricated for sub-minimum audio, no threshold is added, projection
  is exact, the switch is TOML-frozen, and executable result judgment remains
  prohibited.
- **FR16Y**: A raw-authoritative business candidate must preserve an exact
  source range from the frozen production business replay when that range has
  a known stable ID and its decision reason is explicitly allowed by TOML.
  The first candidate allows only `sole_diar_support`; session-refreshed or
  clean-gallery voiceprints may fill ranges that remain unknown or unsupported
  but may not relabel an authoritative Sortformer range. Raw and enhanced tracks
  must reconstruct identical immutable ASR text per `text_id`, use the same
  active identity set and common-clock alignment metadata, and retain source
  hashes. Projection is by exact source character range through forced
  alignment. The allowed reason set is categorical, not review-tuned, and no
  executable method may judge or rank the product result.
- **FR16Z**: An exact-phrase dual-gallery candidate may rewrite only one
  punctuation-bounded, forced-aligned source range when the session-refreshed
  TitaNet registry and the frozen clean multi-prototype gallery independently
  produce eligible decisions for that exact phrase and select the same stable
  identity. The phrase must contain no eligible direct anchor for another
  identity, the baseline must disagree, and the write may not extend outside
  the exact source-character range. Raw local-slot agreement is not required:
  this path exists specifically for a short, independently voice-identified
  interjection that Sortformer leaves inside another speaker's active run.
  Either registry abstaining or disagreeing, a competing direct anchor, an
  overlap with an accepted different-identity overlay, or any source/time-base
  mismatch preserves the baseline. Both registries reuse the already frozen
  duration, score, and margin gates; the boolean switch is TOML-owned and no
  transcript phrase, reference label, known timestamp, or executable product
  judgment may enter generation or promotion.
- **FR16ZA**: A native-channel-guarded dual-gallery candidate strengthens
  FR16Z with independent Sortformer evidence. The stable identity selected by
  both TitaNet galleries must map to a native channel that is top-1 in at least
  one frame inside the exact phrase. The phrase is rejected if any different
  channel has a contiguous top-1 run at or above the inherited FR16K activity
  threshold for at least the inherited sustained-run floor. These values must
  be read from and mechanically equal the frozen TOML policy; no new numerical
  gate is permitted. All FR16Z exact-projection, direct-anchor, conflict,
  provenance, and no-executable-judgment requirements remain mandatory.
- **FR16ZB**: A known-conflict/regular-anchor guard further strengthens FR16ZA.
  An overlay must replace at least one known baseline identity different from
  the selected identity; a phrase whose baseline range is only `unknown` or
  already selected is evidence-only. When any eligible direct anchor exists in
  the phrase, at least one agreeing anchor must be regular-duration; a phrase
  supported only by short direct anchors is not expanded. This is a categorical
  use of existing decision reasons and introduces no score, duration, activity,
  or reference-derived parameter. All FR16ZA contracts remain mandatory.
- **FR16ZC**: A robust clean-gallery evidence view must score each query against
  every frozen clean prototype of an identity and aggregate the highest half by
  arithmetic mean. The half count is derived from the complete TOML-owned
  gallery size; no candidate-result-tuned count is allowed. A complete gallery
  remains mandatory, embeddings must be L2-normalized and dimensionally equal,
  source intervals/status fields remain unchanged, and the existing
  duration-aware score/margin gates are applied only after aggregation. The
  tool reads no transcript reference and emits no correctness, accuracy,
  ranking-between-candidates, or promotion field. Product use still requires
  complete manual contextual semantic review.
- **FR16ZD**: A VAD-bounded relative-top-1 phrase candidate may expose a
  low-volume Sortformer transition without changing the native `0.5` activity
  threshold. Inside one punctuation phrase, only frames covered by the frozen
  VAD speech track participate. A candidate piece requires one unchanged native
  top-1 channel for at least the FR16J `minimum_sustained_run_sec`; the run and
  piece floors must be copied from and mechanically equal to FR16J. The exact
  piece audio must independently pass the unchanged duration-aware TitaNet
  gates against both the session-refreshed registry and the robust complete
  clean gallery. Both TitaNet views and the local-slot mapping must select the
  same stable identity, and the baseline must contain a different known
  identity in the exact source range. Any VAD gap, top-1 change, registry
  abstention/disagreement, mapping disagreement, source/time-base mismatch, or
  overlapping different-identity overlay preserves the baseline. A qualifying
  write is limited to its forced-aligned source-character range; it cannot
  propagate through a neighbouring phrase or local-slot epoch. All switches
  and inherited values are TOML-owned. No reference label, known timestamp,
  transcript phrase, executable product judgment, result aggregation, or
  automated candidate/parameter selection may enter generation or promotion.
- **FR16ZE**: A local-channel island candidate may recover a bounded speaker
  interruption that punctuation segmentation omits or truncates. Within one
  continuous frozen VAD interval, the candidate channel must form a complete
  `A-B-A` top-1 island: both immediately adjacent runs use the same different
  channel, and all three runs meet the unchanged FR16J sustained-run floor.
  TitaNet must query the complete B-run audio rather than its shorter text
  projection. The session-refreshed registry, robust clean gallery, and frozen
  local-slot mapping must all pass unchanged gates and select one identity.
  Only forced-alignment units wholly contained in the B run may be rewritten,
  and the exact baseline source range must contain a different known identity.
  Any VAD boundary, incomplete alignment unit, registry abstention or conflict,
  mapping conflict, non-bracketed transition, source mismatch, or overlapping
  different-identity overlay preserves the baseline. The topology may not pad
  the query with A-channel audio, infer a long slot epoch, lower the native
  activity threshold, or use reference content, known timestamps, executable
  correctness judgment, result aggregation, or automated candidate/parameter
  selection. All switches and inherited run floors are TOML-owned.
- **FR16ZF**: The production GPU VAD must apply the TOML-owned
  `speech_pad_ms` to every published speech interval on the common sample time
  base. Padding extends both boundaries, clamps the start to sample zero and
  the end to the processed-audio horizon, and must preserve positive,
  monotonic, non-overlapping intervals. The endpoint state machine and Silero
  probabilities remain unchanged. A mechanical test must prove that identical
  audio and configuration differing only in `speech_pad_ms` produce identical
  endpoint count and the exact configured boundary extension, including stream
  start/end clipping. Product impact is judged only through manual contextual
  semantic review after a real WebSocket run; no executable result evaluation
  or parameter selection is permitted.
- **FR16ZG**: A short VAD-utterance candidate may expose complete interjections
  that are filtered by punctuation phrase minimum-character rules. One frozen,
  padded VAD interval must be at least the inherited FR16J run floor and no
  longer than the existing TitaNet short-duration boundary. Every native frame
  inside it must have the same top-1 local channel. The complete VAD audio must
  independently pass the unchanged session-refreshed and robust-gallery
  TitaNet gates; both views and the frozen local-slot mapping must select one
  identity. Only forced-alignment units wholly contained in that same VAD
  interval may challenge a different known baseline identity. A local-channel
  change, missing or partial alignment, registry abstention or conflict,
  mapping conflict, unknown-only baseline, duration violation, time-base
  mismatch, or overlapping different-identity overlay preserves the baseline.
  The rule may not lower punctuation, VAD, Sortformer, or voiceprint thresholds,
  pad beyond the TOML VAD interval, use reference content or known timestamps,
  or perform executable result evaluation, aggregation, ranking, selection, or
  verdict.
- **FR16ZH**: A padded-VAD edge-run candidate may expose a real speaker handoff
  that punctuation filtering or an unpadded endpoint hides. One continuous VAD
  interval must contain at least two native top-1 runs. Only the first or last
  run is eligible, and both that run and its immediate neighbour must meet the
  unchanged FR16J sustained-run floor. The eligible run must fit wholly inside
  the existing TOML-owned TitaNet maximum embedding window so its complete
  audio is queried. Session-refreshed TitaNet, robust-gallery TitaNet, and the
  frozen local-slot mapping must pass unchanged gates and select one identity.
  Only forced-alignment units wholly contained in the eligible run may rewrite
  a different known baseline identity. A single-run VAD interval, internal
  run, short adjacent run, partial alignment, registry abstention or conflict,
  mapping conflict, unknown-only baseline, time-base mismatch, or overlapping
  different-identity overlay preserves the baseline. The rule may not use
  reference content or known timestamps, lower or add a numerical inference
  gate, or perform executable product evaluation, aggregation, ranking,
  candidate/parameter selection, or verdict.
- **FR16ZI**: A low-activity guard strengthens FR16ZH. Every frame in the
  eligible edge run must remain strictly below the unchanged FR16J
  `frame_activity_threshold`; the threshold must be copied from TOML and
  mechanically equal the FR16J value. This limits the rewrite to a sustained
  relative top-1 transition that native absolute-threshold postprocessing
  cannot publish. All FR16ZH multi-run, edge, adjacent-run, complete-query,
  dual-TitaNet, local-map, forced-alignment, known-conflict, provenance, and
  no-executable-judgment requirements remain mandatory.
- **FR16ZJ**: An active padded-VAD edge-handoff candidate may restore the mapped
  stable identity when both the eligible edge run and its immediate adjacent
  run remain at or above the unchanged FR16J `frame_activity_threshold` for
  every frame and both satisfy the unchanged sustained-run floor. The edge run
  must fit the TOML-owned maximum embedding window, and only wholly contained
  forced-alignment units may challenge a different known baseline identity.
  Session-refreshed and robust-gallery TitaNet use unchanged duration-aware
  gates as an orthogonal conflict veto: when both select the same stable
  identity different from the mapped edge channel, the baseline is preserved.
  Either view abstaining, or the two views disagreeing, cannot erase the active
  Sortformer handoff. A single-run VAD, internal run, inactive frame, short
  adjacent run, agreed dual-voiceprint conflict, partial alignment,
  unknown-only baseline, source/time mismatch, or overlapping
  different-identity overlay preserves the baseline. No reference content,
  known timestamp, new numerical gate, executable product evaluation,
  aggregation, ranking, candidate/parameter selection, or verdict is allowed.
- **FR16ZK**: A complete local-phrase candidate may challenge one known
  baseline identity only for an entire finalized punctuation phrase. The
  phrase must fit wholly inside one padded-VAD interval and the existing
  TOML-owned TitaNet embedding window; every native frame whose centre lies in
  the phrase interval must have one identical top-1 local channel. Every
  baseline fragment in the exact phrase source range must carry the same known
  identity, and that identity must differ from the frozen local-slot mapping.
  Session-refreshed and robust-gallery TitaNet evaluate the exact complete
  phrase under unchanged duration-aware gates. Either view selecting a known
  identity different from the mapped local identity vetoes the write. An
  otherwise abstaining view also vetoes when its raw top-ranked identity is the
  uniform known baseline identity; sub-gate ranks may only preserve the
  baseline and never supply a positive rewrite identity. An agreeing view or
  abstention without that baseline support does not erase the complete
  Sortformer phrase.
  The write replaces the exact whole phrase source range and never a prefix,
  suffix, isolated alignment unit, adjacent phrase, mixed/unknown baseline, or
  phrase containing a native channel transition. All duration and frame
  contracts must be copied from existing TOML values and checked for exact
  parity. No reference content, known timestamp, identity pair, new numerical
  gate, executable product evaluation, aggregation, ranking,
  candidate/parameter selection, or verdict is allowed.
- **FR16ZL**: A bracketed unknown-phrase candidate may fill an entire finalized
  punctuation phrase only when every baseline fragment in its exact source
  range is unknown and the immediately adjacent source fragments on both sides
  exist, are known, and carry one identical stable identity. The phrase must
  satisfy FR16ZK's complete padded-VAD containment, inherited duration bounds,
  and one-top-1-local-channel contract; the frozen local-slot mapping must equal
  the two adjacent identities. Either unchanged TitaNet view selecting a
  different identity vetoes the fill. The phrase must not cross an ASR source
  boundary, skip another unknown fragment, use non-adjacent context, or project
  less than the complete phrase. No transcript meaning, reference content,
  known timestamp, new numerical gate, executable result judgment,
  aggregation, ranking, candidate/parameter selection, or verdict is allowed.
- **FR16ZM**: A bracketed known-conflict phrase may replace one uniform known
  identity only when the immediately preceding and following finalized
  punctuation phrases in the same ASR source each carry another one identical
  uniform known identity, and the frozen local-slot mapping for every candidate
  phrase frame equals that bracket identity. FR16ZK's complete padded-VAD containment,
  inherited duration bounds, one-top-1-local-channel requirement, exact whole-
  phrase projection, and either-view eligible different-identity TitaNet veto
  remain mandatory. Missing, mixed, unknown, non-adjacent, cross-source, or
  disagreeing phrase brackets;
  a mapped channel differing from the bracket; mixed phrase identity; or any
  eligible voiceprint conflict preserves the baseline. No transcript meaning,
  reference content, known timestamp, identity pair, new numerical gate,
  executable result judgment, aggregation, ranking, candidate/parameter
  selection, or verdict is allowed.
- **FR16ZN**: An expanded-local-run phrase candidate may challenge one uniform
  known baseline identity only when the exact complete punctuation phrase
  satisfies FR16ZK and is strictly contained in one maximal contiguous native
  top-1 run for the same local channel. The acoustic query must use that whole
  run, clipped only by the containing padded-VAD interval, and the whole query
  must fit the unchanged FR16J sustained floor and TOML-owned TitaNet maximum
  embedding window. It may not crop a longer run to fit. The session-refreshed
  registry and robust clean gallery must both pass their unchanged duration-
  aware gates, select the same stable identity, and equal the frozen local-slot
  mapping. Every baseline fragment in the exact phrase source range must carry
  one different known identity. Only the exact phrase source range may be
  projected; query expansion never expands the write range. A missing or
  non-maximal run, query/VAD mismatch, overlong run, registry abstention or
  disagreement, local-map disagreement, mixed or unknown baseline, source/time
  mismatch, or overlapping different-identity projection preserves the
  baseline. No transcript meaning, reference content, known timestamp,
  identity pair, new numerical gate, executable result judgment, aggregation,
  ranking, candidate/parameter selection, or verdict is allowed.
- **FR16ZO**: A bounded local-run voiceprint override may replace one uniform
  known phrase identity only when the exact complete punctuation phrase
  satisfies FR16ZK and lies in one contiguous native top-1 local run. The
  acoustic query uses the whole run when it fits the existing TOML embedding
  window; otherwise it uses one deterministic maximum-length window centred on
  the phrase and clamped within that run while still containing the entire
  phrase. Session-refreshed and robust clean-gallery TitaNet must both pass
  unchanged duration-aware gates and select the same stable identity. That
  identity must differ from both the uniform baseline identity and the frozen
  mapping of the native local channel. This rule therefore acts only when two
  voiceprint registries jointly contradict both existing speaker views; local
  agreement can never authorize it. The native Sortformer channel mapped to
  the selected voiceprint identity must also be active inside the query for at
  least the unchanged FR16J sustained-run floor; top-1 ownership is not
  required, so this is independent secondary-channel confirmation. Projection
  replaces only the exact whole phrase. Registry abstention or disagreement,
  identity agreement with either baseline or local mapping, missing sustained
  selected-identity channel support, incomplete phrase/run containment, mixed
  or unknown baseline, source/time mismatch, or overlapping different-
  identity projection preserves the baseline. Window placement, duration
  bounds, and all switches are TOML-owned. No transcript meaning, reference content,
  known timestamp, identity pair, new numerical gate, executable result
  judgment, aggregation, ranking, candidate/parameter selection, or verdict is
  allowed.
- **FR16ZQ**: A temporally stratified clean gallery must preserve every FR16O
  prototype eligibility gate and the unchanged complete per-identity gallery
  size. Eligible prototypes for each identity are ordered by absolute common-
  clock start, divided into exactly the TOML-owned prototype-count number of
  non-empty contiguous rank strata, and the existing `confidence * duration`
  quality maximum is selected independently inside each stratum. This replaces
  global quality truncation with deterministic full-session coverage without
  changing an eligibility threshold, identity mapping, embedding interval, or
  gallery size. Initial-registry, terminal-registry, and raw-local identity
  agreement, overlap rejection, complete gallery enforcement, normalized
  embeddings, source hashes, and robust top-half scoring remain mandatory.
  Time strata may not use transcript content, reference labels, known error
  intervals, or candidate results. Gallery comparison and promotion still
  require manual contextual semantic review; no executable mechanism may
  judge correctness, aggregate accuracy, rank/select a result or parameter, or
  issue a verdict.
- **FR16ZR**: Bounded local-run voiceprint queries must use the committed
  production `orator.toml` maximum embedding window rather than an experiment-
  local shorter cap. The FR16ZO query placement, dual-registry identity
  conflict, selected-identity sustained native-channel support, exact complete-
  phrase projection, and all abstention rules remain unchanged. The complete
  phrase and bounded query maxima must each be mechanically equal to
  `[speaker].max_embed_window_sec = 10.0`; a longer same-top-1 run uses the same
  deterministic phrase-centred crop. Selected-identity native support may use
  either FR16ZO's unchanged sustained active-channel contract or categorical
  top-2 unanimity across every query frame. Top-2 support may not tolerate a
  missing/disagreeing frame or introduce a probability threshold. Score and
  margin thresholds, gallery, local mapping, VAD, frame evidence, and baseline remain frozen. This is a
  configuration-parity correction, not a result-derived duration search. No
  transcript meaning, reference label, known error interval, executable result
  judgment, aggregation, ranking, candidate/parameter selection, or verdict is
  allowed.
- **FR16ZS**: A complete-source voiceprint projection may extend an accepted
  FR16ZR phrase write only when that phrase is the sole indexed punctuation
  phrase in its ASR source, every non-separator source character has a valid
  forced-alignment interval wholly contained in the unchanged FR16ZR query,
  and every baseline fragment across the complete source has one uniform known
  identity. The dual-registry conflict, selected-identity sustained or
  unanimous-top-2 native support, VAD, local-run, production-window, and source
  hash contracts remain unchanged. The write replaces the complete ASR source
  or abstains; it may not append a neighbouring source, skip an indexed phrase,
  project a partially aligned character, or infer a boundary from transcript
  meaning. No reference content, known interval, identity pair, new numerical
  gate, executable result judgment, aggregation, ranking, candidate/parameter
  selection, or verdict is allowed.
- **FR16ZT**: A complete edge-contribution candidate may replace FR16ZJ's
  disconnected alignment-unit writes only with one or more adjacent complete
  punctuation clauses. Every non-separator character in each clause must have
  forced-alignment evidence wholly inside one unchanged active padded-VAD
  terminal edge run; the clauses must be adjacent in the ASR source and the complete group
  must have one uniform known baseline identity. The edge and adjacent runs
  retain the inherited `0.4 s` sustained floor, `0.5` all-frame activity gate,
  terminal edge position, stable local mapping, and dual-TitaNet agreed-different veto.
  The complete group is projected to the mapped raw-local identity or the
  candidate abstains. Partial clauses, unaligned characters, internal runs,
  source gaps, mixed baseline identities, and disconnected characters are
  forbidden. Punctuation text is structural only; no reference content,
  speaker name, known interval, executable result judgment, aggregation,
  ranking, candidate/parameter selection, or verdict is allowed.
- **FR16ZU**: A secondary-channel complete-phrase candidate may use a
  TitaNet result below the regular score gate only when the session registry
  and robust clean gallery have the same top-ranked active identity, each
  retains the existing duration-class margin, and the mapped raw Sortformer
  channel for that identity remains continuously active at or above the
  inherited `0.5` threshold for the inherited `0.4 s` floor inside the exact
  phrase. The selected identity must differ from one uniform known baseline
  identity, and its channel must be non-top-1 on at least one phrase frame so
  this path cannot duplicate the authoritative local-run path. Phrase duration,
  punctuation, visible-character, TitaNet crop, frame clock, local mapping, and
  exact complete-phrase projection are TOML-owned and frozen before generation.
  A score gate may be bypassed only by the orthogonal sustained raw-channel
  contract; the existing margin may not be lowered. No reference content,
  known interval, speaker pair, executable result judgment, aggregation,
  ranking or selection of candidates/parameters, or verdict is allowed.
- **FR16ZV**: A bracketed local-churn contribution candidate may consolidate a
  bounded region only when two stable native top-1 runs for the same local
  channel surround one or more different-channel runs in one ASR source on the
  common clock. Both outer runs must satisfy the inherited `0.5` all-frame
  activity threshold and `0.4 s` sustained floor. The query must contain the
  complete intervening run region plus at least `0.4 s` of each outer run and
  fit the production `10.0 s` embedding window. Session-registry and robust-
  gallery TitaNet must independently pass the unchanged regular `0.55/0.04`
  score/margin gates, agree with each other, and select the frozen identity
  mapped from the outer local channel. Projection is limited to complete
  punctuation phrases intersecting the intervening region, with every
  non-separator character aligned inside the query. Each projected phrase must
  carry one uniform known baseline identity different from the outer identity,
  and the session and robust phrase-level TitaNet rankings must both place the
  outer identity first. Phrase-level eligibility gates may abstain, but their
  top-ranked active identity may not disagree with the outer evidence. A single
  different local channel is additionally vetoed when both phrase-level
  TitaNet views pass unchanged gates and agree with that channel's frozen
  mapped identity. A complete-clause-group expansion may include short clauses
  omitted by the indexed phrase set only when every intervening stable run has
  a lower mean native top-1 margin than both outer runs and no indexed phrase
  has the same different top-ranked active identity in both TitaNet views. The
  native `active_count` must equal one on every intervening run frame, so an
  overlapping real contribution is a cannot-link boundary and is never
  consolidated. The
  expanded write contains only adjacent complete punctuation clauses whose
  non-separator characters all align inside the unchanged query. Missing
  brackets, weak outer activity, an overlong or
  cross-source query, registry disagreement or abstention, incomplete phrase
  alignment, mixed or unknown phrase ownership, phrase-rank disagreement,
  source gaps, overlapping different selected identities, or no actual speaker
  conflict preserves the baseline. Every switch, threshold,
  duration, and projection rule is TOML-owned and frozen before generation.
  No reference text, speaker name, known interval, identity pair, executable
  result judgment, accuracy aggregation, candidate or parameter ranking or
  selection, or verdict is allowed.
- **FR16ZW**: A maximal multi-slot envelope candidate may consolidate a long
  contribution across ASR source boundaries only when the frozen native frame
  stream has stable runs for one identical outer local channel on both sides
  and at least two distinct different local channels inside. For each left run,
  the farthest qualifying right run whose complete interior plus the inherited
  `0.4 s` support from each side fits the production `10.0 s` speaker window is
  selected deterministically. Remaining window capacity is divided between
  available left and right outer audio, then consumed on either side if the
  other side has no audio. Both outer runs retain the inherited `0.5` all-frame
  activity and `0.4 s` sustained contracts, and their combined duration must
  exceed the combined duration of all different-channel inner runs. Session-
  registry and robust-gallery TitaNet must independently pass the unchanged
  regular `0.55/0.04` gates, agree, and select the outer channel's frozen
  identity. The different inner channel with the greatest total stable-run
  duration is determined without a result score; if session and robust phrase
  rankings both place its mapped identity first on any intersecting indexed
  phrase, the envelope is vetoed as a supported inner contribution. Writes may
  cross ASR boundaries only as separate adjacent complete punctuation clauses;
  every non-separator character must align inside the one query on the common
  clock. All projected clauses must also be wholly contained in one identical
  frozen VAD segment from the comprehensive timeline; a projection split by a
  VAD endpoint or silence gap preserves the baseline even when the longer
  speaker query agrees. Conflicting accepted envelopes both abstain. No reference text,
  speaker name, known interval, identity pair, new numerical threshold,
  executable result judgment, accuracy aggregation, candidate or parameter
  ranking or selection, or verdict is allowed.
- **FR16ZX**: A VAD-complete contribution candidate may consolidate local-slot
  fragmentation only inside one frozen comprehensive-timeline VAD segment no
  longer than the inherited production `10.0 s` speaker window. At least two
  distinct native local channels must each provide an inherited stable run
  with all-frame activity at least `0.5` for at least `0.4 s`. One local
  channel must have strictly more total stable-run duration than all other
  channels combined; a tie or non-majority abstains. Session-registry and
  robust-gallery TitaNet must independently pass the unchanged regular
  `0.55/0.04` gates on the complete VAD query, agree, and select that dominant
  channel's frozen identity. If both phrase galleries rank the same different
  active identity first on any indexed phrase wholly contained in the VAD,
  the contribution is vetoed regardless of phrase duration eligibility.
  Every individual clause proposed for a write must additionally pass the
  unchanged duration-class gate in both the session and robust TitaNet
  galleries, and both clause identities must equal the complete-VAD identity.
  The baseline clause must already contain that selected identity as an anchor.
  Every contiguous baseline fragment inside the clause that is unknown or has
  another identity must independently pass both galleries on its exact forced-
  alignment interval with the same selected identity; one unavailable,
  abstaining, disagreeing, or different fragment preserves the whole clause.
  Writes contain only those independently agreed adjacent complete punctuation
  clauses per ASR source whose every non-separator character aligns inside the
  same VAD segment and whose baseline identity actually conflicts.
  Conflicting accepted contributions both abstain. No reference text, speaker
  name, known interval, identity pair, new numerical threshold, executable
  result judgment, accuracy aggregation, candidate or parameter ranking or
  selection, or verdict is allowed.
- **FR16ZY**: A complete-ASR-source unanimous-phrase candidate may use one
  finalized ASR source as voiceprint context without projecting the complete
  source. The source must fit the inherited production `10.0 s` window and
  every non-separator source character must have forced-alignment evidence
  inside that source interval. It must contain at least one indexed punctuation
  phrase, and every indexed phrase must be wholly contained in one frozen VAD
  segment. Session-registry and robust-gallery TitaNet must independently pass
  the unchanged duration-class gates on the complete source, agree, and select
  one active identity. That identity's frozen native local channel must have at
  least one inherited `0.5` all-frame, `0.4 s` stable run inside the source.
  Both phrase galleries must then rank that same identity first for every
  indexed source phrase; one unavailable phrase, top-rank disagreement, or
  different top rank preserves the source. A projected phrase must already
  have the selected identity on both its first and last overlapping baseline
  fragments, so a source crop cannot absorb a handoff at either phrase edge.
  Writes project only complete indexed phrases with an internal actual
  baseline identity conflict. Unindexed text,
  source gaps, incomplete alignment, and conflicting overlays preserve the
  baseline. No reference text, speaker name, known interval, identity pair,
  new numerical threshold, executable result judgment, accuracy aggregation,
  candidate or parameter ranking or selection, or verdict is allowed.
- **FR16ZZ**: Equal-overlap diarization candidates in the business-speaker
  projection MUST use one explicit TOML-selected tie policy. The existing
  `shorter_span` policy remains reproducible. A `higher_confidence` candidate
  MUST compare diarization confidence before total segment length, retain the
  shorter segment only as a deterministic secondary tie-break, preserve every
  raw evidence track, and expose the unchanged selected/rejected candidate
  audit. This candidate addresses only equal-overlap arbitration; it MUST NOT
  smooth time, infer from transcript content, or alter Sortformer/TitaNet
  evidence. Its full-session changed contexts MUST be read manually before it
  may replace the frozen candidate. No executable mechanism may assign result
  labels, aggregate accuracy, rank/select the policy, or issue a verdict.
- **FR16AAA**: A primary-speaker projection candidate MUST derive one
  non-overlapping native Sortformer top-1 track on the immutable session clock.
  Frames are eligible only inside frozen comprehensive-timeline VAD speech and
  at or above the inherited native `0.5` activity threshold. A same-slot run
  shorter than the inherited FR16J `0.4 s` sustained floor MUST abstain; it may
  not inherit either neighbour. Eligible runs use the frozen local-to-global
  identity mapping and the existing business projection without changing ASR,
  VAD, alignment, activity-diarization, voiceprint, or frame evidence. The
  candidate may then receive only exact source ranges whose decision reason
  belongs to a TOML allowlist of previously completed, manually retained
  orthogonal voiceprint reviews. The allowlist MUST NOT include baseline direct
  voiceprint, generic neighbour propagation, transcript values, real speaker
  names, known timestamps, or reference-derived conditions. Every changed
  context and the complete candidate require manual contextual semantic review;
  no executable mechanism may evaluate, aggregate, rank/select, or issue a
  product verdict.
- **FR16AAB**: The native primary-speaker track MUST be evaluated as bounded
  arbitration evidence before it can replace activity diarization. Raw
  activity diarization and primary top-1 MUST remain separate typed tracks on
  the common time base. Primary evidence may change a business attribution
  only when at least two activity candidates have the same maximum overlap
  with the exact projected interval and exactly one tied candidate has the
  same resolved identity as the maximum-overlap primary segment. It MUST NOT
  add text boundaries, fill gaps, override a unique activity winner, change
  identity enrollment, or participate in support coverage. An absent,
  unknown, ambiguous, or nonmatching primary identity MUST fall back to the
  existing TOML-selected activity tie policy. The complete changed contexts
  and any promoted full candidate require manual contextual semantic review;
  no executable mechanism may evaluate or select the result.
- **FR16AAC**: A primary-aligned-island candidate MAY reassign only complete
  forced-alignment units wholly contained by one VAD-bounded primary run when
  production activity supports the same mapped identity for the inherited
  `0.4 s` floor and robust clean-gallery TitaNet independently selects that
  identity under the existing duration-class gates. The affected baseline
  source range MUST have one known conflicting identity. The candidate MUST
  abstain on missing alignment or activity, TitaNet abstention or disagreement,
  baseline agreement, a conflict with a TOML-allowlisted reviewed overlay, or
  source/time mismatch. It MUST NOT
  inspect reference transcript values, speaker names, known timestamps, review
  results, or introduce a fitted threshold. Every changed context and the full
  candidate require manual contextual semantic review; no executable mechanism
  may evaluate, aggregate accuracy, rank/select, or issue a product verdict.
- **FR16AAD**: A complete-phrase cross-prototype candidate MAY reuse an accepted
  `cross_prototype_margin_veto` challenge only when its exact source range is
  identical to one complete frozen punctuation phrase. The challenge's initial,
  source, mapped local identity, and selected identity MUST agree; its terminal
  direct override MUST differ. The current baseline range MUST contain one
  known conflicting identity and MUST NOT intersect a TOML-allowlisted reviewed
  overlay. Partial phrases, merged adjacent challenges, unknown or mixed
  baseline identity, source/time mismatch, and overlapping proposals MUST
  abstain. The rule MUST NOT inspect reference text values, speaker names,
  known timestamps, review results, or introduce a score or duration threshold.
  Every changed context and the full candidate require manual contextual
  semantic review; no executable mechanism may evaluate, aggregate accuracy,
  rank/select, or issue a product verdict.
- **FR16AAE**: A relative-top-1 phrase expansion MAY extend an already accepted
  FR16ZD piece to its complete enclosing punctuation phrase only when the piece
  identity, frozen local-slot mapping, and both piece-level TitaNet views have
  already passed all FR16ZD gates. The complete phrase's session-registry and
  robust-gallery views are boundary vetoes only: both MUST be available, rank
  that already selected identity first among active identities, and exceed the
  existing TOML margin; they MUST NOT select a new identity or bypass the
  duration-class absolute score gate. The piece MUST be wholly contained by the
  phrase. Every baseline phrase character MUST have a known identity, at least
  one MUST conflict, and no character may carry a TOML-protected reviewed
  overlay. Missing evidence, registry disagreement, insufficient margin,
  source/time mismatch, or overlapping expansions MUST abstain. No reference
  value, name, timestamp, review result, or new threshold is permitted, and all
  changed/full result judgment remains manual contextual semantic review only.
- **FR16AAF**: A complete-VAD-phrase voiceprint challenge MAY override a stable
  raw local-slot mapping only when one short padded VAD utterance contains one
  unique complete punctuation phrase within one native frame of boundary
  quantization. The VAD query and the exact phrase query MUST each pass the
  unchanged duration-class gates in both the session registry and robust clean
  gallery, and all four views MUST select the same active identity. Because the
  rule challenges stable raw separation, both outer VAD views MUST additionally
  meet the already configured regular-score floor; this reuses the existing
  floor and MUST NOT introduce a new threshold. That
  identity MUST differ from the single raw local-slot mapping and from the one
  uniform known identity currently covering the complete phrase. A mixed or
  unknown baseline phrase, more than one enclosing phrase, missing alignment,
  any registry abstention or disagreement, a protected reviewed overlay,
  source/time mismatch, or overlapping challenge MUST preserve the baseline.
  The tolerance MUST be one frame derived from frozen metadata, not a fitted
  time value. The rule MUST NOT inspect reference text, names, known timestamps,
  review results, or introduce a score/duration threshold. Every changed/full
  result judgment remains manual contextual semantic review only.
- **FR16AAG**: A contextual VAD phrase challenge MAY project one complete
  punctuation phrase inside a longer padded VAD interval when one primary raw
  local run covers the complete phrase within one derived native frame and its
  mapped identity differs from the proposed identity. The VAD session, VAD
  robust, phrase session, and phrase robust views MUST all top-rank the same
  active identity with the existing margin floor; at least one outer VAD view
  MUST also meet the existing regular-score floor. The current complete phrase
  MUST have one uniform known conflicting identity and MUST NOT contain a
  protected reviewed overlay. Missing or multiple containing VAD intervals,
  missing or multiple covering primary runs, view abstention/disagreement,
  inadequate existing margin/outer score, source/time mismatch, or overlapping
  proposals MUST abstain. No new numerical threshold, reference value, name,
  known timestamp, or executable result judgment is permitted.
- **FR16AAH**: An edge-anchor-trimmed phrase challenge MAY relabel only the
  source-character remainder of one punctuation phrase when eligible direct
  voiceprint evidence for a different identity occupies exactly one phrase
  edge. The competing anchor overlap MUST be contiguous from that edge, and
  the forced-aligned remainder MUST be separated from the anchor by at least
  one native frame derived from the frozen frame table. The complete phrase's
  session-registry and robust-gallery views MUST both pass the unchanged
  duration-class score and margin gates and select the same active identity.
  Every known baseline identity in the remainder MUST conflict with that
  identity, at least one known identity MUST exist, and no remainder character
  may carry a TOML-protected reviewed overlay. Anchors on both edges, an
  interior anchor, an agreeing anchor, missing or overlapping alignment,
  registry abstention/disagreement, inadequate derived separation, source/time
  mismatch, or overlapping proposals MUST preserve the baseline. The write
  MUST NOT cover the competing anchor or expand beyond the original phrase.
  No transcript phrase, reference value, name, known timestamp, review result,
  new numerical threshold, or executable result judgment is permitted.
- **FR16AAI**: An adjacent-subminimum-clause envelope MAY combine exactly two
  source-adjacent complete punctuation clauses from one aligned ASR item only
  when each clause independently has at least one positive-duration alignment
  unit and remains below the existing punctuation-phrase minimum duration,
  while their combined first-to-last aligned span falls within the existing
  punctuation-phrase minimum and maximum durations. The envelope's session-
  registry and robust-gallery views MUST both pass the unchanged duration-class
  score and margin gates and select the same active identity. Its exact source
  range MUST currently have one uniform known conflicting identity and MUST NOT
  contain a TOML-protected reviewed overlay. Missing alignment, an independently
  eligible constituent clause, evidence abstention/disagreement, mixed or
  unknown baseline identity, overlapping accepted envelopes, source/time
  mismatch, or a protected range MUST preserve the baseline. The pair count is
  categorical and TOML-owned; no transcript phrase, reference value, name,
  timestamp, review result, new numerical threshold, or executable result
  judgment is permitted.
- **FR16AAJ**: A two-phrase primary-run anchor expansion MAY project both
  complete punctuation phrases to the stable identity mapped from a primary raw
  run only when exactly two complete, source-adjacent phrases from one ASR item
  lie wholly inside that run within one derived native frame. One anchor phrase
  MUST contain the mapped identity in the current business view, and its
  session-registry and robust-gallery views MUST both pass the unchanged
  duration-class gates and select that identity. Across both phrases every
  label MUST be known, the mapped identity MUST be present, at least one
  conflicting label MUST be present, and every conflicting label MUST be the
  same identity.
  Its two voiceprint views MUST NOT both return eligible decisions unless both
  select the mapped identity; one eligible competing view paired with one
  abstention remains non-authoritative. Protected overlays, an unknown or third
  identity, non-adjacent source ranges, another contained phrase, missing raw
  mapping, dual-view competition/disagreement, overlap, or source/time mismatch
  MUST preserve the baseline. An accepted write MUST NOT extend outside the
  combined exact source range. No new threshold, transcript phrase, reference
  value, name, timestamp, review result, or executable judgment is permitted.
- **FR16AAK**: A phrase-led outer-abstention challenge MAY relabel one complete
  punctuation phrase only when exactly one containing VAD interval and exactly
  one covering primary raw run exist within one derived native frame. The
  current phrase MUST have one uniform known identity equal to the primary-run
  mapping, every source character in the phrase MUST carry an explicitly
  configured unconfirmed baseline reason, and that identity MUST differ from
  the proposed identity. Both outer VAD voiceprint views MUST top-rank the
  current mapped identity but abstain under the unchanged duration-class gates.
  Both exact-phrase voiceprint views MUST top-rank the same proposed identity;
  exactly one MUST pass the unchanged duration-class gates and the other MUST
  abstain solely on the unchanged margin gate. Missing evidence, a non-margin
  phrase abstention, an eligible outer VAD decision, top-rank disagreement,
  mixed or unknown baseline identity or provenance, a protected reviewed
  overlay, source/time mismatch, or overlapping proposal MUST preserve the
  baseline. An accepted write MUST cover only the phrase's exact source range.
  No new threshold, transcript phrase, reference value, name, timestamp,
  review result, or executable correctness judgment is permitted.
- **FR16AAL**: A secondary-channel edge closure MAY relabel one complete
  punctuation phrase only when its current source labels form exactly two
  contiguous known identity runs: a leading selected-identity anchor and a
  competing suffix. The leading anchor MUST carry only explicitly configured
  direct-anchor provenance. The competing suffix MUST contain exactly one
  positive-duration forced-alignment unit; zero-duration aligned characters
  and punctuation MAY remain in the same suffix but MUST NOT create another
  timed unit. Both exact-phrase voiceprint registries MUST top-rank the selected
  identity with the unchanged duration-class margin. The selected native
  Sortformer channel MUST sustain the existing activity floor for the existing
  minimum duration across the phrase, and every raw frame intersecting the one
  timed suffix unit MUST keep that selected channel active while top-ranking
  the mapped competing identity. Exactly one containing VAD interval is
  required. Unknown or third identities, another timed suffix unit, weak or
  absent secondary activity, raw/label mapping disagreement, registry
  abstention/disagreement, disallowed provenance, a protected reviewed overlay,
  source/time mismatch, or overlapping proposal MUST preserve the baseline.
  Projection MUST use the exact phrase source range. No new numerical threshold,
  transcript phrase, reference value, name, timestamp, review result, or
  executable correctness judgment is permitted.
- **FR16AAM**: Runtime fusion MAY let one complete punctuation phrase override
  a conflicting coarse business-interval voiceprint only when three independent
  acoustic views agree on the same active identity. The primary-speaker track
  MUST cover the complete phrase with that identity, activity diarization MUST
  support it for at least the TOML `speaker_fusion.min_embed_sec`, and both the
  session and robust-gallery phrase embeddings MUST independently select it
  under the existing TOML duration-class score and margin gates. The primary
  and activity tracks remain immutable and separate; this rule neither creates
  a boundary nor changes enrollment. A short primary run, incomplete primary
  coverage, absent activity support, one-gallery decision, gallery disagreement,
  score or margin abstention, missing alignment, or source/time mismatch MUST
  preserve the current business attribution. The write MUST cover only the
  phrase's forced-alignment source range. No threshold, transcript phrase,
  reference value, speaker name, timestamp, review result, or executable
  correctness judgment may be added.
- **FR16AAN**: With TOML `speaker_overlap_tie_policy = "primary_speaker"`, the
  runtime business view MAY use primary-speaker transitions to refine only a
  time range where at least two different activity-diarization identities are
  simultaneously active. Each refined interval MUST be fully covered by one
  unambiguous primary identity, that identity MUST be one of the activity
  candidates active in the same interval, and projection MUST remain on
  complete forced-alignment units. A primary-only boundary outside activity
  overlap, a primary identity absent from activity, a primary gap or conflict,
  unknown identity, missing alignment, or source/time mismatch MUST preserve
  the activity result. Raw activity and primary tracks, support coverage,
  identity enrollment, ASR, and alignment remain unchanged. This rule MUST use
  no reference value, speaker name, known timestamp, transcript phrase, fitted
  threshold, review result, or executable correctness judgment.
- **FR16AAO**: A runtime business-interval voiceprint query MUST NOT use a
  partial punctuation phrase at either edge to influence complete phrases
  inside the interval. When an interval starts inside one complete phrase and
  continues beyond it, or ends inside one after earlier complete context, the
  partial edge MUST be queried as a separate exact source/time range. Complete
  phrases between the trimmed edges remain one contextual business query; an
  interval wholly contained in one phrase remains unchanged. Every emitted
  subrange MUST use forced-alignment character times and the existing TOML
  embedding duration and score gates. Missing alignment, non-reconstructing
  source, or an invalid subrange MUST abstain rather than borrow another time
  range. No transcript phrase, reference value, speaker name, known timestamp,
  fitted threshold, review result, or executable correctness judgment may be
  introduced.
- **FR16AAP**: When source-ordered business pieces receive overlapping or
  slightly reversed forced-alignment times, publication MUST preserve the ASR
  source order. Adjacent pieces MAY share a normalized boundary only when both
  resulting spans remain positive at the configured sample rate. The repair
  MUST NOT change source ranges, text, speaker labels, evidence decisions, or
  any TOML threshold. If no valid common boundary exists, voiceprint projection
  MUST abstain and preserve the reconstructing diarization baseline. Terminal
  business entries and the latest live business revision MUST reconstruct the
  original ASR text byte-for-byte in the same order. No executable mechanism
  may use correctness labels or semantic outcomes to choose the boundary.
- **FR16AAQ**: A complete punctuation phrase MAY challenge a current dynamic
  local-slot identity only when the session's initial stable identity for one
  Sortformer local slot, that slot's complete activity coverage of the phrase,
  and both the session and robust-gallery phrase embeddings select the same
  stable identity. This path applies only from the TOML
  `speaker_fusion.short_max_sec` boundary through
  `speaker_fusion.phrase_max_sec`; both galleries MUST pass the configured
  duration-class margin, while the regular absolute score MAY be ignored only
  because the independent local-slot view supplies the third agreement. The
  phrase source range and forced-alignment time range are the only permitted
  write boundary. Missing initial identity, partial or competing local-slot
  activity, gallery disagreement, margin abstention, a non-uniform current
  identity, protected primary arbitration, missing alignment, or source/time
  mismatch MUST preserve the current attribution. The initial mapping is
  derived from the earliest immutable diarization identity for each local slot;
  it is not a replacement for later identity epochs and may not rewrite raw
  tracks or enrollment. No transcript phrase, reference value, speaker name,
  timestamp, review result, or executable correctness judgment is permitted.
- **FR16AAR**: Every terminal and live JSON time code used for speaker evidence,
  forced alignment, revisions, or business projection MUST retain enough
  decimal precision to preserve a positive one-sample interval at the configured
  sample rate. Serialization MUST NOT collapse a positive common-time-base span
  to zero duration or change source-order projection when the same typed tracks
  are replayed. Precision is a transport contract and MUST NOT change the
  underlying time values or any product judgment.
- **FR16AAS**: A complete punctuation phrase MAY challenge one uniform current
  identity when exactly one containing VAD interval and exactly one covering
  primary-speaker run exist within the configured alignment boundary tolerance.
  The VAD session, VAD robust, phrase session, and phrase robust score sets MUST
  all top-rank the same conflicting stable identity. Exactly three of those four
  views MUST pass their existing TOML duration-class score and margin gates; the
  fourth MUST pass its existing score gate and abstain solely because its
  top-two margin is below the unchanged margin gate. The covering primary run
  MUST map to the current phrase identity, not the proposed identity, and the
  phrase MUST contain at least TOML
  `speaker_fusion.four_view_min_aligned_units` distinct positive-duration
  forced-alignment units. Gallery
  disagreement, fewer or more than three eligible views, score-gate abstention,
  missing/ambiguous containment, mixed or unknown current identity, protected
  primary arbitration, missing alignment, or source/time mismatch MUST preserve
  the current attribution. The write MUST cover only the complete phrase's
  source and forced-alignment range. No new numerical threshold, transcript
  phrase, reference value, speaker name, timestamp, review result, or executable
  correctness judgment is permitted.
- **FR16AAT**: Every raw WebSocket acceptance client MUST implement RFC 6455
  control-frame liveness for long observer sessions. On receipt of a server
  PING it MUST promptly return one masked PONG with the identical payload on
  the same connection, without adding that control frame to the application
  event stream or changing audio production. This MUST keep producer, early
  observer, and late observer connections valid beyond libwebsockets' default
  300-second PING and 310-second hangup interval. PONG transmission MUST be
  serialized with any application frames on the same socket. The server's
  validity policy, event order, producer ownership, typed tracks, and terminal
  timeline MUST remain unchanged. This is a transport conformance contract and
  MUST NOT participate in product-result evaluation.
- **FR16AAU**: Final voiceprint evidence MUST be available when the observed
  stable-identity gallery reaches the TOML
  `speaker_fusion.minimum_gallery_size`; it MUST NOT implicitly require every
  configured Sortformer slot to have resolved a global identity. Evidence MUST
  rank only the stable identities present in that final timeline snapshot and
  MUST preserve all existing embedding-duration, score, margin, robust-gallery,
  primary, alignment, and phrase guards. An unresolved local identity MUST NOT
  be synthesized, selected, enrolled, or treated as a gallery candidate by
  this rule. The accepted v2.1 profile sets the minimum observed gallery size
  to three so a three-party stable gallery can revise evidence before a fourth
  participant has accumulated enough clean enrollment audio. This field MUST
  be supplied by `orator.toml`; no code path or command line may override it for
  acceptance.
- **FR16AAV**: A short business-interval or punctuation-phrase voiceprint MUST
  abstain from changing one uniform current identity when its evidence range
  crosses at least two typed VAD speech intervals, no single VAD interval
  contains the complete range within the existing alignment tolerance, and
  both immutable Sortformer views completely and uncontestedly cover that
  range with the same current identity. The protection applies only from the
  existing TOML `speaker_fusion.min_embed_sec` floor up to the existing
  `speaker_fusion.short_max_sec` boundary. A containing VAD interval, fewer
  than two overlapping VAD intervals, shorter or regular-duration evidence,
  missing identity, incomplete coverage, any competing native identity, or
  mixed current source labels MUST NOT synthesize this protection. Existing
  explicitly specified challenges that add independent evidence, including
  FR16AAQ and FR16AAS, are evaluated first and retain their own contracts. This
  rule MUST use only typed VAD, activity, primary-speaker, forced-alignment,
  and current business-label evidence; it MUST NOT inspect a transcript phrase,
  reference value, speaker name, known timestamp, review result, or executable
  correctness judgment.
- **FR16AAW**: Generic aligned-unit voiceprint evidence MUST NOT overwrite a
  conflicting identity already selected for that exact source character range
  by activity-overlap primary arbitration. This protection applies only to
  labels whose typed decision reason is `primary_speaker_tie_break` or
  `primary_speaker_overlap_refinement`; it MUST NOT make primary authoritative
  outside activity overlap, expand the primary time range, suppress an aligned
  voiceprint that agrees with primary, or change the separately specified
  FR16AAQ and FR16AAS challenge order. The rule adds no threshold and MUST NOT
  inspect transcript content, reference values, speaker names, known
  timestamps, review results, or executable correctness judgments.
- **FR16AAX**: A forced-alignment run MUST become splittable after an aligned
  unit that straddles a native speaker transition when activity and primary
  Sortformer independently start the same new stable identity within the
  existing TOML `speaker_fusion.align_boundary_split_tolerance_sec`. The
  matching start in each view MUST follow a native evidence gap of at least the
  TOML `timeline.align_snap_pause_sec`, MUST last for at least the TOML
  `speaker_fusion.min_embed_sec`, and MUST have no competing stable identity in
  that minimum-duration range. The nearest preceding interval in each view
  MUST exist and name a different stable identity. The
  activity boundary remains the business-turn boundary; primary contributes
  only corroboration that the continuous alignment run may break. Both views
  MUST name the same non-empty stable identity, and an activity start without a
  matching primary start, a primary-only start, a different-identity match, or
  a match outside the configured tolerance, insufficient native gap or
  duration, same-identity resumption, or competition during the minimum range
  MUST preserve the existing run-coherence behavior. The split MUST occur only
  between complete aligned units, MUST preserve source text byte-for-byte, and
  MUST NOT inspect
  transcript content, reference values, speaker names, known timestamps,
  review results, or executable correctness judgments.
- **FR16AAY**: A short punctuation phrase MAY recover the immutable initial
  stable identity of its currently selected local Sortformer slot only when
  every source character is uniformly attributed by an activity-overlap
  primary arbitration, one activity slot carrying that current identity covers
  the complete phrase, and that same slot's initial stable identity is one
  different active candidate. The phrase MUST be at least TOML
  `speaker_fusion.min_embed_sec` and shorter than TOML
  `speaker_fusion.short_max_sec`, contain at least TOML
  `speaker_fusion.four_view_min_aligned_units` positive-duration aligned units,
  and have exactly one containing robust-complete VAD interval within the TOML
  alignment tolerance. Both VAD galleries MUST top-rank the initial identity
  and pass their existing duration-class score and margin gates. The phrase
  robust gallery MUST top-rank that initial identity while passing its score
  gate and abstaining only on margin; the phrase session gallery MUST top-rank
  the current identity while also passing its score gate and abstaining only on
  margin. Missing or ambiguous slot/primary/VAD/alignment evidence, a score-gate
  failure, a passing phrase margin, any other ranking topology, mixed current
  labels, or a non-initial candidate MUST preserve the current attribution. The
  write MUST cover only the exact phrase source/alignment range, add no
  threshold, and MUST NOT inspect transcript content, reference values, speaker
  names, known timestamps, review results, or executable correctness judgments.
- **FR16AAZ**: A short business interval immediately following one complete
  punctuation phrase MAY inherit that phrase's conflicting stable identity only
  when the preceding phrase's session and robust galleries both pass their
  existing score and margin gates for the same identity, and the target
  interval's session and robust galleries both top-rank one uniform current
  identity while passing score and abstaining only on margin. The preceding
  identity MUST be the unique runner-up in both target galleries. One activity
  identity with that stable ID MUST cover the complete preceding phrase and
  continue without a gap through at least TOML `speaker_fusion.min_embed_sec`
  of the target interval. One primary run carrying the current target identity
  MUST cover the complete target within the alignment tolerance, and the target
  MUST contain at least TOML `speaker_fusion.four_view_min_aligned_units`
  positive-duration forced-alignment units. Source ranges MUST be exactly
  adjacent and their aligned time ranges MUST meet within the existing TOML
  alignment tolerance. Missing, ambiguous, weak, non-adjacent, non-contiguous,
  mixed, non-runner-up, or differently ranked evidence MUST preserve the current
  attribution. The write MUST cover only the target business interval, use a
  typed direct-anchor reason so weaker later evidence cannot erase it, add no
  threshold, and MUST NOT inspect transcript content, reference values, speaker
  names, known timestamps, review results, or executable correctness judgments.
- **FR16ABA**: A short punctuation phrase MAY replace one uniform conflicting
  coarse direct attribution with the immutable initial identity of its sole
  covering local activity slot when both exact-phrase galleries top-rank that
  initial identity, pass their existing score gates, and abstain only on their
  existing short-span margin gates. The phrase MUST be at least TOML
  `speaker_fusion.min_embed_sec` and shorter than TOML
  `speaker_fusion.short_max_sec`; every current source label MUST come from one
  typed `voiceprint_direct_*` reason; exactly one local activity slot with the
  current stable identity MUST cover the full phrase without any other activity
  slot overlap; and that slot MUST have one different non-empty immutable
  initial stable identity equal to both phrase top ranks. Exactly one primary
  run carrying the current identity MUST cover the phrase within the alignment
  tolerance, and at least TOML `speaker_fusion.four_view_min_aligned_units`
  positive-duration aligned units MUST lie inside it. A passing phrase margin,
  gallery disagreement, non-initial candidate, mixed/non-direct current labels,
  activity competition, missing/ambiguous primary, or insufficient alignment
  MUST preserve the current attribution. The exact phrase is the only write
  range; no threshold, text, identity, timestamp, reference value, review result,
  or executable correctness judgment may be added or inspected.
- **FR16ABB**: A positive-duration forced-alignment unit shorter than TOML
  `speaker_fusion.min_embed_sec` and therefore carrying no unit embedding MAY
  recover the immutable initial stable identity of its sole covering local
  activity slot only when the unit is temporally isolated from the nearest
  positive-duration aligned unit on both sides by at least TOML
  `timeline.align_snap_pause_sec`. Every current source label in the exact unit
  range MUST be one uniform non-empty native attribution that has not already
  been selected by voiceprint evidence. Exactly one local activity slot with
  that current stable identity MUST completely cover the unit, no other
  activity segment may overlap it, and that slot MUST have one different
  non-empty immutable initial stable identity. Exactly one primary-speaker run
  with the current identity MUST completely and uncontestedly cover the unit
  within the existing alignment tolerance. Exactly one robust-complete VAD
  interval MUST contain the unit within that tolerance, and both of its
  galleries MUST top-rank the initial identity and pass their existing
  duration-class score and margin gates. A boundary unit, missing neighboring
  unit, insufficient pause, available unit embedding, missing or ambiguous
  identity epoch, activity or primary competition, absent/weak/disagreeing VAD
  evidence, mixed/current voiceprint labels, or any non-initial candidate MUST
  preserve the current attribution. Only the exact aligned-unit source range
  may be written; no threshold, transcript content, identity, timestamp,
  reference value, review result, or executable correctness judgment may be
  added or inspected.
- **FR16ABC**: A positive-duration forced-alignment unit shorter than TOML
  `speaker_fusion.min_embed_sec` and carrying no unit embedding MAY follow one
  conflicting primary-speaker micro-run only when that run completely contains
  the exact unit, has positive duration shorter than the same TOML minimum, and
  shares an exact gapless common boundary with immediately preceding and
  following primary runs carrying the same uniform current identity. The containing
  primary identity MUST be non-empty and different from the current identity;
  no other primary run may overlap the unit. Every current source label in the
  exact unit range MUST come from one uniform typed `voiceprint_direct_*`
  attribution. Exactly one local activity slot with that current identity MUST
  completely cover the unit, no different identity or slot may overlap it, and
  the candidate primary identity MUST have no activity support in the unit.
  The nearest positive-duration aligned unit on each side MUST exist and MUST
  not overlap the candidate primary micro-run. A partial primary overlap,
  unbracketed, gapped, overlapping, or differently bracketed run,
  regular-duration run or unit,
  available unit embedding, mixed/non-direct current label, activity
  competition, candidate activity support, missing neighbor, or neighboring
  unit inside the micro-run MUST preserve the current attribution. Only the
  exact aligned-unit source range may be written; no threshold, transcript
  content, identity, timestamp, reference value, review result, or executable
  correctness judgment may be added or inspected.
- **FR16ABD**: One robust-complete typed VAD interval MAY challenge a uniform
  conflicting `voiceprint_direct_*` attribution only for the maximal contiguous
  run of complete positive-duration forced-alignment units contained by that
  VAD interval. The VAD's session and robust galleries MUST both select the same
  different active stable identity and pass their existing duration-class score
  and margin gates. The VAD interval MUST be isolated from its nearest typed VAD
  interval on both sides by at least TOML `timeline.align_snap_pause_sec`, and
  the aligned run MUST contain at least TOML
  `speaker_fusion.four_view_min_aligned_units` units from exactly one ASR
  `text_id`, with contiguous source ranges and no positive-duration aligned unit
  straddling either VAD boundary. Exactly one local activity slot and exactly
  one primary-speaker run carrying the uniform current identity MUST completely
  and uncontestedly cover the aligned run; the proposed identity MUST have no
  activity support in that run. A boundary VAD, insufficient pause, gallery
  disagreement or weak evidence, mixed or non-direct current labels, multiple
  aligned runs or text IDs, source discontinuity, activity or primary
  competition, candidate activity, or insufficient alignment MUST preserve the
  current attribution. The write MUST cover only the contained aligned source
  range, not the full VAD interval, and MUST add no threshold or inspect
  transcript content, reference values, speaker names, known timestamps,
  review results, or executable correctness judgments.
- **FR16ABE**: A primary-speaker run MAY challenge one uniform typed
  `voiceprint_*` current attribution at the onset of a containing VAD interval
  only when the run lasts at least TOML `speaker_fusion.min_embed_sec` and less
  than TOML `speaker_fusion.short_max_sec`. The nearest preceding primary run
  MUST carry the uniform current identity and end at least TOML
  `timeline.align_snap_pause_sec` before the candidate run; the immediately
  following primary run MUST carry that same current identity and share an
  exact gapless boundary with the candidate run. Exactly one robust-complete
  VAD interval MUST begin within the existing alignment boundary tolerance of
  the candidate run and contain its aligned range, and the nearest preceding
  typed VAD MUST end at least TOML `timeline.align_snap_pause_sec` before that
  VAD onset. The VAD MUST continue through at least TOML
  `speaker_fusion.min_embed_sec` of the gaplessly following current-primary run,
  and its session and robust galleries MUST both top-rank that current identity.
  These containing-VAD rankings constrain a known mixed interval and MUST NOT
  supply the candidate identity or lower an existing score or margin gate. The
  candidate primary identity MUST also have exactly one local
  activity slot completely covering the aligned range; any third activity
  identity or missing candidate activity MUST preserve the current result. The
  run MUST contain at least TOML `speaker_fusion.four_view_min_aligned_units`
  complete positive-duration units from one text ID, and no unit may straddle
  the primary boundary. Those units MUST contain exactly one internal source
  discontinuity with no positive-duration unit for the skipped source range.
  That discontinuity is a mandatory projection split: all contained units
  before it MUST preserve the current attribution, and only the contiguous
  aligned suffix after it MAY receive the candidate identity. No leading or
  trailing source range may be synthesized and no timed unit may be skipped. A
  short primary
  micro-run, regular or overlong run, missing pause, non-gapless recovery,
  different brackets, insufficient current continuation, disagreeing VAD
  ranking, mixed current labels,
  activity or primary competition, zero or multiple source discontinuities, an
  omitted timed unit, or insufficient
  alignment MUST abstain. Only the contained aligned source range may be
  written; no threshold, transcript content, reference value, speaker name,
  timestamp, review result, or executable correctness judgment may be added or
  inspected.
- **FR16ABF**: One positive-duration business interval shorter than TOML
  `speaker_fusion.min_embed_sec` and carrying no interval embedding MAY recover
  the immutable initial stable identity of its sole covering local activity
  slot when both typed VAD and forced-alignment evidence prove that interval is
  isolated. Every current source label in the exact interval MUST be one
  uniform non-empty native attribution not selected by voiceprint. Exactly one
  local activity slot with that current identity and exactly one primary run
  with the same local slot and current identity MUST completely and
  uncontestedly cover the interval. The slot MUST have one different non-empty
  immutable initial stable identity, and that candidate identity MUST have no
  overlapping activity support. No typed VAD interval may overlap the business
  interval; the nearest VAD on each side MUST exist and leave at least TOML
  `timeline.align_snap_pause_sec`. At least TOML
  `speaker_fusion.four_view_min_aligned_units` complete positive-duration units
  from the same text ID MUST lie inside the interval, no timed unit may straddle
  either edge, and the nearest positive-duration aligned unit outside each edge
  MUST also leave at least the same TOML pause. A boundary interval, available
  embedding, insufficient VAD or alignment isolation, missing initial epoch,
  activity or primary competition, mixed or voiceprint current labels, or
  insufficient aligned structure MUST preserve the current identity. Only the
  exact typed business-interval source range may be written; no threshold,
  transcript content, reference value, speaker name, known timestamp, review
  result, or executable correctness judgment may be added or inspected.
- **FR16ABG**: A short punctuation phrase MAY recover the immutable initial
  stable identity of its sole covering local activity slot when four exact or
  containing current-audio rankings form one consistent two-identity near tie.
  The phrase MUST be at least TOML `speaker_fusion.min_embed_sec` and shorter
  than TOML `speaker_fusion.short_max_sec`, have complete session and robust
  phrase scores, and be contained by exactly one robust-complete VAD interval.
  Every current phrase source label MUST be one uniform non-empty native
  attribution not selected by voiceprint. Exactly one local activity slot and
  exactly one primary run with the same current identity MUST completely and
  uncontestedly cover the phrase; the slot MUST have one different immutable
  initial identity. Across phrase session, phrase robust, VAD session, and VAD
  robust scores, the initial candidate and one same competing stable identity
  MUST occupy the top two ranks in every view. The candidate MUST rank first in
  exactly one view and second in the other three; the same competitor MUST have
  the inverse ranking, and the current identity MUST appear in neither top-two
  pair. Every view MUST pass its existing short-span score gate and abstain on
  its unchanged short-span margin gate. At least TOML
  `speaker_fusion.four_view_min_aligned_units` positive-duration units MUST lie
  completely inside the exact phrase. A passing margin, any other rank count,
  changing competitor, current identity in the pair, missing initial epoch,
  activity or primary competition, mixed or voiceprint labels, incomplete VAD,
  or insufficient alignment MUST preserve the current attribution. Only the
  exact phrase source/alignment range may be written; no threshold, transcript
  content, reference value, speaker name, timestamp, review result, or
  executable correctness judgment may be added or inspected.
- **FR16ABH**: A short punctuation phrase MAY override one uniform direct
  business-interval attribution when phrase-scale and containing-VAD evidence
  form one symmetric two-identity near tie at different scales. The phrase
  MUST be at least TOML `speaker_fusion.min_embed_sec` and shorter than TOML
  `speaker_fusion.short_max_sec`, have complete session and robust scores, and
  be contained by exactly one robust-complete VAD interval. Every current
  source label in the phrase MUST have one uniform known identity and an
  explicit `voiceprint_direct_short` or `voiceprint_direct_regular` reason.
  Exactly one activity slot and exactly one primary run with that current
  identity MUST completely and uncontestedly cover the phrase, and the
  proposed identity MUST have no overlapping activity. Phrase session and
  phrase robust views MUST top-rank the same different proposed identity, pass
  the unchanged short score gate, and abstain on the unchanged short margin
  gate. VAD session and VAD robust views MUST top-rank the current identity and
  fail both the unchanged regular score and margin gates. The same current and
  proposed identities MUST be the complete top-two pair in all four views.
  At least TOML `speaker_fusion.four_view_min_aligned_units` positive-duration
  aligned units MUST lie completely inside the exact phrase. A third top-two
  identity, any changed top rank, an outer eligible view, an eligible phrase
  view, mixed or non-direct labels, activity or primary competition, incomplete
  VAD, or insufficient alignment MUST preserve the current attribution. Only
  the exact phrase source range may be written; no new threshold, transcript
  content, reference value, speaker name, timestamp, review result, or
  executable correctness judgment may be added or inspected.
- **FR16ABI**: A short business interval MAY override one uniform primary-
  selected attribution when exact-interval voiceprint evidence is eligible in
  both galleries and the containing-VAD evidence independently excludes that
  primary identity while abstaining. The interval MUST be at least TOML
  `speaker_fusion.min_embed_sec` and shorter than TOML
  `speaker_fusion.short_max_sec`, with complete session and robust scores.
  Every current source label MUST have one uniform known identity and an
  explicit `primary_speaker_tie_break` or
  `primary_speaker_overlap_refinement` reason. Exactly one primary run and one
  activity slot with that current identity MUST completely cover the interval.
  Exactly one different activity identity MAY overlap the interval; it MUST
  completely cover the same interval, and the proposed identity MUST have no
  overlapping activity. Interval session and robust views MUST top-rank the
  same proposed identity and pass their unchanged short score and margin
  gates. Exactly one robust-complete VAD interval MUST contain the business
  interval. Its session and robust views MUST both fail the unchanged regular
  score and margin gates, MUST expose the proposed identity and the one
  competing activity identity as their complete top-two pair, and MUST place
  each identity first in exactly one view. The current primary identity MUST
  appear in neither VAD top-two pair. At least TOML
  `speaker_fusion.four_view_min_aligned_units` positive-duration aligned units
  MUST lie completely inside the exact business interval. A non-primary
  current label, missing or additional covering activity, candidate activity,
  an eligible VAD view, any changed VAD top-two identity, same VAD top rank,
  incomplete robust evidence, or insufficient alignment MUST preserve the
  current attribution. Only the exact typed business-interval source range may
  be written; no new threshold, transcript content, reference value, speaker
  name, timestamp, review result, or executable correctness judgment may be
  added or inspected.
- **FR16ABJ**: A positive but subminimum business interval with no embedding MAY
  restore one native activity/primary identity after a containing punctuation
  phrase selected a different voiceprint identity only when containing-VAD and
  phrase-scale evidence form a strict cross-scale abstention topology. Every
  current source label in the exact interval MUST have the same different
  identity, `voiceprint_phrase_session` reason, and voiceprint provenance.
  Exactly two different activity slots and exactly one primary run MUST
  completely cover the interval. One activity slot and the primary run MUST
  carry the proposed native identity; the other activity slot MUST carry one
  different competing identity. The current voiceprint identity MUST have no
  overlapping activity. Exactly one robust-
  complete punctuation phrase MUST contain the interval and top-rank the
  current voiceprint identity in both galleries; its session view MUST pass the
  unchanged short score and margin gates, and its robust view MUST pass score
  but fail margin. Exactly one robust-complete VAD interval MUST contain the
  exact interval and top-rank the proposed native identity in both galleries;
  both VAD views MUST pass the unchanged short score gate, fail the unchanged
  short margin gate, and place the current voiceprint identity second. The exact
  business interval MUST contain exactly one positive-duration forced-alignment
  unit and no positive-duration unit may straddle either edge; zero-duration
  source units inside the typed interval remain part of the same atomic write.
  An available interval embedding, regular-length interval, mixed current
  provenance, activity or primary competition, candidate activity, missing or
  additional containing phrase/VAD evidence, changed rank or gate state, or
  invalid alignment MUST preserve the current attribution. Only the exact typed
  business-interval source range may be written; no new threshold, transcript
  content, reference value, speaker name, timestamp, review result, or
  executable correctness judgment may be added or inspected.
- **FR16ABK**: A complete ASR source whose outer source interval is regular-
  length MAY be evaluated with the unchanged short-span gates only when
  positive-duration typed business intervals form an exact non-overlapping
  partition of the complete source, every interval contains at least one
  positive forced-alignment unit wholly inside its source and time bounds, and
  the partition's complete speech envelope is shorter than TOML
  `speaker_fusion.short_max_sec`. The complete-source session and robust views
  MUST agree on one proposed identity, pass the unchanged short score and
  margin gates, and both fail only the unchanged regular score gate when ranked
  against the padded outer source interval. Exactly one robust-complete VAD
  interval MUST contain the full aligned speech envelope; both VAD views MUST
  independently select the same proposed identity through their unchanged
  short score and margin gates. Exactly one robust-complete punctuation phrase
  MAY exist in the source; both phrase views MUST margin-abstain and MUST place
  the proposed identity in their top-two pair. Current source labels MUST
  contain only the proposed identity and one known incumbent, with the proposed
  identity confined to one contiguous source edge and carrying primary
  provenance. Activity and primary evidence over the aligned envelope MUST
  contain exactly those two identities: incumbent activity MUST cover the
  envelope, proposed activity MUST contribute but not cover it, and the union
  of the two primary identities MUST cover the envelope with both identities
  contributing. Missing or overlapping source coverage, an unanchored
  interval, missing or repeated VAD/phrase evidence, any third identity, an
  interior current-identity island, eligible phrase
  evidence, gallery disagreement, regular-score eligibility, or changed native
  coverage MUST preserve the current attribution. A successful challenge MUST
  write the exact complete source atomically. No new threshold, transcript
  content, reference value, speaker name, timestamp, review result, or
  executable correctness judgment may be added or inspected.
- **FR16ABL**: The evidence stage MUST emit an `adjacent_business_pair` query
  for every pair of typed business intervals whose source and time ranges are
  exactly adjacent, whose leading interval is positive but shorter than TOML
  `speaker_fusion.min_embed_sec`, whose following interval reaches that same
  minimum, and whose combined duration is shorter than TOML
  `speaker_fusion.short_max_sec`. The business projector MAY restore only the
  exact leading source interval when the pair begins at source index zero, the
  leading interval has no embedding, the following interval has complete
  dual-gallery evidence, and the pair's session and robust views agree on one
  candidate through the unchanged short score and margin gates. Current labels
  on the leading interval MUST be one different non-voiceprint identity with
  native `sole_diar_support` or `competing_diar_interval_policy` provenance;
  current labels on the following interval MUST already be the candidate
  through primary-speaker provenance. Exactly
  one candidate primary run MUST begin inside the leading interval within the
  existing alignment boundary tolerance and cover the following interval.
  Activity over the pair MUST contain the candidate and incumbent: the
  incumbent MUST cover the pair within that same boundary tolerance, and the
  candidate MUST begin inside the leading interval and cover the following
  interval. At most one additional activity identity MAY fully cover the pair
  only when its overlap-weighted confidence is strictly lower than both the
  incumbent and candidate; a partial, stronger, tied, or fourth identity MUST
  preserve the current attribution. At least TOML
  `speaker_fusion.four_view_min_aligned_units`
  positive-duration alignment units MUST be contained by the pair, including
  at least one in each component. Exactly one immediately following typed
  business interval MUST continue the source after a nonnegative time gap and
  MUST carry the incumbent through primary provenance, so the later handoff is
  preserved. Missing or repeated components, a noninitial pair, a gap or
  overlap, gallery abstention or disagreement, changed current provenance,
  incomplete primary/activity closure, insufficient alignment, or a changed
  following handoff MUST preserve the current attribution. Only the exact
  leading source interval may be written. No transcript content, identity,
  timestamp, reference value, review result, or executable correctness
  judgment may enter either evidence generation or selection.
- **FR16ABM**: A generic punctuation-phrase voiceprint decision MUST NOT erase
  a sustained native multi-identity handoff already present in the exact phrase
  source range. Before any voiceprint write, the phrase-selected identity MUST
  occur in the immutable base business projection, and one different known
  base identity MUST form the only other contiguous identity run. The exact
  phrase MUST therefore contain two known base-identity runs and exactly one
  handoff, with no unknown span, return transition, or third identity. The
  different run MUST retain `primary_speaker_tie_break` or
  `primary_speaker_overlap_refinement` provenance for at least existing TOML
  `speaker_fusion.min_embed_sec` of positive-duration forced-alignment coverage
  inside the phrase. When all conditions hold,
  the generic `voiceprint_phrase_session`, `voiceprint_phrase_dual_gallery`,
  and `voiceprint_phrase_dual_gallery_override` paths MUST abstain. A shorter
  primary fluctuation, an internal identity island, a phrase identity absent
  from the base projection, or a transition lacking primary provenance MUST
  NOT activate this guard. This
  protection does not make primary top-1 authoritative and does not block a
  separately specified challenge whose evidence contract explicitly admits
  mixed current identities and independently proves its bounded write. The
  guard introduces no new score, margin, duration, confidence, lexical,
  speaker-specific, or reference-derived condition; it reuses only the frozen
  minimum embedding duration and immutable pre-voiceprint source ownership.
  Source text, timing, and every raw track MUST remain unchanged.
- **FR16ABN**: A source-contiguous group of punctuation-delimited clauses whose
  combined positive forced-alignment duration is shorter than existing TOML
  `speaker_fusion.min_embed_sec` MAY recover one intervening native identity
  when forced alignment has placed that complete short group after a native
  evidence island. The group MUST begin after a gap of at least existing TOML
  `timeline.align_snap_pause_sec` from the nearest preceding positive-duration
  aligned unit, MUST end before the next punctuation clause having at least the
  existing minimum positive aligned duration, and MUST contain at least TOML
  `speaker_fusion.four_view_min_aligned_units` positive-duration aligned units.
  Every source character in the group and the immediately adjacent source
  context on both sides MUST carry one uniform non-empty incumbent identity
  after the existing fusion rules. The incumbent activity and primary views
  MUST both cover the delayed aligned group, MUST both begin their covering
  return run within existing TOML
  `speaker_fusion.align_boundary_split_tolerance_sec` of the group's aligned
  onset, and the incumbent activity MUST cover the nearest preceding aligned
  unit. Inside the alignment gap, exactly one different known identity MUST
  have at least the existing minimum duration of overlapping activity and
  primary evidence. That candidate activity MUST end before the incumbent
  return by at least the existing alignment snap pause, and no third identity
  may satisfy the native-island requirement. Exactly one adjacent typed VAD
  pair MUST place a VAD gap of at least the existing alignment snap pause
  between the preceding aligned unit and the delayed group; the candidate
  island MUST overlap the leading VAD and the incumbent return MUST overlap the
  following VAD. Current labels outside typed direct or native
  activity/primary provenance, an available phrase embedding, insufficient or
  ambiguous aligned/native/VAD evidence, a non-return transition, a candidate
  still active on the delayed group, or any edge/source mismatch MUST preserve
  the current attribution. Only the exact short clause group may change. The
  rule MUST consume the existing TOML punctuation set and thresholds, add no
  fitted parameter, alter no raw track or timestamp, and inspect no lexical
  value, speaker name, known timestamp, reference value, review result, or
  executable correctness judgment.
- **FR16ABO**: A complete punctuation phrase MAY challenge a uniform current
  identity when the same Sortformer local slot enters a different stable
  identity epoch shortly after the phrase. The future epoch is corroborating
  evidence only: the identity stage, its epoch boundaries, and all raw tracks
  MUST remain unchanged. The phrase duration MUST be at least TOML
  `speaker_fusion.min_embed_sec` and at most
  `speaker_fusion.phrase_max_sec`. Exactly one diarization local slot and one
  primary-speaker run MUST each cover the complete phrase with the same current
  identity, and no competing local slot may overlap it. The first later
  different identity for that same local slot MUST begin within TOML
  `speaker_fusion.future_epoch_lookahead_sec`; both diarization and primary
  tracks MUST expose that future local/identity pair for at least the existing
  minimum embedding duration. The future identity MUST rank first in the
  robust gallery with the existing duration-class score and margin and MUST appear in
  the session gallery's top two, while neither gallery may rank the incumbent
  first. At least the configured number of positive-duration aligned units
  MUST lie wholly inside the phrase. Missing, ambiguous, out-of-window,
  voiceprint-rewritten, competing-slot, gallery-conflicting, or insufficiently
  aligned evidence MUST preserve the current result. Only the exact phrase may
  change, with a distinct audit reason. The rule MUST inspect no text value,
  speaker name, known timestamp, reference datum, prior review label, or
  executable correctness result. Its TOML lookahead is disabled at zero and
  must be frozen before candidate generation.

### 4.5 Product surface and operations

- **FR17**: Web UI live rows must converge exactly to the terminal business view
  without duplicate IDs, stale partials, missing revisions, or browser-side
  speaker re-splitting. One connection owns audio production for a session;
  browser and diagnostic observer connections receive the same live events and
  terminal timeline without resetting, replacing, or otherwise mutating that
  session. Connecting or disconnecting an observer must not change the common
  time base, producer ownership, track contents, or terminal result.
- **FR18**: Full tests must capture `tegrastats` continuously and report CPU,
  RAM, GPU utilization, GPU memory, temperature, rail/system power, and thermal
  or allocation failures. GPU utilization must be present during GPU load.
- **FR19**: Closure validation must use the real incremental WebSocket path at
  the constitutional 120 s, 360 s, 600 s, and full-length levels.
- **FR20**: Final acceptance requires two independent full-length runs: one with
  an empty isolated speaker registry and one after process restart with the
  frozen enrolled-registry fixture.
- **FR25**: The unified real-WebSocket acceptance client MUST send `end`
  directly after the final audio frame. A preceding `flush` is permitted only
  in an explicitly requested non-acceptance command-contract scenario. The
  artifact and sidecar manifest MUST record whether `flush` was requested, its
  independent request-to-timeline wait, the `end` request-to-timeline wait, the
  final-frame-to-terminal wait, and whether that observation is eligible for
  the 30-second terminal-latency gate. A `flush`-primed observation MUST NOT
  satisfy that gate.
- **FR26**: Final multi-resolution speaker evidence MUST NOT defer every
  TitaNet embedding until `end`. When
  `speaker_fusion.precompute_interval_sec` is positive, a session-owned worker
  MUST periodically read only typed `ComprehensiveTimeline` evidence and cache
  the acoustic embedding for available final evidence spans. Live work MUST
  wait until the typed diarization snapshot contains at least
  `minimum_gallery_size` active identities and diarization, ASR, and VAD have
  all reached the sampled common-clock input head. Each live cycle MUST cache
  no more than `speaker_fusion.precompute_max_spans_per_cycle` successful spans.
  Precomputation MUST NOT score a speaker, inspect reference text, mutate a
  producer track, or publish a business decision. TitaNet extraction MUST use
  a scheduler-owned background CUDA stream at the device's lowest available
  priority, and its maximum configured scratch MUST be warmed before live audio
  starts. Periodic evidence work MUST NOT share the default CUDA stream or grow
  device scratch during a live session. After every producer and forced-
  alignment worker drains, the final path MUST ignore the live per-cycle limit,
  cache every remaining span, rescore all cached embeddings against the mature
  session and robust galleries, compute any cache miss from the same retained
  audio, and run the unchanged speaker-fusion policy. The cached and uncached
  paths MUST produce exactly equal speaker-voiceprint and business-speaker
  tracks for identical typed inputs, model, and registry state. Periodic
  cadence and per-cycle limit are typed TOML settings and MUST appear in
  `resolved_config`. Finalization diagnostics MUST separately expose worker
  drain, evidence precompute drain, primary construction, voiceprint
  construction, business reprojection, serialization, and terminal emission
  wall times; diagnostics are mechanical performance evidence only.
- **FR27**: Periodic GPU telemetry MUST schedule against
  `std::chrono::steady_clock` absolute deadlines. The first sample deadline is
  one configured `telemetry.gpu_interval_sec` after the worker starts. After a
  sample is serialized and emitted, the next deadline MUST be the first
  original-cadence deadline strictly after the current monotonic time; probe
  and emission latency MUST NOT shift every later deadline. Expired deadlines
  are skipped rather than emitted in a catch-up burst, and stop notification
  MUST remain interruptible while waiting. This change MUST preserve the
  checked-in TOML interval, telemetry JSON payload, and disabled-by-zero
  behavior. Focused deterministic timing coverage and a real incremental
  WebSocket cadence check are mechanical engineering evidence only and MUST
  NOT evaluate or reopen speaker correctness.
- **FR28**: VAD-gated ASR segmentation MUST be invariant to the relative
  scheduling of the independent ASR and VAD workers. VAD MUST publish its
  active padded speech onset, a stable active-speech decision horizon, finalized
  speech segments, and a monotonic confirmed-silence horizon only through the
  typed `ComprehensiveTimeline`. ASR MUST NOT read `GpuVad`, receive a direct
  VAD callback, wait on a VAD cursor, or feed audio beyond that typed stable
  evidence. Audio ahead of the decision horizon MUST remain in an ASR-owned
  pending buffer. On a new speech region, ASR MUST begin at the typed onset
  minus TOML `asr.vad_lead_ms`; it MUST consume decided speech using TOML
  `asr.vad_gate_chunk_ms` batches so publication timing cannot alter decoder
  call boundaries. Confirmed silence beyond the existing TOML trailing
  interval is skipped. If another stable speech region begins within that
  interval, every intervening source sample MUST remain in the same decoder
  session; the gate MUST NOT concatenate the two speech regions after removing
  their natural pause. If no speech resumes within the interval, the final ASR
  record MUST retain the confirmed trailing source-clock bound for downstream
  forced alignment without requiring silence-only decoder input. The gate MUST
  reserve TOML `asr.vad_lead_ms` for the next segment when lead and trailing
  windows would otherwise overlap.
  Session finalization MUST freeze the final VAD evidence before ASR drains its
  pending tail and emits finals. Focused tests MUST feed identical audio and
  identical final VAD evidence under different publication orders and require
  identical ASR reset positions, fed sample ranges, finals, and typed time
  codes. These exact-equality checks are mechanical determinism evidence only;
  they MUST NOT assign transcript, speaker, endpoint, or product correctness.
- **FR29**: The business projection MUST preserve a speaker handoff corroborated
  by both immutable Sortformer activity and primary-speaker views when a
  forced-alignment unit anomalously crosses that handoff. The following
  identity and onset MUST agree in both views within existing TOML
  `timeline.align_boundary_split_tolerance_sec`; each following run MUST last
  at least existing TOML `speaker_fusion.min_embed_sec`; the nearest preceding
  known identity in both views MUST agree and differ from the following
  identity; at least one view MUST expose a preceding-to-following gap of at
  least existing TOML `timeline.align_snap_pause_sec`; and the crossing aligned
  unit MUST begin before the corroborated
  onset, end after it, and last longer than existing TOML
  `timeline.align_snap_pause_sec`. Only then MAY the base projector terminate
  the preceding aligned source run at that unit and begin later aligned source
  content with the following native run. If the first following aligned unit
  has zero duration exactly at the crossing unit's end and closes before the
  next unit at one punctuation mark from existing TOML
  `speaker_fusion.punctuation`, that unit and intervening punctuation MUST stay
  on the preceding source run; later source content remains on the following
  run. A containing `business_interval`
  voiceprint decision MUST NOT repaint both immutable base-identity runs with
  its majority identity when the competing base run retains at least existing
  TOML `speaker_fusion.min_embed_sec` of positive aligned time corroborated by
  both native views. Missing, ambiguous, one-view-only, overlapping,
  subminimum, normally timed, return-transition, unknown-identity, or
  source-order evidence MUST preserve existing behavior. This rule MUST add no
  fitted parameter; inspect no transcript value, speaker name, known timestamp,
  reference datum, or review result; and alter no ASR, alignment, diarization,
  primary-speaker, VAD, identity, or common-time-base record. The existing
  final speaker-evidence stage intentionally derives `business_interval`
  voiceprint query partitions from the revised base business view after the
  final primary-speaker deposit. A retained handoff therefore MAY replace one
  containing derived business-interval query with source-complete queries on
  the two revised business runs. That derived-evidence change MUST be confined
  to the corroborated handoff; it MUST NOT modify any upstream producer record,
  model output, common-clock value, or TOML value.
- **FR30**: The production Silero VAD sensitivity used by FR28 MUST remain a
  typed TOML value and MUST be validated as a single-variable candidate. The
  candidate changes only `vad.threshold` from `0.5` to `0.3`; model weights,
  minimum speech, minimum silence, padding, ASR lead, ASR trail, ASR feed
  quantum, and every speaker-fusion value remain unchanged. The production
  `GpuVad` path MUST remain deterministic and time-base valid, the frozen
  silence fixture and three independent real-WebSocket silence sessions MUST
  emit no product records, and repeated 120-second real-WebSocket runs MUST
  have identical seven-track entries before a 600-second contextual gate may
  begin. A 600-second candidate MUST receive complete chronological and reverse
  review against all in-scope `test.txt` contributions. No interval count,
  coverage calculation, transcript comparison, or other automated result may
  rank or accept this threshold. A new full A/B run is forbidden until the
  600-second contextual gate is manually retained. After retention, Run A MUST
  use an empty isolated registry and Run B MUST use Run A's frozen registry in
  a restarted process. Each full artifact MUST receive its own complete
  chronological 556-contribution review and independent reverse-600-second-
  block review before FR30 may be promoted or rejected.
- **FR31**: An ordinary punctuation-phrase or complete-source voiceprint write
  MUST preserve an already projected primary-speaker return island when both
  immutable Sortformer views corroborate that exact native identity. The
  protected source character MUST already carry typed
  `primary_speaker_tie_break` or `primary_speaker_overlap_refinement`
  provenance before the voiceprint write, have positive forced-alignment time
  intersecting exactly one primary run with that identity, and that primary run
  MUST last at least existing TOML `speaker_fusion.min_embed_sec` and less than
  existing TOML `speaker_fusion.short_max_sec`. The nearest known primary run
  on each side MUST carry the same different identity selected by the proposed
  voiceprint write, so the native topology is A-B-A rather than an unresolved
  transition. Immutable activity diarization with identity B MUST cover the
  complete primary B run for at least the same existing minimum duration. A
  missing aligned character time, one-view-only identity, subminimum or regular
  run, non-return topology, unknown identity, different voiceprint incumbent,
  or activity coverage gap MUST preserve existing behavior. The guard applies
  only to ordinary phrase and complete-source repainting; explicit specialized
  multi-view challenge rules retain their existing precedence. It MUST add no
  threshold, inspect no transcript value, speaker name, known timestamp,
  reference datum, or review result, and alter no ASR, VAD, diarization,
  primary-speaker, alignment, voiceprint, identity, or common-time-base record.
  Frozen T111 and T123 inputs MUST each replay deterministically. Tools may
  arrange changed contexts and verify only structural contracts; every changed
  conversational context MUST be read completely in chronological and reverse
  order against `test.txt` before FR31 can be retained or enter a real-WebSocket
  promotion run.
- **FR32**: A broader punctuation-phrase or complete-source voiceprint write
  MAY preserve an aligned primary B character only when one exact typed
  `business_interval` supplies independent cross-scale corroboration for the
  complete primary B run. The character MUST already carry base
  `primary_speaker_tie_break` or `primary_speaker_overlap_refinement`
  provenance, and its positive forced-alignment midpoint MUST lie inside one
  primary B run lasting at least existing TOML
  `speaker_fusion.min_embed_sec` and less than existing TOML
  `speaker_fusion.short_max_sec`. The nearest primary runs on both sides MUST
  carry the proposed broader-write identity A. Exactly one same-text typed
  `business_interval` MUST have the same common-clock start and end as the
  primary B run, have an available embedding and complete robust gallery, and
  independently select B in both the session-refreshed and robust galleries
  under the unchanged duration-class score and margin gates. Activity B MUST
  cover the complete run, and no activity identity other than A or B may
  overlap it. Missing or non-exact interval evidence, gallery abstention or
  disagreement, one-view native support, a third activity identity, missing
  alignment, non-return topology, or an out-of-range duration MUST preserve
  existing behavior. FR32 MUST add no score, duration, confidence, or fitted
  threshold; inspect no transcript value, speaker name, timestamp constant,
  reference datum, or review result; and alter no producer evidence or common
  clock. Frozen T111 and T123 inputs MUST replay deterministically, and every
  changed context MUST receive complete chronological and reverse contextual
  semantic review against `test.txt` before retention or a real-WebSocket run.
- **FR33**: An ordinary short punctuation-phrase session write MAY preserve
  the existing identity only when the complete typed evidence proves one
  partition-invariant cross-scale abstention topology. Every phrase character
  and base label MUST carry one non-voiceprint current identity. One activity
  slot and one primary slot of that identity MUST cover the phrase completely
  without a competing overlap, and the phrase MUST satisfy the existing
  aligned-unit minimum. The phrase session gallery MUST select one challenger
  under the existing short score and margin gates; the complete phrase robust
  gallery MUST rank that challenger first and the current identity second,
  pass score, and fail margin. Exactly one embedding-backed, robust-complete
  typed VAD interval MUST contain the phrase. Its session gallery MUST select
  the same challenger with the current identity second, while its robust
  gallery MUST reverse that pair, rank the current identity first, pass score,
  and fail margin. Exactly one containing same-text `business_interval` and
  exactly one containing same-text `complete_source` MUST be embedding-backed,
  robust-complete regular spans. Their session and robust galleries MUST all
  rank the current identity first and the same challenger second, pass the
  unchanged regular margin, and fail only the unchanged regular absolute-score
  gate. Specialized challenges MUST retain precedence. Any missing, duplicate,
  ineligible, differently ranked, differently gated, competing, unaligned, or
  non-native evidence MUST preserve ordinary behavior. FR33 MUST add no TOML
  value, score, margin, duration, identity, transcript, name, timestamp,
  reference datum, or fitted constant and MUST alter no producer evidence or
  common-clock coordinate. Frozen T111/T123 inputs MUST replay
  deterministically, and every changed context MUST receive complete forward
  and reverse contextual semantic review against `test.txt` before retention
  or a new audio run.
- **FR34**: A short punctuation phrase MAY replace one uniform coarse direct
  identity A with one different identity B only when the complete typed
  evidence proves an exact phrase/unique-VAD conflict rather than a broad
  interval repaint. Every phrase character MUST currently carry the same
  voiceprint-direct identity A. Exactly one containing same-text regular
  `business_interval` MUST be embedding-backed and robust-complete, and its
  session and robust galleries MUST both select A under the unchanged regular
  score and margin gates. The phrase MUST be embedding-backed,
  robust-complete, meet the existing aligned-unit minimum, and have session
  and robust galleries that both select B under the unchanged short gates.
  Exactly one containing typed VAD interval MUST be embedding-backed and
  robust-complete, and both of its galleries MUST independently select B under
  its unchanged duration-class gates. Activity for B MUST cover the complete
  phrase. Exactly one primary segment carrying a third identity C MUST overlap
  and cover the phrase completely; activity for C MUST also cover the phrase,
  and no activity identity other than B or C may overlap it. A, B, and C MUST
  be non-empty and pairwise different. Existing specialized challenges MUST
  retain precedence, and the write MUST cover only the exact phrase source and
  forced-alignment range. Missing or duplicate containment, mixed or protected
  current labels, gallery abstention or disagreement, insufficient alignment,
  incomplete native coverage, primary ambiguity, an additional activity
  identity, or any source/time mismatch MUST preserve existing behavior. FR34
  MUST add no TOML value, score, margin, duration, identity, transcript,
  speaker name, timestamp, reference datum, or fitted constant and MUST alter
  no producer evidence or common-clock coordinate. Frozen T111/T123 inputs
  MUST replay deterministically, and every changed complete conversation MUST
  receive chronological and reverse contextual semantic review against
  `test.txt` before retention or a new audio run.
- **FR35**: The existing isolated no-embedding aligned-unit challenge MAY
  treat a neighbouring aligned-unit gap as satisfying the configured
  isolation pause only when adding at most the existing
  `timeline.align_boundary_split_tolerance_sec` reaches
  `timeline.align_snap_pause_sec`. The tolerance MUST apply only to the
  preceding and following gap comparisons. The existing positive-duration,
  subminimum duration, no-embedding, uniform native-label, unprotected
  provenance, unique activity local-slot, distinct initial-identity, exact
  current-primary coverage, unique containing complete-gallery VAD, and dual
  VAD gallery selection conditions MUST remain conjunctive and unchanged.
  An out-of-tolerance gap, zero configured tolerance, missing or competing
  evidence, gallery abstention or disagreement, independently embedded unit,
  protected label, or source/time mismatch MUST preserve existing behavior.
  FR35 MUST add no TOML value, score, margin, duration, identity, transcript,
  speaker name, timestamp, reference datum, or fitted constant and MUST alter
  no producer evidence or common-clock coordinate. Frozen T111/T123 inputs
  MUST replay deterministically, and every changed complete conversation MUST
  receive chronological and reverse contextual semantic review against
  `test.txt` before retention or a new audio run.
- **FR36**: A regular-duration punctuation phrase MAY replace one uniform
  native current identity A with the different initial identity B of the same
  local slot only when all partition-invariant typed scales prove one strict
  cross-scale reversal. The phrase MUST be embedding-backed,
  robust-complete, within the existing regular phrase duration bounds, and
  contain the existing minimum aligned-unit count. Every current label MUST
  be unprotected native `sole_diar_support`, equal its base identity, and name
  A. Activity and primary MUST each expose exactly one completely covering
  local slot with current identity A, those slots MUST be identical, and no
  competing slot may overlap. That slot's unique initial identity MUST be B.
  Both exact-phrase galleries MUST rank A first and B second, both MUST fail
  the unchanged regular absolute-score gate, and exactly one MUST pass the
  unchanged regular margin. Exactly one embedding-backed, robust-complete VAD
  and exactly one same-text embedding-backed, robust-complete complete-source
  record MUST contain the phrase. All four outer galleries MUST rank B first
  and A second and fail both unchanged regular score and margin gates.
  Existing specialized challenges MUST retain precedence, and the write MUST
  cover only the exact phrase source and forced-alignment range. Missing,
  duplicate, eligible, differently ranked, differently gated, competing,
  mixed, protected, unaligned, or source/time-inconsistent evidence MUST
  preserve existing behavior. FR36 MUST add no future lookahead, TOML value,
  score, margin, duration, identity, transcript, speaker name, timestamp,
  reference datum, or fitted constant and MUST alter no producer evidence or
  common-clock coordinate. Frozen T111/T123 inputs MUST replay
  deterministically, and every changed complete conversation MUST receive
  chronological and reverse contextual semantic review against `test.txt`
  before retention or a new audio run.
- **FR37**: A short embedding-backed `business_interval` MAY replace one
  uniform unprotected primary identity A with the different initial identity B
  of A's local activity slot only when a complete adjacent-boundary and
  bracketed-primary topology corroborates B. The interval MUST remain within
  the existing primary-consensus and short duration bounds, be robust-complete,
  and contain the existing minimum aligned-unit count. Exactly one containing
  primary run in A's local slot MUST cover it; that run MUST remain below the
  existing short bound and be gaplessly bracketed by immediately preceding
  and following primary runs which each meet the existing primary-consensus
  minimum and name the same third identity C. Activity MUST expose exactly the
  completely covering A and C local slots and no other overlap. A's slot MUST
  have initial identity B, pairwise distinct from A and C, and B MUST NOT be a
  current activity identity. Both interval galleries MUST rank A first and pass
  the unchanged short score and margin gates, with C second in the session
  view and B second in the robust view. Exactly one embedding-backed,
  robust-complete same-text punctuation
  phrase MUST end at the interval's source and common-clock start within the
  existing alignment boundary tolerance. Both phrase galleries MUST rank B
  first and pass the unchanged short score gate, with exactly one passing the
  unchanged short margin. Exactly one embedding-backed, robust-complete VAD
  MUST contain the interval, and both VAD galleries MUST rank B first and pass
  their unchanged duration-class score and margin gates. Existing specialized
  interval challenges MUST retain precedence, and the write MUST cover only
  the exact interval source and forced-alignment range. Missing, duplicate,
  non-gapless, differently ranked, differently gated, unaligned, protected,
  additional-activity, or source/time-inconsistent evidence MUST preserve
  existing behavior. FR37 MUST add no TOML value, score, margin, duration,
  future lookahead, identity, transcript, speaker name, timestamp, reference
  datum, or fitted constant and MUST alter no producer evidence or common-clock
  coordinate. Frozen T111/T123 inputs MUST replay deterministically, and every
  changed complete conversation MUST receive chronological and reverse
  contextual semantic review against `test.txt` before retention or a new
  audio run.
- **FR38**: The no-embedding tail of one source-leading punctuation phrase MAY
  inherit the preceding direct identity B only when a strict cross-VAD
  partition topology separates it from the following native identity C. The
  embedding-backed phrase MUST start at source zero, remain within the
  existing regular phrase bounds, and consist of one leading embedded
  interval plus one source-contiguous subminimum tail interval. The tail MUST
  contain exactly one positive-duration visible aligned character followed
  only by configured punctuation. One separately embedded following interval
  MUST start exactly at the phrase source end, and both of its galleries MUST
  rank C first. The three intervals MUST be ordered without time overlap.
  Leading labels MUST be uniformly ordinary
  short-direct B over uniform native A; tail and following labels MUST be
  unprotected native sole-support C; A, B, and C MUST be pairwise distinct.
  Activity and primary MUST each expose exactly one completely covering A
  local slot over the leading interval and one completely covering C local
  slot over the tail and following interval, with no competing overlap. Both
  leading interval galleries MUST rank B first and A second and pass the
  unchanged short gates. The phrase session view MUST rank A then B, the
  robust view B then A, both MUST fail the unchanged regular score gate, and
  exactly one MUST pass the unchanged regular margin. Exactly one leading VAD
  MUST rank B then A in both galleries under the unchanged short score gate
  with exactly one margin pass. Exactly one different tail VAD MUST begin
  within the existing alignment boundary tolerance, contain the tail, extend
  into the following interval, rank C then B in both galleries, and pass both
  unchanged short gates. Existing specialized writes MUST retain precedence,
  and only the exact tail source/alignment range may change. Missing,
  duplicate, differently ranked, differently gated, mixed, protected,
  unaligned, non-leading, non-punctuation, competing, or source/time-
  inconsistent evidence MUST preserve existing behavior. FR38 MUST add no
  TOML value, score, margin, duration, future lookahead, identity, transcript,
  speaker name, timestamp, reference datum, or fitted constant and MUST alter
  no producer evidence or common-clock coordinate. Frozen T111/T123 inputs
  MUST replay deterministically, and every changed complete conversation MUST
  receive chronological and reverse contextual semantic review against
  `test.txt` before retention or a new audio run.
- **FR39**: A source-leading exact punctuation phrase MAY return from one
  uniform unprotected native identity B to the different initial identity A
  of its sole covering activity slot only when a separately aligned terminal
  tail explains the cross-scale reversal. The exact phrase, containing short
  interval, containing VAD, and complete-source views MUST expose only A/B in
  the strict rank and unchanged-gate topology recorded by the frozen
  diagnosis. One third-identity primary segment MUST cover exactly the one-
  visible-character tail after the existing configured pause. Activity and
  primary MUST uniquely cover their required ranges, and only the exact
  source-leading phrase may change. Missing, duplicate, differently ranked,
  differently gated, mixed, protected, unaligned, competing, or source/time-
  inconsistent evidence MUST preserve existing behavior. FR39 MUST add no
  TOML value, threshold, fitted constant, transcript lookup, speaker name,
  timestamp, or reference datum and MUST alter no producer evidence or common-
  clock coordinate. Frozen T111/T123 inputs MUST replay deterministically,
  and every changed complete conversation MUST receive chronological and
  reverse contextual semantic review against `test.txt` before retention or a
  new audio run.
- **FR40**: One no-embedding, one-visible-character aligned unit MAY replace a
  conflicting uniform `voiceprint_direct_*` label with its exact primary
  identity only inside a fully corroborated two-unit handoff. Exactly one
  robust-complete VAD MUST contain exactly two positive aligned units across
  any ASR partition. Each unit MUST be followed by configured punctuation;
  their raw gap MUST be at least the existing alignment-pause value and, after
  subtracting at most the existing alignment-boundary tolerance, remain below
  the existing primary-consensus minimum. Unique non-overlapping primary runs
  MUST name A then B over the two units. Exactly one activity local slot
  carrying A MUST cover both units without competing activity. Both VAD
  galleries MUST rank B first and A second and pass the unchanged short score
  and margin gates. Only a candidate whose current direct label differs from
  its exact primary identity may change, and the write MUST stop after its
  immediately following configured punctuation. Missing, duplicate,
  differently ranked,
  differently gated, embedded, mixed, protected, additional-unit, competing,
  or source/time-inconsistent evidence MUST preserve existing behavior. FR40
  MUST add no TOML value, threshold, score, margin, duration, transcript
  lookup, speaker name, known timestamp, reference datum, or fitted constant
  and MUST alter no producer evidence or common-clock coordinate. Frozen
  T111/T123 inputs MUST replay deterministically, and every changed complete
  conversation MUST receive chronological and reverse contextual semantic
  review against `test.txt` before retention or a new audio run.
- **FR41**: The retained primary-onset aligned-island rule MAY recognize its
  existing paused A-before/B/gapless-A-after topology when alignment exposes
  exactly one visible candidate unit inside B and places the punctuation-
  separated previous unit before B. The nearest previous and following
  positive aligned units MUST belong to the same ASR source. The previous unit
  MUST end before B, meet the existing alignment-pause value, and reach the
  candidate through a nonempty configured-punctuation-only source gap. The
  following unit MUST be source-adjacent to the candidate and begin only after
  B ends. Previous plus candidate units MUST satisfy the existing configured
  minimum aligned-unit count. The candidate MUST be one visible,
  non-whitespace, non-punctuation character; existing labels through it MUST
  be uniform voiceprint-backed A. Every existing duration, primary recovery,
  candidate-activity, competing-identity, previous-VAD pause, containing-VAD
  continuation/completeness, and dual-gallery A-ranking condition MUST remain
  required. Only the exact candidate character may change to B. Missing,
  duplicate, multi-unit, nonpunctuation-gap, short-gap, nonadjacent-following,
  in-run-following, punctuation-target, insufficient-unit, mixed-label,
  primary, activity, or VAD evidence MUST preserve existing behavior. FR41
  MUST add no TOML value, threshold, duration, score, margin, transcript
  lookup, speaker name, known timestamp, reference datum, or fitted constant
  and MUST alter no producer evidence or common-clock coordinate. Frozen
  T111/T123 inputs MUST replay deterministically, and every changed complete
  conversation MUST receive chronological and reverse contextual semantic
  review against `test.txt` before retention or a new audio run. See
  `primary-onset-single-unit-partition-diagnosis-2026-07-19.md`.
- **FR42**: The retained isolated-VAD aligned-island rule MAY recognize one
  alignment-dropout representation when the positive aligned units equal the
  existing configured minimum count and exactly one visible source character
  lies between one neighboring unit pair. Every unit MUST map exactly one
  valid source character, remain temporally ordered, and belong to the current
  ASR source wholly inside the VAD. The missing character MUST be visible,
  non-whitespace, and non-configured-punctuation; all other unit pairs MUST be
  source-contiguous. The temporal gap around the missing character MUST be
  strictly below the existing alignment-pause value. Every existing VAD
  duration, embedding, robust completeness, isolation, preceding/following
  pause, dual-gallery identity/rank/gate, uniform direct-label, single-slot
  activity, competing-activity, covering-primary, and current-identity
  condition MUST remain required. Only the bounded source span from the first
  positive unit through the last positive unit, including the one internal
  missing character, may change. Missing, duplicate, extra-unit,
  multi-character-unit, invalid-source, punctuation-gap, whitespace-gap,
  multi-character-gap, long-time-gap, mixed-label, activity, primary, VAD, or
  gallery evidence MUST preserve existing behavior. FR42 MUST add no TOML
  value, threshold, duration, score, margin, transcript lookup, speaker name,
  known timestamp, reference datum, or fitted constant and MUST alter no
  producer evidence or common-clock coordinate. Frozen T111/T123 inputs MUST
  replay deterministically, and every changed complete conversation MUST
  receive chronological and reverse contextual semantic review against
  `test.txt` before retention or a new audio run. See
  `isolated-vad-single-character-alignment-gap-diagnosis-2026-07-19.md`.
- **FR43**: The retained complete-source aligned-VAD closure MAY recognize one
  zero-duration local-pair-tie representation. Exactly one positive business
  interval MUST lack a positive aligned-unit anchor and MUST map exactly one
  visible, non-whitespace, non-configured-punctuation source character. Every
  such character MUST have exactly one zero-duration unit in the raw alignment
  track. Every other positive business interval MUST be anchored, and all
  intervals MUST partition the complete source exactly once. Every positive
  aligned unit MUST map exactly one valid source character. Unique source-
  adjacent units MUST
  surround the missing character, remain temporally ordered with a gap below
  the existing alignment-pause value, and have their temporal bridge contained
  by the unanchored interval. Every existing complete-source, aligned-duration,
  outer-duration, dual-gallery, short-gate, regular-abstention, label, edge,
  activity, primary, coverage, phrase-cardinality, containing-VAD, and VAD-
  ranking condition MUST remain required. The candidate and incumbent MUST be
  the only locally active and primary identities. Both phrase galleries MUST
  remain score-eligible and margin-abstaining, agree on the same top identity
  outside that local pair, and expose candidate and incumbent scores whose
  absolute difference is below the existing configured short margin. The
  nonlocal top identity MUST have no activity or primary coverage inside the
  aligned envelope. Missing, duplicate, multi-character, punctuation,
  whitespace, multiply unanchored, source-inconsistent, overlapping, reversed,
  pause-sized, locally decisive, differently ranked, incomplete, competing,
  or differently gated evidence MUST preserve existing behavior. Only the
  complete source may change. FR43 MUST add no TOML value, threshold, score,
  margin, duration, transcript lookup, speaker name, known timestamp,
  reference datum, or fitted constant and MUST alter no producer evidence or
  common-clock coordinate. Frozen T111/T123 inputs MUST replay
  deterministically, and every changed complete conversation MUST receive
  chronological and reverse contextual semantic review against `test.txt`
  before retention or a new audio run. See
  `complete-source-local-pair-tie-diagnosis-2026-07-19.md`.
- **FR44**: A generic regular-duration punctuation-phrase write MAY abstain on
  one three-run middle-slot representation. The source MUST contain exactly
  three contiguous known base-identity runs `A-B-A`; both outer runs MUST have
  the same identity, the phrase-selected identity MUST be the middle run, and
  that run MUST contain exactly one visible source character, and the
  configured punctuation set MUST be present. The character
  immediately before, inside, and after the middle run MUST have positive
  alignment and typed primary tie-break/refinement provenance. Unique primary
  segments at their alignment midpoints MUST form the same ordered `A-B-A`
  sequence, and the middle primary segment MUST meet the existing configured
  primary-consensus duration. Each outer run MUST expose at least that duration
  through positive aligned time and through matching activity and primary
  coverage on those intervals. The session phrase view MUST pass the existing
  regular score and margin gates. The complete robust gallery MUST have the
  same unique raw top identity and pass the existing regular margin gate, but
  MUST abstain only on the existing regular score gate. Exactly two positive,
  ordered, non-overlapping VAD records MUST overlap the phrase; neither may
  contain it, the phrase MUST begin inside the first and end inside the second,
  and both VAD galleries MUST rank the outer identity first on the first VAD
  and the selected middle identity first on the second VAD under the existing
  margin gate. Missing, duplicate, tied, differently ranked, unknown,
  multi-character, unaligned, zero-duration, source-inconsistent,
  under-covered, overlapping, reversed, containing-VAD, short-duration, or
  differently gated evidence MUST preserve existing behavior. FR44 MUST only
  prevent the generic phrase write; it MUST assign no identity, move no source
  or common-clock boundary, and add no TOML value, threshold, transcript,
  speaker name, timestamp, reference datum, or fitted constant. Frozen T111
  and T123 inputs MUST replay deterministically, and every changed complete
  conversation MUST receive chronological and reverse contextual semantic
  review against `test.txt` before retention or a new audio run. See
  `three-run-middle-slot-phrase-abstention-diagnosis-2026-07-19.md`.
- **FR45 evidence gate**: No alignment-gap speaker repair MAY be specified from
  the primary/activity topology alone. `ref-0066` has no T123 ASR source span
  and MUST remain outside the business projector unless an upstream producer
  supplies content or a separately specified speech-only product contract.
  For source-present `ref-0192`, the exact primary/activity island MAY be
  considered only after a read-only identity replay starts from the captured
  empty-registry state, consumes the original chronological diar snapshots,
  mechanically reproduces their identity assignments, and displays complete
  session and robust gallery evidence for that exact common-clock interval.
  The probe MUST preserve its existing final-segment mode, reject malformed or
  unordered snapshots, and reject full-audio preload when configured retention
  cannot hold the complete session. Producer equality, hashes, and embedding
  scores are mechanical evidence only. A product candidate requires separate
  specification and complete forward/reverse contextual semantic review; no
  code may select it or issue a verdict. See
  `primary-island-alignment-gap-evidence-diagnosis-2026-07-19.md`.
- **FR45**: The typed speaker-evidence snapshot MUST include the immutable
  primary-speaker track, and typed voiceprint evidence MUST preserve explicit
  session-gallery completeness. The producer MAY emit a
  `primary_alignment_gap_echo` query only for one unambiguous source/time
  mapping. Its exact short middle primary run MUST meet the existing primary
  minimum and remain below the existing short-span ceiling, be bracketed within
  the existing configured alignment-boundary tolerance by two primary runs
  with the same nonempty identity distinct from the middle, and lie wholly
  inside exactly one temporal gap between
  consecutive positive aligned characters of one punctuation phrase. The
  immediately following punctuation phrase MUST be source-contiguous and its
  visible non-punctuation/non-whitespace codepoints MUST form a nonempty strict
  suffix of the preceding phrase's visible codepoints. The following phrase
  MUST itself retain positive alignment. Multiple matching islands, alignment
  gaps, phrase pairs, or source mappings MUST emit no query. The query MUST use
  the exact middle primary interval as its acoustic range and the complete
  following phrase as its source range; source repetition locates source only
  and MUST NOT choose an identity.

  Fusion MAY rewrite that source range only when the query embedding is
  available; both session and robust galleries are explicitly complete;
  their nonempty identity lists are unique, equal, and cover every active
  session identity; and both galleries independently pass the existing short
  score and margin gates with the same top identity. That identity MUST equal
  the unique middle primary identity and its matching activity slot MUST cover
  the full exact island. The two unique adjacent outer primary runs MUST share
  one identity distinct from the middle, each meet the existing primary
  minimum, and each be fully covered by matching activity. The target source
  MUST remain uniformly assigned to that outer identity through only baseline,
  primary arbitration, or generic direct-short evidence. The policy MUST
  independently reconstruct the suffix phrase, alignment-gap, primary, and
  activity topology rather than trust the evidence kind alone. Missing,
  duplicate, tied, incomplete, differently ranked, differently gated,
  non-suffix, unaligned, non-bracketed, overlapping, source-inconsistent,
  already-specialized, or competing evidence MUST preserve existing behavior.
  FR45 MUST change only target speaker label, identity, support audit, and
  reason. It MUST add no TOML value, threshold, fitted constant, transcript
  lookup, speaker name, known timestamp, reference datum, model change,
  producer/common-clock boundary, or automated product judgment. Frozen T111
  and T123 inputs MUST replay deterministically, and every changed complete
  conversation MUST receive chronological and reverse contextual semantic
  review against `test.txt` before retention or a new audio run. See
  `primary-island-alignment-gap-evidence-diagnosis-2026-07-19.md` and
  `primary-island-alignment-gap-echo-review-2026-07-19.md`.
- **FR46 evidence gate**: No further speaker-fusion rule MAY be specified from
  one residual, one known timestamp, or one manually preferred output label.
  The capture-faithful empty-registry identity replay MUST first display
  explicit session and robust gallery evidence for every positive-duration
  frozen T123 primary-speaker run. The query set MUST be derived only from the
  immutable primary track, preserve its exact common-clock bounds, carry no
  reference speaker or correctness field, and include short or unavailable
  spans rather than filtering them by a desired outcome. Replay MUST use the
  exact streamed PCM and original chronological diar snapshots already proven
  equal to the capture. Automation MAY validate input order, completeness,
  hashes, replay equality, and raw evidence cardinality; it MUST NOT assign a
  speaker-product label, aggregate accuracy, rank a topology, select a rule,
  or issue a verdict.

  Every manually retained FR32-FR45 repair MUST then be reconciled against the
  complete T123 error ledger to establish the current residual set without
  code. Every remaining critical context MUST be read completely in forward
  and reverse order against `test.txt` and inspected across final business,
  ASR, forced alignment, activity, primary, VAD, and both complete identity
  galleries on the common clock. Manual diagnosis MUST distinguish at least
  source-absent content, source-present alignment displacement, independently
  corroborated native speaker islands, gallery disagreement, and producer-
  wrong evidence. A shared implementation MAY be specified only if this full
  review establishes a source-free topology across multiple residuals and
  identifies independent abstention controls. Otherwise the evidence phase
  MUST stop without production code or a new audio run.

FR46 completes under the stop branch. Complete chronological and reverse
contextual review finds only `ref-0099` with mutually corroborating correct
source, primary, activity, VAD, and dual-gallery evidence that final fusion
overwrites. The other critical residuals cross distinct source-absence,
alignment-displacement, short-span, disagreement, partial-support, or
producer-wrong boundaries. No shared source-free multi-residual policy is
therefore specified, and FR46 makes no code, TOML, model, audio, ledger, or
acceptance change. See
`session-wide-primary-residual-review-2026-07-19.md`.

- **FR47 orthogonal-evidence gate**: Before another speaker policy is
  specified, the complete four-channel Sortformer v2.1 posterior MUST be
  reconstructed from the exact T123 streamed PCM with the frozen T123 TOML.
  The existing diagnostic probe MUST be run independently at least twice.
  Frame count, channel count, common-clock origin/period/extent, output hash,
  and the mechanical top-1 compression contract MUST be repeatable and MUST
  reproduce the frozen T123 primary-speaker producer before the posterior is
  treated as capture-faithful evidence. The v2.1 weights and exact PCM hashes
  MUST remain those already frozen by T123. No alternate diar model, decoder,
  audio input, CLI tuning value, or reference-derived parameter is permitted.

  For every FR46 critical residual, all four raw Sortformer channels MUST then
  be displayed over the complete conversational context beside the frozen
  local-slot identity history, activity, primary, VAD, exact-span TitaNet
  evidence, ASR, alignment, and final business output. The same display MUST
  include independently accepted neighboring turns as abstention controls.
  The reviewer MUST read all contexts chronologically and in reverse against
  `test.txt` and manually record whether the listener-verified identity is
  absent from Sortformer, present only as an inactive secondary channel,
  present as sustained overlapping activity, displaced in time, or contradicted
  by TitaNet. Automation MAY reproduce, hash, validate, and display these raw
  values only; it MUST NOT label a speaker result, aggregate coverage or
  accuracy, rank a channel or candidate, select a threshold, or issue a
  verdict.

  A production change MAY be specified only if the complete manual review
  establishes one reference-free topology shared by multiple critical
  residuals, identifies independent accepted controls that force abstention,
  and adds no fitted numerical parameter. Existing TOML thresholds MUST be
  reused unless a separately approved component-oracle experiment establishes
  a new value without consulting product outcomes. If the correct identity is
  absent or non-complementary in the raw posterior, or only one residual fits,
  FR47 MUST stop without production code, TOML change, new audio run, ledger
  change, or closure claim.

FR47 completes the manual evidence gate and authorizes only a frozen candidate.
The complete forward and reverse review rejects general secondary-channel,
primary-preference, source-continuity, and wider identity-backfill policies.
It finds one shared bounded topology at `ref-0507` and `ref-0509`: the same
local slot is top-1 or top-2 throughout an exact aligned range, crosses the
existing frame-activity gate, and then receives a different stable global
identity within the existing identity backfill horizon. A candidate MUST reuse
the existing frame gate, horizon, minimum future-epoch duration, phrase bounds,
and forced-alignment ranges. It MUST require matching future primary support,
one uniform conflicting current identity, one unique eligible local slot, and
abstention on any missing frame, rank tie, threshold dropout, identity
ambiguity, or competing slot. It MAY add a TOML boolean but MUST add no numeric
parameter. The raw frame track, producer tracks, identity epochs, source text,
and common-clock coordinates remain immutable. Frozen replay and complete
changed-context forward/reverse review are required before any retention or
audio run. See
`sortformer-v21-orthogonal-context-review-2026-07-19.md`.

The first FR47 frozen implementation is not retainable. Its two deterministic
replays expose posterior overrides in sixteen finalized source records. The
complete chronological and reverse semantic review against `test.txt` finds
that records `175-181`, `253-257`, and `264-271` cross independent speaker
turns before a later identity epoch and introduce wrong attributions. Only the
two changes inside source record `283` match the listener transcript. A retry
MUST therefore keep every future-epoch correction inside one immutable ASR
final: the future epoch MUST begin before that source ends, and both its
existing minimum duration and matching primary support MUST be satisfied
inside the same source bounds. This is a categorical source-ownership and
common-clock invariant, not a fitted duration. It adds no parameter and MUST
still satisfy every earlier FR47 abstention condition. See
`sortformer-v21-posterior-future-epoch-candidate-review-2026-07-19.md`.

The source-bounded FR47 retry passes its frozen gate. The warning-clean build
and all `70/70` CTest entries pass. Two candidate replays are byte-identical,
and a contemporaneous boolean-disabled control is separately byte-identical.
The only revised reason appears on two exact ranges inside source `283`.
Complete forward and reverse reading of that conversation retains Tang
Yunfeng's `5.6个亿` expectation and first `对` while preserving Shi Yi's second
`对，一点影响都没有` and all neighboring turns.

The reviewer then reads all 556 contributions chronologically and again in
reverse fixed windows against `test.txt`. No executable mechanism labels,
counts, compares, ranks, or decides the product result. That review initially
transcribes `521/556`. FR48 later repeats the complete speaker-only review and
corrects only `ref-0375`, whose ASR wording is wrong but whose final canonical
speaker is already correct. The current speaker ledger is `522/556`: 28
confident-wrong, five missing, and one uncertain contribution remain,
including 20 business-critical failures. All complete 600-second and per-
speaker natural-turn floors pass, but critical, confident-wrong, time-based,
independent real-path, holdout, report, and release gates remain open.

FR47 is retained only as a frozen transitional candidate. It MAY enter a clean
real-WebSocket ladder on one committed revision: two independent 120-second
runs, then one 600-second run, then full empty-registry and restarted frozen-
registry runs. Every stage MUST pass its mechanical contracts and complete in-
scope chronological and reverse contextual semantic review before the next
stage. Each full run MUST receive a separate complete 556-contribution review;
frozen replay evidence MUST NOT be reported as a real-path result. See
`post-fr47-residual-reconciliation-2026-07-19.md`.

The first 600-second FR47 promotion attempt exposes a terminal serialization
defect before any contextual review can begin. The server finishes the session
and persists a `speaker_voiceprint` track, but its fixed 256-byte formatting
buffer truncates sufficiently long evidence records, including JSON boolean
tokens. The producer therefore receives no parseable terminal document within
its 600-second wait. This attempt is mechanical failure evidence only and MUST
NOT be treated as a speaker-product result.

Before promotion resumes, every terminal speaker-voiceprint record MUST be
serialized without a fixed output-size assumption. A focused regression MUST
cover evidence and speaker identifiers longer than the former buffer and MUST
preserve escaped strings, complete boolean tokens, score arrays, and the exact
record suffix. The warning-clean build and complete CTest suite MUST pass. The
fix changes no model, fusion policy, time code, speaker assignment, or TOML
parameter. Because it changes the runtime binary, promotion MUST restart from
two independent 120-second runs on one new clean committed revision before a
new 600-second attempt.

Commit `70f1186d2b9e0b1b12808ebc644a164d1e21983c` completes that restarted
promotion. Two independent empty-registry 120-second streams close every
product track at 1,920,000 samples, pass direct-end, common-clock, observer,
provenance, and telemetry contracts, and have the same normalized seven-track
entry bundle. Complete forward and reverse reading of `ref-0001` through
`ref-0018` finds no new contextual speaker regression. A clean 600-second run
then closes all tracks at 9,600,000 samples and passes the same contracts;
complete two-direction reading of `ref-0001` through `ref-0093` preserves the
FR47 contextual ledger and authorizes full A/B.

Full Run A starts from an isolated empty registry. Full Run B restarts the same
binary with only Run A's frozen registry. Both consume exactly 57,841,920 PCM
samples in `3615.120 s` at `0.995x`, close all seven tracks on the common time
base, converge producer and observer terminal documents, provide complete
required telemetry, and deliver parseable terminal JSON in `17.559 s` and
`17.392 s`. The registry remains byte-identical after Run B.

Each full artifact receives its own complete 556-contribution chronological
review and independent reverse-fixed-window review against `test.txt`. No
executable mechanism assigns correctness or aggregates the result. FR48's
fresh speaker-only reread corrects only `ref-0375` in the initially transcribed
FR47 ledger. Both artifacts retain `522/556`: 28 confident-wrong, five missing,
and one uncertain contribution remain, including 20 business-critical
failures. The seven fixed-window and four per-speaker natural-turn floors pass,
and no whole-session permutation or accumulating late identity drift appears.
Critical-attribution-zero, confident-wrong-zero, speaker-time, per-speaker-
time, source-time-offset, locked holdout, report, and release gates remain
open. See `fr47-real-path-promotion-review-2026-07-19.md`.

- **FR48 speaker-only reconciliation and hierarchical-consensus guard gate**:
  The current closing target evaluates the final business view's speaker
  ownership independently from ASR lexical fidelity. A recognizable acoustic
  contribution that is present under the listener-verified canonical speaker
  MUST NOT fail the speaker ledger solely because its decoded words are wrong.
  A contribution that is absent from the final business view, assigned to a
  different speaker, or materially split across wrong speakers remains a
  speaker-business failure. This separation changes no immutable `test.txt`
  field and does not claim ASR accuracy. Because it changes the review rubric,
  both FR47 full artifacts MUST receive a fresh complete 556-contribution
  chronological and reverse-fixed-window reconciliation before any revised
  speaker ledger is reported. No executable mechanism may label, count,
  aggregate, or decide that reconciliation.

  Before another rewrite policy is considered, the exact full Run A and Run B
  typed tracks MUST be replayed to inspect every ordinary short business-
  interval voiceprint decision whose selected identity conflicts with the
  uniform native identity. Automation may list only reference-free structural
  scope. Complete contextual review MUST establish whether one reusable
  evidence-precedence invariant exists and MUST include accepted neighboring
  controls in both directions.

  A frozen guard MAY be implemented only for an ordinary short direct
  voiceprint decision that lacks eligible dual-gallery agreement. The current
  identity MUST be uniform over the source characters and MUST completely and
  uncontestedly cover the exact aligned interval in both Sortformer activity
  and primary views. Exactly one containing VAD query and exactly one complete-
  source query for the same immutable ASR final MUST each have complete session
  and robust galleries whose unique top identities agree with that current
  identity under existing margin gates. The direct candidate MUST differ. Any
  missing range, competing native identity, gallery tie/disagreement,
  incomplete robust gallery, multiple containing query, source mismatch, or
  already eligible exact dual-gallery decision MUST preserve current behavior.

  The guard only prevents a lower-scope overwrite; it does not assign a new
  identity, alter text or time, consume reference data, or mutate a producer
  track. It MUST reuse the existing primary-consensus minimum, short-span
  maximum, gallery margins, alignment tolerance, and typed evidence. It MAY
  add one false-by-default TOML boolean and MUST add no numeric parameter. Both
  full A and B frozen inputs require independent repeated candidate replay and
  a contemporaneous disabled control. Every changed complete conversation MUST
  be read forward and reverse against `test.txt`; if retained, each complete
  556-contribution speaker-only review MUST be repeated before any real-
  WebSocket run. A single-context fit, changed-context regression, or missing
  independent control ends FR48 without an audio run.

  FR48 completes on the stop branch. Both exact full artifacts receive fresh,
  independent 556-contribution chronological and reverse-window speaker-only
  review. Only `ref-0375` changes: its decoded wording is wrong, while its
  final `spk_2` ownership is already 徐子景. The corrected manual ledger is
  `522/556`, with 28 confident-wrong, five missing, one uncertain, and 20
  critical residuals. Direct typed export also exposes and repairs a mechanical
  omission of `session_gallery_complete`, restoring all 1,716 replay business
  entries without changing runtime behavior. Complete forward/reverse review
  of 24 ordinary direct-short/native conflicts finds that accepted direct
  evidence is necessary in many controls; only `ref-0099` has the proposed
  full hierarchy topology. Because that is a single material context, no
  guard, TOML field, candidate replay, or audio run is authorized. See
  `fr48-speaker-only-reconciliation-and-consensus-diagnosis-2026-07-19.md`.
  The exporter-focused suite passes `3/3`, the complete build has no warning
  or error diagnostic, and all `70/70` CTest entries pass in `53.12` seconds.
  These checks validate engineering contracts only and do not produce the
  contextual product verdict.

- **FR49 full-residual short-evidence and source-clock gate**: FR49 MUST keep
  commit `64034af` and the exact FR47 full Run A/B artifacts frozen. It MUST
  investigate all 34 manually signed speaker residuals, not only the 20
  critical rows or a selected tail window. Each residual and its accepted
  neighboring controls MUST be read in complete chronological and reverse
  conversational context against `test.txt`, while ASR source, forced
  alignment, activity, primary, four-channel Sortformer posterior, identity
  epochs, VAD, session/robust galleries, and final business output remain on
  the common clock.

  Automation MAY emit a reference-free inventory of every short primary island
  that intersects a positive aligned unit, a zero-duration unit, or a gap
  between adjacent aligned units and whose identity conflicts with at least one
  mechanically associated current business piece. The inventory MUST include
  the immediate same-source and adjacent primary/business controls. It MAY copy
  and order raw typed evidence and verify hashes and source/time reconstruction.
  It MUST NOT read `test.txt`,
  attach a reference ID or expected speaker, label correctness, aggregate a
  result, rank a topology, select a candidate, or issue a verdict. The reviewer
  MUST inspect every inventory match and its controls in both directions; a
  residual-only list is insufficient.

  A product rule MAY be specified only when at least two independent material
  contributions from different conversational contexts share the same exact
  reference-free topology and independently reviewed accepted controls define
  its abstention boundary. The rule MUST preserve immutable source text and
  producer times, MUST NOT synthesize a contribution that ASR did not retain,
  MUST reuse existing TOML-owned durations, thresholds, margins, punctuation,
  and common-clock tolerances, and MUST add no fitted numeric parameter. A
  source-absent row remains diagnostic unless an existing typed source range
  can be located without reference data. A single-context fit, inconsistent
  A/B evidence, or any accepted-control conflict ends FR49 before product code,
  TOML changes, candidate replay, or a new audio run.

  The complete FR49 review authorizes exactly one bounded topology, named the
  source-leading right-bounded primary-prefix restore. It applies only when a
  short Sortformer primary run is at least `speaker_fusion.min_embed_sec` and
  shorter than `speaker_fusion.short_max_sec`; the same identity completely
  covers that run in the activity view; and one exact `primary_run` TitaNet
  query has complete session and robust galleries that both rank that identity
  first, pass both configured score gates, and pass at least one configured
  margin gate. A different immediately following primary run MUST start at the
  same boundary, last at least `speaker_fusion.min_embed_sec`, and be completely
  covered by its own activity identity.

  The writable source range MUST be one existing leading business interval
  made entirely of positive-duration forced-alignment characters. Its complete
  aligned time MUST be contained by the short primary run. The current final
  labels over that range MUST be one uniform voiceprint identity equal to the
  following primary identity, and the immediately preceding source character
  MUST not have that following identity. Before the writable prefix, no
  positive-duration source character whose aligned interval overlaps the same
  short primary MAY already have the short-primary candidate identity. This
  establishes a swallowed candidate ownership island rather than extending an
  already retained candidate tail into the next contribution. Exactly one
  source-adjacent following business interval MUST retain the following-primary
  identity. The temporal gap between the two source intervals MUST be positive
  and no larger than
  `timeline.align_snap_pause_sec`; the short-primary end MUST remain inside
  that gap within `timeline.align_boundary_split_tolerance_sec`. No third
  activity identity may overlap the writable aligned prefix. The following
  business interval MUST be contained by the following primary run.

  When every condition holds, only the existing characters in the leading
  business interval MAY be restored to the short-primary identity. Producer
  intervals, source offsets, text, and all other labels remain immutable.
  Zero-duration-only, source-gap-only, interior-phrase, subminimum-primary,
  noncontiguous-primary, gallery-incomplete, dual-gallery-disagreement,
  both-margin-fail, extra-activity, candidate-source-already-present, and
  source-absent cases MUST abstain. The behavior and its matching `primary_run`
  evidence production MUST share one false-by-default TOML switch,
  `speaker_fusion.source_leading_primary_prefix_enable`; no numeric setting is
  added or changed.

  The corrected implementation passes the frozen reproducibility and semantic
  review gates. Candidate A and B each replay byte-identically twice with 1,718
  business entries; their shared SHA-256 is
  `91e1e7ab08f6c593b73762b158b5c4ee9c58eaf68ea59eb8f9ee34c21f747c30`.
  Independently generated disabled A/B controls contain 1,716 entries, share
  SHA-256
  `75fc0b39fdf4530ec98a54f8e6ac113e8eef1aee00839c3d9c6577adafb8302e`,
  and reproduce the FR48 baseline. The raw changed scope contains only
  `467.564-467.644` and `817.692-818.412`; the rejected `ref-0304` tail leak is
  absent.

  Every one of the 556 reference contributions is read chronologically and in
  reverse fixed windows for candidate A and then independently for candidate B.
  Only `ref-0061` and `ref-0121` move from confident-wrong to accepted. The
  manually transcribed frozen ledger is `523/556`, with 27 confident-wrong, five
  missing, one uncertain, and the same 20 critical residuals. This retains FR49
  as frozen policy evidence only; it does not establish a new real-WebSocket or
  closing result. The final clean build emits no warning or error diagnostic,
  all `71/71` CTest entries pass in `53.22 s`, and both new Python files pass
  syntax compilation. These are engineering checks only. See
  `fr49-source-leading-primary-prefix-diagnosis-2026-07-19.md`.

- **FR49 real-path promotion gate**: The retained FR49 policy MUST be committed
  and pushed before new audio is streamed. One exact clean revision and the
  checked-in `orator.toml` MUST then run two independent 120-second production
  WebSocket captures at `1.0x` with 100 ms frames, direct `end`, observers,
  required runtime/device telemetry, and a genuinely empty isolated registry
  for each run. Automation MAY verify transport, JSON, provenance, common-
  clock, terminal, observer, telemetry, raw repeatability, and artifact-
  identity contracts only. Each artifact MUST receive its own complete
  chronological and reverse contextual semantic review of every in-scope
  `test.txt` contribution before FR49 may advance. Any contextual speaker
  regression stops promotion for diagnosis. Only if both 120-second reviews
  retain FR49 MAY one 600-second run execute under the same contracts and
  receive complete two-direction in-scope review. Only if that review retains
  FR49 MAY full Run A start from an empty registry and full Run B restart from
  Run A's frozen registry. Each full artifact MUST receive an independent
  556-contribution chronological and reverse fixed-window contextual semantic
  review. No executable result, hash, equality check, metric, or mechanical
  contract may promote, reject, or score the candidate. The pre-existing
  business storage MUST be preserved and restored after isolated validation.

  The gate is complete on clean pushed commit `1f09052`. Two restarted
  120-second runs and one 600-second run pass their mechanical contracts and
  their separate complete in-scope chronological/reverse contextual reviews.
  The 120-second scope is `ref-0001` through `ref-0018`; `ref-0061` is checked
  at 600 seconds and `ref-0121` in the full review. Full Run A starts empty and
  Run B restarts with only A's frozen registry. Both consume exactly
  57,841,920 samples, close all seven common-clock extents, converge producer
  and observers, provide complete required telemetry, and finish direct-end in
  `29.015 s` and `28.820 s`. The small margin below 30 seconds remains an
  operational follow-up, not a product-accuracy judgment.

  Every one of the 556 contributions in full A is read chronologically and in
  reverse fixed windows, and full B is independently read the same two ways.
  All four complete contextual readings retain only the `ref-0061` and
  `ref-0121` repairs. The manually signed real-path ledger is `523/556`, with
  27 confident-wrong, five missing, one uncertain, and the same 20 critical
  residuals. No whole-session identity permutation, accumulating late drift,
  or new tail-only regression is found. FR49 is therefore the current
  repeatable real-WebSocket candidate, but speaker-business closure remains
  open. See `fr49-real-path-promotion-review-2026-07-20.md`.

- **FR50 full-residual earliest-evidence-loss audit**: FR50 MUST freeze
  documentation commit `ae522cc`, the exact FR49 full Run A/B artifacts
  captured on clean implementation commit `1f09052`, checked-in
  `orator.toml`, `test.mp3`, `test.txt`, and both isolated registry states.
  Its scope is the manually transcribed FR49 ledger of 33 residual references,
  including the separately signed 20-critical subset. This inventory freezes
  the audit population only; it MUST NOT be treated as an executable score or
  as completion of the all-turn criticality gate.

  A display-only worksheet tool MAY arrange evidence for manually authored
  context bounds and accepted-control IDs. It MUST copy raw values without
  assigning an expected speaker, correctness label, causal class, score, rank,
  candidate identity, aggregate, or verdict. Each independent A/B packet MUST
  include the complete reference context, final business pieces and decision
  audit, ASR, forced alignment, VAD, diarization activity, primary speaker,
  intersecting and source-related TitaNet evidence, local identity epochs, and
  all four Sortformer posterior channels on the common time base.

  Before the frozen T191 posterior can be reused, mechanical provenance MUST
  establish that its model, TOML, PCM, local-slot ordering, frame extent, and
  top-1 primary compression are the exact producer inputs and output of both
  FR49 artifacts. A mismatch requires two repeated raw captures from the exact
  current PCM and checked-in TOML. This check may establish identity and
  repeatability only; it may not interpret speaker correctness.

  A reviewer MUST read every residual and its controls against `test.txt`
  chronologically for A and independently for B, then repeat both complete
  reviews in reverse context order. Only those four complete contextual
  semantic readings may identify the earliest layer where useful
  speaker-business evidence becomes absent, displaced, contradictory, or
  overwritten. Any causal grouping, count, prioritization, candidate choice,
  or acceptance decision MUST be manually derived and recorded.

  FR50 MAY authorize a later implementation specification only when the four
  readings find one reference-free, low-blast final-fusion topology shared by
  at least two independent material contexts and bounded by explicit accepted
  controls. Source-absent evidence MUST NOT be synthesized. If the earliest
  loss is in ASR, VAD, or forced alignment, that upstream contract MUST be
  specified, validated, and frozen before the final speaker-business seal.
  Otherwise FR50 stops without production code, TOML, model, audio replay,
  ledger, or closure change.

  The FR50 capture gate is complete. Because the historical T191 TOML hash
  differs from the current checked-in TOML, the exact PCM is run twice through
  the current v2.1 probe. Both executions emit byte-identical 45,189-row raw
  frame files and 755 segment rows. Mechanical `0.5` top-1 compression
  independently reproduces all 1,348 ordered local-slot primary runs in each
  FR49 artifact within the frozen serialization tolerance. Separate A/B
  worksheet trees each contain all 33 contexts, every required track, content
  manifests, and no empty file; repeated exports are deterministic. These are
  provenance and evidence-availability findings only. No contextual causal
  reading, product change, or closure decision has yet occurred. See
  `fr50-full-residual-evidence-capture-2026-07-23.md`.

  The four complete contextual-semantic readings are now manually reconciled.
  T228 authorizes one **experimental, right-bounded short-primary aligned-unit
  restore** because `ref-0327` and `ref-0417` independently expose the same
  reference-free final-reconstruction topology. The authorization is not a
  product acceptance and does not change the FR49 ledger.

  The experiment MUST be disabled by the C++ default and enabled only through
  `speaker_fusion.right_bounded_short_primary_unit_enable` in `orator.toml`.
  It MUST reuse the existing `min_embed_sec`, `short_max_sec`, punctuation,
  alignment, activity, primary, and identity-epoch contracts; it MUST add no
  numeric tuning parameter. Duration gates use only one common-time-base sample
  as serialization tolerance. For one source, it MAY restore exactly one aligned
  lexical codepoint, plus only its immediately trailing configured
  punctuation, from incumbent identity A to primary identity B only when all
  of the following raw conditions hold:

  - one and only one mapped primary-B run intersects the lexical unit; its
    duration is at least `min_embed_sec` and strictly below `short_max_sec`;
  - the lexical unit is wholly contained by that run, or it is one unique
    zero-duration alignment point strictly inside that run; no duplicate,
    multi-codepoint, or second wholly contained aligned unit may overlap the
    run. A positive-duration unit must be strictly longer than the existing
    `align_boundary_split_tolerance_sec`; a boundary-scale positive unit
    abstains. The sole permitted partial overlap is the immediately following
    A codepoint starting at that unique zero point and crossing B's right
    boundary; every other partial overlap abstains;
  - thresholded activity contains no B interval overlapping the candidate;
    this explicitly identifies a primary-versus-activity filtering boundary,
    rather than treating the two correlated Sortformer views as independent
    votes;
  - every writable source codepoint currently has one known identity A with
    reason `voiceprint_direct_regular`, and A differs from B;
  - exactly one primary-A run starts at the candidate's right boundary, lasts
    at least `min_embed_sec`, and is fully covered by thresholded activity A;
    the immediately following non-punctuation source codepoint also remains A;
  - B is already a known global identity from the current immutable speaker
    evidence. The policy may relabel existing text only; it may not synthesize
    text, time, identity, or producer evidence.

  The experiment MUST abstain when disabled; on source-absent text; on missing,
  duplicate, multi-codepoint, or ambiguous alignment; on any partial overlap
  other than the exact zero-point/immediate-A-successor form above; when a
  second aligned unit is wholly contained by the short primary; when B activity is present; when
  the right-bounded primary/activity continuation is missing, duplicated,
  short, gapped, or owned by another identity; when the incumbent is unknown,
  mixed, non-voiceprint, or not `voiceprint_direct_regular`; when B is already
  retained; or when any third-identity evidence enters the exact writable
  range. `ref-0118`, `ref-0135`, `ref-0102`, `ref-0049`, `ref-0390`, and all
  source-absent contexts remain explicit contextual abstention boundaries.
  The accepted Tang continuation at `2241.356-2241.436 s`, discovered by the
  frozen candidate replay, is an additional mandatory abstention control: its
  positive aligned unit is exactly boundary-scale and MUST remain Tang.

  Focused tests may verify only these structural gates, immutable source/time
  preservation, deterministic projection, reason/source diagnostics, and
  disabled-default behavior. A frozen FR49 A/B replay may display changed
  contexts but may not score, rank, select, or accept the experiment. Any
  product judgment requires complete manual contextual-semantic reading of all
  changed contexts and accepted controls, followed by independent complete A/B
  chronological and reverse review against `test.txt`. A real audio run is
  prohibited until that frozen-evidence review authorizes the next gate.

- **FR50 direct-end margin remediation gate**: The first authorized FR50 real-
  WebSocket ladder MUST remain non-promotable because full empty-registry Run A
  reaches its complete terminal document `30.144 s` after direct `end`, above
  the existing `30.0 s` mechanical limit. Run B reaches the same boundary in
  `29.258 s`; passing one restarted run does not compensate for the failed A
  run. Both full artifacts have nevertheless completed their independent
  chronological and reverse contextual-semantic reviews. Those readings MAY
  establish product-output observations for the captured source revision, but
  they MUST NOT waive or average the failed latency gate.

  The remediation MUST address the terminal critical path without increasing
  the client timeout or changing a model, speaker decision, producer value,
  common-clock coordinate, or business parameter. FR49 introduced source-
  independent `primary_run` voiceprint queries, while the current controller
  constructs the primary track only after all producers and alignment have
  drained. The full artifact contains 1,348 such runs, so their acoustic
  embeddings cannot enter the existing low-priority live precompute path and
  first become eligible during finalization.

  Completed raw Sortformer top-1 runs MAY therefore be queued for acoustic-only
  TitaNet precomputation as their immutable frame blocks arrive. Run formation
  MUST use only each block's common-clock start, frame period, reset-aware local
  slot, and the existing TOML activity threshold. It MUST NOT read mutable
  identity epochs from the background thread. The final primary track and final
  gallery scoring remain constructed in the existing finalization order; an
  early embedding cache hit may remove work only, never supply a speaker vote.
  Because extraction is acoustic-only, its population gate MUST depend on the
  existing minimum number of distinct typed diarization-local speaker tracks,
  not on global identity/gallery maturity. The unchanged final evidence stage
  MUST still require mature global identities before it emits scores.
  Per-cycle work remains bounded by the existing TOML
  `speaker_fusion.precompute_interval_sec` and
  `speaker_fusion.precompute_max_spans_per_cycle` settings. No new numeric
  parameter is authorized.

  Focused tests MUST prove block-boundary run coalescing, activity and local-
  slot boundaries, final trailing-run drain, disabled behavior, bounded live
  cycles, and cached/uncached final evidence identity. Mechanical diagnostics
  MUST separate live and final-drain precompute work. After a warning-clean
  build and full CTest, the exact clean revision MUST repeat the 120-second,
  600-second, full empty-registry A, and restarted frozen-registry B ladder.
  Every output-affecting contextual gate remains subject to complete semantic
  review under Article VI; no timing, hash, equality check, or executable result
  may promote or accept the candidate.

  T232C now satisfies this remediation gate on exact clean pushed commit
  `a6f0d33730326b19a3831019b1aba21fd900f126`. The 120-second A/B and
  600-second production-WebSocket runs complete their full in-scope forward
  and reverse contextual readings. Full empty-registry A and restarted
  frozen-registry B each complete all 3,615.120 seconds at `0.993x`; their
  direct-end waits are independently `26.013 s` and `26.789 s`, below the
  unchanged `30.0 s` limit. Each full artifact receives complete
  chronological and reverse fixed-window reading of all 556 reference
  contributions. Those readings manually retain the FR50 `525/556`
  interpretation, including the repairs at `ref-0327` and `ref-0417`, with no
  whole-session permutation, accumulating drift, or tail-only collapse.
  T232 therefore promotes FR50 as the current real-path speaker baseline. The
  remaining 19 business-critical residuals and the other Spec 013 gates keep
  canonical speaker-business closure open.

FR40 passes its frozen gate. Repeated T123 outputs are byte-identical and
change only Xu Zijing's `184.240-184.320` response. Repeated T111 outputs are
separately byte-identical and split only Zhu Jie's first reaction from the
merged two-reaction source tail. Complete forward and reverse reading of
`02:07-03:45` retains the common Zhu-to-Xu-to-Zhu sequence and finds no
neighboring contribution change. The current T123-derived manual ledger
advances by only `ref-0025` to `514/556`; T111's complementary `ref-0024`
repair is partition evidence and is not counted again. Zhu Jie recall and all
other conjunctive closing failures remain open. See
`two-unit-primary-handoff-review-2026-07-19.md`.

FR41 passes its frozen gate. Final clean-binary T123 replays are byte-identical
and split only the two-reaction `ref-0268` source span: the first `啊` moves to
Zhu Jie while the second `啊` and Xu Zijing's continuation remain with Xu.
Repeated T111 output is separately byte-identical and unchanged from FR40.
Complete forward and reverse reading of `31:17-33:23` retains the
Xu-to-Zhu-to-Xu handoff and finds no neighboring contribution change. Only
current T123 `ref-0268` advances the manually reconciled frozen ledger to
`515/556`; T111 is partition evidence and is not double-counted. The Zhu Jie
natural-turn ledger advances to `75/83`, so all four per-speaker natural-turn
floors now pass. Critical attribution, confident-wrong attribution, time-based
evidence, real-path repeatability, and holdout gates remain open. See
`primary-onset-single-unit-partition-review-2026-07-19.md`.

FR42 passes its frozen gate. Final clean-binary T123 replays are byte-identical
and split only the `ref-0432` response source: the bounded `什么意` fragment
moves to Zhu Jie, while the unsupported zero-duration `思？` suffix remains
with Tang Yunfeng as an explicit source-time residual. T111 is separately
byte-identical and unchanged from FR41. Complete forward and reverse reading
of `45:47-49:42` retains the Tang-to-Zhu-to-Tang valuation handoff and finds no
neighboring contribution change. Only current T123 `ref-0432` advances the
manually reconciled frozen ledger to `516/556`; T111 is partition evidence and
is not double-counted. Zhu Jie advances to `76/83`, and the 2400-3000 block
advances to `118/129`. Critical attribution, confident-wrong attribution,
time-based evidence, real-path repeatability, and holdout gates remain open.
See
`isolated-vad-single-character-alignment-gap-review-2026-07-19.md`.

FR43 passes its frozen gate. Final clean-binary T123 replays are byte-identical
and merge only the `ref-0194` response source under Xu Zijing. T111 is
separately byte-identical and unchanged from FR42. Complete forward and reverse
reading of `20:07-22:56` retains the Shi-to-Xu-to-Tang response sequence and
finds no neighboring contribution change. Only current T123 `ref-0194`
advances the manually reconciled frozen ledger to `517/556`; T111 is partition
evidence and is not double-counted. The 1200-1800 block advances to `75/80`,
and Xu Zijing advances to `69/73`. Critical attribution, confident-wrong
attribution, time-based evidence, real-path repeatability, and holdout gates
remain open. See
`complete-source-local-pair-tie-review-2026-07-19.md`.

FR44 passes its frozen gate. Final clean-binary T123 replays are byte-identical
and change only `text_id=43`: the existing Shi-Tang-Shi base sequence is no
longer replaced by the generic Tang session-only phrase. T111 is separately
byte-identical and unchanged from FR43. Complete forward and reverse reading
of `07:42-08:33` retains Shi Yi's calculation, Tang Yunfeng's short
interjection, Shi Yi's `44/45` answer, and Tang Yunfeng's veto statement. Only
current T123 `ref-0071` advances the manually reconciled frozen ledger to
`518/556`; neighboring source partition changes are not additional natural-
turn repairs. The 0-600 block advances to `88/93`, and Shi Yi advances to
`199/211`. Critical attribution, confident-wrong attribution, time-based
evidence, real-path repeatability, and holdout gates remain open. See
`three-run-middle-slot-phrase-abstention-review-2026-07-19.md`.

FR45 passes its frozen gate. The final complete build has no warning/error
diagnostic and all `70/70` CTest entries pass. Final T123 replays are byte-
identical at
`5a595ca1aa5816612b2603062d8467ee60bc3a342219cf5eda066cfddc3bb61a`
and split only `text_id=111`: `没有意见，` moves to `spk_0`, while
`赶紧说，快！` remains with `spk_3`. T111 is separately byte-identical and
unchanged from FR44 at
`ad2abee782ab30ff67be1a86fa46f4ec0c16b5422ae18954107c398131157aa4`.
Complete forward and reverse reading of `20:33-22:42` retains Zhu Jie's
independent response between Shi Yi's answer and request. Only current T123
`ref-0192` advances the manually reconciled frozen ledger to `519/556`, the
1200-1800 block to `76/80`, and Zhu Jie to `77/83`. The output preserves
existing forced-alignment times, so speaker-time and source-time-offset gates
remain open. Critical attribution, confident-wrong attribution, real-path
repeatability, and holdout gates also remain open. No new real-WebSocket result
or closure claim is attributed to FR45. See
`primary-island-alignment-gap-echo-review-2026-07-19.md`.

The first FR31 implementation passed its focused engineering test and produced
byte-stable repeated T111 and T123 replays. Complete forward and reverse review
of every changed source context rejected it: true short returns were restored,
but the same A-B-A/activity topology also preserved primary boundary leakage
inside uninterrupted contributions by Xu Zijing, Tang Yunfeng, and Shi Yi.
FR31 therefore does not enter production or a new audio run. See
`corroborated-primary-return-review-2026-07-19.md`.

FR32 then produces byte-stable repeated frozen replays. T111 remains unchanged;
T123 changes only the source containing `ref-0154`. Complete forward and
reverse contextual review confirms that the exact `不含` answer returns to Tang
Yunfeng while Shi Yi's question and all following contributions remain
unchanged. The clean build has no warning/error lines and all 69 CTest entries
pass as engineering evidence only. FR32 is retained for the real-WebSocket
promotion ladder; this does not close any full-session acceptance gate. See
`exact-cross-scale-primary-return-review-2026-07-19.md`.

T142 subsequently completes FR32's real-WebSocket promotion ladder from clean
transitional commit `72d81c8084757b4c4210ba90ac14b5d1c1155e89`.
The silence, repeated 120-second, and complete 600-second gates pass their
mechanical contracts and complete in-scope contextual reviews. Independent
full empty-registry Run A and restarted frozen-registry Run B are byte-stable
across the seven normalized product tracks, close all active tracks at
`57,841,920` samples, remain at `0.995x`, and terminate in `16.768 s` and
`16.684 s`. Every one of the 556 contributions in each run is read in complete
chronological context and again in reverse 600-second blocks. Both reviews
manually record `506/556`: FR32 repairs only `ref-0154`, with no new
contextual regression or long-session identity swap. The 2400-3000 and
3000-3600 blocks, 朱杰 recall, critical-turn, and confident-wrong gates still
fail. FR32 is therefore retained as a bounded repeatable repair, while
speaker-business closure remains open. See
`exact-cross-scale-primary-return-full-promotion-review-2026-07-19.md`.

The frozen T123 diagnosis after T142 identifies a separate partition-sensitive
abstention at `ref-0517`. Activity and primary uniformly support Tang Yunfeng.
The short phrase's session gallery alone selects Zhu Jie, its robust gallery
abstains, the unique containing VAD reverses to Tang in the robust view, and
both broader same-text scales rank Tang first in both galleries while failing
only the unchanged regular absolute-score gate. FR33 is therefore specified as
a preservation rule over the complete typed topology, not a new speaker
override or threshold adjustment. Implementation, replay, and complete
changed-context review follow the same frozen-evidence gate. Repeated T123
replays are byte-identical and change only `text_id=289`; repeated T111
replays are byte-identical to FR32. Complete forward and reverse reading of
`ref-0508` through `ref-0525` confirms that the changed phrase is Tang
Yunfeng's `ref-0517` question, while Shi Yi's preceding `ref-0516` and every
following contribution remain unchanged. FR33 is retained on frozen evidence,
moving the manually reconciled frozen candidate to `507/556`. The two late
fixed blocks, 朱杰 recall, critical-turn, and confident-wrong gates continue to
fail, and no new real-path result is attributed to FR33. See
`partition-invariant-cross-scale-abstention-diagnosis-2026-07-19.md`.
See also
`partition-invariant-cross-scale-abstention-review-2026-07-19.md`.

The next frozen diagnosis isolates a different `ref-0406` topology. A broad
same-text business interval directly writes Zhu Jie across three adjacent
contributions, while the exact short phrase and its unique containing VAD both
select Tang Yunfeng in both galleries under existing gates. Tang activity
covers the exact phrase; the overlapping activity/primary arbitration selects
Xu Zijing, so no native top-one view alone supplies the answer. FR34 therefore
tests only the pairwise-distinct coarse-direct, exact-acoustic, and primary-
conflict topology. It does not restore the archived FR16AAG experiment or
claim its incomplete historical review. See
`exact-phrase-vad-direct-conflict-diagnosis-2026-07-19.md`.

Repeated FR34 T123 replays are byte-identical and split only the
`text_id=236` direct interval: `才十个工作日，` moves to Tang Yunfeng while
the preceding `对，`, Zhu Jie's question, and Zhu Jie's following continuation
remain unchanged. T111 remains byte-identical to FR33. Complete reading of the
changed `44:28-46:57` conversation chronologically and in reverse retains the
exact phrase repair. The remaining `2744.940-2745.100` `对，` attribution is
recorded as a `0.160 s` source-time boundary residual; it does not erase the
contextually complete ten-working-day answer, and it is not hidden from the
still-open speaker-time gate. The warning-clean build and all 69 CTest entries
pass as engineering evidence. The manually reconciled frozen candidate is
`508/556`; both late fixed blocks, Zhu Jie recall, critical attribution,
confident-wrong attribution, and speaker-time sign-off still fail. No new
real-WebSocket result or closure claim is attributed to FR34. See
`exact-phrase-vad-direct-conflict-review-2026-07-19.md`.

FR35 returns to the frozen T111/T123 partition regression at `ref-0420`. The
activity, primary, VAD, initial identity, and TOML evidence are unchanged, but
T123 extends the no-embedding aligned `嗯` through punctuation and leaves a
`0.240 s` following gap, `0.010 s` below the configured `0.25 s` isolation
pause. The same boundary moves by less than the existing configured `0.08 s`
alignment tolerance. FR35 therefore applies that tolerance only to the two
neighbour-gap checks in the already-conjunctive isolated-unit/initial-slot/VAD
rule. Every identity, activity, primary, embedding, uniqueness, and dual-gallery
gate remains unchanged. No TOML value or acoustic threshold changes. See
`aligned-unit-isolation-tolerance-diagnosis-2026-07-19.md`.

Repeated FR35 T123 replays are byte-identical and change only the isolated
`text_id=242` response: `2809.436-2809.676` `嗯` moves from Tang Yunfeng to
Zhu Jie while the punctuation-only residual and every neighbouring
substantive contribution remain unchanged. T111 remains byte-identical to
FR34. Complete reading of the changed `46:19-47:53` conversation
chronologically and in reverse confirms the Tang instruction, Zhu
acknowledgement, Shi confirmation question, and Zhu explanation. The
warning-clean build and all 69 CTest entries pass as engineering evidence. The
manually reconciled frozen candidate is `509/556`; the 2400-3000 and
3000-3600 fixed blocks, Zhu Jie recall, critical attribution,
confident-wrong attribution, and speaker-time sign-off still fail. No new
real-WebSocket result or closure claim is attributed to FR35. See
`aligned-unit-isolation-tolerance-review-2026-07-19.md`.

FR36 isolates the separate T111/T123 partition regression at `ref-0350`.
Activity and primary are identical and expose one uncontested local slot whose
current identity is Tang Yunfeng and whose initial identity is Zhu Jie. T123's
exact phrase ranks the current identity first in both galleries, while its
unique containing VAD and complete-source evidence reverse to the initial
identity in all four outer views; all six views abstain under the existing
regular gates in the strict pattern specified above. T111 includes a trailing
discourse particle in the punctuation phrase, ranks the initial identity first
in both phrase galleries, and activates the existing regular initial-slot
rule. FR36 tests only this same-slot six-view partition-invariant reversal. It
uses no future epoch, new threshold, TOML change, transcript value, or known
time. See
`partition-invariant-regular-initial-slot-diagnosis-2026-07-19.md`.

Repeated FR36 T123 replays are byte-identical and change only the
`2483.660-2485.500` `text_id=217` phrase from Tang Yunfeng to Zhu Jie. T111
remains byte-identical to FR35. Complete reading of the changed
`40:45-42:32` conversation chronologically and in reverse confirms Zhu Jie's
response between Tang Yunfeng's profitability question and follow-up question,
then preserves Zhu's explanation and Tang's answer. The warning-clean build
and all 69 CTest entries pass as engineering evidence. The manually reconciled
frozen candidate is `510/556`; the 2400-3000 fixed block now passes at
`117/129`. The 3000-3600 block, Zhu Jie recall, critical attribution,
confident-wrong attribution, and speaker-time sign-off still fail. No new
real-WebSocket result or closure claim is attributed to FR36. See
`partition-invariant-regular-initial-slot-review-2026-07-19.md`.

FR37 isolates the separate T111/T123 partition regression at `ref-0478`.
T111 retains `我向国家交` as a short punctuation phrase and activates the
existing initial-slot/VAD challenge. T123 preserves the same aligned words only
as a `0.400 s` business interval, so ordinary direct evidence writes the
current identity before any equivalent phrase challenge exists. The current A
primary run is a short island gaplessly bracketed by the same C identity;
activity A and C both cover the interval, A's slot has initial identity B, and
both the source-adjacent preceding phrase and unique containing VAD select B in
the specified unchanged-gate patterns. FR37 reconstructs only this exact typed
topology. It changes no boundary, producer track, TOML value, transcript, or
known time. See
`bracketed-primary-adjacent-vad-reconstruction-diagnosis-2026-07-19.md`.

Repeated FR37 T123 replays are byte-identical and change only the
`3075.096-3075.496` `我向国家交。` interval from Tang Yunfeng to Zhu Jie.
T111 remains byte-identical to FR36. Complete reading of the changed
`50:35-52:28` conversation chronologically and in reverse confirms Zhu Jie's
answer between Shi Yi's question and clarification and preserves the already
accepted T111 handoff structure. The warning-clean build and all 69 CTest
entries pass as engineering evidence. The manually reconciled frozen candidate
is `511/556`; the 3000-3600 fixed block advances to `78/87` but still fails.
Zhu Jie recall, critical attribution, confident-wrong attribution, and
speaker-time sign-off also remain open. No new real-WebSocket result or closure
claim is attributed to FR37. See
`bracketed-primary-adjacent-vad-reconstruction-review-2026-07-19.md`.

FR38 corrects the separate T123 `ref-0504` boundary rather than restoring the
disabled broad future-epoch experiment. T123 already assigns the leading
`就在这边站` interval to Tang Yunfeng. Its source-leading punctuation phrase
continues across a VAD gap with one aligned visible tail character plus
punctuation, and that no-embedding tail inherits the following Shi Yi native
clause. FR38 requires the exact three-identity interval/phrase, unique
activity/primary, leading-VAD, and tail-VAD topology specified above and
rewrites only the tail. It changes no TOML value, threshold, producer track,
text, or common-clock coordinate. See
`cross-vad-phrase-tail-reconstruction-diagnosis-2026-07-19.md`.

Repeated FR38 T123 replays are byte-identical and split only `text_id=280`:
`3301.164-3301.244` `着，` moves from Shi Yi to Tang Yunfeng, while the
following `3301.404-3303.964` clause remains on Shi Yi's original sole-support
path. T111 remains byte-identical to FR37. Complete reading of the changed
`53:49-55:45` conversation chronologically and in reverse confirms Tang's
complete ownership correction and preserves the adjacent known `ref-0503` and
`ref-0505` failures. The warning-clean build and all 69 CTest entries pass as
engineering evidence. The manually reconciled frozen candidate is `512/556`;
the 3000-3600 natural-turn block now passes at `79/87`. Zhu Jie recall,
critical attribution, confident-wrong attribution, speaker-time sign-off,
real-path repeatability, and holdout evidence remain open. No new
real-WebSocket result or closure claim is attributed to FR38. See
`cross-vad-phrase-tail-reconstruction-review-2026-07-19.md`.

## 5. Acceptance Gates

All gates are conjunctive. A single failure keeps the spec open.

| Area | Required result |
|---|---|
| Full speaker-time accuracy | At least 90.0% on the final business view, using human-reviewed `test.txt` time blocks at their recorded precision |
| Full natural-turn speaker accuracy | At least 90.0% over all ledger turns |
| Fixed 600 s speaker blocks | At least 90.0% by speaker time and natural turns in every block; final 15.12 s reported separately |
| Per-speaker recall | At least 90.0% for each real speaker by both time and turn count |
| Critical speaker turns | 100% correct; uncertain and missing count as failures |
| Confident wrong attribution | 0 critical turns and at most 2.0% of all turns |
| Source-time offsets | Every attribution-affecting offset is manually annotated against `test.txt` at its one-second precision and counted by the applicable turn/time gate; no unsupported sub-second percentile or threshold is reported |
| ASR semantic accuracy | At least 90.0% full-length and in every fixed 600 s block |
| Critical ASR meaning | 100% preservation of critical numbers, negations, names, and decisions after allowed semantic equivalence |
| Silence hallucination | 0 substantive final transcripts in each of three independent silence runs |
| Forced alignment | 100% of final ASR IDs aligned; no missing/extra IDs; units monotonic, in bounds, and text-reconstructing |
| Time base | One source clock; all track extents reconcile exactly to the common sample count |
| Live/final identity | Exact `text_id` and content convergence across WS events, tracks, revisions, and Web UI |
| Real-time behavior | Full stream speed >= 0.98x at 1.0x input pacing; terminal timeline <= 30 s after end-of-stream |
| Stability | No crash, out-of-memory, CUDA error, data race finding, or unbounded backlog |
| Telemetry | Continuous `tegrastats`; required GPU/memory/power fields present for at least 95% of load samples |
| Engineering | Clean build with no warnings; all C++ and registered real-WebSocket integration tests pass |
| Repeatability | Both required full runs independently pass every applicable gate |
| Documentation | Spec/tasks/project state/final report match the accepted code and artifacts |

The 93 percent frozen-evidence target is a development margin. The product gate
remains 90 percent on the real runtime output.

## 6. Industrial-Readiness Evidence

Constitution 1.6.0 retains `test.mp3` as the mandatory canonical gate and permits
additional safety and locked holdout recordings. Supplemental recordings never
replace the canonical result and must follow the provenance requirements in
Article VI.

After that amendment, industrial readiness additionally requires:

- at least five independent sessions and three total hours not used for tuning;
- at least eight distinct speakers across two-, three-, and four-speaker sessions;
- microphone, far-field, background-noise, interruption, overlap, and silence
  conditions;
- the same 90 percent full business-view gates, with no session below 90 percent;
- no critical wrong attribution and no silence hallucination.

Without this holdout evidence, the allowed statement is only: "accepted on the
Orator canonical full-session scene." It must not be generalized to all
industrial audio.

## 7. Non-Goals

- Previous numerical-oracle and stability evidence is not discarded; it remains
  component evidence and must be rechecked where the accepted candidate changes
  behavior.
- Code-derived labels, totals, percentages, rankings, and acceptance decisions
  are prohibited; they do not choose model parameters.
- Text content, speaker identities, or reference-specific lexical rules are not
  hardcoded for `test.mp3`.
- Accuracy is not improved by converting wrong attribution to `unknown`; that
  only changes the safety profile and still receives no accuracy credit.

## 8. Constitution Check

- **Article I**: no new runtime dependency is permitted.
- **Article II**: model changes require stage-level trusted-oracle comparison;
  the 90 percent business gate is additional, not a replacement.
- **Article III**: current time-base ownership, direct pipeline subscriptions,
  and inference inside `ComprehensiveTimeline` must be corrected before
  acceptance.
- **Article IV**: every acceptance result comes from incremental WebSocket input
  and the terminal comprehensive document.
- **Article VI**: full item-by-item contextual semantic review is the only
  result-evaluation method. Automation stops at mechanical/numerical validation
  and unjudged evidence display. Supplemental safety and locked holdout
  recordings follow the 1.7.0 provenance and evaluation rules and do not replace
  `test.mp3`.
- **Article IX**: runtime parameter changes are TOML-only, and the implementation
  loading order must be corrected to defaults, TOML, environment, then CLI.
- **Articles X-XI**: this spec, its plan/tasks, code, tests, and project state must
  advance together with evidence.
