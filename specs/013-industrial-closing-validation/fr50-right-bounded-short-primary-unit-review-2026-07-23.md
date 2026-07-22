# FR50 Right-Bounded Short-Primary Unit Review (2026-07-23)

## Scope and authority

This record covers T229-T231 for the FR50 right-bounded short-primary aligned-
unit experiment. It covers the production-policy implementation, mechanical
contract tests, exact frozen FR49 A/B replay, and four complete contextual-
semantic readings of the resulting candidate.

`test/data/reference/test.txt` is the authoritative human-listened reference.
No compiled code, script, query, formula, notebook, metric, hash, equality
check, or algorithm assigned correctness, counted or aggregated a product
result, ranked or selected a candidate, or issued the decision below.
Automation only built the code, exercised structural contracts, reproduced
frozen inputs, checked deterministic bytes and provenance, and arranged raw
evidence for reading. Every correctness class, ledger entry, and retention
decision below was manually transcribed after reading the complete conversation
against `test.txt`.

This review retains the experiment for the T232 production-WebSocket ladder.
It does not replace FR49's real-path ledger, close the speaker business, or
authorize any ASR-accuracy claim.

## Frozen implementation and inputs

| Item | Frozen value |
|---|---|
| Implementation base commit | `0a3249e983632363ed2b30cf1a70b432f3ea4fdb` |
| Candidate source | The transitional T229 change recorded with this review |
| `build/business_speaker_replay_probe` SHA-256 | `44c4770c7e4f0aa87e3b0c4e4f86b6bfc65a0f3236b4c9641c088eacab1f2a9c` |
| Candidate `orator.toml` SHA-256 | `d00150ae376d802af0fcf8c0a89aa3fae1e0abb2bf5d10601c55cefd570a40db` |
| `test.txt` SHA-256 | `35e8695057be82f3028877f7dc159f10ecfa0ab7f06c444cefa9a079b0e24a86` |
| Full A source timeline SHA-256 | `64abe31baf51185b685c91a58529096b25d281540afe21c3bbc2354cffb5432e` |
| Full B source timeline SHA-256 | `0ac66dbfc7dd95d21fcb271ad3b3a020d79c565b4c62ccc0a97f5e9a14f63813` |
| Frozen input extent | 57,841,920 samples at 16,000 Hz (`3615.120 s`) |
| Diarizer evidence | Streaming Sortformer v2.1 from the FR49 real-path artifacts |
| Candidate switch | `speaker_fusion.right_bounded_short_primary_unit_enable = true` |

The A and B input manifests each expose 755 activity-diarization intervals,
1,348 primary runs, 308 ASR sources, 13,327 forced-alignment units, and 17,452
voiceprint records. The corresponding per-track TSV hashes agree between A and
B. These values are provenance inventory only and do not evaluate speaker
correctness.

## Production policy

The C++ default is `false`; only checked-in `orator.toml` enables the
experiment. Configuration loading, resolved-config serialization,
`AuditoryStream` wiring, the production `BusinessSpeakerPipeline`, and the
frozen replay probe all use the same typed boolean. No numerical parameter was
added.

The policy records source-complete aligned units on the existing common time
base and may rewrite exactly one lexical codepoint plus immediately trailing
configured punctuation. It requires all of the following frozen structure:

- one unique short primary run for candidate B within the source;
- candidate duration within the existing primary-consensus and short-span
  limits, with at most one-sample clock serialization tolerance;
- no thresholded B activity and no usable `primary_run` embedding for B;
- one contiguous, sufficiently long following primary-A run fully covered by
  thresholded activity A;
- one unique, source-complete aligned codepoint wholly inside B, or one unique
  zero-duration point strictly inside B with only the exact crossing A
  successor permitted;
- a positive-duration aligned unit strictly longer than the existing
  `align_boundary_split_tolerance_sec`;
- an incumbent A label produced uniformly by `voiceprint_direct_regular`, with
  no already-retained B or third-identity evidence in the writable range.

The retained result emits reason
`primary_speaker_right_bounded_aligned_unit_restore` and source diagnostic
`sortformer_primary_top1+thresholded_activity_absence+following_activity+speaker_identity_epoch+forced_alignment`.
All raw producer tracks, source text, alignment times, identity epochs, and
voiceprint evidence remain immutable.

Focused tests cover positive-duration and zero-duration retention, the
false-default switch, source preservation, subminimum and overlapping primary
runs, B or third-identity activity, missing/gapped/short/duplicate continuation,
missing/partial/boundary-scale/duplicate/multi-codepoint alignment, boundary
zero points, usable candidate embeddings, nonregular incumbents, and already-
retained candidates.

The final full build emits no warning or error diagnostic. All `72/72` CTest
entries pass in `53.56 s`. These are engineering and mechanical-contract gates
only; they do not contribute to the product ledger.

## Boundary discovered by the first replay

The first broader implementation also reassigned the `对` at
`2241.356-2241.436 s` from Tang Yunfeng to Shi Yi. Complete `test.txt` context
shows that codepoint belongs to Tang's uninterrupted explanation, so the
reviewer rejected that behavior as a contextual regression. The aligned unit
is exactly `0.08 s`, equal to the existing alignment boundary-split tolerance.

The final candidate therefore requires every positive-duration target to be
strictly longer than that existing tolerance. It still admits the `0.16 s`
positive target used by `ref-0417`, while `ref-0327` uses the separately
specified unique zero-duration-point form. The accepted Tang codepoint at
`2241.356-2241.436 s` is an explicit abstention control. This boundary was
chosen from complete conversational reading; no executable comparison selected
it.

## Frozen replay evidence

Artifacts are retained at
`/tmp/orator-spec013/release-0a3249e-fr50-bounded/` on the validation host.

| Artifact | SHA-256 |
|---|---|
| Full A candidate | `d36227140dc8fcba9e40f116946ed8f8222e1b0aad73777665478a0f9f14d88c` |
| Full B candidate | `d36227140dc8fcba9e40f116946ed8f8222e1b0aad73777665478a0f9f14d88c` |
| Full A candidate repeat | `d36227140dc8fcba9e40f116946ed8f8222e1b0aad73777665478a0f9f14d88c` |
| Full B candidate repeat | `d36227140dc8fcba9e40f116946ed8f8222e1b0aad73777665478a0f9f14d88c` |
| Full A baseline control | `814dc276749bec2f652df896aa662811baf99de3361c607ef6b341ad8700ac32` |
| Full B baseline control | `814dc276749bec2f652df896aa662811baf99de3361c607ef6b341ad8700ac32` |

A/B and their repeats are byte-identical because their frozen typed producer
inputs agree. This establishes deterministic projection only. It does not
establish correctness or supply the retention decision.

The display-only changed-context packets expose `ref-0327`, `ref-0417`, and
the overlapping substantive `ref-0418` context. The final candidate changes
the `对，` at `2344.060-2344.780 s` to `spk_3`, and the `嗯，` at
`2804.396-2804.556 s` to `spk_1`; it leaves the following `ref-0418`
substantive statement with `spk_3`. The packet contains raw comparison evidence
and no correctness field.

## Complete contextual-semantic review

The speaker mapping is `spk_0 = 朱杰`, `spk_1 = 唐云峰`, `spk_2 = 徐子景`,
and `spk_3 = 石一`. Each candidate artifact was read against all 556 complete
reference contexts in windows `0-600`, `600-1200`, `1200-1800`, `1800-2400`,
`2400-3000`, `3000-3600`, and `3600-3615.12`, first chronologically and then
again from the final fixed window to the first.

| Independent complete reading | Display packet SHA-256 | Manual result |
|---|---|---|
| Full A chronological | `5c6205b360ea09361ea1a03726729a6530610ab2cd169621cabef11e8bd86d69` | Retain |
| Full A reverse windows | `14b66c0e0e590f60898d142fbbd9beff174549013719b25ff0b48371dcf8715a` | Retain |
| Full B chronological | `8aa85b6ebb02367709801c7113e41c50bad857b83a169ea834dc8f46b8214ad0` | Retain |
| Full B reverse windows | `4f84932ff9ba0934476845ae1dc2182a7487141896fda5c3c53d9ccf6edba95c` | Retain |

All four readings independently agree on the product interpretation:

- `ref-0327` is Shi Yi's short response; the candidate repairs the prior Tang
  attribution without changing the following Tang continuation;
- `ref-0417` is Tang Yunfeng's short response; the candidate restores the
  previously missing contribution;
- `ref-0418` remains Shi Yi's substantive statement;
- the named FR50 controls, including `ref-0326`, `ref-0328`, `ref-0329`,
  `ref-0413` through `ref-0422`, and `2241.356-2241.436 s`, remain
  contextually correct;
- no other attribution regression, whole-session identity permutation,
  accumulating late drift, or tail-only collapse is observed.

The four readings manually reconcile the frozen candidate at `525/556`, with
26 confident-wrong contributions, four missing contributions, one uncertain
contribution, and 19 business-critical residuals. This ledger was transcribed
from the complete contextual judgments; it was not calculated by the replay or
worksheet tools.

The missing contributions are `ref-0063`, `ref-0341`, `ref-0409`, and
`ref-0442`; `ref-0506` remains uncertain. The 19 critical residuals are
`ref-0049`, `ref-0058`, `ref-0066`, `ref-0099`, `ref-0102`, `ref-0118`,
`ref-0252`, `ref-0313`, `ref-0331`, `ref-0333`, `ref-0354`, `ref-0390`,
`ref-0426`, `ref-0442`, `ref-0444`, `ref-0461`, `ref-0499`, `ref-0503`, and
`ref-0505`. The remaining noncritical confident-wrong contributions are
`ref-0135`, `ref-0171`, `ref-0221`, `ref-0239`, `ref-0241`, `ref-0298`,
`ref-0457`, and `ref-0537`.

## Decision and next gate

T231 retains the exact candidate and authorizes T232. The manually reviewed
frozen candidate ledger is `525/556`; the current production-WebSocket ledger
remains FR49's `523/556` until the same source revision, binary, TOML, and
registry sequence complete the real 120-second, 600-second, full empty-registry
A, and restarted frozen-registry B ladder with contextual-semantic review at
every gate.

No `test.mp3` live run has occurred for this candidate yet. Speaker-time,
per-speaker time, source-time offsets, all remaining critical-attribution
requirements, locked holdout, ASR accuracy, browser/microphone acceptance,
final report, review, and release tag remain open.
