# FR43 Complete-Source Local-Pair Tie Diagnosis (2026-07-19)

## Scope and authority

This diagnosis follows retained FR42 and examines the frozen T111/T123
partition regression at critical `ref-0194`. The immutable producer packages,
checked-in `orator.toml`, and human-listened `test.txt` are unchanged. No code,
script, query, formula, metric, score operation, or interval operation assigns
correctness, aggregates accuracy, ranks or selects FR43, or issues a product
verdict. Tools only display typed evidence on the common clock and verify
mechanical provenance.

The speaker identity map is `spk_0=Zhu Jie`, `spk_1=Tang Yunfeng`,
`spk_2=Xu Zijing`, and `spk_3=Shi Yi`.

## Complete conversational context

The complete `20:07-22:56` conversation was read chronologically against
`test.txt` and then read again from the later valuation discussion back to the
decision exchange. Tang Yunfeng asks whether the proposed allocation is
acceptable. Shi Yi and Zhu Jie state that they have no objection. Shi then
asks again for any remaining objection. Xu Zijing answers that Tang's share is
still a little low. Tang resumes immediately with `reasonable` and explains
his own objective. The reference contribution at `21:53` is therefore Xu's
bounded response between Shi's prompt and Tang's continuation.

This reading fixes the diagnosis scope. It does not retain an implementation
that has not yet been replayed and reviewed.

## Frozen output difference

T111 and T123 use byte-identical diarization and primary-speaker tracks. The
current production projector produces these mechanically different views:

```text
T111  1313.160-1314.200  那老有点少，还有点少。  spk_2
      voiceprint_complete_source_aligned_vad_closure

T123  1313.196-1313.957985  那老有点小，还有  spk_1
      1313.957985-1314.156  点              spk_1
      1314.156-1314.236     小。            spk_2
```

The T111 complete source already enters the retained complete-source aligned-
VAD closure. T123 preserves only a terminal Xu fragment and leaves the
material response under Tang.

## Common-clock producer evidence

Both packages expose the same overlapping activity:

- Tang: `1313.039970651-1314.559970617`;
- Xu: `1313.759970635-1314.799970612`.

Both expose Tang primary through `1314.159970626`, followed by Xu primary at
`1314.159970626-1314.319970623` and again from `1314.399970621`. The only two
identities with activity or primary coverage across the aligned response
envelope are Tang and Xu.

The containing VAD is `1313.156-1314.524`. Its session scores are
`spk_2=0.316634387`, `spk_3=0.216433436`, `spk_1=0.198198646`, and
`spk_0=0.173806906`; its robust scores are `spk_2=0.303246528`,
`spk_3=0.204491735`, `spk_1=0.194867373`, and `spk_0=0.184727982`.
Both typed views independently satisfy the existing short score and margin
gates for Xu.

T123 complete-source evidence covers source `0-11` and the padded outer
interval `1312.956-1315.524`. Its session and robust views both select Xu. The
same scores satisfy the existing short gates over the typed aligned envelope,
while the padded outer duration follows the existing regular-score abstention
contract. No threshold or query duration differs from T111.

## Coupled representation difference

T123 source index 8 is a visible source codepoint whose forced-alignment unit
has zero duration at `1314.156`. Typed positive aligned units therefore cover
source `7-8` through `1314.156` and source `9-10` beginning at `1314.156`, but
the positive business interval at source `8-9` has no positive aligned-unit
anchor. Every other positive business interval remains anchored and the
business intervals still partition source `0-11` exactly once.

T123's sole punctuation phrase covers source `0-6` at
`1313.196-1313.916`. Both phrase galleries pass the existing short score gate
and abstain on the existing short margin gate. They agree on `spk_3`, which
has no activity or primary coverage in the aligned response envelope. Within
the only locally supported pair, the raw session scores are
`spk_1=0.292846501` and `spk_2=0.256438553`; the raw robust scores are
`spk_1=0.273525506` and `spk_2=0.252747804`. In both views the local pair is
closer than the checked-in `speaker_fusion.short_min_margin=0.04`, so the
phrase cannot distinguish Tang from Xu under the existing configured margin.

The retained rule currently rejects T123 twice: it requires every business
interval to have a positive aligned-unit anchor, and it requires the complete-
source candidate to appear in each phrase's top two. T111 happens to satisfy
both representation checks. The complete-source, VAD, activity, primary,
source-partition, duration, and label topology otherwise remains the same.

## Rejected broad alternatives

Removing the phrase guard is rejected because it would allow complete-source
voiceprint to override an affirmative conflicting local phrase. Treating any
unanchored interval as aligned is rejected because punctuation, multi-
character gaps, and temporally disconnected intervals have no equivalent
forced-alignment support. Selecting Xu from `test.txt`, a speaker name, or the
known timestamp is prohibited. Lowering a threshold or fitting a new score,
margin, or duration is also rejected.

## FR43 contract

FR43 extends only the retained complete-source aligned-VAD closure with one
zero-duration local-pair-tie representation. The current fully anchored and
phrase-top-two representation remains unchanged. The extension is eligible
only when every condition is true:

1. Exactly one positive business interval lacks a positive aligned-unit
   anchor. It maps exactly one visible, non-whitespace, non-configured-
   punctuation source character, and the raw alignment track maps exactly one
   zero-duration unit to that character. Every other positive business
   interval is anchored, and all intervals partition the complete source
   exactly once.
2. Every positive aligned unit maps exactly one valid source character. The
   missing character has no positive unit and has exactly one source-adjacent
   previous and following positive unit.
3. The adjacent units are temporally ordered, their gap is strictly below the
   existing alignment-pause value, and the unanchored business interval
   contains their temporal bridge. Missing, duplicate, overlapping, reversed,
   or pause-sized evidence preserves current behavior.
4. Every existing complete-source kind, source extent, embedding, robust-
   completeness, padded-duration, aligned-duration, dual-gallery identity,
   short-gate, outer regular-abstention, label, edge, activity, primary,
   coverage, and exact-partition condition remains required.
5. The candidate and incumbent remain the only identities with activity and
   primary coverage across the aligned envelope. Both must retain their
   existing partial/full coverage topology.
6. The unique phrase remains dual-gallery score-eligible and margin-abstaining.
   Both galleries must agree on the same top identity outside the local pair,
   and that identity must have no activity or primary coverage in the aligned
   envelope. Candidate and incumbent must each appear exactly once in both
   score lists, and their absolute score difference in each view must remain
   strictly below the existing configured short margin.
7. The unique containing VAD must continue to select the complete-source
   candidate in both galleries and pass every existing score and margin gate.

Only the already defined complete source may be projected to the candidate.
FR43 adds no TOML field, threshold, score, margin, duration, transcript lookup,
speaker name, timestamp, reference datum, or fitted constant. It changes no
producer track or common-clock coordinate and reuses the existing
`voiceprint_complete_source_aligned_vad_closure` provenance.

## Verification and decision boundary

Focused tests must preserve the existing positive path and independently
exercise every added source, character, anchor, adjacency, temporal-bridge,
local-pair, phrase, activity, primary, and score-list rejection boundary. A
warning-clean build and complete CTest pass are engineering evidence only.

The production projector must replay frozen T123 and T111 packages at least
twice. Automation may verify immutable hashes, deterministic bytes, source
reconstruction, time order, and display the complete raw change scope. Every
changed complete conversation must then be read chronologically and in reverse
against `test.txt`. Only that contextual semantic review may retain or remove
FR43 and update a manually derived ledger. A retained frozen replay is not a
new real-WebSocket result and cannot close the speaker business by itself.
