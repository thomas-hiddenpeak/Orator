# Plan: Industrial Closing Validation

## 1. Planning Decision

The project will not optimize the existing final speaker labels one parameter at
a time. It will first establish a compliant evidence contract and a complete
manual reference ledger, then determine the accuracy upper bound available from
the currently implemented models. Runtime implementation proceeds only when the
frozen-evidence candidate has sufficient margin above the 90 percent product
gate.

Work remains on `master` in small, auditable commits. Each failed experiment is
either removed or left disabled with an explicit experimental status. After
three failed solution families with the same root limitation, work returns to
the last accepted commit and the model strategy is reconsidered.

All product-result evaluation is performed only through complete contextual
semantic review. Code may execute/capture runs, validate mechanical and
numerical contracts, generate reference-free candidate views, and display
unjudged evidence. It may not assign labels, aggregate an accuracy result,
compare or rank candidates, select parameters, or issue a gate verdict.

### 1.1 Accepted-policy maintainability refactor

The canonical speaker-business result is frozen before structural work. The
refactor changes ownership and file boundaries only:

1. Keep `BusinessSpeakerPipeline` as the registered orchestration boundary for
   subscriptions, synchronization, base diar/text projection, revisions, and
   decision-audit construction.
2. Move the accepted voiceprint/primary/VAD/alignment rule executor behind one
   internal `SpeakerFusionPolicy` type. The policy receives the owning pipeline
   state read-only and returns projected entries; it does not subscribe, publish,
   mutate producer tracks, or read configuration independently.
3. Preserve the existing policy order and all reason/source strings exactly in
   the first extraction. Do not combine, delete, reorder, generalize, or tune a
   rule in the same change.
4. Freeze a full-session production replay before editing and require exact
   byte equality after editing. This is a mechanical equivalence contract, not
   result evaluation.
5. Run focused policy tests, the complete CTest suite, and a real-WebSocket
   smoke after extraction. If any output or contract changes, restore the last
   accepted implementation before attempting a smaller extraction.
6. Only after the extraction is accepted may a later task consolidate common
   guards and archive rejected experimental assets. That later work receives its
   own exact-equivalence gate and may not alter the accepted speaker result.

### 1.2 Active-surface consolidation

T095 is a second, independently frozen equivalence change:

1. Replay the full retained typed input at commit `52b3b22` and use its complete
   serialized output as the only behavior baseline for this change.
2. Consolidate only repeated, semantically identical policy guards: ranked
   top-two voiceprint extraction and minimum forced-alignment-unit coverage.
   Keep every challenge function, invocation order, abstention return, reason,
   source, and threshold unchanged.
3. Keep `test_business_speaker_pipeline` as the active exhaustive regression for
   accepted fusion and abstention behavior. Keep active Python tests only for
   raw evidence generation, replay-input integrity, provenance, and unjudged
   review-packet construction.
4. Move every standalone `speaker_*_candidate.py`, its matching integration
   test, and every non-production speaker TOML under an explicit Spec 013
   historical archive. Remove their CTest registrations. Keep Sortformer high-
   and low-latency numerical-oracle profiles because they remain active model
   parity fixtures, not product candidates.
5. Delete legacy scripts that calculate product speaker-attribution accuracy or
   model identification accuracy and remove the compiled reference-projection
   harness. Do not retain runnable copies in the archive.
6. Require byte identity with the frozen full replay, a clean build, all
   remaining CTest entries, and a new-binary real-WebSocket smoke. Test-count
   reduction caused solely by removal of archived candidate tests is expected
   and is not a reduction in production regression coverage.

## 2. Phase 0: Governance and Baseline Reset

1. Review and approve Spec 013 before runtime implementation.
2. Mark the product as not closed in `PROJECT_STATE.md`; retain historical runs
   as component evidence only.
3. Propose a separate Constitution patch that:
   - preserves one session-owned common time base while allowing private
     per-pipeline audio stores;
   - keeps `test.mp3` mandatory but permits supplemental silence, microphone,
     and locked holdout tests for safety and industrial-readiness claims.
4. Create a reproducibility manifest format and record the current `master`
   commit, dirty state, data/model hashes, device, power mode, clocks, registry
   state, TOML hash, and tool versions.
5. Freeze a named current-candidate full JSON package for diagnosis. It is a
   baseline measurement, not an accepted result.

## 3. Phase 1: Correct the Product Contracts

### 3.1 Session clock

`AuditoryStream` will own one immutable `core::TimeBase` created when the session
audio ingest is initialized. Each private audio cache receives that clock source,
and every worker receives a copy derived from that same object. Cache and worker
constructors will no longer infer origin from their own sample-rate parameter.

End-of-stream reconciliation will cover diarization, ASR, VAD, forced alignment,
speaker evidence, and business fusion. A mismatch is a test failure, not only a
debug log.

### 3.2 Typed evidence flow

`ComprehensiveTimeline` becomes the authoritative typed evidence store:

```text
audio ingest + session clock
  -> private pipeline audio stores
  -> diar / asr / vad raw tracks
  -> align / speaker-evidence tracks read from ComprehensiveTimeline
  -> business-speaker fusion track
  -> one terminal document containing every immutable raw track and the final view
```

The protocol layer remains responsible for topic envelopes, persistence, replay,
and WebSocket transport. It is not used as a private data path between workers.
Typed incremental cursors or subscriptions on `ComprehensiveTimeline` replace
the ASR `VadCache` pointer and direct align enqueue subscription.

The current speaker selection, text projection, and gap-fill policy moves from
the container into a registered `business_speaker` fusion pipeline. The
container may align and index track records but does not choose or invent track
content.

### 3.3 Identity and view contract

ASR allocates one ID before publishing a final and reuses it in all destinations.
The ASR terminal track serializes this ID explicitly. Forced alignment and the
business view use the same ID; no tool reconstructs IDs from list position.

The Web UI consumes the server's business-view revisions and terminal track. It
does not perform another speaker split. Browser tests assert that partial,
final, revision, reconnect, and terminal states converge without duplicate or
stale rows.

The WebSocket session has one audio producer and any number of observers. The
first connection that sends audio owns production until it ends, resets, or
disconnects. Opening an observer never calls `AuditoryStream::Reset()` and all
stream-generated events are broadcast to every connected observer. A second
connection that attempts to send audio while a producer is active receives an
explicit error and its bytes are not ingested. The unified WebSocket client
opens a concurrent observer in the registered integration gate and requires
the producer and observer to receive identical terminal timeline content. A
real browser is also connected before a promotion stream to verify that live
rows and telemetry update while the unified client remains the producer.

### 3.4 Configuration

Startup order is corrected to:

1. compile-time defaults;
2. `orator.toml`;
3. `ORATOR_*` environment variables;
4. CLI arguments.

Every behavioral environment switch is either represented by a typed TOML
field or removed from acceptance builds. The resolved runtime configuration is
included in the run manifest. The obsolete matrix mode in
`ws_unified_test.py` is removed because it labels overrides without applying
them and ranks candidates by unknown duration rather than business accuracy.

The migration keeps environment overrides only at the process entry point,
where they are parsed into `AuditoryStream::Config` before any model is
constructed. Model, GPU, and transport implementations never call `getenv()`.
The accepted typed controls are:

- `[asr]`: system prompt, EOS-ban steps, decode batch, profiling, encoder
  attention mode, and CUDA-graph enablement;
- `[align]`: profiling;
- `[debug]`: log level, time-base diagnostics, Sortformer progress, GPU
  scheduling, and the optional WebSocket text-frame log path.

Legacy kernel-forcing variables used only for local A/B experiments are
removed rather than promoted to product configuration. The terminal timeline
contains a canonical `resolved_config` object after defaults, TOML,
environment, and CLI resolution. The unified client copies that object into a
sidecar run manifest and hashes the canonical representation.

## 4. Phase 2: Preserve the Full Human Reference and Build the Run Review

`test.txt` is the completed human-listened reference for all 556 timestamped
entries; it is not an unreviewed machine transcript. The review record preserves
each stable reference ID, source line, real speaker, text, timestamp, line order,
and whole-second source precision. Duplicate and backward timestamps remain
unchanged and are interpreted from the surrounding conversation rather than
being reordered or assigned invented sub-second boundaries.

For each exact run, the reviewer reads the final business view against all 556
reference entries in two complete passes: chronological order and reversed
600-second block order. The reviewer then records the contextual speaker
judgment, critical/non-critical class based on reference meaning, uncertainty or
confident-wrong class, source-time offset notes, and any genuine ambiguity.
Disagreements between passes are reconciled from the already captured complete
context. A new audio transcription or a second audible-boundary pass is not a
prerequisite for using the authoritative `test.txt` reference.

The review tools may seek audio and display system tracks beside a ledger row.
They do not assign correctness, total results, calculate percentages, compare
thresholds, rank candidates, or emit acceptance booleans. Reviewers manually
derive all totals from signed contextual judgments, and a separate manual pass
independently checks those totals against the ledger.

## 5. Phase 3: Establish the Reproducible v2.1 Closing Baseline

The closing line is fixed to streaming Sortformer v2.1 with the checked-in
`340/1/188/188` profile. Compile-time defaults and `orator.toml` select the same
weight file and profile. The v2 checkpoint and its obsolete CTest are removed;
only prior reports and hashes remain to explain historical implementation
findings. They cannot enter candidate selection or acceptance totals.

1. Correct and register real-WebSocket integration tests in CTest.
2. Run a clean build, warning check, all CTest tests, JavaScript syntax checks,
   and selected sanitizer checks.
3. Run 120 s, 360 s, 600 s, and full-length incremental WebSocket tests using
   the checked-in `orator.toml` without runtime tuning overrides.
4. Capture all live events, terminal tracks, UI state, server logs, and
   continuous `tegrastats`.
5. Perform the full 556-row contextual review on the final business view.
6. Publish the baseline score by full session, fixed 600-second block, speaker,
   criticality, boundary offset, uncertainty, and confident-wrong attribution;
   manually tally and independently verify every value without code.

This v2.1 baseline replaces every earlier selected-window, v2, or script-derived
accuracy claim for closing purposes. The earlier `413/556` result belongs to a
different 936-entry artifact and remains a historical cut-oriented diagnostic.
The exact clean 935-entry package has a complete manual written-context result
of `443/556`; it still is not an accepted speaker-time or closing score because
the failed historical candidate was rejected before the remaining manual
breakdowns were derived.

## 6. Phase 4: Determine the Existing-Model Upper Bound

All candidate work uses the frozen v2.1 baseline package until a runtime
candidate is selected. Evidence from v2 may explain a regression but cannot
decide a candidate.

Before comparing business candidates, the exact Sortformer execution profile is
checked against NeMo. The gate binds the source checkpoint revision and hash,
converted runtime weight hash, TOML hash, processed-input hash, and output
fixture hash. For an asynchronous profile, the oracle fixture crosses at least
three chunks, retains output produced after repeated cache compression, and
verifies that model input is
`speaker cache + FIFO + current chunk`, that no frame is discarded before FIFO
overflow transfer, and that the official cache-update period controls the
transfer. A mismatch is a model-port defect and is corrected before threshold
tuning or model replacement continues.

The 2026-07-15 correction first isolated an implementation defect with NVIDIA
v2. The exact v2 runtime profile now passes a five-chunk NeMo oracle at
`max_abs=1.43051e-6`; unsupported local cache controls were removed, and stale
TOMLs containing them fail loading. The corresponding full-session diagnostic
changed assignments throughout the recording without improving the frozen
written-context candidate, so model/fusion escalation continues without
treating FIFO parity as an accuracy fix. This is historical remediation
evidence, not the closing model gate. See
`sortformer-oracle-2026-07-15.md`.

The first v2.1 comparison changed only the model weights and retained the legacy
Orator asynchronous profile (`188/340/188`, FIFO `188`, context `1+1`). This is
useful for isolating checkpoint differences, but it is not the v2.1 deployment
upper bound. NVIDIA's published v2.1 model card recommends two different
streaming profiles, all values in 80 ms frames:

- high latency: chunk/right/FIFO/update `340/40/40/300`;
- low latency: chunk/right/FIFO/update `6/7/188/144`.

Both official profiles must receive separate multi-chunk NeMo/C++ numerical
gates and full-session frozen diar evidence before another integrated
real-WebSocket run. The high-latency profile is an accuracy diagnostic unless
its 30.4-second input-buffer latency is explicitly accepted; the 1.04-second
low-latency profile is the deployment candidate when its compute throughput and
contextual accuracy pass. The checkpoint-native synchronous profile remains a
separate diagnostic and cannot validate either asynchronous candidate.

The 2026-07-15 screening completed these gates. High latency passed numerical
parity but recorded 385 / 170 / 1 natural turns; low latency passed numerical
parity but recorded 377 / 178 / 1. Both are below the inherited v2.1
real-WebSocket diagnostic of 413 / 142 / 1 and below the 90 percent gate, so
neither official profile advances to another integrated run. Work returns to
reference-free multi-pipeline evidence fusion on the active inherited v2.1
profile. The owner decision on 2026-07-15 designates that profile as the sole
closing baseline because it is the strongest deployable v2.1 result. This does
not waive the 90 percent product gate or the 93 percent frozen-candidate
promotion gate.

### 6.1 Independent evidence extraction

For every natural turn candidate, preserve these inputs separately:

- Sortformer per-speaker posterior coverage, top-one/top-two margin, overlap,
  local slot, and temporal continuity;
- TitaNet similarity to each enrolled global speaker, top-one/top-two margin,
  audio duration, and clean-speech quality, computed per natural turn rather
  than per Sortformer label bucket;
- VAD speech support and pause endpoints;
- forced-alignment unit boundaries and gaps;
- ASR `text_id` and final span as a content/time anchor only;
- all source timestamps on the same absolute session clock.

TitaNet evidence must not be grouped by the Sortformer label it is intended to
check. This avoids making the second model repeat the first model's assignment.
Before runtime integration, generate a reference-free rolling TitaNet track at
multiple TOML-defined window sizes. Windows are paired by their absolute centre
time, ranked only against identities active in the captured session, and
accepted only when the configured number of scales agree and independently
pass the configured score and margin gates. The resulting points and runs are
evidence only: they do not read `test.txt`, assign correctness, or mutate any
captured pipeline track.

### 6.2 Candidate decision model

The first candidate is a constrained sequence decision over natural turns:

- Sortformer supplies local ownership likelihood;
- TitaNet supplies independent global-identity likelihood;
- VAD and forced alignment supply legal boundaries;
- simultaneous speech supplies cannot-link constraints;
- temporal continuity is used only when current acoustic evidence supports it;
- low-evidence cases remain uncertain instead of inheriting a neighbour.

Rolling voiceprint transitions may split or rewrite the business view only at
VAD or forced-alignment boundaries on the same session clock. Known-speaker
overrides require sustained multi-scale evidence; filling an unknown span may
use a separately configured lower gate. A boundary that cannot be supported by
the alignment/VAD tracks remains unchanged.

The 2026-07-15 T059G frozen experiment implements this decision model with
3-second and 5-second native TitaNet windows at a 1-second step. Existing
business-span edges are preserved for whole-span decisions; candidate-strength
partial rewrites require a forced-alignment pause. The policy changes nine of
936 source entries. Full contextual review of all 11 affected reference rows
finds five repairs and no regression, for 418 / 137 / 1 (`75.1799%`). This fails
the 93 percent gate below, so the experiment is not integrated and no further
policy tuning is permitted. See `speaker-sliding-v21-2026-07-15.md`.

The next candidate addresses a different model-contract limitation rather than
retuning the rejected rolling-voiceprint policy. The v2.1 checkpoint declares a
90-second training-session length, while the inherited baseline keeps one
Sortformer state for the entire 3615-second recording. The candidate therefore
rotates only the Sortformer streaming state at exact TOML-defined 90-second
boundaries while preserving the immutable session clock. Each window receives
a disjoint local-slot namespace. TitaNet independently embeds clean spans from
each session-qualified slot and maps the slot to the frozen global registry;
short spans inherit only that acoustically established slot mapping. No
reference text, real speaker name, known error interval, or correctness label
enters evidence generation or identity stitching.

The frozen candidate must preserve the baseline ASR, VAD, and forced-alignment
tracks. It may rebuild the diarization and business-speaker views from the same
audio and time base. Before runtime integration, the complete 556-row candidate
view is reviewed chronologically and again in reversed fixed-block order using
contextual semantics. A failed result ends this candidate family; the rotation
period and identity thresholds are not swept against the reference.

The 90-second rotation candidate stopped at the complete 0-120-second
contextual promotion review: its session-qualified TitaNet mappings were
acoustically coherent, but v2.1 still assigned several short interjections to
the wrong local slot. The failure therefore precedes cross-session stitching.
No rotation period is retuned and no full-session verdict is claimed.

The next candidate keeps the accepted continuous v2.1 state and targets the
short-interjection evidence gap directly. Forced-alignment/business intervals
of at least 0.8 seconds receive an independent TitaNet embedding under a
separate TOML. Existing strong score and margin gates remain unchanged. A
short-phrase identity may rewrite a business entry only when it is strong after
filtering the registry to identities acoustically active in the session; weak
or stale-registry evidence cannot override Sortformer. The candidate is built
once, frozen, and reviewed in full without threshold or duration sweeps.

The strong-only candidate is retained as an auditable negative screening
result, not as a promoted result. Its complete changed-context worksheet
contains only 16 reference contributions and also exposes a mixed-speaker
business interval that cannot be repaired by assigning one identity to the
whole interval. No full-session accuracy is claimed for that candidate.

The next topology uses the already configured candidate score/margin gates but
does not change their values. A current business interval can be rewritten by
direct interval TitaNet evidence or overlap-weighted diar-interval TitaNet
evidence. If both sources pass a gate, they must agree. Candidate-strength
evidence is accepted only as current acoustic evidence; it is never propagated
through a Sortformer local slot or a neighbouring turn. Ineligible and
conflicting intervals preserve the frozen baseline assignment. This isolates
the value of independent current-phrase identity from the rejected local-slot
continuity assumption before introducing finer forced-alignment splits.

The direct-evidence topology stops at its 0-120-second contextual gate. It
leaves the existing early short-turn failures unresolved and incorrectly
rewrites the Zhu Jie phrase "没说完" to Tang Yunfeng. The failure is caused by
an eligible short-interval voiceprint, not by neighbour propagation, so the
candidate is not widened or threshold-tuned.

Before adding another fusion layer, restore the pinned v2.1 model's native
postprocessing contract. The current runtime inherited an onset/offset policy
of `0.45/0.35`, deletes activity shorter than 0.5 seconds, and joins same-slot
activity across one-second gaps. The pinned NeMo implementation declares
`0.5/0.5`, zero minimum on duration, and zero minimum off duration. One
candidate changes exactly those four TOML values. It reuses the captured ASR,
VAD, and alignment tracks, exports full frame and segment evidence, maps the
four continuous local slots to active global identities using TitaNet audio
only, and rebuilds the frozen business view on the common clock.

The first projection onto the old 935-entry business grid is diagnostic only.
It proves that the native postprocessor exposes more boundaries, but it cannot
apply them because the old grid has already fused some speaker changes. Export
the frozen native diar segments plus the unchanged ASR and forced-alignment
tracks into typed replay inputs. Feed those inputs to the production
`ComprehensiveTimeline` and `BusinessSpeakerPipeline`, then review the newly
split business track. This replay is the candidate governed by the promotion
gate; the old-grid projection is not.

Production replay yields 1,233 business intervals from the native 1,370-segment
diar view. The complete 0-120-second context fixes two prior short-turn
failures, but three contributions remain incorrect, so the diar-only replay
does not pass the local development gate and receives no full-session result.

The next candidate embeds each of those rebuilt business intervals directly
with native TitaNet. This also gives diar-unsupported ASR intervals independent
speaker evidence. The dedicated TOML separates sub-1.5-second evidence from
regular evidence: short intervals use `0.25` minimum cosine and `0.05` minimum
top-two margin; regular intervals use the existing candidate gate `0.58/0.04`.
The lower short-window absolute gate reflects the documented duration loss of
speaker-verification cosine, while the margin remains mandatory. The policy is
frozen once and reviewed without a threshold or duration sweep.

That first replay-interval candidate is rejected at the complete 0-120-second
context gate. Replacing the initial registry with the final session-refreshed
registry repairs most of the long opening contribution, but the Shi Yi
interjection at 72 seconds and Tang Yunfeng turn at 78 seconds retain the wrong
speaker, while the start of Tang Yunfeng's 82-second turn also remains wrong.
No full-session result is claimed.

One distinct duration-calibrated topology is permitted before changing the
model architecture. `SpeakerIdentityStage` refreshes the persistent identities
from the best current-session long spans before saving the final registry, so
that registry is the native session prototype source for a rewritable terminal
view. The production `0.55` absolute match gate is explicitly calibrated for
approximately 3-4 seconds and therefore remains the regular-interval gate, but
it does not apply below 1.5 seconds. Short intervals instead require the
existing `0.04` candidate top-two margin across only the four active identities.
The candidate is frozen from `speaker-v21-duration-calibrated.toml`; no value is
selected from a reference sweep, and failure of the contextual gate ends this
topology.

The complete chronological and reversed-block review of that frozen topology
records 469 correct, 86 incorrect, and one ambiguous contribution, manually
derived from all 556 contexts (`84.35%`). It repairs the early direct-evidence
failures but remains below the industrial gate. The remaining failures are
concentrated in low-evidence sub-1.5-second fragments, diar boundaries that cut
one natural phrase into several entries, and fragments whose text projection
straddles a rapid speaker change. This closes threshold work: no TitaNet score,
margin, duration boundary, or Sortformer postprocessing value is changed.

The next topology changes evidence aggregation instead. It treats every
eligible direct current-audio TitaNet decision as an immutable acoustic anchor.
Within one ASR `text_id`, forced-alignment units are partitioned at the existing
`align_snap_pause_sec` boundary. A consecutive low-evidence bridge may inherit
an identity only when the nearest eligible anchor on each side belongs to that
same alignment run and both independently select the same active identity. The
bridge span is capped by the existing 1.5-second short-evidence boundary, every
inter-entry gap is capped by the alignment pause, and no accepted direct anchor
is rewritten. Conflicting, one-sided, overlong, cross-pause, or cross-text
cases preserve the duration-calibrated input exactly. This is current-phrase
acoustic aggregation, not unconstrained neighbour propagation.

`speaker-v21-aligned-phrase.toml` freezes this policy before generation. The
generator reads only the frozen replay candidate, its direct voiceprint audit,
forced-alignment units, the active-identity mapping, and that TOML. It does not
open the reference ledger or emit correctness fields. Focused tests cover
same-identity two-sided bridging, conflict preservation, immutable anchors,
duration rejection, and forced-alignment-run isolation. The candidate first
faces the complete 0-120-second contextual gate; only a pass permits the full
chronological and reversed-block review.

The two-sided bridge implementation produces nine legal bridges and changes
ten tiny entries over the complete session. It leaves 0-120 seconds unchanged.
This is retained as a bounded evidence-aggregation probe, but it does not
provide enough current-audio coverage to become the closing topology; its
distance and pause limits are not widened.

The next topology obtains independent evidence at the natural phrase level.
Final ASR punctuation divides each `text_id` into source phrases. Punctuation
does not carry time: the matching forced-alignment characters define each
phrase's exact absolute interval. Only phrases between 0.5 and 4.0 seconds are
embedded, matching the existing short-evidence floor and the documented
3-4-second production TitaNet calibration region. Identity gates remain the
duration-calibrated `0.04` short margin and production `0.55/0.04` regular
score/margin contract.

The phrase track is an additional current-audio view, not a replacement for
stronger local evidence. Before an eligible phrase can fill its source-text
range, every overlapping eligible direct-interval voiceprint anchor must select
the same identity. A single conflict rejects the entire overlay. Accepted
direct anchors remain immutable, while ineligible diar fragments inside an
agreed phrase receive the phrase identity. Source characters are reprojected
onto forced-alignment time and then regrouped into a terminal business view;
their concatenation must remain exactly equal to the finalized ASR source for
every `text_id`.

`speaker-v21-punctuation-phrase.toml` freezes phrase enumeration and projection
before TitaNet runs. The generator has no reference argument or correctness
field. Focused tests cover punctuation partitioning, subsequence alignment,
duration-aware identity gates, direct-anchor conflict rejection, and exact
source-text preservation.

The unanchored punctuation topology fails the 0-120-second contextual gate.
It changes Zhu Jie's `81.6-82.16` "然后呢" to Xu Zijing even though no eligible
direct interval anchor supports that phrase identity, while the existing
96-second split remains unresolved. This is an evidence-sufficiency defect, not
a numeric threshold defect; no score or duration value is changed and no full
review is claimed.

One stricter topology reuses the exact frozen phrase spans and TitaNet evidence
but requires at least one agreeing direct interval anchor before an overlay can
write the terminal view. Phrases with zero anchors remain audit evidence only.
`speaker-v21-anchored-punctuation-phrase.toml` differs from the rejected policy
only by this boolean evidence requirement. The candidate is regenerated once,
then restarts the 0-120-second contextual gate.

The anchored candidate passes the 0-120-second promotion gate and therefore
receives the complete 556-row chronological review followed by the reversed
fixed-block review. The manual contextual result is 469 correct, 86 incorrect,
and one ambiguous row, approximately 84.35 percent. It repairs `ref-0033`
because forced-alignment timing exposes Zhu Jie's preceding answer, but it
regresses `ref-0121`: one Tang Yunfeng anchor inside a punctuation phrase
propagates across Shi Yi's short interjection "你不低于". The result is exactly
neutral relative to the duration-calibrated candidate and does not advance.
This is direct evidence that unanimous present anchors are insufficient when a
phrase contains a real speaker change with only one eligible anchor. Numeric
gates remain frozen; the next topology must constrain identity propagation at
speaker boundaries rather than retune confidence values.

The next topology restores those boundaries before phrase fusion. Raw v2.1
frame posteriors are kept as an independent evidence track rather than reduced
to the postprocessed diar segments. Within each forced-aligned punctuation
phrase, a sustained active top-one local-slot transition cuts the source
character range on the same absolute clock. Every resulting piece is embedded
independently by TitaNet. A write requires agreement between the piece's local
slot mapping and its TitaNet identity; this replaces the anchored topology's
unsafe assumption that one agreeing phrase anchor can speak for an entire
phrase. Regular direct-interval voiceprint anchors remain immutable. A short
direct anchor can be superseded only by a regular-duration, two-model-consensus
piece, which separates short-window voiceprint contamination from genuine
Sortformer corrections such as the early 72-second interjection.

`speaker-v21-posterior-bounded-phrase.toml` freezes the native `0.5` frame
activity boundary, the minimum sustained run inherited from TitaNet's `0.4`
second embedding floor, existing duration-calibrated score/margin gates, and
the overwrite policy before generation. The evidence exporter reads only the
frozen v2.1 frame CSV, aligned phrase metadata, active local/global mapping,
duration-calibrated candidate, and TOML. It emits no reference-derived field.
The complete 0-120-second contextual review is mandatory before any full
review.

The posterior-bounded candidate passes that early contextual gate. The complete
556-row chronological review and reversed fixed-block review are then performed
manually from conversational context. No code, script, formula, query,
notebook, metric, or algorithm assigns correctness, aggregates accuracy,
ranks/selects this candidate, or issues the verdict. The manual result is 472
correct, 83 incorrect, and one ambiguous row, approximately 84.9 percent. It
repairs `ref-0033`, `ref-0156`, and `ref-0459` without introducing a semantic
speaker regression. `ref-0096` and `ref-0186` cross mechanical reference time
cuts but retain the correct speaker in their surrounding utterances, so they
are not regressions under the required semantic review. This is a real but
insufficient improvement and does not advance to production integration.

The residual evidence rejects a global identity remap or a single long-range
drift explanation. A recurring defect is a mixed-speaker direct interval whose
one identity remains immutable even after punctuation, forced alignment, and
raw posterior transitions expose a bounded semantic subphrase. The next
topology must test whether independently identified subphrases can split such
mixed enclosing anchors while preserving exact-bound anchors and refusing
unanchored short evidence. Numeric gates remain frozen; this is an evidence-
topology change rather than a threshold sweep.

The multiresolution topology keeps the posterior-bounded candidate's exact
phrase and character projection. It adds raw active runs down to one native
v2.1 frame so that sub-0.4-second interjections are not discarded merely
because TitaNet cannot embed them independently. Such a micro piece writes only
when its mapped local identity agrees with the already-frozen TitaNet identity
of the enclosing aligned phrase. If direct anchors exist, that identity must
already be represented among them; a sole conflicting anchor vetoes the write.

For independently embeddable pieces, three agreeing views -- mapped raw local
identity, piece TitaNet, and enclosing-phrase TitaNet -- may split an enclosing
regular direct anchor. A two-voiceprint correction against the mapped local
identity is narrower: it reuses the production regular score gate and rejects
phrases containing competing direct-anchor identities. Short-score corrections
are allowed only for an isolated active local run no longer than the existing
short-duration boundary and only when the phrase has no direct anchor. These
rules address missing micro-turns and mixed enclosing windows while preserving
long homogeneous raw runs such as the Xu Zijing response around 1871 seconds.

`speaker-v21-multiresolution-phrase.toml` freezes the one-frame micro floor and
all enabled evidence paths. The candidate is generated once without any
reference input or correctness output. Mechanical tests prove source-text
identity, common-clock bounds, deterministic priority, conflicting-anchor
vetoes, and unchanged inherited gates. Product promotion still requires the
complete manual 0-120-second gate, all 556 chronological contexts, and reversed
fixed-block review. No executable automation may evaluate or aggregate those
results.

The candidate passes the 0-120-second contextual gate. The complete 556-row
chronological review and reversed fixed-block review are then performed
manually. No code, script, formula, query, notebook, metric, or algorithm
assigns correctness, aggregates accuracy, ranks or selects the candidate, or
issues the verdict. The manual result is 475 correct, 80 incorrect, and one
ambiguous contribution, approximately 85.4 percent. It repairs `ref-0035`,
`ref-0037`, and `ref-0109` without a contextual regression, but remains below
the industrial gate and is rejected from integration. This closes threshold-
preserving phrase overlays as the primary path: the next evidence topology must
improve the deployable TitaNet identity prototypes before timeline fusion.

The first identity-stage correction targets an implementation gap already
present in the production epoch state machine. A candidate-strength competing
span is currently retained for later confirmation, but a second independent
span naming the same competing identity does not confirm it; only one span
crossing the stronger gate can do so. This discards temporal agreement and
leaves a reused Sortformer slot bound to its original identity even when
multiple clean current-audio observations agree on a different enrolled
speaker.

The repeated-candidate epoch topology keeps every existing score, margin,
duration, overlap, and backfill gate. A TOML integer specifies the required
number of non-overlapping candidate spans. Repeated candidates for one identity
increment a pending confirmation; a conflicting candidate replaces it, and a
strong clean observation of the active epoch clears it. On confirmation, the
stage starts one time-ordered epoch at the existing bounded backfill start. A
later strong competing observation can immediately start the return epoch.
Focused tests must prove one-candidate preservation, repeated confirmation,
conflict replacement, stable-evidence cancellation, bounded backfill, and
return splitting before frozen full-session evidence is generated.

The full-session candidate must be produced through the C++ identity stage from
the frozen native v2.1 segments, original audio, initial registry, and one
TOML. Its diarization identities are then replayed through the production
business-speaker pipeline on the common clock. Tools may verify only mechanical
provenance and arrange context. The complete 0-120-second gate and, on success,
all 556 chronological plus reversed-block contexts are judged manually. No
code, script, formula, query, notebook, metric, or algorithm may assign
correctness, aggregate accuracy, choose a parameter, rank candidates, or issue
the integration verdict.

The repeated-candidate epoch replay is mechanically stable after unknown-
identity drift is disabled, but the only resulting interval rewrite binds one
local slot to Tang Yunfeng from 2864.64 through 3108.48 seconds. Manual review
of all 20 changed reference contexts shows that the slot rapidly alternates
Tang Yunfeng and Zhu Jie inside that interval. The epoch repairs some Tang
turns but regresses multiple Zhu turns, so the topology is rejected without
integration. This establishes that the remaining identity problem is
per-natural-turn and cannot be solved by a longer local-slot epoch.

The next identity topology retains multiple deployable prototypes per stable
registry ID rather than averaging all voice modes into one centroid. The
persistent pre-session registry contributes one prototype and the session-
refreshed terminal registry contributes a second. For each independent current-
audio query, the two cosine scores belonging to one identity are reduced by
maximum; those identity-level values then compete under the already frozen
duration-calibrated score and margin gates. Taking the maximum within an
identity preserves channel or prosody modes, while taking the top-two margin
after reduction exposes cross-identity conflict rather than hiding it.

The first frozen candidate changes only the 1,233 replay business intervals.
If its complete 0-120-second contextual gate passes, every changed context and
then the full chronological and reversed-block views are reviewed manually. A
successful direct candidate extends the same frozen gallery operation to
posterior-bounded pieces and punctuation phrases before the terminal
multiresolution projection is rebuilt. All score fusion, candidate generation,
hashing, and context arrangement remain reference-free. No code, script,
formula, query, notebook, metric, or algorithm may assign correctness,
aggregate accuracy, rank candidates, choose a parameter, or issue the verdict.

Manual contextual review rejects the direct maximum-over-prototypes topology.
It repairs two previously incorrect contexts, but it also introduces clear
regressions where the pre-session prototype conflicts with stronger
session-refreshed phrase evidence. The prototype source is therefore useful as
a challenger, not as an unconditional identity-level maximum.

The next candidate keeps the accepted multiresolution result as its immutable
baseline. A pre-session direct identity may challenge the session-refreshed
direct identity only on forced-aligned characters where the raw v2.1 local
posterior maps to the same stable registry ID. The maximum-within-identity
gallery also acts as a conflict detector rather than a replacement scorer: if
it retains the terminal identity first and the source baseline second but the
unchanged top-two margin no longer passes, the terminal direct override may be
withdrawn where the raw local posterior agrees with that source identity.
Session-refreshed TitaNet piece and punctuation-phrase decisions act as vetoes
for both paths: any eligible conflicting identity preserves the
multiresolution baseline, while an ineligible decision abstains. This topology
preserves the current-audio phrase corrections that the direct maximum
candidate damaged while allowing either an eligible initial voice mode or an
explicit cross-prototype ambiguity to restore the raw-local identity. It
reuses the frozen duration-aware gates without a threshold change and projects
only exact source-character intersections on the common clock.

The candidate generator verifies direct-track parity, active identity and
mapping completeness, exact ASR text reconstruction, non-overlapping source
ranges, and source hashes. It emits an audit record for every attempted
challenge but no correctness field. The complete 0-120-second prefix is judged
manually first. Only a passing prefix proceeds to every changed context and
then the full chronological and reversed-block semantic review. No executable
automation may evaluate, aggregate, rank, select, promote, or reject the
candidate.

If the bounded two-prototype topology remains below the closing floor, the
next evidence-layer step preserves the production stage's clean-reference
selection while removing its lossy final reduction. Native v2.1 diar segments
must independently agree with both the pre-session and session-refreshed
registries and with their raw local-slot stable mapping under the existing
production score, margin, confidence, duration, edge, and overlap contracts.
For each stable ID, the existing `confidence * duration` order retains at most
`max_ref_segs` individual normalized embeddings. No reference transcript or
known failure interval participates in eligibility or retention.

Current-audio business intervals are embedded once on the common clock. Their
score for one identity is the maximum cosine over that identity's retained
clean prototypes; cross-identity competition and the existing duration-aware
gates are applied only after this within-ID reduction. The direct gallery view
is diagnostic evidence, not an automatic replacement. It first enters the
same raw-local and current piece/phrase veto topology as FR16N, then receives
the complete manual semantic review before any runtime design is selected.

The clean-gallery candidate adds one additional bounded correction path. When
its direct identity disagrees with the raw local mapping, the write is allowed
only if an eligible session-refreshed posterior-piece or punctuation-phrase
identity independently names the same stable ID and no eligible current-audio
identity conflicts. This is two-view current-audio agreement across distinct
prototype sets and temporal resolutions. If both current-audio views abstain,
the stricter FR16N raw-local agreement remains mandatory. Projection stays on
the exact forced-aligned source intersection and never extends through context
or transcript meaning.

For rapid turns where the raw top-1 slot disagrees and the session-refreshed
piece/phrase views abstain, a stricter four-view path uses the full native
Sortformer posterior. The clean-gallery direct interval, exact piece, and
enclosing phrase must all name one identity, and the native Sortformer channel
mapped to that identity must independently remain active above the existing
0.5 activity gate for at least the existing one-frame micro floor inside the
piece. This distinguishes a real secondary/overlap speaker channel from a
  gallery-only voiceprint error without adding a new threshold.

When a direct interval is itself too short or ambiguous, the same evidence is
evaluated without treating that abstention as a negative speaker decision. The
clean gallery must agree independently at exact-piece and enclosing-phrase
resolution, the raw local mapping must name that same stable identity, and its
native Sortformer channel must pass
the unchanged activity/run contract inside that piece, and every eligible
session-refreshed piece or phrase must agree. This three-source topology is an
exact forced-alignment overlay only; it cannot rewrite an adjacent phrase or
infer identity from transcript context. Its complete changed-context output is
manually reviewed before it can replace the baseline.

One additional abstention-only variant handles a temporal scale that cannot
pass the existing duration-aware voiceprint gates. Exactly one clean-gallery
piece/phrase scale may be eligible; the other must abstain rather than name a
competitor. Raw local identity, native channel activity, and all eligible
session-refreshed identities must agree with the surviving gallery scale. It
uses the same exact-piece projection and no new numerical threshold.

A current-audio counterpart handles short direct intervals whose identity is
overruled even though the exact posterior piece and enclosing punctuation
phrase independently select one session-refreshed identity. Raw local mapping
and the mapped native channel must agree, while either clean-gallery scale may
agree or abstain but never conflict. This is limited to a baseline-disagreeing
forced-alignment piece and does not relax any voiceprint or activity gate.

The current-audio path also has an abstention-only variant: exactly one of the
session-refreshed exact-piece or enclosing-phrase identities may be eligible.
Raw local identity, native channel activity, and every eligible clean-gallery
identity must agree with it, and at least one clean-gallery scale must be
eligible. Two eligible current scales follow the stricter
multiscale path, while disagreement remains a veto.

When all eligible voiceprint views agree or abstain, a sustained raw-local
fallback may withdraw a conflicting terminal direct assignment. The mapped
native channel must remain active for the existing FR16K sustained-run floor,
not merely the one-frame micro floor. The floor is copied into the candidate
TOML and checked against the inherited policy. The write remains confined to
the exact forced-alignment piece.

A raw-local-independent candidate is permitted only at the strongest available
voiceprint topology: both exact-piece and enclosing-phrase resolutions must
independently agree under both the session-refreshed and clean-gallery
registries. All four views must be eligible and name one stable identity. This
adds no threshold and writes only the exact piece; any abstention preserves the
baseline.

Sub-0.4-second micro pieces are not padded into a TitaNet query because that
would mix adjacent speakers at the exact boundary being resolved. Instead, a
diar-only micro path is limited to categorical native-frame purity: every frame
must expose one active channel, the same channel must be top-1 throughout, and
it must equal the frozen micro local slot. The write uses the already forced-
aligned micro source range and no new numerical gate.

The next fusion boundary restores model ownership: Sortformer decides speaker
separation, while TitaNet maps local slots and fills unsupported business
ranges. A frozen raw business range with known identity and
`sole_diar_support` is projected over the enhanced candidate without allowing
per-turn voiceprint relabeling. Unknown and non-sole ranges retain the enhanced
view. Both tracks must reconstruct the same ASR source and common-clock align
metadata before projection.

The complete changed-context manual review rejects that broad raw-authority
boundary. A sole Sortformer interval is not necessarily the correct real
speaker at rapid turns: restoring every such range reintroduces clear identity
errors at the beginning, middle, and end of the meeting. The experiment remains
an evidence artifact and must not be integrated or widened.

The next bounded topology handles the opposite evidence case without a weighted
decoder. A finalized punctuation phrase defines one exact source range through
forced alignment. The session-refreshed TitaNet registry and the frozen clean
multi-prototype gallery each evaluate that same audio independently with their
unchanged duration-aware gates. Only two eligible, identical identities may
rewrite the phrase, and a competing eligible direct anchor vetoes the complete
write. This permits a short voice-identified interjection inside a wrong raw
local run while naturally preserving phrases where the two registries disagree
or either abstains. The rule adds one TOML boolean, no numerical parameter, and
no reference-derived input. Every changed context is manually reviewed before
any complete candidate review; executable tools only generate and arrange the
unjudged evidence.

The unguarded dual-gallery candidate is also rejected by its complete
changed-context manual review. Both galleries share TitaNet and can therefore
make the same identity error; agreement between different prototype sets is not
model-orthogonal evidence. The next variant requires native Sortformer support
inside the exact phrase. Its target mapped channel must appear as frame top-1,
while a different channel's sustained active top-1 run rejects the phrase as a
real speaker-transition container. The activity threshold and run duration are
copied from the frozen FR16K TOML and checked for exact equality. This guard is
categorical and introduces no score weighting or result-tuned number.

The native-channel guard removes the known cross-speaker phrase propagation,
but its complete changed-context manual review exposes two remaining evidence
gaps. First, two shared-model gallery decisions plus a native top-1 frame can
turn a purely unknown baseline range into a confident wrong identity. Second,
a short direct anchor can be the same contaminated evidence that both galleries
then expand. The final guarded variant therefore acts only when a different
known baseline identity is actually challenged. If direct anchors are present,
at least one agreeing regular direct anchor is required; short-only anchor
support abstains. Both rules consume existing categorical audit fields and add
no numerical gate.

The final phrase guards produce a safe but low-coverage candidate. The next
work moves below phrase fusion into clean-gallery score construction. The
current maximum-over-prototypes rule lets one atypical prototype dominate an
identity score. Frozen prototype-to-prototype component evidence shows both a
low-cohesion Xu Zijing prototype and close Tang Yunfeng/Zhu Jie cross-identity
pairs. These are reference-free model diagnostics, not product-result metrics.

The robust gallery keeps all six frozen clean prototypes per identity and
computes each identity score as the arithmetic mean of that query's highest
half of prototype similarities. `top_half_mean` is categorical TOML policy;
the half count derives from the complete gallery rather than a tuned number.
Every embedding, interval, and source hash remains frozen, and all downstream
duration/score/margin gates remain unchanged. The resulting phrase evidence is
first substituted into the already guarded FR16ZB topology, then all changed
contexts are manually reviewed before any broader use.

The complete changed-context review retains that guarded phrase substitution:
it preserves the prior safe path and repairs an additional short phrase without
a whole-turn regression. A broader experiment substituted the same robust
piece and phrase evidence throughout the accepted R/T/U control chain. Its two
changed contexts both regress real speaker boundaries, so that substitution is
rejected. Robust gallery evidence remains confined to FR16ZB; it is not a
general replacement for maximum-prototype evidence in the current control
chain. See `robust-gallery-review-2026-07-15.md`.

Residual review then separates long-slot drift from low-volume local
transitions. The native frame table contains several real short-speaker runs
whose channel is continuously top-1 but whose absolute probability remains
below the unchanged `0.5` postprocessing threshold. Lowering that threshold
would alter the complete diarization track and is not permitted by this
candidate. Instead, VAD removes non-speech frames and a relative-top-1 run
defines only an evidence boundary inside an already forced-aligned punctuation
phrase. The run must last for the existing FR16J `0.4 s` floor. Its exact audio
is scored by the session-refreshed registry and by the robust clean gallery;
both must pass the existing duration-aware gates and agree with the mapped
native slot before the exact source range can challenge a known baseline
identity. This is a three-view local decision, not an epoch remap or a new
Sortformer threshold. Every changed context is reviewed before broader use.

FR16ZD's first frozen candidate is mechanically valid but does not repair a
complete business contribution. Three accepted pieces only move a same-speaker
boundary, while the fourth repairs the middle of one Shi Yi phrase but leaves
its tail assigned to Tang Yunfeng. The cause is evidence-window truncation:
punctuation phrases omit very short leading interjections and can shorten a
real local-channel run until TitaNet hears an adjacent speaker.

The next topology therefore starts from a complete native top-1 island rather
than a punctuation phrase. Inside one continuous VAD interval, only an
`A-B-A` run sequence whose three runs meet the inherited `0.4 s` floor is
eligible. The complete B-run audio is independently evaluated by the
session-refreshed and robust clean-gallery TitaNet views, while forced alignment
projects only complete character units contained by B. This preserves the
model's exact local transition as the acoustic query without extending the
business rewrite into either A run. Both registries and the frozen local-slot
mapping must agree, and a different known baseline identity must be challenged.
Changed contexts are read manually before any complete candidate review.

The local-island experiment exposes a production endpoint defect before it
justifies another fusion rule. `speech_pad_ms` is parsed, serialized, and passed
to `GpuVad`, but the detector publishes unpadded endpoints. This truncates a
real channel transition at VAD onset or end and can reduce a complete short
turn below the inherited `0.4 s` evidence floor. `GpuVad::DrainSegments` will
apply the configured sample padding only when emitting a completed interval:
the acoustic state machine and probabilities remain unchanged, starts clamp at
zero, ends clamp at the processed horizon, and the existing minimum-silence
contract prevents adjacent padded intervals from overlapping. A paired real-
audio mechanical test compares zero-padding and configured-padding instances
before any frozen evidence or WebSocket rerun is considered.

Padded VAD evidence also exposes a separate phrase-layer coverage gap. A real
short utterance can satisfy the VAD duration floor and use one native local
channel throughout, yet disappear before speaker fusion because punctuation
phrase construction requires more visible characters. The bounded remedy uses
the complete padded VAD interval as both acoustic query and legal container.
It is restricted to the existing TitaNet short-duration class, requires one
top-1 channel for every contained native frame, and requires agreement between
session-refreshed TitaNet, robust-gallery TitaNet, and the frozen local map.
Only wholly contained forced-alignment units are projected. This path adds no
new numerical threshold and is reviewed on every changed context before any
complete candidate review.

The complete FR16ZH changed-context review retains one clear repair but rejects
the unrestricted candidate because another boundary trades adjacent speakers
and several writes alter only isolated characters. The repaired edge run has a
distinct native contract: it is continuously top-1 for one channel, but every
frame remains below the existing FR16J `0.5` activity threshold. The other
accepted edge runs contain active frames and are already visible to native
postprocessing. FR16ZI therefore adds a no-active-frame guard, mechanically
equal to the frozen TOML threshold. This introduces no identity, timestamp,
text, or new numerical condition and directly scopes the rule to the evidence
gap it is designed to fill. See `vad-edge-run-review-2026-07-15.md`.

FR16ZI leaves one displayed assignment change. Complete manual contextual
review confirms that `ref-0146` is repaired and finds no other changed context.
The guarded low-activity edge-run candidate is retained as the next composition
baseline. Any later topology must be layered on this candidate and must
preserve this repaired contribution during changed-context review.

The complementary active-edge path addresses a different evidence gap. Some
continuous VAD intervals contain a sharp native channel handoff where every
frame on both sides exceeds the existing activity threshold, but an enclosing
ASR/TitaNet interval erases the short edge speaker. FR16ZJ reuses FR16ZH's
first/last-run scope, adjacent sustained-run contract, complete acoustic query,
and exact alignment projection. It additionally requires every frame in both
runs to meet the existing FR16J activity threshold. The mapped Sortformer
channel is authoritative only when the two unchanged TitaNet views do not form
an agreed different-identity veto. This distinguishes absence or disagreement
of voiceprint evidence from positive orthogonal evidence against the local
handoff. All changed contexts are reviewed manually before composition.

The complete 24-context FR16ZJ manual review rejects the unrestricted active
edge rule. It completely repairs `ref-0221`, but sustained local activity can
begin before or end after the corresponding semantic contribution. Projecting
all wholly contained alignment units therefore regresses several otherwise
continuous contributions, including `ref-0005`, `ref-0068`, `ref-0299`,
`ref-0358`, and `ref-0382`. The local-slot identity is useful evidence; the
missing contract is a complete contribution boundary. FR16ZI remains the
retained baseline, and no reference-derived timestamp, identity pair,
transcript token, or numerical threshold is introduced from these rows. See
`vad-active-edge-review-2026-07-15.md`.

FR16ZJ establishes that stable local identity is insufficient when projection
starts or stops inside a semantic contribution. FR16ZK moves the ownership
boundary to an existing finalized punctuation phrase. It admits a phrase only
when the complete aligned phrase lies in one padded-VAD interval, all native
frame centres throughout it expose the same top-1 local channel, and every
existing fragment in that exact source range has one known competing identity.
The phrase duration is bounded by the existing FR16J sustained floor and the
TOML speaker embedding window. Session-refreshed and robust-gallery TitaNet
query the same complete phrase; either eligible different identity is an
orthogonal veto. A sub-gate raw top rank matching the uniform baseline identity
also vetoes, but can never authorize a rewrite; this uses rank only as
conservative counter-evidence and introduces no score threshold. Projection
replaces the whole phrase or abstains, so no edge fragment can be extended into
an adjacent contribution. The frozen mapping,
VAD, alignment, frame table, evidence tables, baseline, policy, and source
hashes are recorded mechanically. Every changed context is then read manually;
the generator cannot judge or select the result.

The first FR16ZK candidate exposed one unsafe local-slot phrase where an
otherwise abstaining TitaNet view still ranked the uniform baseline identity
first. The categorical top-ranked-baseline veto removes that rewrite without
using a score threshold. The guarded candidate leaves two complete phrases.
Manual review of all four displayed contexts finds one previously incorrect
semantic contribution repaired at `ref-0069`, one already-usable contribution
strengthened at `ref-0156`, and no regression. The guarded FR16ZK candidate is
retained as the next baseline. See
`complete-local-phrase-review-2026-07-15.md`.

FR16ZL addresses only a pure-unknown complete phrase. The exact source range
must be immediately bracketed inside one ASR source by known fragments carrying
the same stable identity. The complete phrase must also pass FR16ZK's padded-
VAD, inherited duration, and single native top-1 channel contracts, with the
frozen local mapping equal to the bracket identity. Session-refreshed and
robust-gallery TitaNet may agree or abstain, but either eligible different
identity vetoes. This combines temporal continuity, orthogonal local separation,
and conflict-only voiceprint evidence without letting transcript meaning or a
distant neighbor assign identity. Projection fills the exact whole phrase or
abstains, and all changed contexts receive manual semantic review.

The frozen FR16ZL candidate produces no rewrite. Of the mechanically eligible
complete phrases, only one is covered entirely by unknown baseline evidence,
and its unknown fragment extends beyond the phrase source boundary, so no
immediately adjacent known bracket exists. The contract is retained as a
tested no-op and is not relaxed to search across unknown source text.

FR16ZM applies immediate finalized-phrase brackets to a uniform known-conflict
phrase. The preceding and following punctuation phrases in the same ASR source
must each carry one uniform stable identity; every native candidate-phrase
frame must map to that same identity, while the current phrase baseline
uniformly carries a different known identity. Either eligible TitaNet conflict
vetoes. This topology can
repair an isolated complete-phrase mislabel while rejecting a local-slot island
whose surrounding contribution still belongs to the baseline identity. It
adds no threshold and cannot skip a phrase or use cross-source conversational
context. Business-track fragment cuts are not used as semantic boundaries.

The frozen FR16ZM candidate produces no rewrite. Twenty-seven complete
known-conflict phrase islands have valid same-identity phrase brackets, but in
every case the native local channel maps to the island identity rather than the
bracket identity. The local separation therefore supports a real intervening
contribution instead of temporal smoothing. Close this topology as a tested
no-op; do not weaken local-map agreement or skip intervening phrases.

FR16ZN addresses the remaining short-query evidence gap without changing the
semantic write boundary. It begins with an FR16ZK-complete phrase and expands
only the acoustic query to the maximal contiguous native top-1 run containing
that phrase inside the same padded-VAD interval. The query is rejected rather
than cropped when the maximal run exceeds the existing speaker embedding
window. Session-refreshed and robust clean-gallery TitaNet must both pass their
unchanged duration-aware gates, agree with each other, and equal the frozen
local-slot mapping. The current phrase must still have one uniform different
known baseline identity. Projection remains the exact complete phrase, so
extra query audio can improve voiceprint evidence but cannot move a text or
speaker boundary. Source hashes and the phrase/query intervals are frozen in
metadata. Tools may execute and arrange this evidence, but every changed
context and any complete product result are judged only by manual contextual
semantic review.

The frozen FR16ZN candidate admits one phrase. Complete manual review of both
affected reference contexts shows that `都已经聊到这了` belongs to Zhu Jie but
the candidate assigns it to Xu Zijing; Xu Zijing's next contribution starts
after that phrase. Reject the positive rewrite rule and retain guarded FR16ZK.
The longer same-slot query remains useful only as veto evidence because it also
blocks several locally confident, voiceprint-conflicting phrases around
`2865-2869` seconds. See
`expanded-local-run-phrase-review-2026-07-15.md`.

FR16ZO reverses the role of the longer local-run query. It no longer treats the
mapped local channel as positive identity evidence. Instead, the local run
supplies only a homogeneous acoustic context boundary. The query uses that
whole context up to the existing three-second embedding limit; a longer run
uses a deterministic phrase-centred maximum window clamped inside the run.
Both session-refreshed and robust clean-gallery TitaNet must pass the unchanged
gates and agree on an identity that differs from both the current uniform
phrase identity and the local-slot mapping. This isolates stable Sortformer
misclassification cases while excluding the FR16ZN failure where all three
views agreed on the same wrong identity. The first frozen candidate still
contains two voiceprint-only regressions where the selected identity has no
corresponding native channel in the query. The guarded contract therefore also
requires the selected identity's mapped Sortformer channel to be active for the
unchanged FR16J sustained-run floor inside the query. This treats an observed
secondary channel as positive orthogonal support and rejects a voiceprint-only
override. The exact complete phrase remains the only write range. Candidate
generation records full-run, bounded-query, and per-channel support intervals
and does not consult reference content or evaluate the result.

The guarded FR16ZO candidate admits only `ref-0258`. Complete manual context
review shows that the main `这个你说了算` statement and the following
`我不插话` now share Tang Yunfeng's identity. A repeated `说了算` fragment of
approximately `0.4 s` retains the old Shi Yi identity and is recorded as a
boundary residual. The meaningful contribution is repaired without another
assignment change, so guarded FR16ZO becomes the retained composition
baseline. See `bounded-local-run-voiceprint-review-2026-07-15.md`.

FR16ZQ corrects a gallery coverage defect below fusion. Global
`confidence * duration` truncation places all six Xu Zijing prototypes in the
first seven minutes even though the session lasts more than one hour. For each
identity, all segments first pass the unchanged duration, confidence, overlap,
initial/terminal TitaNet, and local-map gates. The eligible list is then sorted
on the common clock and divided into six contiguous rank strata, where six is
the existing TOML gallery size. The original quality maximum selects one item
inside each stratum. This keeps every prototype clean while representing the
full session. The prototype embedding probe and robust top-half query scorer
remain unchanged. First substitute this gallery into the retained FR16ZO path;
only after complete changed-context review may it be used by earlier fusion
layers.

FR16ZQ changes no speaker sequence in the retained FR16ZO path and swaps two
internally accepted phrase overlays without changing the guarded phrase
business output. Close it as component evidence rather than replace the
quality-truncated gallery.

Component calibration also shows that the robust `0.55` regular score gate is
not the abstention root cause: deterministic three-second crops from every
frozen clean prototype retain their independently established identity above
the existing gate. The remaining configuration discrepancy is query length.
Committed production `orator.toml` permits a ten-second TitaNet embedding
window, while the recent complete-phrase experiments set three seconds. FR16ZR
restores exact production-window parity in the otherwise unchanged guarded
FR16ZO topology. Long same-top-1 runs remain deterministic context boundaries;
only the exact complete phrase can be written. When a selected identity never
crosses the existing `0.5` activity threshold, FR16ZR also admits categorical
secondary-channel support only if that mapped channel is native top-2 on every
query frame. This does not lower the activity gate or add a score; one missing
top-2 frame rejects the alternative.

FR16ZS closes a projection gap exposed by short punctuation clauses. The
punctuation index intentionally omits clauses below its existing character
floor, so an otherwise complete contribution can be split even when its whole
ASR source is aligned inside one FR16ZR query. The extension is categorical:
the source must have exactly one indexed phrase, every non-separator character
must be wholly aligned in the same query, and the complete baseline source must
carry one known identity. It then reuses the unchanged FR16ZR identity decision
and projects the complete source. Multi-phrase or partially aligned sources
remain untouched.

The frozen FR16ZR candidate changes only `ref-0194`. Manual contextual review
finds the identity evidence correct but the punctuation-only projection
incomplete: the indexed prefix moves to Xu Zijing while the short suffix remains
on Tang Yunfeng. FR16ZS accepts that same unchanged evidence because this ASR
source has one indexed phrase, complete in-query forced alignment, and one
uniform known baseline identity. Its complete-source projection assigns the
whole contribution to Xu Zijing. Manual review confirms the repair and no other
speaker-sequence change, so FR16ZS becomes the retained composition baseline.
See `production-window-complete-source-review-2026-07-15.md`.

FR16ZT revisits the useful native handoff exposed by rejected FR16ZJ without
restoring its disconnected character projection. It enumerates punctuation
clauses directly from the ASR source, including short clauses omitted by the
phrase character floor. A clause is eligible only when every non-separator
character is aligned wholly inside one sustained, all-active terminal run
of a multi-run padded VAD interval. Consecutive eligible clauses may be merged
only when their source ranges are adjacent. The complete group must replace one
uniform known baseline identity. Identity comes from the frozen local-slot map;
session and robust TitaNet retain the inherited agreed-different veto. This
topology makes the semantic contribution boundary an input contract and
prevents the isolated-character regressions seen in FR16ZJ.

Manual review of the first seven accepted groups separates terminal from
initial edge behavior. All four terminal-edge changes are contextually correct
or attribution-neutral; two initial-edge fillers are ambiguous and one initial-
edge `啊` belongs with the following speaker. The initial edge ends exactly at
the next speaker transition, where forced-alignment lag can place the next
speaker's onset on the outgoing channel. FR16ZT therefore accepts only the
terminal edge, where the complete post-handoff clause is bounded before VAD
closure. This is a general alignment-direction contract; no timestamp, text,
identity pair, or reference datum enters the candidate.

FR16ZU consumes overlap evidence that top-1-only projection discards. For each
complete punctuation phrase, the frozen raw frame table records continuous
per-channel probabilities on the common clock. Session-registry and robust-
gallery TitaNet independently rank the same phrase. A sub-score-gate identity
can challenge one uniform known baseline only when both TitaNet views rank it
first with the unchanged duration-class margin and its mapped Sortformer
channel is continuously active for the inherited activity and duration floors.
At least one frame must place that channel below top-1, explicitly scoping the
rule to overlapping/secondary evidence. The exact complete phrase is the only
legal write boundary. This combines two voiceprint registries with a genuinely
orthogonal raw model signal without lowering a numerical gate.

The frozen FR16ZU generation enumerates 1195 structurally complete phrase
contexts but produces no timeline write: no context simultaneously satisfies
the two-registry same-top-rank contract, the inherited margin, sustained mapped
secondary-channel activity, and a uniform known baseline conflict. This is a
mechanical candidate-generation fact, not a product-quality judgment. Because
there is no changed context, there is nothing to submit to manual semantic
review and FR16ZU is closed as a no-op without changing the retained FR16ZT
baseline. No executable result evaluation, accuracy aggregation, candidate or
parameter ranking, selection, or verdict is derived from the enumeration.

FR16ZV addresses a different failure mode: a contextually continuous
contribution can contain rapid native top-1 changes and short voiceprint
decisions that mutually reinforce those local changes. The candidate derives
stable native runs directly from the frozen frame table. A same-channel pair
may bracket a churn region only when both outer runs satisfy the inherited
all-frame activity and sustained-duration contract. A deterministic query
contains the complete interior and the required acoustic support from both
outer runs, without exceeding the production speaker embedding window.

The two frozen TitaNet galleries then evaluate that longer query independently.
Both must pass the existing regular gates and select the outer channel's stable
identity. Writes use finalized punctuation only: each intersecting phrase is
included whole and every non-separator alignment unit must lie inside the
query. The first complete changed-context review shows that whole-query
agreement can still absorb a real short contribution when the outer speaker
dominates the longer crop. The guarded topology therefore evaluates each
projected phrase independently: its baseline ownership must be one uniform
different identity, and both phrase-level galleries must rank the outer
identity first even when a duration gate causes abstention. A one-channel
interior also retains the stronger eligible-agreement veto for its mapped
identity. Mixed phrases and phrase-rank disagreements abstain instead of being
smoothed. Overlapping accepted regions may compose only when they select the
same identity; otherwise both abstain. This design changes neither the model
nor any score threshold and introduces no reference-derived timestamp, text,
identity, or numerical value.

The guarded exact-phrase path leaves useful evidence incomplete when short
punctuation clauses are absent from the indexed phrase set. FR16ZV therefore
also records the native top-1 margin for every stable run. A complete clause
group can expand across the interior only when every intervening run has lower
mean margin than both outer runs, making the comparison local and relative
rather than introducing a tuned threshold. Any phrase for which both TitaNet
views rank the same different identity first vetoes expansion, regardless of
score eligibility. Every intervening frame must also report exactly one active
native channel; simultaneous activity is a cannot-link constraint and blocks
consolidation. The write range is reconstructed from adjacent complete
punctuation clauses with all non-separator alignments inside the existing
query; it never fills an arbitrary timestamp gap.

Complete manual review rejects the unrestricted 22-piece FR16ZV candidate: a
whole-query identity can absorb real short contributions when overlapping or
high-confidence inner speech is present. The phrase guards reduce the candidate
to two partial rewrites, and the relative-margin clause path initially exposes
one remaining overlap regression. Requiring one active native channel on every
inner frame leaves three changed contexts. Manual contextual review confirms a
complete Shi Yi contribution repair at `ref-0441` and attribution-consistent
cleanup at `ref-0452` and `ref-0554`, with no reviewed regression. The final
FR16ZV candidate is retained as the next composition baseline. See
`bracketed-local-churn-review-2026-07-15.md`.

FR16ZW targets long contributions that FR16ZV cannot represent because the
nearest same-channel closure ends before a stable right context, or because the
contribution crosses an ASR source boundary. It scans the same frozen stable-run
table but chooses the farthest qualifying closure inside the unchanged
production embedding capacity. The query first reserves the complete interior
and inherited support on both sides, then uses remaining capacity for actual
outer-run audio. This gives the two TitaNet galleries maximal context without
moving a source or acoustic boundary.

The topology is restricted to two-or-more foreign local channels. A single
inner channel remains the real-interruption case governed by FR16ZV and is
ineligible here. Outer support must exceed total foreign-run duration. The
dominant foreign channel is derived only from stable-run duration; same-top-rank
phrase evidence from both galleries for its mapped identity vetoes the whole
envelope. Accepted writes are reconstructed as complete aligned clauses per
ASR source, while all clauses share the one common-clock acoustic decision.
The complete projected clause set must be contained by one frozen VAD segment
from the comprehensive timeline. This preserves independent speech endpoints:
a long speaker embedding cannot bridge a VAD boundary and absorb a separate
contribution. The VAD interval contributes no new score or tuned duration.
This is a distinct multi-slot-separation contract, not a threshold retune.

FR16ZX addresses fragmented contributions that have a strong VAD boundary but
do not form an A--multi-slot--A native envelope. It treats the frozen padded VAD
segment as the acoustic and semantic containment boundary. Stable native runs
inside that segment contribute duration by local slot; only a strict duration
majority is eligible. The full VAD segment is evaluated independently by the
session and robust TitaNet galleries, and both identities must equal the frozen
mapping of the dominant native slot.

Projection remains textual rather than timestamp filling. Forced alignment
must place every non-separator character of each adjacent complete punctuation
clause inside that same VAD segment. A dual-gallery phrase top rank for any
other active identity vetoes the entire contribution, preserving a supported
interjection even when the longer VAD query is dominated by its surrounding
speaker. The first unrestricted changed-context review confirms that VAD-level
agreement alone can still absorb short replies. Each write clause therefore
receives an exact forced-alignment query in both galleries and must pass the
unchanged duration-class gate with the same VAD identity. A clause abstention
does not fill its text or time range. To prevent a long clause crop from
absorbing a real short reply, the selected identity must already occur in the
baseline clause and every contiguous conflicting baseline fragment is queried
on its own exact aligned interval. All fragment queries must independently
agree with the VAD and clause identity before the complete clause can be
written. Only baseline conflicts are written, and overlapping different
identities abstain. Every boundary and numerical gate is inherited from the
frozen TOML and common time base.

FR16ZY separates source context from write scope. The complete forced-aligned
ASR source supplies up to the production TitaNet window, while only its indexed
complete punctuation phrases can be projected. Both source-level galleries
must agree, the selected identity's mapped native channel must have an
inherited stable run inside the source, and every indexed phrase must be VAD-
contained and rank the same identity first in both phrase galleries. This
allows complete phrases split by local-slot churn to use longer acoustic
context without allowing the source query to absorb an unindexed reply or fill
an alignment gap. The first changed-context review finds one merged-source
handoff dominated by the trailing speaker. Requiring the selected identity on
both baseline phrase boundaries limits writes to internal churn and preserves
edge handoffs. Projection remains phrase-exact and baseline-conflict-only.

The complete FR16ZG changed-context review rejects the unrestricted rule. One
of its two changed contexts inserts a false Shi Yi interruption into Tang
Yunfeng's continuous statement because the VAD interval projects disconnected
characters from a longer contribution. The other context moves two characters
to the correct Shi Yi identity but leaves the surrounding contribution split,
so it is not a complete business repair. No character-count or identity-pair
guard is introduced from these rows. The next evidence topology must cover a
complete contribution boundary or abstain. See
`vad-utterance-review-2026-07-15.md`.

The rejected FR16ZG rows separate isolated short evidence from a real speaker
handoff. In the native frame stream, several residual transitions are complete
top-1 channel runs at the start or end of one continuous VAD interval. They are
omitted by punctuation minimum-character filtering, while the older unpadded
VAD endpoint can also shorten them below the inherited run floor. FR16ZH uses
only a first or last run from a multi-run padded VAD interval. The edge run and
its immediate neighbour must both satisfy the existing FR16J run floor, and the
complete edge run must fit in the existing TitaNet embedding window. The exact
audio is evaluated by session-refreshed and robust-gallery TitaNet, then only
wholly contained alignment units may challenge a known baseline identity when
both voiceprint views and the frozen local map agree. Single-run VAD intervals
are excluded, which prevents FR16ZG's unsupported isolated interruption from
re-entering this path. Every changed context is read manually before any
complete candidate review.

No transcript phrase, real speaker name, known timestamp, or reference label is
part of runtime logic. Each output includes an audit record containing source
evidence, chosen speaker, rejected alternatives, confidence margin, and reason.

The first runtime audit increment is attribution-neutral. Each
`business_speaker` entry receives a structured `speaker_decision` object with:

- `speaker_source` and `text_projection_source`;
- a decision `reason` that distinguishes no support, sole support, same-speaker
  gap fill, and competing diar support;
- all local/global diar candidates, with union overlap, entry coverage,
  overlap-weighted confidence, island count, and a selected flag;
- selected-versus-best-rejected overlap and confidence margins.

The object is computed only from immutable typed evidence on the common clock.
It is serialized identically in live revisions and terminal output, retained by
the browser model/export path, and included in convergence checks. This step
does not change attribution, support thresholds, uncertainty, or raw tracks. A
later decision-policy experiment may use the audit only after its own frozen
TOML policy and complete contextual review are defined.

Legacy frozen packages are replayed without another model run. A tools-only
decision-evidence utility consumes the terminal diarization and
`business_speaker` tracks, reconstructs the runtime audit with the same
candidate grouping, interval union, confidence weighting, deterministic order,
reason classification, and JSON precision, and emits a source-hashed package.
If an input already contains runtime decisions, every structural field must be
equal. Historical diar confidence was serialized to three decimal places, so
legacy confidence replay is accepted only inside the proven +/-0.0005 candidate
and +/-0.001 margin envelopes. Historical and current timeline boundaries are
serialized to milliseconds, so overlap, coverage, and overlap-margin checks use
duration/island-count bounds derived from that quantum. Candidate identity,
selection, ordering, reason, projection source, and island count remain strict.
Current live and terminal diar serialization retains round-trip confidence.
Every bounded result is marked as quantized. This is a parity and evidence-
indexing step only: the utility does not read `test.txt`, choose a new speaker,
or score accuracy.

### 6.2.1 Equal-overlap arbitration audit

The root-layer audit after FR16ZY found that the production business projection
orders equal-overlap diarization candidates by total segment length before
confidence. At an overlapping micro-run, this makes an 80 ms secondary segment
override a longer, more confident primary segment even though both have exactly
the same overlap with the atomic interval. The defect is in business projection,
not in the model output or local-to-global identity mapping.

FR16ZZ introduces a typed TOML policy with two deterministic values:
`shorter_span` preserves the prior behavior and `higher_confidence` compares
the segment confidence first, then uses shorter span only when confidence also
ties. The policy changes no boundary, model score, raw track, voiceprint, VAD,
ASR, or alignment record. Focused C++ tests cover both policies and verify that
overlap remains the primary decision criterion.

The frozen full-session raw tracks are replayed once with
`higher_confidence`. Tools may verify source hashes, deterministic output, and
unchanged evidence tracks and may arrange every changed context. The candidate
is judged only by reading all changed conversational contexts manually; no
code, script, formula, query, notebook, metric, or algorithm may label a
result, aggregate accuracy, select either policy, or issue the verdict.

The complete changed-context reading stops FR16ZZ. Confidence-first arbitration
repairs weak micro-slot chatter but also absorbs genuine short replies such as
`对`, `相差0.7`, and `你们俩可以`. The experiment is retained only as a typed,
default-off diagnostic; it is not promoted to `orator.toml` and does not enter
the closing candidate.

### 6.2.2 Native primary-speaker evidence

Frame inspection distinguishes the rejected cases without a new fitted score:
genuine short replies form a sustained native top-1 run, whereas the observed
chatter is a one-frame top-1 flip inside another speaker's longer run. FR16AAA
therefore derives a separate, non-overlapping primary-speaker view from the
already captured v2.1 frame posterior. It uses frozen VAD containment, the
native `0.5` activity boundary, and the existing FR16J `0.4 s` sustained-run
floor. Short runs abstain and are never smoothed into a neighbour. Identity is
the frozen local-slot mapping on the same absolute clock.

The primary view is replayed through the production C++ business pipeline. A
second mechanical composition may project only exact source fragments carrying
decision reasons from manually retained orthogonal voiceprint layers. The
allowlist is frozen in TOML and excludes all generic baseline/direct reasons.
Tools verify time, source-text, hash, deterministic, and allowlist contracts;
they do not compare with the reference or decide whether a write is correct.
Every changed context is read manually before the complete chronological and
reverse-block review.

### 6.2.3 Primary evidence as bounded tie arbitration

The complete FR16AAA review shows that primary top-1 is useful evidence but is
not a safe replacement for the activity/voiceprint view: it repairs sustained
short turns and also overwrites correct long voiceprint assignments. FR16AAB
therefore stores primary top-1 beside, not inside, activity diarization. The
business projector keeps activity boundaries, support calculation, and unique
maximum-overlap decisions unchanged. Only an exact maximum-overlap tie may
consult the primary track, and only one tied activity identity matching the
maximum-overlap primary identity may win. Missing or ambiguous primary support
falls back to the existing deterministic activity policy.

The frozen full-session experiment replays the original activity track and the
independent primary track through the production C++ projector twice. Tools
check typed-track isolation, source preservation, hashes, and determinism only.
Every displayed speaker-sequence change is read manually before this policy
can enter a complete two-direction contextual review or runtime TOML.

### 6.2.4 Primary-aligned acoustic islands

The broad primary replacement and generic tie-arbitration experiments show that
no one diarization view is independently authoritative. FR16AAC instead forms a
small acoustic island only where three deployable tracks agree on the common
clock: a VAD-bounded sustained primary top-1 run, production activity support
for the same mapped identity, and a robust clean-gallery TitaNet decision under
the already frozen duration-class score and margin gates. The primary run uses
the inherited `0.5` posterior and `0.4 s` duration requirements; activity must
cover at least the same `0.4 s`. No new fitted number is introduced.

Forced alignment contributes boundaries, not identity. Only complete aligned
units wholly inside the agreed acoustic island may be projected, and only when
the corresponding baseline characters carry one known, conflicting identity.
Zero-duration aligned units may remain attached to a contiguous positive-time
unit, but unaligned edge punctuation is excluded. Existing reviewed overlays,
source text, and absolute time remain immutable; any overlap conflict or missing
contract causes abstention. Tools may execute models, verify those mechanical
contracts, record hashes, and arrange changed contexts. They may not inspect the
reference, classify product correctness, aggregate accuracy, rank/select a
candidate or parameter, or issue the acceptance verdict.

### 6.2.5 Complete-phrase cross-prototype challenges

The frozen prototype-local experiment already contains a categorical challenge
that restores an initial prototype only when a multiprototype reduction fails
solely on margin, the terminal direct override is different, the reduced top
pair has the required order, the raw local map agrees with the initial identity,
and eligible current piece/phrase evidence does not veto it. FR16AAD narrows
that existing evidence rather than changing any model gate: the accepted source
range must exactly equal one complete punctuation phrase. Adjacent accepted
challenges are not merged across a phrase boundary.

The composer applies an exact phrase only over one uniform known conflicting
identity in the current retained baseline. Manually retained overlay reasons
are protected by a TOML allowlist, and all source text and absolute times remain
immutable. Tools verify challenge provenance, phrase equality, identity parity,
source reconstruction, non-overlap, hashes, and determinism. They do not read
the reference, assign product correctness, aggregate accuracy, rank/select a
candidate or parameter, or issue a verdict.

### 6.2.6 Relative-top-1 complete-phrase expansion

FR16ZD can establish a stable identity on a sustained low-activity subpiece
while leaving the enclosing semantic contribution split at the subpiece edge.
FR16AAE may expand that already selected identity only to the exact enclosing
punctuation phrase. The original FR16ZD local map, session-registry, robust-
gallery, duration, and score decisions remain the identity authority.

The complete phrase is queried independently in both registries as a boundary
veto. Both must rank the already accepted piece identity first and satisfy the
existing TOML margin. The phrase query cannot use its rank to create a new
identity decision and cannot bypass the regular absolute-score gate. Every
phrase character must have a known current-baseline identity, a conflict must
exist, and reviewed overlay reasons remain protected. Source text and absolute
time are reconstructed exactly. Tools verify these contracts, hashes, and
determinism only; all result judgments and the promotion verdict remain manual.

### 6.2.7 Complete VAD phrase challenges

FR16AAF handles the complementary failure where Sortformer emits one stable
local slot through a short speech island but both TitaNet registry views identify
the island as another enrolled speaker. A short padded VAD interval supplies the
outer acoustic boundary, while its unique enclosing punctuation phrase supplies
the exact semantic write range. Both the VAD query and complete phrase query are
already frozen and are evaluated by the session registry and robust clean
gallery under the unchanged duration-class score and margin gates.

The first unrestricted candidate changes three contexts. Manual reading finds
that two low-score outer VAD queries split continuous same-speaker statements,
while the strong outer query repairs a real handoff. A raw-slot challenge
therefore also requires both outer VAD views to meet the existing regular-score
floor. Exact phrase views retain their normal duration-class gates. This is a
stricter reuse of the frozen score floor, not a new fitted value.

All four voiceprint views must select the same active identity, which must
challenge both the mapped raw local slot and one uniform known baseline identity.
The phrase may extend beyond the padded VAD edge only by one frozen native frame
to account for the existing alignment/VAD quantization; the tolerance is derived
from metadata rather than expressed as seconds. Reviewed overlay reasons remain
protected, and the projection reconstructs the exact source text and absolute
times. Tools verify these contracts, hashes, and determinism only. They do not
read the reference, assign correctness, aggregate accuracy, rank/select a
candidate or parameter, or issue a product verdict.

### 6.2.8 Contextual VAD phrase challenges

FR16AAG extends the same evidence ownership to a complete phrase inside a
longer VAD interval. The outer VAD crop supplies contextual speaker evidence;
the complete punctuation phrase remains the only write boundary. A single
primary raw run must cover that phrase, but its mapped identity is conflict
evidence rather than authority because the candidate exists to recover a
speaker merged into another local slot.

All four VAD/phrase registry views must top-rank one identity under the inherited
margin. At least one outer VAD registry view must also meet the inherited
regular-score floor; the other outer view remains an independent top-rank and
margin veto. No phrase absolute gate is bypassed to select among identities:
all four top ranks must already be identical. The current phrase must be
uniformly assigned to one other known identity, reviewed overlays remain
protected, and boundary tolerance is exactly one frame derived from the frozen
native frame table. Tools verify contracts, hashes, and determinism only; all
changed and full result judgments remain manual contextual semantic review.

### 6.2.9 Edge-anchor-trimmed phrase challenges

FR16AAH addresses a forced-alignment phrase that straddles a real handoff. A
regular direct voiceprint anchor for the preceding or following speaker can
legitimately occupy one phrase edge while causing the existing whole-phrase
conflict veto to discard an independently voice-identified reply at the other
edge. The new path does not weaken that anchor: it trims the write range at the
anchor's exact source-character boundary and preserves the anchored text.

The competing direct-anchor overlap must form one contiguous prefix or suffix;
anchors on both edges, interior anchors, gaps in anchor source coverage, and an
anchor agreeing with the proposed identity all abstain. The first aligned audio
unit in the remainder must begin at least one frozen native frame after the last
anchored unit ends, or symmetrically for a suffix. The frame duration is derived
from the frozen frame table and the multiplier is TOML-owned as exactly one, so
no fitted seconds threshold is introduced.

The complete punctuation phrase is still the acoustic evidence query. Its
session registry and robust clean-gallery views must independently pass the
unchanged duration-class score and margin gates and choose the same identity.
Only the unanchored source remainder is projected, all known current identities
there must conflict, and protected reviewed overlays abstain. Source text and
absolute alignment times remain immutable. Tools verify these contracts,
hashes, and determinism only; every changed-context and final result judgment
remains manual contextual semantic review.

### 6.2.10 Adjacent subminimum-clause envelopes

FR16AAI fills an evidence-coverage gap in the punctuation phrase extractor.
Rapid replies such as a discourse marker followed by a short noun phrase can
be represented as two complete adjacent clauses, each shorter than the frozen
phrase embedding minimum. Neither clause is queried even when their combined
aligned audio is long enough for the existing model contract. The new evidence
unit is exactly two adjacent clauses from one immutable ASR source.

Each constituent must have positive-duration forced-alignment support and must
remain below the existing punctuation minimum. The combined first-to-last
aligned span must satisfy the existing punctuation minimum/maximum; no new
duration or silence threshold is added. Both the session registry and robust
clean gallery query that same combined audio and must independently pass the
unchanged duration-class gates with one identity.

Projection covers the exact combined source-character range only when its
current labels are one uniform known conflicting identity. Protected reviewed
overlays abstain. If independently eligible envelopes overlap, all overlapping
proposals abstain rather than selecting among them. Tools may enumerate spans,
run models, verify contracts and hashes, and arrange contexts. They may not
read references, assign correctness, aggregate accuracy, rank/select results or
parameters, or issue the verdict.

### 6.2.11 Two-phrase primary-run anchor expansion

FR16AAJ uses an already stable primary Sortformer run as a local grouping
contract, not as sole identity authority. Exactly two complete punctuation
phrases from one ASR source must be fully contained by the run and adjacent in
source characters. One phrase is the anchor: its retained business range
contains the run's mapped stable identity, and both frozen TitaNet registries
independently confirm that identity under unchanged gates.

Across both phrases every label must be known, the mapped identity and exactly
one competing identity must both occur, and no third identity is permitted.
It can be expanded only when its two exact-phrase voiceprint views do not both
produce eligible non-mapped evidence: dual eligible agreement on the mapped
identity is allowed, while any eligible disagreement or jointly eligible
competitor preserves the baseline. This distinction keeps ordinary dual-
voiceprint authority intact while allowing one low-information short view to
abstain instead of defeating the orthogonal raw-run and anchored-phrase pair.

An accepted proposal closes both exact phrases to the mapped identity; this
also repairs a misprojected edge fragment inside the otherwise confirmed anchor
instead of treating that old label as new evidence. The run-to-phrase tolerance
is one native frame derived from frozen frame metadata. Reviewed overlays
remain protected, writes use exact source ranges, and overlapping proposals all
abstain. Tools verify evidence and contracts and arrange contexts only; all
correctness and promotion judgments remain manual.

### 6.2.12 Phrase-led outer-abstention challenge

FR16AAK addresses a narrow evidence topology in which the long outer evidence
unit and the exact phrase carry different acoustic identities. It does not
lower a gate. One VAD interval and one primary raw run must uniquely contain
the exact phrase within one frame. The retained phrase must be uniformly mapped
to that raw-run identity and must consist only of TOML-listed unconfirmed
baseline provenance.

Both outer VAD registry views must rank that current identity first but abstain
under the existing duration-class gates. Both exact-phrase registry views must
instead rank the same challenge identity first. Exactly one phrase view must
pass the existing gates; the other may abstain only because its first/second
margin remains below the same existing margin gate. Score, availability, or
incomplete-score abstention is not sufficient. This establishes local acoustic
agreement without converting a weak top rank into an eligible decision.

Projection is restricted to the phrase's immutable source-character range.
Protected overlays, mixed current identities or provenance, eligible outer VAD
evidence, any top-rank disagreement, non-margin phrase abstention, missing or
multiple containers, and overlapping proposals all abstain. TOML owns every
categorical safeguard and inherited threshold. Tools may run the frozen models,
verify evidence and source/time contracts, and arrange contexts; they may not
judge correctness, aggregate accuracy, rank or select candidates or parameters,
or issue the product verdict.

### 6.2.13 Secondary-channel single-unit edge closure

FR16AAL extends the already frozen FR16ZU secondary-channel evidence contract
to one narrowly mixed boundary shape. A complete phrase must contain exactly
two contiguous known business identities: a configured direct-anchor prefix
for the proposed identity and one competing suffix. Forced alignment must show
that the competing suffix contributes exactly one positive-duration unit;
zero-duration characters do not manufacture additional acoustic evidence.

The two complete-phrase registries must independently rank the anchored
identity first under the inherited duration-class margin. As in FR16ZU, the
absolute score gate may be ignored only because the same mapped Sortformer
channel sustains the inherited `0.5` activity floor for the inherited `0.4 s`
minimum. The suffix adds a stronger overlap contract: every raw frame touching
its single timed unit must top-rank the mapped competing channel while the
selected channel remains simultaneously active at `0.5`. This distinguishes a
two-channel boundary overlap from an unsupported identity rewrite.

One VAD interval must contain the complete phrase. The write closes only its
immutable source range. Additional identity runs, another positive-duration
suffix unit, a non-direct prefix, disallowed competing provenance, missing raw
mapping, weak selected-channel activity, protected overlays, or overlapping
proposals abstain. TOML inherits the existing thresholds and owns the
categorical provenance lists. Tools may verify evidence and arrange contexts
only; all correctness and retention judgments remain manual.

### 6.2.14 Runtime primary/activity/phrase consensus

FR16AAM addresses a runtime evidence-order defect without making primary top-1
authoritative. Coarse business-interval TitaNet queries can span a real handoff
and overwrite a more local phrase where activity, primary top-1, and both
phrase embeddings agree. The runtime projector therefore evaluates complete
punctuation phrases after direct intervals and treats the three-view agreement
as a bounded challenge.

The phrase keeps its existing forced-alignment source and time range. One
resolved primary identity must cover that whole time range; activity
diarization must support the same identity for the existing TOML
`speaker_fusion.min_embed_sec`; and session plus robust-gallery phrase queries
must both pass the unchanged short/regular score and margin gates with that
identity. Any missing, short, conflicting, or abstaining view preserves the
current label. The policy introduces no fitted constant and does not alter raw
tracks, enrollment, or source text.

The production C++ projector is first replayed over source-hashed frozen typed
tracks. Tools verify only parsing, determinism, source/time preservation, and
track isolation. Every changed context is read manually before 600-second and
full-session real-WebSocket promotion. No executable mechanism may assign
correctness, aggregate accuracy, rank/select a policy or parameter, or issue a
product verdict.

### 6.2.15 Primary refinement inside activity overlap

FR16AAN narrows primary-speaker boundary use to the condition where the
activity track already declares a genuine multi-identity overlap. Primary
boundaries are inserted into the projector only while two distinct resolved
activity identities cover the same time. For each resulting atomic interval,
one primary identity must cover the interval without a competing primary label
and must match one of those active identities. It may then select between the
overlapping candidates before forced-alignment units are projected.

This differs from the rejected broad primary replacement: primary cannot add a
speaker, cross an activity gap, split a unique activity winner, or contribute
support coverage. The decision audit records
`primary_speaker_overlap_refinement`; the existing exact-tie reason remains
separate. Voiceprint evidence is applied later under FR16AAM and the existing
TOML gates.

Frozen typed-track replay is used only to verify deterministic behavior and to
arrange all changed contexts. Every change and then the whole block are read
manually before any real-WebSocket promotion. No executable mechanism judges,
aggregates, ranks/selects, or issues a product verdict.

### 6.2.16 Partial-phrase edge isolation for direct voiceprints

FR16AAO prevents a projector-created boundary from turning one fragment of a
complete phrase into evidence against that whole phrase. Business intervals
retain their long acoustic context whenever their source range begins and ends
on phrase boundaries. Only a leading or trailing range that cuts through a
complete punctuation phrase is detached and queried independently. An interval
entirely inside one phrase is not split, and complete interior phrases remain
coalesced in one query.

All subranges are derived from immutable source-character ranges and
forced-alignment times. TitaNet still uses the TOML `min_embed_sec`, maximum
window, and duration-class selection gates; a detached edge that is too short
simply emits unavailable evidence. This is evidence-boundary repair, not score
tuning. Frozen replay verifies source/time/determinism mechanically, followed
by manual changed-context and full-block review only.

### 6.2.17 Source-order-preserving business time projection

FR16AAP handles a mechanical forced-alignment condition exposed by FR16AAO:
two adjacent source characters may have overlapping or slightly reversed model
times. A newly visible identity boundary can then create two business pieces
whose raw starts would be sorted against source order by the terminal timeline.

The projector first derives every piece in immutable source-character order.
For an adjacent overlap only, it chooses one shared boundary between the left
piece's positive lower bound and the right piece's positive upper bound, using
one sample as the minimum duration. It then constructs both entries from the
normalized spans so support metadata is calculated from the published times.
If that interval is infeasible, the projector returns the unchanged,
reconstructing diarization result. Focused tests verify identity preservation,
byte-exact reconstruction, and monotonic publication; real-WebSocket contracts
must pass before any contextual accuracy review resumes.

### 6.2.18 Initial-slot corroborated phrase challenge

FR16AAQ retains both interpretations of a long-session Sortformer local slot:
the current identity epoch remains the production baseline, while the earliest
immutable identity observed for that local slot remains an independent stable
mapping view. The initial view is consulted only for one complete punctuation
phrase where the current source labels are uniform but conflicting.

The exact phrase must be at least `speaker_fusion.short_max_sec` and no longer
than `speaker_fusion.phrase_max_sec`. One local Sortformer slot must cover the
whole phrase without another local slot covering any part, its initial stable
identity must differ from the current phrase identity, and the session and
robust-gallery phrase scores must independently top-rank that initial identity
under the configured duration-class margin. In that three-view condition the
regular absolute score may abstain without blocking the challenge. The rule
does not alter identity epochs, raw tracks, enrollment, text, alignment, or
neighbouring phrases.

The frozen production tracks are replayed twice through the same C++ projector.
Tools verify only hashes, parsing, source reconstruction, timing, determinism,
and track isolation, then arrange every changed context. Each change is read
manually in surrounding conversation before the candidate can enter a complete
chronological and reverse-block review. No executable mechanism may label,
aggregate, rank/select, or issue a product verdict.

### 6.2.19 Sample-preserving JSON time codes

FR16AAR serializes common-clock seconds with sufficient decimal precision for
one 16 kHz sample. This prevents a positive projected character interval from
rounding to a zero-duration JSON span and ensures frozen typed tracks reproduce
the same source-order boundaries. Focused tests parse the emitted JSON and check
only positive duration, precision, reconstruction, and replay contracts; they
do not evaluate speaker correctness.

### 6.2.20 Four-view single-margin-abstention challenge

FR16AAS uses the already emitted exact-phrase and containing-VAD TitaNet views
without changing a score or margin. A single primary run covers the phrase and
confirms which current mapped identity is being challenged. Both galleries for
both acoustic ranges must rank one different identity first. Three views must
pass their existing duration-class gates; the fourth may abstain only on the
unchanged top-two margin after already passing its score gate.

A single positive-duration aligned unit is not enough independent acoustic
structure for this exception. The typed TOML field
`speaker_fusion.four_view_min_aligned_units` therefore defaults to two; the
projector counts only distinct positive-duration units inside the exact phrase.

This topology distinguishes one weak ranking margin from missing or conflicting
evidence. The phrase remains the exact write boundary, while the longer VAD is
context evidence only. Frozen replay, deterministic/source/time checks, manual
reading of every changed context, and then full bidirectional contextual review
remain mandatory; executable result judgment and aggregation remain prohibited.

### 6.2.21 RFC 6455 observer validity

Three 360-second runs showed the same boundary: the producer received its
complete flush timeline, while the receive-only observers were closed shortly
after 300 seconds. The close time was invariant under bounded writable batches
and explicit libwebsockets partial-buffer handling, so those two server-side
experiments were reverted under the three-failure rule.

The bundled libwebsockets source defines the actual lifecycle: its default
validity policy sends a WebSocket PING after 300 seconds without confirmed
bidirectional traffic and closes the connection at 310 seconds if no PONG
arrives. The producer continuously sends audio and confirms validity in the
server RECEIVE callback. The raw Python observer never sends application data
and, unlike a browser, ignored PING instead of returning the mandatory PONG.

FR16AAT fixes the authoritative acceptance client, not the server. Its sole
reader thread recognizes opcode `0x9` and sends a client-masked opcode `0xA`
frame with the identical payload through the connection's shared send lock.
PING/PONG frames do not enter captured application events. A socket-pair test
verifies masking, payload echo, and exclusion from event lists. The real
360-second observer gate must then show all connections alive and identical
terminal timelines before 600-second or full-length promotion resumes.

The corrected 360-second observer repeat completed with the producer, early
observer, and late observer connections valid beyond the 300/310-second
validity boundary. Their ordered live evidence and terminal timelines passed
the existing mechanical contracts. Complete chronological and reverse-block
manual semantic review then found 35 accepted and 4 incorrect reference
contributions, or a manually calculated 89.74 percent. The semantic promotion
therefore stopped before 600 seconds. Because this diagnostic run reused a
registry modified by earlier sessions, the next attempt starts from an empty
isolated registry and freezes that run's registry before restart validation.

### 6.2.22 Observed-gallery finalization

The independently terminated 360-second run exposed a finalization gate rather
than a transport or diarization-progress gap. Its speaker-identity extent
reached the full 5,760,000-sample common clock, but the terminal
`speaker_voiceprint` track was empty. The same configuration at 600 seconds
emitted 2,475 voiceprint evidence intervals and retroactively repaired three
early contributions. The difference is the observed stable-identity set: only
three identities are resolved at 360 seconds, while the TOML required a gallery
of four before `SpeakerEvidenceStage::BuildVoiceprint()` could emit anything.

FR16AAU changes only the checked-in TOML minimum from four to three. Evidence is
still evaluated against every stable identity present in the snapshot, and all
existing session/robust score, margin, duration, alignment, primary, and phrase
guards remain unchanged. The unresolved fourth local slot is not a candidate.
This makes final evidence availability depend on the observed gallery rather
than the model's maximum slot count. Independent 120-second, 360-second, and
600-second real-WebSocket runs, each with full chronological and reverse-block
manual semantic review, are required before the profile can advance.

Those three independently terminated runs completed with the same resolved
configuration and binary. Mechanical source, common-time-base, observer,
terminal-timeline, telemetry, and liveness contracts passed in every run. The
complete chronological and reverse-block contextual reviews manually found
17 of 18 accepted contributions at 120 seconds, 36 of 39 at 360 seconds, and
86 of 93 at 600 seconds. The corresponding manually calculated results are
94.44, 92.31, and 92.47 percent. The profile therefore advances to full-length
acceptance; these promotion runs do not substitute for either full run.

### 6.2.23 Native dual-view consensus protection

The first two full-length runs expose a generic evidence-order failure near the
end of the session. One complete business interval and its contained phrase are
uniformly attributed to the same stable identity by both immutable Sortformer
views, with no competing native identity, but ordinary TitaNet interval and
phrase decisions overwrite that agreement. This is not a missing threshold:
the current projector checks whether primary and activity support a proposed
phrase identity, but does not preserve their complete agreement on the current
identity.

The first broad native-consensus replay was rejected during changed-context
review because it also suppressed correct voiceprint identity repair in a
single complete VAD interval. The raw score shapes did not separate that case
from the target defect. The typed VAD topology did: the rejected repair was
contained by one VAD interval, while the target interval and phrase crossed two
separated VAD speech intervals with no complete containing VAD.

FR16AAV therefore adds a narrower reference-free protection after the existing
FR16AAQ and FR16AAS specialized challenges and before ordinary short direct or
phrase writes. The projector requires at least two overlapping typed VAD
intervals, no single containing VAD within the existing alignment tolerance,
the existing TOML minimum embedding duration and short-duration boundary, one
uniform current source identity, and complete uncontested activity and primary
coverage with that identity. The rule adds no score, margin, text, identity,
or timestamp constant and changes no raw producer track.

The exact Run B producer tracks are exported once and replayed twice through
the production C++ projector. Tools may check only hashes, parsing,
determinism, source reconstruction, time monotonicity, and unchanged input
tracks, and may arrange every changed context for reading. Every changed
context is judged manually in complete surrounding conversation before a
retention decision. A retained candidate then receives a complete
chronological and reverse-block contextual review before any new real-WebSocket
full run. No executable mechanism may label correctness, aggregate accuracy,
rank/select a candidate or parameter, or issue the product verdict.

### 6.2.24 Primary-protected aligned-unit evidence

The full Run B decision trace exposes a second evidence-order inconsistency.
Inside a three-identity activity overlap, the primary view selects one identity
for an exact aligned character range. A later generic aligned-unit TitaNet
query selects a different identity and overwrites that primary arbitration,
although coarse direct evidence already has an explicit conflict guard for the
same condition. The immediately adjacent aligned range retains primary, so the
published phrase is split by processing order rather than by an evidence
boundary.

FR16AAW extends only the existing primary-conflict protection to generic
aligned-unit writes. It recognizes the two typed primary arbitration reasons
already emitted by the projector and abstains only where the proposed
aligned-unit identity conflicts. It does not protect sole activity, introduce
primary-only speakers, expand a boundary, change a TOML gate, or run before the
explicit FR16AAQ and FR16AAS challenges.

The production projector is replayed twice on the same source-hashed Run B
tracks. Mechanical checks remain limited to determinism, source/time/config,
and track isolation. Every changed context is read manually in surrounding
conversation before retention, followed by complete chronological and reverse-
block review only after the combined frozen candidate reaches every speaker
block gate. No executable mechanism may judge, aggregate, rank/select, or
issue the verdict.

### 6.2.25 Dual-Sortformer alignment-run transition

The frozen Run B trace exposes a projection loss rather than an identity loss.
Activity and primary both start the same new stable identity at nearly the same
native time, but the boundary lies inside one unusually long forced-alignment
unit. The projector correctly builds a new native turn, then groups the
crossing unit and all following gapless units into one alignment run. Its
midpoint remains in the preceding turn, so the complete suffix is projected to
the preceding identity and the independent native agreement never reaches the
business view.

FR16AAX records only activity starts that have a same-identity primary start
within the existing TOML alignment-boundary tolerance. Each matched start must
follow an evidence-free native gap at least as long as the existing TOML
alignment snap pause, and its segment must remain uncontested for the existing
TOML minimum embedding duration. The nearest prior interval must name a
different stable identity. These guards distinguish a long alignment unit that
bridges a real dual-view pause from ordinary overlap churn and a short primary
micro-run. While building a gapless forced-alignment run, a corroborated
transition inside the current aligned unit makes the boundary after that unit
splittable. The crossing unit stays intact and is assigned by the existing
midpoint rule; the next complete unit starts a new run and is assigned against
the existing activity-derived turns. No primary time is copied into the
business boundary, no text is split inside an aligned unit, and unmatched,
short, non-isolated, or conflicting transitions retain the current
run-coherence policy.

Focused tests cover a corroborated isolated transition inside a long aligned
unit, activity-only and primary-only abstention, conflicting identity,
insufficient-gap, short-primary and contested-run abstention, source
reconstruction, and evidence-arrival order. The exact frozen Run B
tracks are then replayed twice through the production projector. Automation may
check only determinism, source/time/config and immutable-track contracts and
arrange all changed contexts. Every change is read manually in complete
conversation before retention; no executable mechanism labels correctness,
aggregates accuracy, ranks/selects the candidate, or issues a verdict.

### 6.2.26 Short initial-slot recovery under strong VAD

The retained frozen trace exposes a late-session identity-epoch error inside an
activity overlap. Primary chooses the current stable identity associated with
one local Sortformer slot, while that slot's immutable initial identity and both
strong containing-VAD galleries identify a different participant. The exact
short phrase is itself uncertain: session weakly retains the current identity
and robust weakly returns to the initial identity, with both abstaining only on
the unchanged short-span margin. The existing regular-duration initial-slot
challenge cannot act because the phrase is short, and the existing four-view
challenge correctly abstains because one top rank disagrees.

FR16AAY admits only this complete evidence topology. It requires uniform
primary-arbitrated current labels, one complete current-identity local slot and
that slot's different immutable initial stable identity, the existing minimum
embedding and short-duration bounds, the existing aligned-unit count, and one
containing robust VAD. Both VAD views must pass existing regular duration-class
score and margin gates for the initial identity. Phrase robust must top-rank the
initial identity and phrase session the current identity, with both passing
score and failing only margin. The write remains the exact phrase range. No
score, margin, duration, text, identity, or timestamp constant is added.

Focused tests cover the exact positive topology and abstention for missing slot
epoch, non-primary current labels, a weak VAD view, a passing phrase margin, and
insufficient aligned structure. Frozen Run B is replayed twice; automation only
checks deterministic/source/time/config/immutable-track contracts and arranges
all changes. Every changed context is read manually before retention, with no
executable correctness judgment, aggregation, ranking, selection, or verdict.

### 6.2.27 Adjacent strong-phrase continuation

The frozen trace also exposes a phrase-boundary evidence-order loss. One exact
punctuation phrase is independently accepted by both galleries for a secondary
activity identity. Its immediately adjacent short business interval is assigned
to primary's current identity, but both target galleries abstain solely on
margin and rank the preceding identity second. The secondary activity track
continues without a gap from the accepted phrase through more than the existing
minimum embedding duration of the target. A later low-score aligned-unit write
otherwise reinforces the weak current choice and hides the continuous native
evidence.

FR16AAZ derives the anchor directly from typed evidence rather than processing
order. The preceding punctuation phrase must be exactly source-adjacent and
time-adjacent, and both of its galleries must pass unchanged gates for one
identity. Both target galleries must be margin-only for one uniform current
identity and rank the anchor identity uniquely second. The same activity
identity must cover the complete anchor and the first existing-TOML minimum
duration of the target continuously. One current-identity primary run covers
the target, and the existing aligned-unit count is required. The target receives
an exact-range direct-anchor reason, so only an aligned unit that clears the
already specified strong-conflict score can replace it. No threshold, phrase
text, identity, timestamp, or reference result is added.

Focused tests cover the positive continuation and abstention for weak anchor,
non-runner-up target, short native continuation, missing primary, and
insufficient alignment. Frozen Run B is replayed twice with mechanical checks
limited to determinism and immutable source/time/config/track contracts; every
changed context is read manually before retention. No executable mechanism may
judge correctness, aggregate accuracy, rank/select, or issue a verdict.

### 6.2.28 Exact short phrase versus coarse direct identity

A frozen middle-session trace shows a coarse multi-clause business interval
passing both galleries for the current stable identity, while its exact short
question independently top-ranks another identity in both galleries and misses
only the unchanged short margin. The exact range is covered by one local
Sortformer slot, that slot's immutable initial identity equals the phrase top
rank, and no other activity slot overlaps the range. Primary fully covers the
current identity, so this is an identity-epoch conflict rather than an unbounded
phrase expansion.

FR16ABA permits the exact phrase to challenge only typed coarse direct labels.
It requires the existing minimum/short duration bounds, dual margin-only phrase
rankings, one uncontested current local slot whose different initial identity is
the proposed identity, one covering current primary run, and the existing
aligned-unit count. It changes only the exact phrase and adds no numerical or
reference-specific condition.

Focused tests cover the positive topology and abstention for a passing phrase
margin, gallery disagreement, competing activity slot, missing identity epoch,
non-direct current label, and insufficient alignment. The combined projector is
replayed twice on frozen Run B; automation checks only mechanical contracts and
arranges all changed contexts. Retention requires complete manual contextual
reading, never executable correctness judgment or aggregation.

### 6.2.29 Isolated subminimum aligned unit under strong VAD

The frozen middle-session trace contains one short acknowledgement whose
forced-alignment unit is too short to produce its own embedding. The unit is
separated from the nearest aligned units on both sides by the accepted TOML
pause, while one current local Sortformer slot and one current primary run
cover it. That local slot began the session with a different immutable stable
identity, and both galleries of the one containing robust VAD interval pass
their unchanged duration-class gates for that initial identity. Generic
voiceprint fusion cannot use the unavailable unit embedding and therefore
leaves the native current attribution unchanged.

FR16ABB admits only this complete typed-evidence topology. It requires a
positive but subminimum no-embedding aligned unit, positive-duration neighbors
and the existing pause on both sides, uniform native current labels, one
uncontested covering current local slot with a different initial identity, one
uncontested covering current primary run, and one containing robust VAD whose
two galleries pass unchanged gates for the initial identity. It writes only
the exact aligned-unit source range and introduces no score, margin, duration,
text, identity, or timestamp constant.

Focused tests cover the positive topology and abstention for missing identity
epoch, insufficient isolation, weak or disagreeing VAD, primary mismatch, and
an available unit embedding. The combined projector is replayed twice on
frozen Run B. Automation may verify only determinism and immutable
source/time/config/track contracts and arrange changed contexts; retention is
decided only by complete manual contextual reading.

### 6.2.30 Bracketed primary micro-run aligned unit

Another frozen middle-session trace exposes a different subminimum case. One
coarse direct voiceprint range follows the sole activity identity, while the
orthogonal primary track inserts a short conflicting run that fully contains
exactly one no-embedding aligned acknowledgement. The primary run is bounded
without a gap by the same current identity on both sides. Its nearest aligned
neighbors remain outside that run, so the typed evidence describes one exact
interjection rather than a broader speaker transition.

FR16ABC allows the complete containing primary micro-run to challenge only an
exact subminimum no-embedding aligned unit under this bracketed topology. It
requires uniform typed direct current labels, sole uncontested current activity
coverage with no candidate activity, one different containing primary run
shorter than the existing minimum, same-current primary runs sharing gapless
common boundaries immediately before and after, and neighboring aligned
units outside the candidate run. It writes only the exact unit and adds no
score, margin, duration, text, identity, or timestamp constant.

Focused tests cover the positive topology and abstention for partial primary
coverage, different or gapped bracketing, a regular-duration primary run,
candidate activity, non-direct current labels, and an aligned neighbor inside
the run. Frozen Run B is replayed twice; automation verifies only mechanical
contracts and arranges changes, and complete manual contextual review alone
decides retention.

### 6.2.31 Isolated VAD-aligned island challenge

FR16ABD consumes the already typed VAD voiceprint evidence as an independent
current-audio view without projecting a VAD identity across silence or an ASR
boundary. For each robust-complete VAD interval, the projector first requires
the existing duration-aware session and robust-gallery selectors to agree and
pass. It then finds the maximal contiguous run of complete positive-duration
forced-alignment units contained by the interval. The run is eligible only when
it belongs to one `text_id`, reconstructs one contiguous source range, contains
the existing TOML minimum number of aligned units, and no aligned unit straddles
either VAD edge.

Both neighboring VAD intervals must exist and leave at least the existing TOML
alignment pause on each side. One uncontested local activity slot and one
uncontested primary run must cover the aligned range with the same uniform
current direct identity, while the VAD-selected candidate has no activity
support there. This deliberately makes the rule a narrowly bounded challenge
to a coarse native/direct attribution: it writes only the complete aligned
source range contained by the isolated VAD and adds no numerical parameter.

Focused tests cover the positive topology and abstention for a boundary or
non-isolated VAD, weak/disagreeing galleries, insufficient or discontinuous
alignment, mixed/non-direct labels, activity competition or candidate
activity, and absent/mismatched primary coverage. Frozen Run B is replayed
twice; tools verify only source/time/determinism and arrange every changed
context. Retention remains a complete manual contextual-semantic decision.

### 6.2.32 Pause-onset primary-aligned island challenge

FR16ABE handles a different topology from the subminimum FR16ABC micro-run. A
candidate primary run is long enough for the existing embedding floor but still
inside the existing short-span class. It begins after a real configured pause,
has its own agreeing activity slot, and ends exactly where the prior current
identity resumes in primary. A containing robust-complete VAD begins at the
same onset within the existing alignment tolerance after a separately typed
VAD pause; both VAD galleries still
top-rank the current identity because the VAD continues into that speaker, but
the VAD must include at least the existing minimum embedding duration of that
following current-primary run. This mechanically proves that the VAD query is
mixed and uses its rankings only as a boundary/current-continuation constraint;
the VAD does not supply the candidate identity or alter any score gate.

The projector gathers only complete positive-duration aligned units inside the
candidate primary run. They must come from one text ID, meet the existing
aligned-unit count, and avoid either run edge. Exactly one internal source gap
without a positive-duration aligned unit is required and becomes a mandatory
split. Units before the gap keep the current identity; only the contiguous
aligned suffix after it may receive the candidate identity. No timed unit may
be skipped and neither source edge may expand.
The candidate identity must have one covering activity slot, no third activity
identity may overlap, and the surrounding primary identity must match the
uniform typed voiceprint current attribution. The write is restricted to that
aligned island and introduces no numerical parameter.

Focused tests cover duration, pause, recovery boundary, bracket identity, VAD
onset and current-continuation topology, candidate activity, third-party activity, typed
current labels, and aligned source/run boundaries. Frozen Run B is replayed
twice for mechanical determinism and every changed complete context is read
manually before the rule is retained.

### 6.2.33 VAD-missed isolated interval initial-slot recovery

FR16ABF addresses a subminimum business interval for which TitaNet correctly
abstains because no interval embedding exists and VAD did not emit a speech
segment. It does not manufacture a new identity. The same local Sortformer slot
must uncontestedly cover the exact interval in both activity and primary views,
while its current dynamic stable identity differs from the slot's immutable
initial stable identity. Current labels must still be native, and no activity
for the initial candidate may overlap.

Isolation is established independently on the common clock. No typed VAD may
overlap the interval, and the nearest VAD on both sides must leave the existing
TOML alignment pause. The exact text ID must also contribute the existing
minimum count of complete positive-duration aligned units inside the business
interval; no timed unit may straddle an edge, and the nearest external timed
unit on each side must leave the same pause. Only when both isolation views and
both Sortformer views agree may the exact typed business-interval source range
recover the initial slot identity. No new parameter or semantic input is added.

Focused tests cover missing epochs, available embeddings, VAD overlap or edge
proximity, insufficient or edge-straddling alignment, absent outside aligned
neighbors, activity/primary competition, candidate activity, and mixed or
voiceprint labels. Frozen Run B is replayed twice and all changed contexts are
read manually before retention.

### 6.2.34 Initial-slot four-view near-tie recovery

FR16ABG handles a short exact phrase where neither phrase-scale nor containing-
VAD TitaNet evidence may pass the existing margin gate. The rule does not lower
that gate or turn an abstention into a standalone voiceprint decision. Instead,
it requires all four ranked score sets to identify the same two stable
identities as their top pair, with the local slot's immutable initial identity
first in exactly one view and second in the other three. One same competitor
has the inverse positions, while the uniform current dynamic identity is absent
from every top pair. All four top scores still pass the unchanged short score
gate and all four margins must fail the unchanged short margin gate.

The exact phrase must be covered uncontestedly by one activity slot and one
primary run carrying the current dynamic identity, and that slot alone supplies
the initial candidate. One robust-complete containing VAD and the existing
minimum aligned-unit count are mandatory. Current labels must remain native.
The write is limited to the exact punctuation phrase; the rule adds no score,
margin, duration, rank, or identity constant.

Focused tests cover rank-count changes, competitor inconsistency, a passing
margin, current identity entering the pair, missing epoch, activity/primary
competition, missing VAD, and insufficient alignment. Frozen Run B is replayed
twice and every changed complete context is read manually before retention.

### 6.2.35 Cross-scale symmetric near-tie recovery

FR16ABH addresses the current Run B form of the previously retained phrase-led
outer challenge without importing the older candidate's now-false assumption
that exactly one phrase gallery passes. The current typed evidence instead
shows a symmetric scale conflict: both exact-phrase galleries top-rank one
identity but fail only the unchanged short margin, while both containing-VAD
galleries top-rank the current identity and fail both unchanged regular score
and margin gates. All four top-two pairs contain exactly those same two
identities.

This remains an abstention-resolution topology rather than a threshold change.
One uncontested activity slot and one primary run must carry the uniform current
identity, the candidate may have no overlapping activity, current labels must
come only from the ordinary typed direct interval path, and one robust-complete
containing VAD plus the existing aligned-unit count are mandatory. The write is
restricted to the exact punctuation phrase. No identity, transcript, timestamp,
reference result, or new numerical value enters the rule.

Focused tests cover a changed phrase or VAD top identity, a third top-two
identity, an eligible outer or phrase view, mixed provenance, activity/primary
competition, multiple or incomplete containing VAD evidence, and insufficient
alignment. Frozen Run B is replayed twice; every changed context is then read in
complete conversation before retention or rejection.

### 6.2.36 Exact-interval primary-conflict recovery

FR16ABI addresses a short interval where the primary projection wins an
activity overlap even though both exact-interval voiceprint galleries make the
same eligible decision. The rule does not let voiceprint alone erase a primary
decision. It additionally requires the containing VAD's two robust-complete
views to abstain on both existing regular score and margin gates while exposing
the exact candidate and the one competing activity identity as the same top-two
pair in opposite order. The selected primary identity must be absent from both
pairs.

The current primary identity and the one competing activity identity must each
have one local slot completely covering the exact interval. The candidate may
have no activity there, exactly one primary run must cover with the current
identity, and the existing aligned-unit minimum is mandatory. This establishes
that the exact audio window carries eligible identity evidence while the outer
window rejects the primary projection's identity without pretending that the
outer voiceprint view made an eligible choice. Projection writes only the typed
business-interval source range and introduces no score, margin, duration,
identity, transcript, timestamp, or reference-specific constant.

Focused tests cover gallery disagreement or abstention, an eligible VAD view,
a changed or repeated VAD top-two order, current identity entering the pair,
missing or additional activity, candidate activity, primary provenance and
coverage changes, incomplete robust evidence, and insufficient alignment.
Frozen Run B is replayed twice and every changed context is read manually in
complete conversation before retention or rejection.

### 6.2.37 Subminimum native cross-scale restoration

FR16ABJ restores one exact subminimum business interval after a wider phrase's
session-only decision erased a primary-arbitrated native overlap. It is a post-
projection evidence challenge, not a general preference for primary. Exactly
two activity slots must completely overlap the interval; one and the sole
covering primary run identify the same native identity, while the other carries
one different competing identity and the phrase-selected identity has no
activity there. The unique containing phrase must top-rank the current identity
in both galleries with session eligible and robust margin-only. The unique
containing VAD must instead top-rank the primary-selected native identity in
both galleries, place the current identity second, and remain margin-only in
both views. The VAD evidence therefore independently resolves the activity
overlap instead of treating primary and activity as two independent models.

The typed business interval must have no embedding, remain below the existing
minimum embedding duration, and contain exactly one positive-duration aligned
unit without a straddling unit. This lets the business interval carry adjacent
zero-duration source characters without inventing a timestamp or expanding the
write. All score, margin, duration, and alignment constants come from the
existing TOML profile. No identity, text, timestamp, or reference-specific
constant enters the policy.

Focused tests cover interval embedding or duration, mixed provenance, phrase
gallery disagreement or changed eligibility, VAD identity/runner-up/gate
changes, multiple or incomplete containers, missing or additional activity,
candidate activity, primary mismatch, missing alignment, and multiple or edge-
straddling positive units. Frozen Run B is replayed twice and every changed
complete context is read manually before retention or rejection.

### 6.2.38 Aligned complete-source VAD closure

FR16ABK ports the retained complete-source closure principle into the typed C++
comprehensive-view path without restoring the old offline projector. The
problem is a time-envelope mismatch: an ASR source can include leading and
trailing padding that makes its outer interval regular-length even though all
aligned speech is one short contribution. The stage therefore derives one
speech envelope from positive-duration typed business intervals. Those
intervals must exactly partition the complete source without overlap, and each
must contain at least one positive forced-alignment unit inside both its source
and time bounds. This preserves short repeated characters that the aligner can
omit without accepting an unanchored proportional source range. It never
infers an interval from transcript meaning.

The complete-source session and robust score vectors must agree and be eligible
under the existing short gates while failing only the existing regular score
gate under the padded source duration. A unique containing VAD must make the
same eligible dual-gallery decision. The sole indexed punctuation phrase is
allowed only as abstention evidence: both views must fail margin and keep the
candidate in the top two. This distinguishes a padded short contribution from
a genuinely long or internally mixed source.

Native evidence remains a cannot-link guard. Current labels may contain only
one incumbent plus the candidate at one contiguous source edge with primary
provenance. Exactly those identities may appear in activity and primary over
the aligned envelope. Incumbent activity covers the envelope, candidate
activity contributes without covering it, and both primary identities
contribute while their union remains gap-free. The complete source is written
atomically only after all acoustic, alignment, provenance, and topology checks
close. All durations and score gates come from the current TOML profile.

Focused tests vary outer/aligned duration, interval partition and alignment
anchoring, current edge topology, source provenance, complete-source gate state
and agreement, VAD
count/completeness/decision, phrase count/rank/eligibility, and activity and
primary identity coverage. Frozen Run B is replayed twice; automation checks
only deterministic and structural contracts and arranges changed contexts for
complete manual semantic review.

### 6.2.39 Adjacent primary-supported source-initial prefix

FR16ABL closes a missing evidence scale rather than preferring one pipeline.
The existing business interval for a source-initial interjection can be too
short for TitaNet, while the immediately following primary-selected interval
contains the rest of the same audible contribution. `SpeakerEvidenceStage`
therefore enumerates every mechanically qualifying adjacent interval pair and
evaluates the exact combined audio window against both session and robust
galleries. Enumeration uses typed source and common-clock bounds only; it does
not inspect text, speaker names, timestamps, references, or review outcomes.

The business projector treats the combined query as a challenge limited to the
subminimum prefix. Both galleries must independently make the same eligible
short-span decision. The following component must already carry that identity
through primary provenance, and one primary run plus one activity slot for the
candidate must start inside the prefix and cover the following component. The
incumbent activity must cover the pair within the existing boundary tolerance.
One fully covering lower-confidence activity identity may remain as background
competition only when its overlap-weighted confidence is below both principal
identities; no absolute confidence threshold is added. This topology explains
why the initial proportional interval can lag the native onset without allowing
the query to overwrite a later handoff.

The next typed business interval is an explicit cannot-link guard: it must be
source-adjacent, begin no earlier than the pair ends, and restore the incumbent
through primary provenance. Projection changes only the leading component.
Focused tests cover generation adjacency and duration gates, gallery
eligibility/agreement, source position, current provenance, component count,
primary and activity onset/coverage, alignment support, and following-handoff
preservation. Frozen Run B is replayed twice; automation checks only evidence
and determinism contracts and arranges every changed context for complete
manual semantic reading.

### 6.2.40 Native multi-identity phrase protection

FR16ABM addresses an evidence-order defect in the generic punctuation-phrase
path. `SplitTextByDiarBase` can already project a real handoff inside one ASR
punctuation phrase from activity overlap, primary top-1 refinement, and forced
alignment. The later generic phrase voiceprint pass currently treats the
phrase as one acoustic sample and may repaint all of those source characters
with its majority identity, discarding the more local handoff evidence.

The projector therefore records the immutable base owner of every source
character before applying voiceprints. A generic phrase abstains only when its
selected identity is still present in that base projection and one different
base identity forms exactly one adjacent source run and survives for at least
the existing TOML minimum embedding duration in positive forced-alignment time
through explicit primary tie-break or overlap-refinement provenance. The exact
phrase must collapse to those two runs; an unknown span, third identity, or
return transition abstains. This distinguishes a sustained edge handoff from
short primary churn, internal islands, and a phrase-level correction whose
selected identity is absent from noisy base fragments. No threshold or TOML
value changes. Existing narrowly
specified challenges continue to execute in their established order and may
cross mixed labels only when their own requirements explicitly allow that
topology; the broad session/dual-gallery fallback cannot.

Focused C++ coverage constructs one aligned punctuation phrase with exactly one
sustained primary-refined handoff and eligible dual-gallery phrase evidence,
then requires both identities and exact source reconstruction to survive. A companion case
keeps a subminimum primary fluctuation eligible for the generic phrase overlay.
Homogeneous phrase coverage and the existing primary/activity/dual-gallery
consensus case remain covered. Frozen A and B typed tracks are replayed twice with the unchanged TOML;
automation checks only determinism, source/time preservation, raw-track
isolation, and arranges changed contexts. Every changed conversational context
is read manually in chronological and reverse order before any real-WebSocket
promotion. No executable mechanism assigns correctness, aggregates accuracy,
ranks this rule, or issues an acceptance verdict.

### 6.2.41 Delayed subminimum clause-group recovery

FR16ABN addresses a placement defect that is independent of FR16ABM. In the
frozen `ref-0090` region, the common session clock is consistent across all
tracks, but forced alignment leaves a `1.84 s` internal hole after the outgoing
phrase and collapses the next short response onto `569.26-569.42`. Activity and
primary have already returned to the surrounding identity at that delayed
time, so ordinary business projection and the containing TitaNet query both
support the wrong side of the handoff. Reweighting phrase voiceprint evidence
cannot repair this topology because the short response has no embedding and
the available interval/VAD embeddings include the following sustained speech.

The projector will derive punctuation clauses from the existing TOML
punctuation set and the typed ASR/alignment source map. It may form only a
source-contiguous prefix of subminimum clauses immediately before the next
regular-duration clause. The combined group must contain the existing minimum
aligned-unit count, start after the existing alignment pause, remain below the
existing embedding floor, and be bracketed in source ownership by one incumbent
identity. The delayed aligned group must sit at a corroborated incumbent
activity/primary return onset. Between the preceding aligned unit and that
onset, one and only one different identity must have at least the existing
minimum duration of overlapping activity and primary support, then end before
the incumbent return by the existing pause. One adjacent typed VAD pair must
also expose a configured VAD gap inside the same alignment hole, with the
candidate island on its leading side and the incumbent return on its following
side. This A-B-A topology distinguishes a displaced short response from a
normal speaker transition, overlap churn, and a legitimate delayed filler.

The implementation adds the existing TOML punctuation string to the read-only
business-projector config; no checked-in value or threshold changes. The rule
runs after current fusion challenges and may replace only uniform native or
typed direct labels in the exact short group. It preserves every producer
track and the forced-alignment timestamps, so it repairs business speaker
ownership without claiming to repair audible boundary timing.

Focused C++ coverage includes the complete positive topology and abstention for
missing source brackets, available/regular-duration phrase evidence,
insufficient aligned units, missing or competing native islands, activity-only
or primary-only support, an unseparated candidate, incumbent return outside
tolerance, absent/ambiguous VAD gaps, candidate activity on the delayed group,
and arrival-order/source reconstruction. Frozen A and B typed tracks are then
replayed twice. Automation may verify only deterministic source/time/config and
raw-track contracts and arrange every changed context. Every affected context
is read manually in complete chronological and reverse conversation before
retention; no executable mechanism labels correctness, aggregates a result,
ranks/selects the rule, or issues a verdict.

### 6.3 Decision gate

The contextual reviewers must manually establish that the frozen candidate
reaches at least 93 percent on both speaker-time and natural-turn measures,
retains 100 percent critical-turn correctness, and avoids regression in every
fixed 600-second block. No program performs the comparison or gate decision. If
the signed manual review passes, its policy and TOML parameters are frozen for
runtime implementation.

If it does not, stop policy and threshold tuning. First compare every already
ported, deployable streaming checkpoint under the same profile against trusted
oracles and the same ledger. A replacement model is considered only when it has
a feasible native deployment path, exceeds the 93 percent gate, and has a
stage-by-stage validation plan. Offline-only diarization models are excluded
from the model-selection path; the manually adjudicated reference already
supplies acceptance truth.
Model escalation is also paused whenever the current runtime profile fails its
trusted oracle, because an unfaithful port cannot establish the existing-model
upper bound.

The 2026-07-15 owner review removed an exploratory offline Sortformer/Pyannote
branch from the work product. It duplicated the role of `test.txt` and could
not produce a native streaming deployment candidate. NeMo remains only as the
same-checkpoint numerical oracle required to validate the v2.1 C++/CUDA port.

## 7. Phase 5: Implement and Validate the Selected Candidate

Implementation proceeds in this order:

1. common clock and typed track contract;
2. stable ID and serialization contract;
3. independent per-turn speaker-evidence track;
4. registered business-speaker fusion track;
5. TOML configuration and resolved-config manifest;
6. Web UI convergence and telemetry display;
7. unit, integration, oracle, concurrency, and failure-path tests.

Each model-affecting commit runs the relevant PyTorch/NeMo oracle before the
next layer is changed. A tolerance is never widened to accept the implementation.

## 8. Phase 6: Validation Pyramid

### 8.1 Automated and numerical gates

These gates validate implementation and execution contracts only. They neither
contribute to nor calculate a product accuracy result or promotion verdict.

- clean configure/build under `-Wall -Wextra` with no warnings;
- all C++ tests and registered Python real-WebSocket tests;
- ASR, Sortformer, TitaNet, VAD, and forced-alignment oracle gates for changed
  stages and the exact accepted profile;
- stable-ID, source-clock, track immutability, revision, alignment coverage,
  endpoint, empty-input, and reset/reconnect tests;
- selected AddressSanitizer/UndefinedBehaviorSanitizer host tests and CUDA
  memory checking where supported.

### 8.2 Real streaming levels

| Level | Purpose | Promotion rule |
|---|---|---|
| 120 s | startup, live UI, initial identities, endpoint and telemetry smoke | no contract/stability failure and full contextual review passes |
| 360 s | dense early multi-speaker exchanges and revision behavior | no regression versus 120 s; all IDs and align groups reconcile |
| 600 s | first standard accuracy block and sustained real-time operation | all 90 percent gates for the block pass |
| Full | long-session identity, tail, resource behavior, full semantics | all Spec 013 gates pass over every ledger row |

Tail clips may be used for diagnosis, but they do not count as full-session
acceptance because they reset model history.

### 8.3 Full acceptance runs

Run A starts from an empty isolated speaker registry. Run B starts after a
process restart with the enrolled registry produced by the accepted fixture.
Both use the same committed TOML and model/data files. Each run must pass the
full contextual review independently; averaged scores cannot hide one failure.

The canonical TOML path for both runs is
`/tmp/orator/storage/spec013-speakers.bin`. Acceptance setup removes that file
and its optional `.names` sidecar before run A. Run A's terminal save is hashed
and becomes the immutable enrolled fixture; run B starts only after a process
restart and confirms the same fixture hash before loading it. Registry setup is
an operational precondition, not a command-line or environment parameter.

`tegrastats` remains active throughout. The final report includes sample count,
missing-field rate, min/mean/95th-percentile/max for CPU, RAM, GPU utilization,
GPU memory, temperature, GPU rail power, and total system power, plus any
thermal-throttle or allocation event.

### 8.4 Web UI acceptance

Use the actual browser with both file upload and microphone input. Verify live
partial text, VAD state, revisions, speaker labels, alignment lane, telemetry,
reconnect, end-of-stream, and JSON export. Browser state at terminal completion
must match the server terminal document by `text_id`, time span, text, and
speaker identity. Screenshots and browser-console output are retained as
evidence.

### 8.5 Current-commit seal and remaining work

Commit `6dbc600e4eb5` has completed the required empty-registry A run and
restarted frozen-registry B run. Both full streams pass the natural-business-
turn speaker gate under complete contextual semantic review. This result does
not close the conjunctive acceptance table.

The remaining work proceeds in this order:

1. retain direct `end` after the final audio frame as the unified client's
   default acceptance path and preserve its independent timing evidence. The
   first full recapture at commit `7721024ceb60` completed all 3615.120 seconds
   but required `66.915 s` from the final frame to the terminal document. Raw
   device evidence separates approximately 40 seconds of final TitaNet work
   from a following single-core fusion/reprojection phase, so that artifact is
   a signed mechanical failure and T101 remains open;
2. move acoustic-only TitaNet work off the terminal critical path: a
   session-owned periodic worker reads the reduced typed speaker-evidence
   snapshot, caches embeddings by common-clock sample span, and drains after
   alignment. Final scoring still uses the mature galleries and the unchanged
   fusion policy. Index immutable evidence by exact kind while retaining its
   original order so policy scans do not repeatedly traverse unrelated kinds.
   TitaNet uses the scheduler-owned lowest-priority background CUDA stream,
   with its maximum configured scratch warmed before the server accepts live
   audio, so periodic evidence work neither serializes the default stream nor
   grows device buffers during a session. Live work begins only after the
   active gallery is mature and diarization, ASR, and VAD have all reached a
   sampled common-clock input head. It then caches at most one configured batch
   per cycle, preventing a periodic burst from changing endpoint scheduling.
   Cadence and cycle size are configured only by
   `speaker_fusion.precompute_interval_sec` and
   `speaker_fusion.precompute_max_spans_per_cycle`; a zero interval preserves
   the uncached fallback. The final drain ignores the live cycle limit;
3. prove cached/uncached exact output identity in focused tests, record
   per-phase finalization timings, then rerun short, 600-second, and full A/B
   direct-end evidence. Neither timing diagnostics nor byte equality may be
   interpreted as a product-accuracy judgment;
4. reuse the completed forward and reverse 556-row contextual judgments against
   the human-audited `test.txt` reference; classify criticality, uncertainty,
   confident-wrong attribution, and source-time offsets without replaying or
   retranscribing the reference audio;
5. manually derive and independently cross-check speaker-time, fixed-block,
   per-speaker, critical-turn, confident-wrong, and source-time-offset results
   from those signed contextual judgments at `test.txt`'s recorded precision;
6. close T084 only when both A and B independently satisfy every applicable
   gate, then complete ASR, browser/microphone, holdout, report review, and
   release-signing work.

No new model or fusion parameter sweep starts while these evidence gaps are the
blocking issue.

Items 1 through 3 were completed by clean commit `588bfbe63555` and the
full-length direct-end seal in Section 8.7. Current execution starts at item 4.

### 8.6 Precompute engineering seal (2026-07-18)

T101A and T101B are implemented on the working tree based on parent commit
`7721024ceb60`. `ComprehensiveTimeline` exposes a reduced typed speaker-evidence
snapshot; `SpeakerIdentityStage` caches embeddings by exact common-clock sample
span; and `SpeakerEvidenceStage` owns the bounded periodic acoustic-only worker
plus the unlimited final drain. `AuditoryStream` supplies the common-head
readiness predicate, scheduler-owned lowest-priority CUDA stream, maximum-window
warmup, finalization order, and per-phase diagnostics. Stable VAD, alignment,
business-kind, and text indexes remove repeated full evidence scans without
changing source order or fusion-policy order.

The implementation builds warning-clean and all 68 configured CTest entries
pass. Focused coverage includes exact cached/uncached evidence scores, cache
reset, minimum-gallery gating, the one-successful-span live-cycle bound, and
the unlimited final drain. The following real-WebSocket checks used one binary,
1.0x incremental input, direct `end`, isolated registries, continuous telemetry,
and configuration expressed only in TOML:

- at 120 seconds, cached and uncached terminal waits were `0.805 s` and
  `0.844 s`; all seven track payloads were exactly equal at SHA-256
  `7b291c32fdd5d40679f5ea1f944e3cc2f508061b1dbeebe1ee5bdf3e3a97634f`,
  and both comprehensive views were exactly equal at SHA-256
  `d03abbbf38de27516fe18d2fa68a24d0ee3600ade7d620cdb71628bd72d5aa51`;
- at 600 seconds, cached and uncached terminal waits were `4.514 s` and
  `6.568 s`; all seven track payloads were exactly equal at SHA-256
  `a6a7ea95299ea7568977b220715e5b1e6b3ad3c4317cf6c4a8b4d019124aa11b`,
  and both comprehensive views were exactly equal at SHA-256
  `2a09aee6934daa8af04a6ee412aec29f418cefbc94fc18ab3f8b9c7e10d85c0c`.

The 600-second cached path spent `3895.547 ms` in the unlimited final
precompute drain and `10.886 ms` constructing final voiceprint evidence. The
uncached path spent `0.010 ms` and `5934.740 ms` in the same phases. The cached
log reports 618 precompute operations and 640 cached spans at finalization;
that count includes the final drain and is not a claim that every span was
processed live. Exact equality, hashes, phase times, and latency are mechanical
engineering evidence only. They do not evaluate speaker correctness or replace
complete contextual semantic review. At this engineering checkpoint T101 was
still open; the subsequent full-length seal is recorded below.

### 8.7 Direct-end full A/B seal (2026-07-18)

Clean commit `588bfbe63555` completed both required `3615.120`-second runs
through the 1.0x production incremental WebSocket path. Run A started from an
empty isolated registry; Run B restarted the process with Run A's registry
frozen. The acceptance TOML changes only the isolated registry and storage
paths from the checked-in file. Both runs sent `end` directly after the final
audio frame and did not send `flush`.

Run A reached the terminal timeline in `25.597 s`; Run B reached it in
`26.305 s`. Both artifacts have exact seven-track common-clock reconciliation,
no contract issue, complete producer/observer terminal equality, and required
telemetry coverage above the cadence threshold. This closes T101 and the
mechanical terminal-latency gate.

Complete forward and reverse contextual semantic review manually records
512 accepted / 44 incorrect natural contributions for Run A (approximately
92.09 percent) and 509 accepted / 47 incorrect for Run B (approximately 91.55
percent). No code assigned a judgment or total. Both runs retain the natural-
business-turn gate, but all T102 ledger-derived gates remain open. The complete
hashes, changed-context reconciliation, error lists, and permitted claim are in
`direct-end-full-review-2026-07-18.md`.

This section is the pre-FR16ABM direct-end baseline. The promoted handoff
candidate and its new full-run evidence are recorded in Section 8.8.

### 8.8 FR16ABM full promotion seal (2026-07-18)

Clean commit `1a475e6b7473` passed the warning-clean build and all 68 CTest
entries, then advanced through 120-second and 600-second real-WebSocket runs
with direct terminal waits of `0.808 s` and `4.590 s`. Both promotion runs had
exact common-clock extents, producer/observer convergence, no contract issue,
and complete required telemetry coverage.

Full Run A started from an empty isolated registry; full Run B restarted with
Run A's registry frozen. Both streamed `3615.120` seconds at 1.0x and sent
`end` without `flush`. Their direct terminal waits were `26.503 s` and
`26.185 s`; all seven tracks reconciled exactly, observer terminal payloads
matched, and required telemetry cadence passed. These are mechanical facts.

The reviewer then read every one of the 556 contributions for each run in
chronological order and reread both complete sessions in reverse fixed windows.
The manually established natural-turn results are 514 accepted / 42 incorrect
for Run A (approximately 92.45 percent) and 515 / 41 for Run B (approximately
92.63 percent). FR16ABM repairs `ref-0071` on both paths without a contextual
regression. The reread also accepts `ref-0250`: Tang Yunfeng's contribution is
correctly attributed across the next-timestamp display edge; the whole-second
`test.txt` mark does not provide a contradictory sub-second reference boundary,
so a mechanical interval cutoff cannot reject it. No executable mechanism assigned
correctness, aggregated the result, ranked the runs, or issued the verdict.

T106 is complete and FR16ABM is retained. T107-T109 subsequently isolate and
screen the independent forced-alignment/VAD placement defect around
`ref-0090`, as recorded in Section 8.9. T102 remains open for the manual
fixed-block, per-speaker, speaker-time, criticality, confidence, and
source-time-offset breakdowns; therefore T084 and full speaker
closure remain open. See
`native-handoff-full-promotion-review-2026-07-18.md`.

### 8.9 FR16ABN frozen replay checkpoint (2026-07-18)

FR16ABN reuses the checked-in punctuation, alignment pause/tolerance, minimum
embedding duration, and aligned-unit count to recover one delayed subminimum
clause group only when activity, primary, VAD, and forced-alignment evidence
form the specified source-free A-B-A topology. No TOML value or producer track
changes. The warning-clean build and all 68 CTest entries pass.

The promoted FR16ABM full A/B typed tracks were exported and replayed three
times per path. Candidate output is byte-stable within each path and changes
only the `569.26-569.42` short response speaker sequence. Complete forward and
reverse conversational review assigns that confirmation group to Xu Zijing
while preserving Shi Yi's following substantive contribution. The frozen
candidate is retained for real-WebSocket promotion; it is not yet a new
production baseline. See
`delayed-alignment-clause-review-2026-07-18.md`.

At this checkpoint, T110 was defined as the clean transitional experimental
commit plus 120-second and 600-second direct-end production WebSocket ladder;
T111 was gated on its mechanical contracts and complete changed-context review.
Both tasks subsequently completed in Section 8.10. T102 and all remaining
conjunctive speaker gates remain open.

### 8.10 FR16ABN full promotion seal (2026-07-18)

Transitional experimental commit `6b1cb79fa4f5` passed the warning-clean build
and all 68 CTest entries. It then completed the 120-second and 600-second
production WebSocket ladder with direct terminal waits of `0.803 s` and
`4.607 s`. Both artifacts reconciled every common-clock track, converged across
producer and observers, and met the telemetry contract. Complete forward and
reverse contextual review found no changed speaker sequence at 120 seconds and
retained only the intended delayed-confirmation repair at 600 seconds.

Full Run A started from an empty isolated registry. Full Run B restarted the
server with Run A's registry frozen at SHA-256
`66461a77755984a08231d06306da7ce9e1eeac07be1927e91f8a772fc54c7b3f`.
Both streamed `3615.120` seconds at 1.0x and sent direct `end` without `flush`.
Run A completed at `0.993x` with a `25.849 s` terminal wait. The first Run B
artifact completed mechanically except that its runtime telemetry cadence was
`94.965%`, below the unchanged 95 percent gate, so it is retained as an
excluded failure artifact and received no product verdict. One controlled
retry used the same source, binary, behavioral TOML values, audio, and frozen
registry with new isolated storage paths. It completed at `0.993x`, reached
the terminal timeline in `25.585 s`, and passed telemetry cadence at `95.214%`.

The reviewer read all 556 contributions for Run A and the retained Run B in
chronological order, then reread both complete sessions in reverse fixed
windows. The T102 breakdown reread reconciles the `ref-0160` source-label
conflict and the `ref-0182` boundary-only judgment. T135's later complete A/B
reread corrects five omitted errors and manually establishes 514 accepted and
42 incorrect natural contributions for each run, approximately 92.45 percent.
FR16ABN repairs
`ref-0090` on both paths while preserving the following substantive turn. The
additional correct `ref-0192`/`ref-0194` evidence in Run A and `ref-0215` in
Run B are run-specific outcomes and are not attributed to FR16ABN. No
executable mechanism assigned correctness, aggregated the result, ranked the
runs, or issued the verdict.

T110, T111, and the T135 reconciliation are complete. The standalone 90
percent full natural-turn and mechanical terminal-latency gates pass for both
frozen full runs. T102 remains open for
the manual speaker-time, per-speaker time, and source-time-offset breakdowns
from the already completed `test.txt` contextual judgments. The 3000-3600 fixed
block, 朱杰 recall, critical-speaker, and confident-wrong attribution fail;
consequently T084
and full speaker closure remain open. See
`speaker-gate-breakdown-review-2026-07-18.md`. The telemetry loop
also retains an engineering follow-up to schedule samples against absolute
deadlines rather than accumulating probe latency. See
`delayed-alignment-full-promotion-review-2026-07-18.md`.

### 8.11 GPU telemetry absolute-deadline scheduling

T112 changes only the session-owned GPU telemetry timer. The worker records
`steady_clock::now() + interval` as its first deadline and waits with the
existing stop condition variable until that absolute point. After serialization
and emission, a small pipeline scheduling helper advances from the preceding
deadline by whole configured periods to the first deadline strictly after the
current monotonic time. Normal probe latency therefore consumes the remaining
portion of the current period instead of accumulating into every later period.
If a probe or system pause spans one or more complete periods, those expired
slots are skipped; the worker never emits a catch-up burst.

The helper has deterministic tests for the initial cadence, ordinary probe
latency, exact-deadline completion, and multi-period overrun. The production
thread retains the current zero-disabled behavior, TOML interval, serializer,
payload, emitter lock, stop notification, and lifecycle join. Validation then
runs the warning-clean build, full CTest suite, and a real incremental
WebSocket stream with continuous `tegrastats` to verify cadence and required
field coverage mechanically. No product label, speaker total, or acceptance
decision is derived from this timing work.

Transitional experimental commit `d610de36ed13` implements this design. Its
clean warning-free build and all `69/69` CTest entries pass. A clean
120-second 1.0x production WebSocket run records 119 runtime GPU samples
(`99.167%` cadence), 120 continuous tegrastats samples, no adjacent runtime
step outside the one-second cadence, and 100 percent required-field coverage.
T112 is complete; T102/T084 remain unchanged. See
`gpu-telemetry-deadline-review-2026-07-18.md`.

### 8.12 Future-epoch corroborated phrase challenge

The T102 evidence trace separates two possible fixes for late local-slot
identity lag. Increasing `[speaker].local_drift_competing_backfill_gap_sec`
from 4 seconds to 20 seconds moves the relevant late epoch boundary earlier,
but it also rewrites unrelated identity boundaries throughout the session.
That identity-stage experiment is therefore rejected. FR16ABO leaves the raw
identity state machine untouched and applies later identity evidence only in
the revisable `business_speaker` view.

For each complete punctuation phrase, the projector first proves that one
current local slot and identity cover the full phrase in both diarization and
primary top-1, without another local slot. It then finds the first later
different identity for that same local slot within the explicit TOML
`speaker_fusion.future_epoch_lookahead_sec`. The future pair must itself be
stable for the existing minimum embedding duration in both native views. The
candidate identity must be robust-gallery top-1 with the existing score and margin,
session-gallery top-2, and the incumbent must not top either gallery. Existing
alignment-unit count and duration bounds complete the guard. The rule changes
only the exact phrase and records a dedicated decision reason; it never moves
an identity epoch or mutates producer evidence.

The T116 experimental value is frozen at `120.0` seconds, matching the already
frozen identity-stage competing-candidate backfill horizon while remaining a
separately owned fusion switch. Zero disables the rule. Focused tests cover the
positive topology and every evidence abstention. The production replay probe
then runs twice on each frozen T111 A/B typed package. Automation may verify
only parsing, determinism, source reconstruction, track isolation, and the set
of changed contexts. Every changed conversational context is read manually in
forward and reverse order against `test.txt`; no executable mechanism assigns
correctness, totals an accuracy result, selects the candidate, or issues a
promotion verdict. A rejected candidate is removed before any model or real
WebSocket run.

T116 passed the warning-clean build, all 69 CTest entries, the 120/600-second
real-WebSocket ladder, and full empty/frozen-registry A/B capture at clean
transitional commit `f49a8278e0d8`. Complete chronological and tail-to-start
contextual review of all 556 `test.txt` contributions in each run manually
historically records `518/556` for both paths. T135 did not uniformly reaudit
those artifacts, so their numerical rank against T111 is withdrawn; the A/B
error sets differ. FR16ABO is therefore rejected for
promotion and the checked-in TOML switch returns to `0.0`; see
`future-epoch-full-promotion-review-2026-07-18.md`.

### 8.13 Cross-pipeline evidence stability before the next speaker rule

The next investigation does not add another fusion heuristic. It freezes the
T116 full artifacts and separates raw divergence by typed track: diarization,
primary speaker, speaker-identity epochs, ASR, forced alignment, voiceprint,
and business projection. Structural tooling may locate and display the first
different record, changed boundaries, and replay determinism, but it may not
label either side correct, aggregate an accuracy result, or choose a candidate.

Each full artifact is exported to the existing typed replay format. Replaying
the same package must be byte-stable before projection work proceeds. The A/B
raw-track comparison then determines whether the differing error contexts are
fed by producer variance or introduced by final projection. Only a source-free
correction that preserves frozen-input determinism and passes complete forward
and reverse contextual semantic review against `test.txt` may enter another
120/600/full promotion ladder.

### 8.14 Deterministic VAD-gated ASR plan

T117 establishes that the T116 A/B activity-diarization and primary-speaker
tracks are byte-identical, and that replaying either frozen typed package
through the production business projector is byte-stable. The first raw A/B
divergence is ASR source `text_id=49`: one run opens at `658.7 s` and the other
at `658.8 s`, while the surrounding finalized VAD intervals are identical. The
existing gate feeds audio beyond the VAD horizon immediately, so its next
decoder reset is selected by worker scheduling. The configured
`asr.vad_lead_ms` is not applied by that path. Forced alignment, ASR-span
voiceprint evidence, and final business differences are downstream effects of
that source variance, not evidence that the deterministic projector is random.

The implementation keeps all runtime pipelines independent and changes only
their typed evidence contract and controller lifecycle:

1. Extend `IVad` with one model-independent state snapshot containing the
   processed sample frontier, active padded onset, active stable-speech frontier,
   and confirmed-silence frontier. `GpuVad` derives each value from its existing
   512-sample endpoint state, configured minimum speech/silence durations, and
   configured speech padding.
2. Store the active onset/frontier beside the existing VAD state, finalized
   segments, and silence horizon in `ComprehensiveTimeline`. All values are
   converted by the session `TimeBase`; snapshot reads remain immutable and
   non-blocking.
3. Replace `AsrWorker`'s unused ring and scheduling-sensitive unconfirmed-audio
   fallback with one worker-owned pending buffer. Consume finalized or stable
   active speech in source order, prepend TOML `vad_lead_ms` only when opening a
   new VAD group, skip typed confirmed silence beyond the retained source
   context, and retain a segment across a gap no longer than TOML
   `vad_trail_sec`. Every sample in a retained gap is decoder input; otherwise
   ASR would hear speech regions concatenated on a clock that still contains
   the removed pause.
4. Feed decided speech in TOML `vad_gate_chunk_ms` quanta. Hold an incomplete
   active quantum until more stable evidence arrives; consume the exact final
   remainder only after a finalized VAD boundary exists. This makes decoder
   input calls independent of how often ASR observes an advancing typed
   frontier.
5. Remove ASR finalization from the ASR cache-consumer thread. During terminal
   lifecycle, join that collector and the VAD producer, then drain/finalize ASR
   once from the controller using the frozen final typed VAD snapshot. Keep the
   aligner alive until those ASR finals have been deposited.

No worker waits on another worker, and no direct pipeline callback, shared
cursor, or model downcast is introduced. Focused tests first run the same
samples with VAD evidence pre-published and late-published and compare reset
positions, chunk sizes, fed sample values, events, and typed finals exactly.
Timeline tests cover snapshot immutability and monotonic frontiers; VAD
numerical tests remain against the existing CPU oracle. After a warning-clean
build and full CTest, run two independent 120-second real-WebSocket captures
with the same TOML and isolated registries. Structural tools may compare track
hashes and first differences only. Changed conversational content, if any, is
reviewed forward and reverse against `test.txt` before a 600-second or
full-length promotion decision.

### 8.16 FR28 600-second failure and trailing-context correction

The clean `1d511a9` 600-second real-WebSocket run passes direct-end, telemetry,
observer convergence, and common-clock contracts. Complete chronological and
reverse contextual review nevertheless rejects promotion. In addition to the
known first-block errors, `ref-0037` changes the recognizable Tang Yunfeng
sentence `不能再等了` to Zhu Jie, and `ref-0073` changes Shi Yi's response
`我可以否决了，对，45` to Tang Yunfeng. Both contributions were correctly
attributed in the preceding accepted evidence.

Raw-track inspection shows that Sortformer itself is unchanged at the first
failure. The earlier comprehensive view corrected its local-channel error from
phrase voiceprint evidence after forced alignment. FR28 ends the new isolated
ASR source at the padded VAD endpoint, leaving no configured trailing
source-clock context for alignment. At the second failure, several finalized
VAD regions are inside one TOML trailing group, but FR28 advances the common
clock across each short pause without feeding those samples to the decoder.
The decoder therefore receives concatenated speech while the aligner later
works on the unmodified source clock, moving the phrase boundary across the
speaker transition.

The correction remains inside the FR28 ownership boundary. `AsrWorker` holds
an undecided gap until typed VAD proves either that speech resumes within
`vad_trail_sec` or that the endpoint is final. A short gap is then fed in fixed
quanta as part of the same decoder session. A confirmed long gap closes the
record at the trailing source-clock bound but does not send terminal
silence-only samples through the decoder; excess silence is skipped. If the
next region's TOML lead overlaps that bound, the close point moves earlier so
the lead is preserved without replaying or duplicating samples. Publication
order tests compare decoder samples, calls, resets, finals, and source bounds
for both outcomes. The corrected candidate must repeat engineering, silence,
120-second determinism, and complete 600-second contextual gates before any
full capture.

### 8.17 Corrected 600-second result and cross-view handoff projection

Clean commit `7579bc25411c` passes a warning-clean build and all 69 CTest
entries. Three independent 30-second silence WebSocket runs emit no ASR,
alignment, diarization, VAD, voiceprint, or business records. Two independent
120-second runs are byte-identical across all seven product tracks, and complete
chronological and reverse review of the 18 in-scope `test.txt` contributions
finds no new natural-turn regression. The clean 600-second run passes direct
end, observer convergence, telemetry, and common-clock contracts. Complete
chronological and reverse review of all 93 in-scope contributions restores the
recognizable `ref-0037` sentence to Tang Yunfeng but still rejects promotion at
`ref-0073`: only the middle of Shi Yi's response is retained as Shi Yi, while
its onset and `45` remain Tang Yunfeng.

The frozen raw tracks isolate the remaining failure to projection. Sortformer
activity and primary top-1 both expose Shi Yi before `496.88 s` and Tang
Yunfeng after it. The current forced alignment extends one character from
`495.868 s` through `497.148 s`, crossing that handoff and collapsing the next
characters onto its endpoint. The base projector consequently groups the
preceding phrase with the following Tang run. A containing TitaNet business
interval then measures both speakers together and writes the longer Tang
identity across the complete source range. The frozen T111 alignment places
the same substantive phrase before the handoff, confirming that the raw native
boundary remains useful while the current unit end is not a reliable text-run
boundary.

FR29 corrects only this generic projection failure. The base projector derives
a corroborated handoff onset from matching activity and primary transitions.
It uses that onset only when one aligned unit longer than the existing pause
threshold straddles it, ending the preceding source run after that unit. The
fusion policy then treats the two immutable base identities as separate write
domains: a containing business-interval majority may update its matching run
but may not erase the other run when both native views corroborate at least the
existing minimum aligned duration. Focused tests cover the positive topology
and one-view, identity-disagreement, short-following-run, normal-unit,
insufficient-native-coverage, and source-order abstentions.

The current T126 typed tracks are exported with source hashes and replayed at
least twice through the production C++ projector. Automation verifies only
parseability, source reconstruction, deterministic bytes, and the set of
changed contexts. Every changed conversational context is read completely in
chronological and reverse order against `test.txt`. A rejected projection is
removed before any audio rerun. A retained projection must then pass a clean
build, all CTest entries, repeated 120-second production WebSocket determinism,
and a new complete 600-second chronological/reverse contextual gate before
T123 was authorized. No behavioral value is added or changed in TOML.

T127 completes that frozen gate. Three production-projector replays are
byte-identical and expose one speaker-sequence change spanning `ref-0073` and
its boundary with `ref-0074`; three other displayed rows retain their speaker
sequence. Complete forward and reverse reading of `ref-0067` through
`ref-0082`, plus the unchanged `ref-0018` through `ref-0023` context, retains
the candidate: Shi Yi owns the substantive response through `对，四十五。`,
and Tang Yunfeng begins the following clause. The warning-clean build and all
69 CTest entries pass. T128 now owns the two 120-second captures and the new
600-second complete contextual gate; see
`cross-view-handoff-review-2026-07-18.md`.

T128 completes the real-stream gate from clean commit `2ce4a12b7973`. Two
independent 120-second captures use isolated empty registries and storage,
direct `end`, observers, telemetry, 100 ms frames, and 1.0x pacing. Their seven
typed product-track entry bundles are byte-identical. Complete chronological
and reverse reading of `ref-0001` through `ref-0018` retains the known
cold-start, micro-turn, and boundary defects but finds no new speaker-business
regression. The clean 600-second capture completes in `603.33 s`, receives its
terminal timeline `3.329 s` after direct `end`, and closes every pipeline at
`9,600,000` samples without a gap. Complete chronological and reverse reading
of all 93 in-scope contributions restores the substantive `ref-0073` Shi Yi
answer and keeps `ref-0074` with Tang Yunfeng; no other contextual speaker
regression is introduced.

The real run also corrects an earlier documentation assumption. Diarization,
primary speaker, ASR, VAD, and alignment entries are byte-identical to T126.
The speaker-voiceprint track is intentionally downstream of the base business
projection: `SpeakerEvidenceSnapshot::business_speaker` supplies source ranges
to `SpeakerEvidenceStage::BuildVoiceprintQueries()` after the final primary
deposit. FR29 therefore partitions the old containing `495.788-498.908 s`
business-interval query into `495.788-497.148 s` and `497.308-498.908 s`
queries. This adds one derived record and shifts later ordinals for the same
text; it does not mutate an upstream producer or introduce a feedback cycle.
The code comments and focused unit contract are updated to make that existing
dependency explicit. No behavioral TOML value changes. This authorized T123
from the next clean documentation-and-test commit.

### 8.18 FR29 full-session outcome and frozen diagnosis

T123 completes from clean commit
`2ff9ce3655b2a12e90a5d0def25c0a30f171f2d9`. Full Run A uses an empty
registry; Run B restarts the server with Run A's registry frozen. Both stream
all `3615.120` seconds through the production WebSocket path at `0.995x`, pass
the direct-end, common-clock, observer, provenance, and telemetry contracts,
and produce identical normalized entries in all seven product tracks.

Each run receives a complete chronological review of all 556 human-listened
`test.txt` contributions followed by an independent reverse-600-second-block
review. T135's uniform material-fragment reconciliation corrects the result to
`505/556` for each run. The full
natural-turn average remains above 90 percent, but the 2400-3000 and 3000-3600
fixed blocks, 朱杰 and 唐云峰 turn recall, the 93-percent development margin,
critical attribution, and confident-wrong attribution fail. FR29 promotion is
therefore rejected and T111 remains the best frozen comparison baseline, not
an accepted closing result; see
`cross-view-handoff-full-promotion-review-2026-07-18.md` and
`speaker-baseline-reconciliation-2026-07-19.md`.

No new full audio run follows this rejection. The frozen T123 and T111 exports
show identical Sortformer diarization and primary-speaker views while ASR,
forced alignment, derived voiceprint, and final business tracks differ. The
next diagnosis locates the first changed source interval in each failed late
block and traces it through VAD-gated ASR finalization, alignment, voiceprint
query construction, and revision order. A new source-free rule must first pass
deterministic replay plus complete changed-context review on both frozen paths.

### 8.19 FR30 VAD-sensitivity recovery

The frozen diagnosis is now complete enough to reject another fusion-policy
patch. Replaying T111 diarization, primary, ASR, alignment, and voiceprint
tracks through the current projector changes no reference-interval speaker
sequence from the original T111 final view. Replaying T123 tracks reproduces
the T123 final view apart from sub-microsecond split-point serialization. The
same projector therefore preserves the frozen T111 view when its upstream
evidence is preserved.

The first causal loss is in the FR28 input frontier. T111 consumed undecided
audio while ASR happened to lead VAD, including low-energy speech that Silero
did not finalize. FR28 correctly removed that scheduling dependency and now
feeds only stable typed VAD evidence. With the checked-in TOML threshold `0.5`,
the manually reviewed `2752-2754 s` contribution falls between VAD segments
ending at `2751.996 s` and starting at `2754.724 s`; another reviewed utterance
inside `3278-3284 s` falls between segments ending at `3277.980 s` and starting
at `3284.132 s`. This omission propagates into ASR text boundaries, forced
alignment, short-window TitaNet evidence, and final business revisions.
T135 later confirms that T111 already assigns most of the sustained
`ref-0503` speaker turn to the wrong identity despite retaining more ASR
evidence. The VAD observation therefore explains changed evidence availability,
not the original speaker failure.

FR30 tests one production parameter only. A temporary TOML with
`vad.threshold = 0.4` exposes `2753.668-2754.076 s` and
`3278.276-3278.844 s`. A second temporary TOML with threshold `0.3` exposes
`2753.540-2754.140 s`, `3278.276-3278.908 s`, and
`3281.124-3281.628 s`; the last interval agrees mechanically with independent
Sortformer activity at `3281.040-3281.920 s`. The threshold-0.3 production VAD
probe emits zero segments for the frozen 30-second silence fixture. These raw
interval observations justify one candidate but do not evaluate correctness.

The checked-in candidate changes only `vad.threshold` to `0.3`. It first passes
the VAD numerical oracle, warning-clean build, all CTest entries, the frozen
silence probe, three real-WebSocket silence runs, and two independent
120-second captures with exact seven-track repeatability. Every 120-second
reference contribution is then read chronologically and in reverse. Only if
those gates pass does one 600-second real-WebSocket capture receive complete
93-contribution chronological/reverse contextual review. A full A/B capture is
not authorized before that manual gate is retained.

The checked-in candidate completes the pre-commit engineering portion of T130:
its full-audio VAD export is byte-identical to the temporary threshold-0.3
probe, its frozen-silence export is empty, the VAD numerical test passes, the
clean build warning/error scan is empty, and all `69/69` CTest entries pass.
See `vad-sensitivity-diagnosis-2026-07-19.md` for frozen hashes and the claim
boundary.

T131 is complete at clean commit `5046bccf7ea2`. Three independent 30-second
real-WebSocket silence sessions emit zero product records, and two independent
120-second sessions have exact normalized seven-track equality. All 18
in-scope contributions were then read independently in chronological and
reverse order. The only speaker-sequence display change from T125 removes a
short intervening local-label fragment inside `ref-0008`; complete surrounding
context shows no new natural-turn regression. This manual result authorizes
one clean 600-second gate only. See
`vad-sensitivity-120-context-review-2026-07-19.md`.

T132 is complete at clean commit `30162d1c844d`. Its production 600-second
real-WebSocket artifact closes all seven common-clock extents at `9,600,000`
samples without a gap, converges the producer and both observer terminal
hashes, and has complete required telemetry coverage. All 93 contributions
were read chronologically and again in reverse 60-second windows. The retained
`ref-0037` and `ref-0073` exchanges remain substantively correct. Complete
forward/reverse reading of all ten speaker-sequence changes from T128 finds no
new natural-turn regression and restores several early contributions. This
manual result authorizes a full empty-registry/frozen-registry A/B gate; it does
not promote FR30 or replace T111. See
`vad-sensitivity-600-context-review-2026-07-19.md`.

The full gate starts from the clean commit containing the T132 record. Run A
uses an empty isolated registry. After Run A ends, its registry is frozen and a
new server process and protocol store produce Run B. Both runs use 100 ms
frames, 1.0x pacing, direct `end`, observers, telemetry, and unchanged
checked-in behavioral TOML. Each artifact receives an independent complete
556-contribution chronological read and reverse 600-second-block read. Only the
manually reconciled ledgers may promote or reject FR30.

T133 and T134 complete from clean commit `a96e278ea340`. Run A uses an empty
registry and valid Run B restarts from Run A's frozen registry. Both complete
at `0.996x`, pass direct-end, exact common-clock, provenance, observer,
telemetry, and normalized seven-track repeatability contracts. Each artifact
then receives a complete 556-contribution chronological read and an independent
reverse-block read. The two manually reconciled ledgers originally agreed at
`498/556`. T135's uniform material-fragment reconciliation adds `ref-0099` and
corrects both ledgers to `497/556`. Two fixed blocks, three canonical speakers,
the full 90-percent floor,
critical attribution, and confident-wrong attribution fail. FR30 is rejected
and checked-in `vad.threshold` returns to `0.5`; see
`vad-sensitivity-full-promotion-review-2026-07-19.md`.

The next diagnosis does not lower the global VAD threshold again. It preserves
the threshold-`0.5` segmentation and uses frozen typed-track evidence to test
whether only low-energy gaps corroborated by independent speaker activity can
be admitted on the common time base without merging neighboring VAD/ASR
sessions. Any candidate must be source-free and pass complete changed-context
review before another real-audio promotion run.

### 8.20 Uniform baseline reconciliation and next evidence split

T135 reopens no model output and changes no runtime behavior. It reads both
frozen T111 artifacts across all 556 contributions chronologically and again
in reverse 600-second blocks, then applies the same material-fragment rule to
the already completed T123 and T133 reviews. The corrected natural-turn
results are T111 `514/556`, T123 `505/556`, and T133 `497/556`. T111 is the
best frozen comparison but fails the 3000-3600 block, 朱杰 recall, critical
attribution, and confident-wrong gates.

The corrected comparison prevents the next implementation from treating
`ref-0503` as a new VAD-caused speaker regression. T111 already retains more
words there while assigning most of the sustained turn to the wrong identity.
The next diagnosis therefore separates two evidence classes on frozen tracks:

1. T123-only errors whose recognizable contribution is absent before forced
   alignment because deterministic VAD gating withheld ASR evidence.
2. Errors already present with usable T111 text, where Sortformer, primary,
   voiceprint, and business fusion disagree or a later projection rewrites a
   supported identity.

No global threshold change or full capture follows directly from this audit.
The next candidate must be source-free, preserve the common sample clock, and
show complete contextual repairs without changed-context regressions before it
enters the real-WebSocket promotion ladder. See
`speaker-baseline-reconciliation-2026-07-19.md`.

### 8.21 Corroborated primary-return preservation

T136 completes the frozen split required by T135. Relative to T111, the T123
view has three newly wrong contributions whose recognizable speech is absent
before alignment and thirteen whose recognizable speech is still present but
whose final identity or boundary changes. Displaying the immutable tracks shows
that several present-text failures contain a short A-B-A primary-speaker return
whose B run is independently covered by activity diarization. The base
projector sometimes retains aligned B characters, after which an ordinary
punctuation-phrase or complete-source voiceprint write flattens the whole range
back to A. Other failures have no aligned character in the B run, no agreement
between the two Sortformer views, or require voiceprint evidence to recover a
stable identity; they are outside this candidate.

FR31 adds a guard at the final revisable speaker-evidence layer rather than a
new producer or a global preference for primary top-1. For each source
character that an ordinary phrase or complete-source voiceprint write proposes
to change, the projector requires existing typed primary-arbitration
provenance, positive forced-alignment time inside one short primary B run,
nearest primary runs on both sides carrying the proposed A identity, and full
activity coverage of the B run. Duration bounds reuse TOML
`speaker_fusion.min_embed_sec` and `speaker_fusion.short_max_sec`; no new value
is introduced. Specialized multi-view challenges remain earlier and retain
their current authority. The write skips only the corroborated B characters;
source text, character time, all producer tracks, and the common clock remain
unchanged.

Focused tests cover the positive A-B-A topology and abstention for missing
alignment, one-view evidence, short and regular runs, missing brackets,
different bracket identities, activity gaps, and explicit specialized
challenge precedence. The production replay probe then runs repeatedly on both
frozen T111 and T123 typed packages. Automation may compare bytes, hashes,
source reconstruction, timing, immutable input tracks, and display changed
contexts only. Every changed context is read in complete surrounding
conversation forward and reverse against `test.txt`. A rejected candidate is
removed without an audio run. A retained candidate is committed and pushed
before repeating the silence, 120-second, 600-second, and full A/B real-
WebSocket ladder. See
`corroborated-primary-return-diagnosis-2026-07-19.md`.

The deterministic replays exposed that this native topology is necessary but
not sufficient. It restored real returns, but it also preserved primary-run
boundary leakage inside longer single-speaker contributions. Complete manual
review therefore rejects the broad FR31 guard before any real-WebSocket run.
The rejected implementation and its focused test are removed; the immutable
replay artifacts remain diagnostic evidence. See
`corroborated-primary-return-review-2026-07-19.md`.

### 8.22 Exact cross-scale primary-return precedence

FR32 keeps only the evidence case that can be corroborated independently
without a fitted confidence threshold. A broader phrase or complete-source
write selecting A may preserve a base primary B character only when the
character midpoint lies in a short A-B-A primary run and one same-text typed
business interval has exactly that run's common-clock bounds. That exact
interval must have an embedding, a complete robust gallery, and session plus
robust selections that both pass the existing duration-class gates and agree
on B. Activity B must cover the run, while any third activity identity vetoes
the guard.

This is an evidence-precedence rule, not a preference for primary top-1. It
allows a narrow, exact primary-run TitaNet query corroborated by both galleries
to survive a later broader query, while ordinary A-B-A islands continue to
abstain. It reuses only the checked-in TOML duration and score gates and does
not inspect source text, reference labels, names, or known times. Focused tests
cover exact agreement and abstention for non-exact bounds, unavailable or
incomplete galleries, gallery disagreement, gate failure, missing activity,
third activity, missing alignment, wrong topology, and duration bounds.

The production replay probe then runs twice over each frozen T111 and T123
package. Automation verifies source/time/configuration and byte determinism and
arranges raw changed contexts only. Every changed context is read manually in
full forward and reverse conversation against `test.txt`. Rejection removes
FR32 without an audio run; retention permits a warning-clean full engineering
gate and then the existing real-WebSocket promotion ladder.

The frozen result retains FR32. Both T123 replays have SHA-256
`8fb70821df483cf40b28c701b88d38713404472dcb179a2bad10c14d4fd72ef2` and
change only `text_id=84`; both T111 replays have SHA-256
`646ea91b357cafaf8af82c4f45e5cc771c622a501e71b12f2d1aa1555fb055f2`
and do not change the business view. Complete forward and reverse reading
against `test.txt` retains the exact Tang Yunfeng `不含` repair with no changed
neighboring contribution. The warning-clean build and 69/69 CTest pass permit
the conditional commit and real-WebSocket ladder. See
`exact-cross-scale-primary-return-review-2026-07-19.md`.

### 8.23 FR32 full promotion and partition-invariant abstention

T142 freezes transitional experimental commit
`72d81c8084757b4c4210ba90ac14b5d1c1155e89` and completes the entire
real-WebSocket promotion ladder. The 30-second silence run emits no product
record. Independent 120-second runs have byte-identical normalized seven-track
entries, and complete forward/reverse review of all 18 contributions finds no
new regression. The 600-second run closes all tracks at `9,600,000` samples;
complete forward/reverse review of all 93 contributions also finds no new
regression.

Full empty-registry Run A and restarted frozen-registry Run B both process
`3615.120` seconds at `0.995x`, satisfy direct-end, common-clock, provenance,
observer, telemetry, and normalized seven-track repeatability contracts, and
preserve the same frozen registry. Each artifact receives an independent
complete 556-contribution chronological reading and reverse six-block reading
against `test.txt`. Both manual reviews record `506/556`. FR32 repairs only
`ref-0154` relative to T123 and introduces no new contextual regression or
long-session identity swap. Two fixed blocks, 朱杰 recall, critical
attribution, and confident-wrong attribution fail, so FR32 remains a bounded
transitional repair and does not close the speaker business. See
`exact-cross-scale-primary-return-full-promotion-review-2026-07-19.md`.

The next iteration returns to frozen evidence before another audio run. FR33
tests a partition-invariant abstention topology exposed at `ref-0517`: an
ordinary short phrase may not replace a uniform current identity when phrase-
scale session evidence alone proposes the challenger, phrase robust evidence
abstains with the current identity second, the unique containing VAD reverses
that ordering, and both broader same-text business and complete-source views
select the current identity in both galleries under unchanged gates. Activity
and primary must cover the exact phrase with the current identity, alignment
must meet the existing minimum-unit contract, all galleries must be complete,
and specialized challenge rules retain precedence. FR33 adds no threshold,
duration, identity, transcript, timestamp, or reference input. Focused tests
and repeated T111/T123 production-projector replays precede complete forward
and reverse review of every changed context. A failed frozen review removes the
candidate without an audio run.

The frozen review retains FR33. Repeated T123 outputs are byte-identical and
change only `text_id=289`; repeated T111 outputs remain byte-identical to
FR32. Complete chronological and reverse reading of `ref-0508` through
`ref-0525` assigns the changed question to Tang Yunfeng's `ref-0517` and finds
no change to Shi Yi's preceding contribution or any following turn. The
manually reconciled frozen candidate advances from `506/556` to `507/556`, but
both late fixed blocks, 朱杰 recall, critical attribution, and confident-wrong
attribution still fail. FR33 is retained only as frozen evidence. The next
candidate remains frozen and separately specified; no audio rerun follows
from this result. See
`partition-invariant-cross-scale-abstention-review-2026-07-19.md`.

FR34 remains on the same frozen producer tracks and addresses a different
evidence-precedence topology. The production projector first confirms that one
unique containing regular business interval selected the current direct
identity A in both galleries. For the later exact short phrase, both phrase
galleries and both galleries of one unique containing VAD must select a
different identity B under the unchanged duration-class gates. Activity B must
cover the complete phrase. One and only one overlapping primary segment must
cover the same phrase with a third identity C, activity C must also cover it,
and no fourth or current-identity activity may overlap. This makes primary a
typed conflict boundary rather than speaker authority and prevents the broad
interval, exact phrase, and native overlap views from being treated as votes.
Only the exact phrase source/alignment range may change; all existing
specialized challenges run first.

Focused positive and abstention tests cover every identity, containment,
gallery, alignment, activity, primary, and provenance condition. The
production C++ projector is then replayed twice over frozen T123 and T111.
Automation verifies only hashes, immutable inputs, source reconstruction, time
order, and displayed change scope. Every changed complete conversation is read
chronologically and in reverse against `test.txt`; only that review may retain
or reject FR34. The historical FR16AAG files are diagnostic background only
because their full two-pass review was never completed. No audio is rerun until
the current frozen candidate passes this independent gate. See
`exact-phrase-vad-direct-conflict-diagnosis-2026-07-19.md`.

The independent frozen gate retains FR34. T123 Run A and Run B have the same
SHA-256 and differ from FR33 only by the exact `text_id=236` split. T111 is
byte-identical to FR33. The reviewer reads the entire displayed `44:28-46:57`
conversation forward and reverse, confirms the Zhu question, Tang substantive
answer, and Zhu continuation, and records the preceding `对，` as a bounded
`0.160 s` residual rather than extending the rule beyond typed phrase evidence.
The frozen ledger advances by one reviewed Tang contribution to `508/556`.
Both late fixed blocks and the other conjunctive closing gates remain open, so
FR34 is a transitional frozen repair and not a real-stream acceptance result.
See `exact-phrase-vad-direct-conflict-review-2026-07-19.md`.

FR35 is a boundary-invariance correction inside the existing isolated aligned
unit challenge. The implementation reuses
`align_boundary_split_tolerance_sec` when comparing each neighbouring aligned
gap with `align_snap_pause_sec`; no new configuration field is introduced. The
current rule's no-embedding duration bound, uniform native labels, unique local
slot and initial identity, exact primary coverage, unique containing VAD, and
dual-gallery selection remain required. Focused tests prove that a gap within
the configured tolerance is admitted while an out-of-tolerance gap, zero
tolerance, or any missing evidence still abstains. Repeated frozen T123 and
T111 projector replays then expose only raw change scope. Complete forward and
reverse context review remains the sole retention gate. See
`aligned-unit-isolation-tolerance-diagnosis-2026-07-19.md`.

The independent frozen gate retains FR35. T123 Run A and Run B have the same
SHA-256 and differ from FR34 only by the isolated `text_id=242` source split;
T111 is byte-identical to FR34. The reviewer reads the entire displayed
`46:19-47:53` conversation forward and reverse and confirms the Tang
instruction, Zhu acknowledgement, Shi confirmation question, and Zhu
explanation without a neighbouring change. The frozen ledger advances by one
reviewed Zhu Jie contribution to `509/556`. The 2400-3000 and 3000-3600 fixed
blocks and the other conjunctive closing gates remain open, so FR35 is a
transitional frozen repair and not a real-stream acceptance result. See
`aligned-unit-isolation-tolerance-review-2026-07-19.md`.

FR36 remains on the same frozen producer tracks and addresses a separate
regular-phrase partition regression. The production projector first confirms
uniform native A labels, one uncontested same local slot in activity and
primary, and that slot's different initial identity B. It then requires the
exact phrase's two galleries to rank A first and B second in the specified
one-margin-pass regular abstention pattern. One unique containing VAD and one
unique same-text complete-source record must reverse to B first and A second in
all four outer views while independently failing the unchanged regular score
and margin gates. No individual abstaining view may write the result; only the
complete same-slot six-view topology may challenge the exact phrase. Existing
specialized challenges run first.

Focused positive and independent abstention tests cover every provenance,
slot, identity, alignment, containment, gallery-rank, and existing-gate
condition. The production C++ projector is then replayed twice over frozen
T123 and T111. Automation verifies only hashes, immutable inputs, source
reconstruction, time order, and displayed change scope. Every changed complete
conversation is read chronologically and in reverse against `test.txt`; only
that contextual semantic review may retain or reject FR36. No audio is rerun
until the current frozen candidate passes this independent gate. See
`partition-invariant-regular-initial-slot-diagnosis-2026-07-19.md`.

The independent frozen gate retains FR36. T123 Run A and Run B have the same
SHA-256 and differ from FR35 only at the complete `text_id=217` phrase; T111 is
byte-identical to FR35. The reviewer reads the entire displayed
`40:45-42:32` conversation forward and reverse and confirms Zhu Jie's response
between Tang Yunfeng's profitability question and follow-up question without a
neighbouring change. The frozen ledger advances by one reviewed critical Zhu
Jie contribution to `510/556`, and the 2400-3000 fixed block reaches
`117/129` and passes. The 3000-3600 block and the other conjunctive closing
gates remain open, so FR36 is a transitional frozen repair and not a
real-stream acceptance result. See
`partition-invariant-regular-initial-slot-review-2026-07-19.md`.

FR37 remains on the same frozen producer tracks and addresses a short answer
whose punctuation-phrase challenge disappears under the T123 partition. The
production projector first confirms one unprotected primary-owned interval in
current identity A, one containing A primary run gaplessly bracketed by the
same competitor C, exactly two completely covering activity slots A and C, and
initial identity B for A's local slot. It then requires dual eligible interval
views with exact A/C and A/B top-two order, a source/time-adjacent preceding
phrase whose two views rank B in
the specified one-margin-pass pattern, and one containing VAD whose two views
independently select B under unchanged gates. A, B, and C remain pairwise
distinct. No individual view or primary bracket may write the result; only the
complete partition-reconstruction topology may challenge the exact interval.

Focused positive and independent abstention tests cover every provenance,
slot, identity, alignment, containment, adjacency, primary-bracket,
gallery-rank, and existing-gate condition. The production C++ projector is
then replayed twice over frozen T123 and T111. Automation verifies only hashes,
immutable inputs, source reconstruction, time order, and displayed change
scope. Every changed complete conversation is read chronologically and in
reverse against `test.txt`; only that contextual semantic review may retain or
reject FR37. No audio is rerun until the current frozen candidate passes this
independent gate. See
`bracketed-primary-adjacent-vad-reconstruction-diagnosis-2026-07-19.md`.

The independent frozen gate retains FR37. T123 Run A and Run B have the same
SHA-256 and differ from FR36 only at the complete `我向国家交。` interval;
T111 is byte-identical to FR36. The reviewer reads the entire displayed
`50:35-52:28` conversation forward and reverse and confirms Zhu Jie's answer
between Shi Yi's question and clarification without changing either Shi
contribution. The frozen ledger advances by one reviewed critical Zhu Jie
contribution to `511/556`; the 3000-3600 fixed block reaches `78/87` and still
fails. The other conjunctive closing gates remain open, so FR37 is a
transitional frozen repair and not a real-stream acceptance result. See
`bracketed-primary-adjacent-vad-reconstruction-review-2026-07-19.md`.

FR38 first corrects the diagnosis boundary around `ref-0504`: the disabled
future-epoch experiment repaired only T111's leading interval, while the
retained T123 package already assigns that interval to Tang Yunfeng. The T123
defect is the final one-character aligned tail of a source-leading punctuation
phrase. It crosses into a second VAD and inherits the following native clause,
even though the leading exact interval and the complete phrase's robust view
retain the preceding identity.

The projector will test one strict three-identity topology. A leading embedded
interval is native A but already written to B by eligible dual short evidence;
one source-contiguous no-embedding tail is native C and contains exactly one
visible aligned character plus configured punctuation; and one separately
embedded following interval begins at the phrase source end and remains C.
Activity and primary must uniquely corroborate A on the leading interval and C
on both later intervals. Exact interval, complete phrase, leading VAD, and tail
VAD top-two orders must match the FR38 contract under only the existing
duration-class gates. The tail VAD must begin within the configured alignment
tolerance and extend into the following C interval, proving why it cannot own
the phrase tail independently. Only the tail is rewritten; the following
clause remains untouched.

Focused positive and independent abstention tests cover every source,
alignment, provenance, identity, native-track, gallery, gate, VAD, and
following-clause condition. The production projector is then replayed twice
over frozen T123 and T111. Automation verifies only immutable hashes,
determinism, reconstruction, time order, and displayed change scope. Every
changed complete conversation is read chronologically and in reverse against
`test.txt`; only that contextual semantic review may retain or reject FR38.
No audio is rerun until the frozen gate completes. See
`cross-vad-phrase-tail-reconstruction-diagnosis-2026-07-19.md`.

The independent frozen gate retains FR38. Repeated T123 outputs are
byte-identical and differ from FR37 only by the exact `text_id=280` phrase-tail
split; repeated T111 outputs remain byte-identical to FR37. The reviewer reads
the entire displayed `53:49-55:45` conversation forward and reverse, confirms
Tang Yunfeng's complete ownership correction, and verifies that the preceding
Zhu Jie proposal and following Hangzhou-stake clause remain visible as their
existing errors. The frozen ledger advances by one reviewed critical Tang
contribution to `512/556`, and the 3000-3600 natural-turn block reaches `79/87`
and passes. Zhu Jie recall, critical and confident-wrong attribution,
speaker-time sign-off, real-path repeatability, and holdout gates remain open,
so FR38 is a transitional frozen repair and not a real-stream acceptance
result. See `cross-vad-phrase-tail-reconstruction-review-2026-07-19.md`.

FR39 addresses the reconciled `ref-0518` boundary without reviving the earlier
historical interpretation that treated Zhu Jie's answer as part of Tang
Yunfeng's question. Frozen T111 and T123 expose the same topology: activity and
primary retain Tang on the source-leading exact phrase, but that local slot's
initial stable identity and both exact-phrase galleries identify Zhu. One
independently aligned trailing response follows a configured pause; including
it reverses both short interval galleries to Tang, while the containing VAD and
complete-source views remain low-confidence Zhu/Tang cross-order abstentions.

The projector will require the complete source-leading phrase/isolated-tail
topology. One unique Tang activity slot must cover the complete short interval,
its matching primary segment must cover the phrase, and one third-identity
primary segment must cover only the single aligned tail. Exact phrase,
containing interval, VAD, and complete-source top-two orders must expose only
the initial/current identity pair under their unchanged duration-class gates.
Only the exact phrase may return to the slot's initial identity; the tail and
all neighbouring source remain untouched. No TOML value, threshold, transcript
lookup, identity, or time constant is added.

Focused tests cover the positive topology and independent abstention for every
source, provenance, alignment, slot, native-track, gallery, gate, containment,
and write-scope requirement. The production projector is then replayed twice
over frozen T123 and T111. Automation verifies only immutable hashes,
determinism, reconstruction, time order, and displayed change scope. Every
changed complete conversation is read chronologically and in reverse against
`test.txt`; only that contextual semantic review may retain or reject FR39.
No audio is rerun before the frozen gate completes. See
`source-leading-phrase-tail-isolation-diagnosis-2026-07-19.md`.

The independent frozen gate retains FR39. Repeated T123 and T111 outputs are
separately byte-identical and differ from FR38 only by their corresponding
source-leading exact phrase split. Complete forward and reverse reading of
`54:05-57:54` confirms Tang Yunfeng's question, Zhu Jie's substantive answer,
and Tang's later response as three natural contributions. The isolated
`0.080 s` trailing `对。` remains a visible boundary residual under Tang and no
neighbouring contribution changes. The manually reconciled frozen ledger
advances to `513/556`; the 3000-3600 block reaches `80/87`, while Zhu Jie
recall remains below its 90 percent gate. Critical/confident-wrong attribution,
speaker-time sign-off, real-path repeatability, and holdout gates also remain
open. FR39 is therefore a transitional frozen repair, not a real-stream
acceptance result. See
`source-leading-phrase-tail-isolation-review-2026-07-19.md`.

FR40 addresses the partition-sensitive `ref-0024`/`ref-0025` handoff. T111
places Zhu Jie's `啊？` and Xu Zijing's `嗯？` in one source-tail interval and
the coarse short voiceprint write assigns both to Xu. T123 separates the same
two aligned reactions across adjacent ASR sources; its wider following
interval assigns the source-leading Xu response to Zhu. The immutable common-
clock producer evidence is otherwise identical: one Zhu activity slot covers
both units, primary changes exactly from Zhu to Xu between them, and one VAD
containing exactly the two units ranks Xu then Zhu in both galleries under the
unchanged eligible short gates.

The projector will add one final exact-unit challenge after existing coarse
source reconstructions. It finds the unique containing VAD, requires exactly
two one-character punctuation clauses across either one or two ASR sources,
validates their raw gap with only the configured pause and boundary-tolerance
values, proves unique A-to-B primary runs,
proves one uncontested A activity slot over both units, and requires eligible
B/A VAD session and robust views. It rewrites only a currently conflicting
`voiceprint_direct_*` unit to its primary identity and carries that identity
only through immediately following configured punctuation. No source text,
speaker name, known time, reference row, new threshold, or TOML value enters
the rule.

Focused tests cover the merged-source and split-source positive shapes plus
independent abstention for every source, punctuation, provenance, alignment,
containment, unit-count, timing, primary, activity, VAD rank, gate, and write-
scope condition. The production projector is then replayed twice over frozen
T123 and T111. Automation verifies only immutable hashes, determinism,
reconstruction, time order, and displayed change scope. Every changed complete
conversation is read chronologically and in reverse against `test.txt`; only
that contextual semantic review may retain or reject FR40. No audio is rerun
before the frozen gate completes. See
`two-unit-primary-handoff-diagnosis-2026-07-19.md`.

The independent frozen gate retains FR40. Final clean-build T123 replays are
byte-identical and change only `184.240-184.320` `嗯，` from Zhu to Xu. T111
replays are separately byte-identical and split only the merged `啊？嗯。`
tail into Zhu then Xu. Complete forward and reverse reading of the full
`02:07-03:45` context confirms the same Zhu-to-Xu-to-Zhu sequence in both
partitions and no neighboring change. Only T123 `ref-0025` advances the current
manual ledger, to `514/556`; the T111 `ref-0024` repair is not double-counted.
FR40 remains a transitional frozen repair, not a real-stream acceptance result.
See `two-unit-primary-handoff-review-2026-07-19.md`.

FR41 addresses the remaining T111/T123 partition regression at `ref-0268` by
extending only the retained primary-onset aligned-island rule. Both packages
have the same paused A-before/B/gapless-A-after primary sequence, overlapping
A/B activity, isolated VAD onset, dual-gallery A continuation, and common
clock. T111 places a punctuation-separated previous source unit inside B, so
the existing rule selects Zhu Jie's first `啊`. T123 places that previous unit
before B and leaves exactly one candidate unit inside B; the source and
acoustic boundaries otherwise agree.

The implementation will preserve the current in-run path and add one strict
single-unit boundary path. It locates the nearest previous and following
positive aligned units in the same source, requires a configured-punctuation-
only source gap and configured temporal pause before the candidate, requires
the following source-adjacent unit to begin after B ends, and reuses every
existing primary, activity, label, VAD, duration, and rank guard. It writes
only the candidate character. No TOML value, threshold, transcript, identity,
or producer track changes.

Focused tests will keep the existing positive and abstention matrix and add
independent single-unit checks for previous/following presence, source
identity, punctuation gap, temporal pause, candidate cardinality/type,
adjacency, post-run boundary, configured minimum units, and write scope. The
clean projector will replay frozen T123 and T111 twice. Automation may expose
only mechanical changes; complete chronological and reverse contextual
semantic review against `test.txt` alone decides retention. See
`primary-onset-single-unit-partition-diagnosis-2026-07-19.md`.

The independent frozen gate retains FR41. Final clean-binary T123 replays are
byte-identical and split only `ref-0268` into Zhu Jie's first `啊` followed by
Xu Zijing's second `啊` and technical continuation. Repeated T111 output is
separately byte-identical and unchanged from FR40. Complete chronological and
reverse reading of `31:17-33:23` confirms the Xu-to-Zhu-to-Xu handoff and no
neighboring contribution change. The current manual ledger advances only for
T123 `ref-0268`, to `515/556`; T111 is not double-counted. Zhu Jie's
natural-turn ledger becomes `75/83`, so all four per-speaker natural-turn
floors pass. Critical attribution, confident-wrong attribution, time-based,
real-path repeatability, and holdout gates remain open. FR41 remains a
transitional frozen repair, not a real-stream acceptance result. See
`primary-onset-single-unit-partition-review-2026-07-19.md`.

FR42 addresses the remaining T111/T123 partition regression at critical
`ref-0432` by extending only the retained isolated-VAD aligned-island rule.
Both packages have identical coarse Tang activity and primary tracks, the same
isolated `2851.172-2851.868` VAD, and independent session/robust VAD views that
select Zhu. T111 exposes three source-contiguous positive units and already
uses the retained rule. T123 exposes two positive units around one visible
zero-duration source character; their temporal gap remains below the existing
alignment-pause value.

The implementation preserves the current contiguous path and adds one
strict alignment-dropout path. It requires the existing configured minimum
unit count, one-character units, exactly one one-character source gap, a
visible non-whitespace/non-punctuation missing character, and a sub-pause
temporal gap. It reuses every existing VAD, dual-gallery, isolation, label,
activity, and primary guard and writes only from the first positive unit
through the last. No TOML value, threshold, transcript, identity, or producer
track changes.

Focused tests preserve the existing positive and abstention matrix and add
independent dropout checks for source/unit cardinality, source validity,
missing-character type, gap count and width, temporal order and pause, and
write scope. The warning-clean build and all 69 CTest entries pass. Final T123
and T111 projector replays are separately byte-identical; T111 is unchanged,
and T123 splits only the bounded `ref-0432` source.

Complete chronological and reverse contextual semantic review of
`45:47-49:42` retains FR42. The bounded response island belongs to Zhu Jie;
the remaining `思？` is a non-independent ASR suffix whose zero-duration
alignment falls after the target VAD, so it remains visible under Tang rather
than being repainted. Only current T123 `ref-0432` advances the manual ledger,
to `516/556`; no new real-WebSocket result or closing claim follows. See
`isolated-vad-single-character-alignment-gap-diagnosis-2026-07-19.md` and
`isolated-vad-single-character-alignment-gap-review-2026-07-19.md`.

FR43 addresses the remaining critical T111/T123 partition regression at
`ref-0194` by extending only the retained complete-source aligned-VAD closure.
Both packages have identical activity and primary tracks. Complete-source and
containing-VAD session/robust views independently select Xu, while Tang covers
the aligned envelope and Xu enters at its terminal handoff. T111 already uses
the retained closure. T123 differs in two coupled producer representations:
one visible source character has a zero-duration aligned unit, and the short
phrase's agreed top identity is a third speaker with no local activity or
primary coverage. Within the only locally supported Tang/Xu pair, both raw
phrase score differences remain below the existing configured short margin.

The implementation will preserve the fully anchored and phrase-top-two path.
Its new path requires exactly one one-visible-character unanchored business
interval, one-character positive units, unique source-adjacent units around the
dropout, exactly one raw zero-duration unit on the missing character, a
temporally ordered sub-pause bridge contained by that interval, and an
otherwise exact source partition. The phrase fallback is available only
for this representation and requires session/robust agreement on the same
nonlocal top identity, unique candidate/incumbent scores tied under the
existing short margin, and absence of that top identity from local activity
and primary coverage. Every existing complete-source, outer/aligned duration,
dual-gallery, label, edge, activity, primary, coverage, phrase-abstention, and
containing-VAD gate remains unchanged. No TOML value, threshold, transcript,
identity, producer track, or common-clock coordinate changes.

Focused tests will preserve the existing positive and abstention matrix and
add independent checks for unanchored cardinality, source-character type,
unit cardinality and bounds, adjacent-unit uniqueness, temporal order, bridge
containment, configured pause, score-list completeness, local-pair margin,
nonlocal top agreement, and local activity/primary exclusion. A warning-clean
build and complete CTest pass establish engineering behavior only. The clean
projector will replay frozen T123 and T111 twice; automation may expose only
mechanical changes. Complete chronological and reverse contextual semantic
review against `test.txt` alone decides retention. See
`complete-source-local-pair-tie-diagnosis-2026-07-19.md`.

The independent frozen gate retains FR43. The warning-clean build and all 69
CTest entries pass. Final T123 replays are byte-identical at
`ed9744a6d8a4b9bfb0bfdffd56abe62f91c4ee14b38cb8a0de1c2952e6a02bcc`;
T111 replays are byte-identical and unchanged from FR42 at
`ad2abee782ab30ff67be1a86fa46f4ec0c16b5422ae18954107c398131157aa4`.
Mechanical display exposes only the complete T123 `text_id=113` merge.
Complete chronological and reverse reading of `20:07-22:56` retains Xu
Zijing's response between Shi Yi's prompt and Tang Yunfeng's explanation and
finds no neighboring change. Only current T123 `ref-0194` advances the manual
ledger to `517/556`, the 1200-1800 block to `75/80`, and Xu Zijing to `69/73`.
No new real-WebSocket result or closing claim follows. See
`complete-source-local-pair-tie-review-2026-07-19.md`.

FR44 diagnoses the remaining critical `ref-0071` T111/T123 regression as a
generic punctuation-phrase precedence error. T111 exposes a two-run Tang-Shi
phrase already protected by the retained native-handoff guard. T123 represents
the same exchange as three base runs Shi-Tang-Shi; its one-character Tang
middle run is surrounded by positive aligned Shi primary evidence. The phrase
crosses two ordered non-containing VAD records. Its session view is eligible;
the robust view has the same raw top identity and an eligible margin but falls
below the existing regular score gate. The generic session-only write therefore
replaces all three native runs with Tang.

The implementation will add one read-only predicate beside the retained
native-handoff guard. It will reconstruct exactly three base runs, validate the
single-character middle slot and adjacent aligned primary provenance, resolve
unique primary segments at the three character midpoints, and measure each
outer run only on its positive aligned intervals. It will require matching
activity/primary coverage using the existing primary-consensus duration. It
will independently rank the complete robust phrase scores and the two
overlapping VAD records using the existing gates. Only a session-eligible,
robust-score-abstaining phrase with ordered outer-to-middle VAD rankings may
return `true`. The existing phrase loop will then abstain before applying the
generic phrase write. No label, evidence record, boundary, producer, thread,
configuration value, or later specialized rule changes.

Focused coverage will preserve the two-run handoff case and the existing
outer-selected subminimum A-B-A case. A synthetic FR44 positive will be paired
with independent failures for duration, gallery completeness and ranking,
three-run identity/cardinality, adjacent alignment, primary topology and
duration, outer aligned/activity/primary coverage, and VAD count/order/
containment/ranking. Build and CTest results remain engineering evidence only.
The clean projector will replay frozen T123 and T111 twice, expose the complete
raw change scope, and stop before product judgment. Complete chronological and
reverse contextual reading of every changed conversation against `test.txt`
alone will retain or remove FR44. See
`three-run-middle-slot-phrase-abstention-diagnosis-2026-07-19.md`.

The independent frozen gate retains FR44. The warning-clean build and all 69
CTest entries pass. Final T123 replays are byte-identical at
`174319361040f648b4f930e312986e626f6b5cba9e3d8eaad9aeaa4a0bc7e7f1`;
T111 replays are byte-identical and unchanged from FR43 at
`ad2abee782ab30ff67be1a86fa46f4ec0c16b5422ae18954107c398131157aa4`.
Mechanical display exposes only T123 `text_id=43`. Complete chronological and
reverse reading of `07:42-08:33` retains the Shi-to-Tang-to-Shi-to-Tang
calculation sequence and finds no neighboring contribution change. Only
current T123 `ref-0071` advances the manual ledger to `518/556`, the 0-600
block to `88/93`, and Shi Yi to `199/211`. No new real-WebSocket result or
closing claim follows. See
`three-run-middle-slot-phrase-abstention-review-2026-07-19.md`.

The post-FR44 evidence pass separates two superficially similar alignment-gap
contexts before attempting another fusion rule. T123 omits the complete
`ref-0066` Tang source contribution, so the business projector has no content
coordinate it can assign. T123 retains the repeated `ref-0192` source, while
Zhu Jie's independent activity and primary island lies wholly inside a long
forced-alignment time gap. The current evidence stage never queries that exact
span, and its containing VAD mixes Zhu with Shi's following request. A
single-centroid diagnostic displays Zhu first on the exact island but cannot
establish the independent robust gallery.

Before FR45 can exist, extend the offline `speaker_identity_replay_probe` with
a capture-snapshot input mode. A mechanically exported TSV will group every
raw WebSocket `diar` snapshot by its original event index and carry start, end,
local slot, confidence, and optional captured identity. Snapshot mode will
append full audio only when the configured retention covers it, process each
snapshot in order after stripping captured identities, and support `-` for a
new empty `SpeakerDatabase`. Existing final-segment replay remains unchanged.
The tool will write the final replay view and requested dual-gallery evidence;
captured/replayed identity equality is only a producer-reproduction contract.

The implementation passes a warning-clean build and all `70/70` CTest entries.
The original MP3-float replay diverges only because its decoder does not match
the production client's FFmpeg PCM s16le path. Replaying the exact streamed PCM
twice reproduces all 1,254,049 captured identity values with no difference;
the final identity and dual-gallery artifacts are separately byte-identical.
The exact complete primary island has complete unique session and robust
galleries that independently select its middle primary identity under the
existing short-span gates. The VAD-intersected subset fails the robust margin,
and the mixed VAD and neighboring controls select the outer identity. Manual
component review therefore passes the evidence gate only for the complete
exact island. See
`primary-island-alignment-gap-evidence-diagnosis-2026-07-19.md`.

FR45 will first extend the reduced evidence snapshot with `primary_speaker` and
carry `session_gallery_complete` through the typed timeline and diagnostic JSON.
`SpeakerEvidenceStage` will derive punctuation phrases from configured
punctuation, reconstruct positive character times, and search for one exact
middle primary run inside one positive-character alignment gap. It emits a
`primary_alignment_gap_echo` query only when the next source-contiguous phrase
is a strict visible suffix of the phrase containing the gap, the two outer
primary boundaries are adjacent within the existing configured alignment
boundary tolerance, and the source/time mapping is unique. The query pairs that
following source range with the exact primary-island clock span. It neither
selects nor writes a speaker.

`SpeakerFusionPolicy` will independently reconstruct the phrase, alignment,
primary, and activity topology. It requires explicit complete equal identity
sets in both galleries, independent passage of the existing short score and
margin gates, agreement with the middle primary/activity identity, matching
full-duration activity, sufficiently long same-identity outer primary runs and
activity, and a target uniformly held by the outer identity under only ordinary
base/primary/direct-short provenance. Every ambiguous or incomplete condition
abstains independently. A final specialized pass rewrites only speaker fields
for that target phrase and leaves source, text, forced alignment, and all clock
bounds untouched. Focused tests will cover the producer positive and ambiguity
cases plus every fusion gate separately.

After a warning-clean complete build and CTest pass, the clean projector will
replay frozen T123 and T111 twice. Automation may display only deterministic
hashes and mechanical changed scope. Complete chronological and reverse reading
of every changed conversation against `test.txt` alone decides whether FR45 is
retained. No new real-WebSocket result, ledger change, or closure claim is made
before that review.

The independent frozen gate retains FR45. The final complete build has no
warning/error diagnostic and all `70/70` CTest entries pass. Repeated T123
outputs are byte-identical at
`5a595ca1aa5816612b2603062d8467ee60bc3a342219cf5eda066cfddc3bb61a`;
repeated T111 outputs are byte-identical and unchanged from FR44 at
`ad2abee782ab30ff67be1a86fa46f4ec0c16b5422ae18954107c398131157aa4`.
The complete T123 raw scope splits only `text_id=111`: `没有意见，` moves to
the middle-island identity while `赶紧说，快！` remains with the outer
identity. Complete chronological and reverse reading of `20:33-22:42` against
`test.txt` retains Zhu Jie's independent response between Shi Yi's answer and
request. Only current T123 `ref-0192` advances the manual frozen ledger to
`519/556`, the 1200-1800 block to `76/80`, and Zhu Jie to `77/83`. The
business output keeps existing forced-alignment times, so time-based gates
remain open. No new real-WebSocket result or closing claim follows. See
`primary-island-alignment-gap-echo-review-2026-07-19.md`.

FR46 stops the sequence of single-residual fusion patches and first establishes
a session-wide source-independent identity view. A temporary query list will be
derived mechanically from all 1,348 frozen T123 primary runs. Every row keeps
the primary run's exact clock interval and uses only an ordinal diagnostic ID;
no reference row, expected identity, correctness field, or selection score is
added. The existing capture-faithful replay will start from an empty registry,
consume the original 3,575 chronological diar snapshots over the exact streamed
PCM, and display explicit session/robust evidence for every primary query. The
previous 1,254,049-row producer-equality contract must still hold. Short and
unavailable spans remain visible because their absence can be as important as
an eligible score list.

In parallel, the already signed T123 error table will be manually reconciled
with each retained FR32-FR45 contextual decision. The resulting residual list
will be read against `test.txt`, first chronologically and then in reverse. For
every remaining critical contribution, raw displays will place the current
business output, ASR source, alignment units, activity, primary, VAD, and the
new primary-run dual-gallery evidence on the common clock. The reviewer will
manually record whether content is absent, alignment is displaced from an
independently corroborated island, galleries disagree, or the acoustic models
themselves support the wrong identity.

Only after that complete review may one shared source-free alignment policy be
specified. It must explain multiple residuals through the same topology, reuse
existing TOML boundaries unless a separately justified parameter is required,
and name independent accepted-context controls that force abstention. If the
evidence instead remains case-specific or producer-wrong, FR46 ends as a
diagnosis and no production fusion code or new real-WebSocket run follows.
Code may capture and arrange evidence but cannot perform any of the contextual
classification, candidate selection, or product judgment.

FR46 completes as an evidence-only stop. Complete forward and reverse review
of all 23 critical residuals finds only `ref-0099` with source, alignment,
activity, primary, VAD, and both identity galleries independently agreeing on
the correct speaker while final fusion overwrites it. Every other critical
context has a different upstream or evidence-completeness boundary. One
single-residual veto would not satisfy the FR46 shared-topology gate, so no
production fusion rule, TOML change, model change, audio run, or ledger change
is made. The next plan revision must examine complementary raw evidence from
the two orthogonal speaker models before authorizing another implementation.
See `session-wide-primary-residual-review-2026-07-19.md`.

FR47 moves one level upstream without changing either model. The diagnostic
input is the lossless WAV wrapper around T123's exact streamed PCM, not a new
MP3 decode. `diar_evidence_probe` will load the frozen T123 TOML and current
Sortformer v2.1 weights, run the model twice, and serialize every frame's four
sigmoid values with its absolute common-clock start and `0.08 s` period. A
mechanical replay will coalesce only threshold-eligible frame-wise top-1 runs
using the same existing `speaker_fusion.frame_activity_threshold` contract and
compare their local slot, bounds, and mean probability with the frozen T123
primary track. This validates evidence provenance only.

The frozen T123 diarization/identity history supplies the time-varying mapping
from each local Sortformer slot to its TitaNet global identity. Diagnostic
worksheets will copy raw frame rows intersecting each of the 23 FR46 critical
contexts and the accepted controls named by the FR46 review. They will display,
without derived correctness fields, ASR source, forced-alignment units,
activity segments, primary runs, VAD spans, all four raw posterior channels,
session and robust TitaNet score lists, and final business entries. No weighted
combination, automated channel summary, result-dependent threshold search, or
candidate output belongs in this capture phase.

The reviewer will then traverse every worksheet forward and backward against
`test.txt`. Manual notes will distinguish true overlap evidence from low-level
inactive probability, temporal displacement, identity-map ambiguity, and
shared-model TitaNet agreement. Prior FR16ZU and FR16AAL artifacts are controls:
FR16ZU's session-wide strict secondary-channel rule was a no-op, while FR16AAL
retained one bounded edge closure. FR47 may not broaden either historical rule
without new multi-residual evidence and explicit accepted-context abstention
controls. Only after the complete review may spec/plan/tasks be revised for a
production implementation; otherwise FR47 ends as an upstream evidence
diagnosis.

T191 completes the capture prerequisite. Two exact-PCM runs emit byte-identical
45,189-row four-channel frame tables. Applying only the frozen `0.5` top-1
compression reproduces all 1,348 frozen T123 primary runs in the same order and
local slots; serialized bounds and means differ by no more than one unit in the
sixth decimal place. This is producer provenance, not speaker accuracy. T192
may now arrange this raw posterior beside the frozen typed tracks. See
`sortformer-v21-exact-pcm-posterior-capture-2026-07-19.md`.

T192 is complete. Twenty-three immutable context directories now display every
raw posterior frame beside all six frozen upstream track kinds, the current
business view, intersecting identity epochs, and the complete selected
reference entries. Every FR46 accepted neighboring control is included. The
content manifest covers 141 files and no file is empty. This is evidence
availability only; T193 is the first phase permitted to relate it to product
correctness. See
`sortformer-v21-orthogonal-evidence-capture-2026-07-19.md`.

T193 completes the manual two-direction review. Most raw secondary evidence is
either contradicted by TitaNet, attached to missing/displaced source, or also
present in accepted controls. A broad primary preference similarly cannot
combine the fully corroborated `ref-0099` overwrite with the subminimum,
otherwise contradicted `ref-0327` island. Those paths stop.

T194 authorizes one narrower frozen experiment for `ref-0507` and
`ref-0509`. `ComprehensiveTimeline` already owns each immutable Sortformer
frame block, so the business pipeline will snapshot that existing track rather
than add a producer callback or duplicate model execution. For each exact
punctuation phrase and then each exact positive-duration aligned unit, a pure
policy helper will consider each reset-aware local slot. One slot is eligible
only when it is uniquely top-1 or top-2 on every intersecting frame, has no
rank tie, and crosses the existing `speaker_fusion.frame_activity_threshold`
on at least one frame. Its first later different identity epoch must start
within `[speaker].local_drift_competing_backfill_sec`, satisfy the existing
`speaker_fusion.min_embed_sec`, and have matching primary support for at least
that duration. The current exact source range must have one uniform different
known identity. Missing frames, multiple slots, current/future identity
ambiguity, absent primary support, mixed/protected source labels, or an
already-matching identity preserves the current view.

The experiment adds one false-by-default typed boolean under
`[speaker_fusion]`; the checked-in candidate profile may enable it. It adds no
numeric TOML field and reuses the existing frame gate, identity horizon,
duration, phrase, punctuation, and alignment contracts. The rejected generic
`future_epoch_lookahead_sec` path remains disabled and is not widened.
`business_speaker_replay_probe` will accept an optional frozen frame CSV and
deposit it through the production typed timeline API. Two replays must be
byte-identical. Automation may list raw changed entries and build review
packets only. Every changed complete conversation is then read forward and in
reverse against `test.txt`; only a manually retained candidate may receive a
complete 556-contribution review or a later real-WebSocket ladder. See
`sortformer-v21-orthogonal-context-review-2026-07-19.md`.

The first implementation omitted a source-ownership boundary. Its repeated
frozen output is byte-identical, but the raw changed scope reaches sixteen ASR
finals in four pre-epoch windows. Complete forward and reverse semantic review
rejects every cross-source window: the delayed epoch is later evidence from a
different finalized source and flattens real intervening Zhu, Tang, Xu, and
Shi turns. Source `283` is structurally different because the `3330.640`
identity epoch and its matching primary support occur before that same ASR
final ends at `3338.244`.

The revised candidate will therefore clip future-epoch duration and primary
coverage to `[future_epoch.start, text.end]` and abstain unless both still meet
the existing `speaker_fusion.min_embed_sec`. It will also require the epoch to
start before `text.end`. All posterior, local-slot, alignment, identity, and
uniform-incumbent contracts remain unchanged. This prevents a final view from
using one source record's future evidence to rewrite another source record,
adds no numerical field, and follows the origin-process-result ownership rule.
The revised candidate must repeat the complete build, deterministic replay,
raw-scope inspection, and changed-context forward/reverse review before it can
advance. See
`sortformer-v21-posterior-future-epoch-candidate-review-2026-07-19.md`.

The source-bounded retry completes those gates. The warning-clean build and all
`70/70` CTest entries pass. Repeated candidate output is byte-identical, the
boolean-disabled control is independently byte-identical, and the revised
reason occurs only on two ranges inside source `283`. Complete changed-context
review retains Tang Yunfeng's `ref-0507` expectation and first `ref-0509`
confirmation without changing Shi Yi's intervening strategy explanation or
accepted neighboring controls.

T200 then completes a fresh two-direction read of the entire frozen candidate,
not an extrapolation from changed rows. All 556 `test.txt` contributions are
read chronologically and again in reverse fixed windows. This review initially
transcribes `521/556`. FR48 later corrects the speaker-only boundary at
`ref-0375`, whose ASR wording is wrong but whose canonical speaker is correct.
The current ledger is `522/556`, with 34 residuals: 28 confident-wrong, five
missing, and one uncertain. Twenty remain business-critical. No new
contextual failure is found, so the frozen candidate is retained; this does not
convert it into real-path or closing evidence. See
`post-fr47-residual-reconciliation-2026-07-19.md`.

The next promotion uses one committed revision and the checked-in TOML only.
First run two independent 120-second real-WebSocket streams and complete all
mechanical checks plus the full in-scope forward/reverse context review. Only
if both pass, run one 600-second stream and repeat those gates. Only if that
passes, run a full empty-registry session followed by a full session using the
first run's frozen registry. Each full result receives its own complete 556-
contribution chronological and reverse contextual review. A failure stops the
ladder for diagnosis; hashes and output equality cannot make the product
decision.

The first 600-second attempt stops at that failure boundary. Server-side
session persistence proves finalization completed, while the persisted
`speaker_voiceprint` JSON contains truncated `false` and `true` tokens at the
former 256-byte scratch-buffer boundary. The client reader consequently has no
parseable terminal message. Preserve the malformed persisted session, process
state, and client timeout as mechanical evidence; do not generate a review
packet or infer any speaker result from it.

Repair the serializer at its ownership boundary. Move one complete
speaker-voiceprint record into the shared JSON serialization module. Assemble
escaped variable-length identifiers directly into `std::string`; append only
numeric and boolean fragments with a dynamically sized `vsnprintf` helper; and
serialize score identifiers without a fixed buffer. Add a unit regression with
identifiers longer than 256 bytes and an exact expected record, so truncation,
missing commas, incomplete booleans, and lost suffixes fail deterministically.
No runtime dependency or configuration field is added.

After focused and complete engineering tests pass, commit and push the fix.
Since the server binary changes, discard the old promotion ordering as release
evidence and restart it on that clean commit: independent empty-registry 120 A
and 120 B, complete forward/reverse review, then 600 seconds, then full A/B only
after the preceding gate passes. Mechanical JSON parsing and terminal equality
remain protocol checks, never product evaluation.

That restarted ladder is complete on clean pushed commit `70f1186`. Both
120-second paths pass their mechanical contracts and complete in-scope
two-direction context review before the 600-second path begins. The 600-second
path then passes the same gates and authorizes full A/B. Full Run A uses an
isolated empty registry; Run B restarts the same binary with Run A's frozen
registry. Both exact 3615.120-second streams close every product track,
converge all observer terminals, retain the frozen registry, and satisfy the
direct-end and telemetry contracts.

Each full artifact is reviewed independently across all 556 human-listened
`test.txt` contributions in chronological and reverse fixed-window order.
FR48's fresh speaker-only reread corrects only `ref-0375` in the initial FR47
transcription. The manually signed result in each path is `522/556`, including
20 critical residuals; no executable evaluation contributes a label, total, or
decision. FR47 is therefore promoted from frozen-only evidence to the current
repeatable real-path candidate. The next phase is residual evidence work, not
parameter search: diagnose the remaining critical contexts by common source
topology and independently accepted controls, and specify a bounded policy only
where the complete context establishes reusable, reference-free evidence.
Speaker-time, holdout, report, and release work remain separate later gates.
See `fr47-real-path-promotion-review-2026-07-19.md`.

### 8.35 FR48 speaker-only reconciliation and evidence precedence

FR48 first corrects the evaluation boundary without changing the product. The
reviewer will reread each full A and B artifact across all 556 `test.txt`
contributions chronologically and in reverse fixed windows. Speaker ownership
and ASR semantics are recorded separately. Wrong wording under the correct
speaker remains an ASR defect but is not by itself a speaker-attribution
failure; an absent business contribution or wrong material ownership remains a
speaker-business failure. The reviewer manually reconciles and cross-checks
every row and total. Automation continues to provide only immutable display.

In parallel, extract direct typed replay inputs from both exact FR47 artifacts.
The source-free inventory begins at ordinary `voiceprint_direct_short`
business intervals where the selected identity differs from a uniform current
identity. It then displays activity, primary, containing VAD, complete-source,
exact interval, and robust/session gallery evidence for every structural match.
Reference labels are introduced only when the reviewer reads every resulting
complete conversation and its accepted controls forward and reverse.

The candidate is a precedence guard, not another identity rewrite. Before an
ordinary short business-interval session-gallery result is applied, preserve
the current label when all of the following hold:

1. exact direct session and robust galleries do not already form an eligible
   dual-gallery decision for the same different identity;
2. the current source label is uniform and its identity covers the exact range
   completely and uncontestedly in both activity and primary tracks;
3. exactly one containing VAD query has complete session and robust galleries,
   both uniquely rank the current identity first under existing margin gates;
4. exactly one complete-source query for the same text has complete session and
   robust galleries, both uniquely rank that same current identity first under
   the existing gates;
5. every range and text ID is exact on the common clock.

No score, duration, or tolerance is fitted. The candidate reuses the existing
TOML numerical fields and adds only a false-by-default
`direct_short_native_consensus_guard_enable` switch. Focused tests cover the
positive topology and each abstention boundary. Candidate and disabled-control
replays run twice against both full typed inputs. Raw diffs define review scope
but never correctness. Complete changed-context review decides retention; a
retained result still requires fresh complete 556-contribution A and B reviews
under the speaker-only rubric before an audio ladder can be authorized.

Execution takes the stop branch. Both full A and B artifacts are read across
all 556 contributions forward and reverse under the speaker-only rubric. The
only ledger correction is `ref-0375`, producing the manually transcribed
`522/556` result with 20 critical residuals. Direct replay input export is
repaired to preserve `session_gallery_complete`; both baseline replays then
contain all 1,716 business entries and are byte-identical. Complete contextual
review of all 24 displayed direct-short/native conflicts finds many accepted
direct repairs and only one material context (`ref-0099`) satisfying the full
proposed hierarchy. A single-context fit cannot authorize product policy, so
the boolean, helper, candidate replay, TOML change, and audio run are not
implemented. The focused exporter suite passes `3/3`, the full build emits no
warning or error diagnostic, and all `70/70` CTest entries pass in `53.12`
seconds; these checks establish engineering consistency only. See
`fr48-speaker-only-reconciliation-and-consensus-diagnosis-2026-07-19.md`.

### 8.36 FR49 full-residual short-evidence reconstruction

FR49 keeps the exact FR47 full A/B timelines, their repaired 1,716-entry
baseline replays, the capture-faithful four-channel Sortformer v2.1 posterior,
and the checked-in TOML fixed. The first pass reconciles every one of the 34
remaining manually signed speaker residuals with all producer and identity
evidence on the common clock. It includes noncritical rows so a reusable
topology cannot be mistaken for a critical-reference special case. Each
conversation and its accepted controls is read forward and reverse; no
executable component groups or judges the findings.

The mechanical pass then emits one source-free display for all short primary
runs that touch a positive aligned unit, a zero-duration unit, or one temporal
gap between adjacent units and conflict with at least one mechanically
associated current business piece. Every row also carries immediate same-
source and adjacent primary/business controls and preserves the original text
ID, source bounds, primary interval, intersecting alignment, raw posterior
frames, activity, VAD, voiceprint completeness and raw score lists, current
business pieces, and identity epochs. The display contains no reference file,
expected identity, correctness field, aggregate, candidate score, or ranking.
A and B must expose the same structural inventory before contextual work
continues.

The reviewer reads every displayed conversation in both directions against
`test.txt`, including all apparently correct controls. Only a manually
established topology shared by at least two independent material contexts may
advance. Source-absent speech cannot be repaired by manufacturing text or a
source coordinate. A retained design must use existing TOML-owned gates and
typed evidence, preserve all producer tracks and clock values, and define
independent abstention for positive, zero-duration, source-gap, overlap, and
neighboring-control representations. If those conditions are not met, FR49
records the evidence boundary and stops without implementation or audio.

If and only if the manual gate passes, update spec, plan, and tasks with one
bounded false-by-default behavior switch before editing runtime code. Add
focused positive and independent abstention tests, replay both frozen A and B
inputs twice with a disabled control, expose raw changed scope mechanically,
then read every changed complete context forward and reverse. A retained
candidate still requires independent complete 556-contribution A/B
speaker-only reviews before any real-WebSocket ladder can be authorized.

The gate passed after complete review of all 271 A/B inventory rows and their
controls. `ref-0061` and the newly discovered `ref-0121` residual are two
independent material contexts with the same source-free topology: a
TitaNet-corroborated short primary/activity identity owns a complete leading
business source interval, a positive alignment gap right-bounds that interval,
and a contiguous longer primary/activity identity owns the source-adjacent
continuation that currently absorbed the prefix. Both use the existing
`0.4 s` embedding floor, `1.5 s` short ceiling, `0.25 s` alignment-pause gate,
`0.08 s` boundary tolerance, and configured voiceprint score/margin gates.

Implementation adds `primary_run:N` queries to `SpeakerEvidenceStage` only
when the new `speaker_fusion.source_leading_primary_prefix_enable` switch is
enabled. Query ordinals follow the sorted immutable primary track; they carry
no text ID, source range, or product label. `SpeakerFusionPolicy` then performs
the full conjunction specified by FR49 after existing fusion writes and before
final source reconstruction. It restores only the exact leading
`business_interval` source range and records a dedicated reason and evidence
source. The default in typed C++ configuration is `false`; the checked-in TOML
is the sole production opt-in. See
`fr49-source-leading-primary-prefix-diagnosis-2026-07-19.md`.

FR49 now enters the same staged real-path discipline on the pushed
transitional implementation. First, commit and push this promotion
authorization so every capture identifies one clean source revision. Preserve
the existing `/tmp/orator/storage` tree outside the validation workspace. For
each 120-second run, create a fresh empty path at the exact checked-in TOML
location, start a new production server process with no behavioral environment
or command-line override, stream the first 120 seconds of `test.mp3` at `1.0x`
in 100 ms frames through `ws_unified_test.py`, send direct `end`, and archive
the resulting isolated storage beside the capture and manifest. Stop every
server and device probe before starting the next run.

Mechanical review checks only parseability, exact input/provenance identity,
all seven terminal extents, common-clock reconciliation, producer/observer
convergence, direct-end latency, telemetry coverage/cadence, and raw repeated
scope. The reviewer then reads every in-scope `test.txt` contribution in full
conversation order and again in reverse fixed windows for each artifact. A
hash or normalized equality cannot authorize the next stage. If both 120-
second contextual reviews retain FR49, repeat the process once for 600 seconds
and review every in-scope contribution in both directions. Only that manual
gate authorizes full Run A from an empty registry and restarted full Run B from
Run A's frozen registry. Each full result receives its own two complete 556-
contribution readings. Restore the preserved business storage when the ladder
stops or completes, and record all evidence in an FR49 real-path promotion
review without converting any mechanical observation into a product verdict.

The first frozen candidate replay is an implementation gate, not an accepted
result. Its mechanical changed-scope display exposes a third write at
`2177.260-2177.500`. Complete context against `test.txt` shows that 唐云峰 owns
the whole following contribution, while the draft policy extends 徐子景's
immediately preceding contribution into its first two source characters. The
policy therefore also requires that no earlier positive-duration source
character inside the same short primary already retain the candidate identity.
The preceding 石一 source character for `ref-0121` ends before its candidate
primary and remains admissible. This uses no new numeric setting and rejects a
candidate-tail leak while retaining the two independently authorized
source-leading contexts.

The corrected frozen replay completes the authorized evidence ladder. Two A
and two B candidate replays are byte-identical with 1,718 business entries at
SHA-256
`91e1e7ab08f6c593b73762b158b5c4ee9c58eaf68ea59eb8f9ee34c21f747c30`.
The independently generated disabled A/B controls contain 1,716 entries, are
byte-identical at SHA-256
`75fc0b39fdf4530ec98a54f8e6ac113e8eef1aee00839c3d9c6577adafb8302e`,
and exactly reproduce FR48. The raw changed scope is limited to the existing
`467.564-467.644` and `817.692-818.412` source prefixes; no other text, source
index, time, or speaker label changes.

Complete contextual review first confirms both changed conversations and the
`ref-0304` abstention. Candidate A is then read across all 556 contributions in
chronological and reverse fixed-window order; candidate B is independently read
the same way. The four complete readings agree. Only `ref-0061` and
`ref-0121` move from confident-wrong to accepted relative to the corrected
`521/556` pre-candidate ledger. The manually transcribed frozen result is
`523/556`, with 27 confident-wrong, five missing, one uncertain, and the same 20
critical residuals. FR49 is retained as a frozen policy candidate, not a new
real-WebSocket or closing result. The `--clean-first` build has no warning or
error diagnostic, all `71/71` CTest entries pass in `53.22 s`, and both new
Python files pass syntax compilation. These checks establish engineering
consistency only. See
`fr49-source-leading-primary-prefix-diagnosis-2026-07-19.md`.

The authorized real-path ladder is complete on clean pushed commit `1f09052`.
Two independent restarted 120-second runs pass the mechanical gate, then
receive separate complete forward/reverse contextual reading of `ref-0001`
through `ref-0018`; both retain FR49 without a new regression. The following
600-second run passes mechanically, and complete two-direction reading of all
93 in-scope contributions manually records `89/93`, confirms the `ref-0061`
repair, and authorizes full A/B. No executable result makes either promotion
decision.

Full A starts with empty isolated storage. Full B restarts the same binary with
only A's frozen registry. Both consume the exact 57,841,920 samples, close all
seven tracks on the common time base, converge producer and observers, and
provide complete required telemetry. Direct-end waits of `29.015 s` and
`28.820 s` pass the 30-second gate with limited margin. A is then read across
all 556 contributions chronologically and in reverse fixed windows; B receives
the same two independent complete readings. The manually signed real-path
ledger remains `523/556`, with 27 confident-wrong, five missing, one uncertain,
and the same 20 critical residuals. The reviewer finds no new attribution
regression, whole-session identity permutation, accumulating late drift, or
tail-only collapse. FR49 is promoted to the current repeatable real-WebSocket
candidate but does not close the speaker business. Isolated storage is
archived, the pre-existing business storage is restored, and no validation
process remains. See
`fr49-real-path-promotion-review-2026-07-20.md`.

### 8.15 FR28 120-second outcome and promotion ladder

T117-T121 are complete. The frozen T116 packages replay byte-stably; their
first business divergence traces from scheduling-sensitive ASR `text_id=49`
through alignment and voiceprint evidence into `ref-0102`. FR28 replaces that
path with typed VAD frontiers and an ASR-owned pending buffer. The warning-clean
build, VAD numerical oracle, and all 69 CTest entries pass.

Two independent 120-second production real-WebSocket captures then produce
identical canonical entries for all seven product tracks. Complete forward and
reverse contextual review of `ref-0001` through `ref-0018` finds no natural-turn
speaker regression. It also confirms that the pre-reference `0-3 s` transcript
is removed while existing cold-start identity and short-interjection defects
remain visible. FR28 therefore advances to a clean 600-second gate; this does
not establish full-session accuracy or change the frozen T111 comparison. See
`vad-gated-asr-stability-review-2026-07-18.md`.

The retained code is committed before the next run so every longer artifact is
bound to one clean source revision. One 600-second run must pass direct-end,
common-clock, telemetry, and complete in-scope forward/reverse context review.
Only then may the same revision run a full empty-registry capture followed by a
restarted frozen-registry capture. Each full artifact receives its own complete
556-contribution chronological and reversed-block semantic review. No runtime
or replay tool may convert structural equality into a product decision.

## 9. Phase 7: Final Sign-Off

Create one closing report containing:

- all reproducibility manifests and artifact hashes;
- oracle results and tolerances;
- automated test results;
- the complete signed turn ledger and contextual judgments;
- all required speaker, ASR, boundary, latency, resource, and stability results;
- both full-run comparisons;
- known limitations and the exact permitted product claim.

The project is marked closed only when there are no open priority-zero or
priority-one defects, every conjunctive gate passes, `PROJECT_STATE.md` matches
the accepted commit, and the final report is reviewed. A release tag is created
only after that review.

## 10. Rollback and Experiment Discipline

- One hypothesis family is changed at a time; the TOML diff and expected effect
  are recorded before execution.
- An experiment that fails a lower test level does not progress to full length.
- Accuracy regression in any fixed block rejects the candidate even if the full
  average rises.
- Failed runtime behavior is reverted with a normal revert commit; destructive
  history rewriting is not used.
- Frozen raw evidence and the signed reference ledger are never rewritten by a
  candidate tool.
