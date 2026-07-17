# Spec 013: Industrial Closing Validation

**Status**: Current clean-commit A/B natural-business-turn speaker gate passes;
T084, full canonical closure, release sign-off, and industrial readiness remain
open
**Created**: 2026-07-13
**Scope**: Re-establish a truthful product baseline, recover full-session business
accuracy, and define the evidence required before Orator may be declared closed.
**Constitution**: v1.7.0

## 1. Objective

Orator's v2.1 speaker-business pipeline now has repeatable full-session evidence
above 90 percent for the natural-business-turn speaker-attribution gate. Run A
and Run B were each executed through the real WebSocket path, and all 556
reference contributions were reconciled under complete conversational context.
This signs one conjunctive gate only. Speaker-time, fixed-block, per-speaker,
critical-turn, confident-wrong, audible-boundary, terminal-latency, ASR,
release, and independent-holdout gates remain open.

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
- **Reference turn ledger**: an immutable, manually adjudicated list covering
  every one of the 556 timestamped reference turns. Each row records the real
  speaker, audible start/end, overlap, business meaning, criticality, and any
  uncertainty in the reference.
- **Natural business turn**: one speaker's contextually complete contribution,
  including short replies and interruptions when they change or confirm a
  business position.
- **Critical turn**: a turn containing a decision, commitment, number,
  percentage, negation, ownership position, responsibility, or other content
  whose speaker identity changes the business interpretation.
- **Correct speaker time**: reference speech duration attributed to the correct
  real speaker in the final business view. Overlapping speakers are represented
  as separate reference intervals.
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
    equality. This closes the capture requirement in T044. It does not close
    T031 or T045 because all 556 audible ledger rows remain unsigned. See
    `closing-baseline-v21-2026-07-15.md`.
11. **Clean closing-baseline written-context review complete; audible review
    open**: the exact 935-entry clean package received a full chronological
    manual pass and a reverse fixed-block manual pass. The reconciled result is
    443 correct / 112 incorrect / 1 ambiguous (`79.6763%`). Tools only arranged
    evidence and did not assign correctness. The result fails the full-session
    gate and five fixed 600-second block gates. Exact audible boundaries,
    speaker-time, offsets, criticality, and independent totals remain unsigned,
    so this is not a constitutional closing score. See
    `closing-baseline-v21-context-review-2026-07-15.md`.
12. **Current clean-commit natural-turn gate passed; T084 open**: commit
    `6dbc600e4eb5` completed empty-registry Run A and restarted frozen-registry
    Run B over the full real-WebSocket path. Complete contextual semantic review
    manually records `514/556` for Run A and `513/556` for Run B. The complete
    T084 audit leaves speaker-time, fixed-block, per-speaker, criticality,
    confidence, audible-boundary, and terminal-latency gates unsigned. See
    `current-commit-full-review-2026-07-17.md`.

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

- **FR9**: The reference turn ledger must cover all 556 timestamped turns. No
  sampling, selected-window substitution, or code-inferred correctness is
  permitted.
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
  prohibited. Boundary offsets and overlapping speech must remain visible.
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

## 5. Acceptance Gates

All gates are conjunctive. A single failure keeps the spec open.

| Area | Required result |
|---|---|
| Full speaker-time accuracy | At least 90.0% on the final business view |
| Full natural-turn speaker accuracy | At least 90.0% over all ledger turns |
| Fixed 600 s speaker blocks | At least 90.0% in every block; final 15.12 s reported separately |
| Per-speaker recall | At least 90.0% for each real speaker by both time and turn count |
| Critical speaker turns | 100% correct; uncertain and missing count as failures |
| Confident wrong attribution | 0 critical turns and at most 2.0% of all turns |
| Boundary offsets | Median absolute offset <= 0.25 s; 95th percentile <= 0.80 s; every offset > 1.0 s annotated |
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
