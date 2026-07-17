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

## 4. Phase 2: Build the Full Reference Turn Ledger

The 556 timestamped reference entries are reviewed against the complete audio.
For each entry, the reviewer records:

- stable reference ID and source line;
- real speaker and contextual summary;
- audible start/end on the absolute sample clock;
- overlap participants and intervals;
- critical/non-critical classification decided before candidate output review;
- acceptable semantic equivalents;
- any reference ambiguity.

Review occurs in two complete passes: chronological and by reversed 600-second
block order. Disagreements are resolved by replaying the audio in context. No
reference row is removed to simplify scoring. Empty, duplicate, or ambiguous
source rows remain in an adjudication log.

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
the audible ledger and required breakdowns remain unsigned.

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
4. create and manually sign the audible reference ledger for all 556 rows,
   including start/end boundaries, overlap, criticality, and confidence class;
5. derive speaker-time, fixed-block, per-speaker, critical-turn, confident-
   wrong, and boundary-offset judgments only by complete contextual semantic
   review of that signed ledger;
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
