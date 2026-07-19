# FR36 Partition-Invariant Regular Initial-Slot Diagnosis (2026-07-19)

## Scope and authority

This frozen-evidence diagnosis addresses the T111/T123 speaker-attribution
partition regression associated with `ref-0350`. It does not score ASR text or
run a new audio path. `test/data/reference/test.txt` remains the authoritative
human-listened reference. Complete conversational reading identifies Zhu Jie
as the speaker of the response between Tang Yunfeng's profitability question
and Tang's following `哪个？` question.

No program, script, query, formula, metric, or interval operation assigns
correctness, aggregates accuracy, selects FR36, or issues a product verdict.
Shell tools only display frozen typed evidence on the common source clock. A
complete chronological and reverse contextual review of every changed
conversation decides whether the resulting candidate is retained.

## Frozen partition evidence

The T111 and T123 activity and independent primary-speaker tracks are identical
through the response. One uncontested local slot covers the complete phrase in
both tracks. Its current identity is A, Tang Yunfeng, while its first stable
identity is B, Zhu Jie. No competing activity overlaps the phrase.

Only the ASR and forced-alignment partition changes the acoustic evidence
range:

| Frozen package | Punctuation phrase | Final business identity |
|---|---|---|
| T111 | `2483.620-2486.420` `而且我觉得这个这个点儿听起来还不错哈，` | B through the existing regular initial-slot rule |
| T123 | `2483.660-2485.500` `而且我觉得这个这个点儿听起来还不错。` | A through native sole-diar support |

The T123 exact phrase's session and robust galleries both rank A first and B
second. Both fail the unchanged regular absolute-score gate; exactly one view
passes the unchanged regular margin. The unique containing VAD and the unique
containing complete-source evidence reverse that order in all four views: B is
first and A is second. Each outer view fails both unchanged regular score and
margin gates. Thus no single weak outer view is eligible to override the
native identity, while all independent outer scales agree with the slot's
initial identity. T111's slightly longer punctuation partition directly ranks
B first in both phrase galleries and activates the existing strict rule.

The defect is therefore not a different diarization decision, primary-speaker
decision, identity epoch, VAD interval, model, TOML value, or source clock. The
final view changes because punctuation partitioning moves the same response
between two existing evidence topologies.

## FR36 contract

FR36 adds one regular-duration punctuation-phrase challenge with current
identity A and initial identity B. Every condition below is conjunctive:

1. the phrase is embedding-backed, robust-gallery complete, within the
   existing regular phrase duration bounds, and contains the existing minimum
   aligned-unit count;
2. every source label is native `sole_diar_support`, unprotected, equal to its
   base identity, and uniformly names A;
3. activity and primary speaker each expose exactly one complete covering
   local slot, both use the same slot and current identity A, and no competing
   slot overlaps the phrase;
4. that slot has exactly one different non-empty initial identity B;
5. both exact-phrase galleries rank A first and B second, both fail only the
   unchanged regular absolute-score gate, and exactly one passes the unchanged
   regular margin;
6. exactly one embedding-backed, robust-complete VAD contains the phrase;
   both of its galleries rank B first and A second while failing the unchanged
   regular score and margin gates; and
7. exactly one embedding-backed, robust-complete complete-source record for
   the same text contains the phrase; both of its galleries rank B first and A
   second while failing those same unchanged regular gates.

The rule writes only the exact phrase source and forced-alignment range. It
adds no future lookahead, TOML field, threshold, identity, transcript,
timestamp, reference lookup, or fitted constant. Any missing, duplicate,
eligible, differently ranked, differently gated, competing, mixed,
voiceprint-protected, unaligned, or source/time-inconsistent evidence preserves
ordinary behavior. Existing specialized phrase challenges retain precedence.

## Verification and decision boundary

Focused tests must cover the complete positive topology and independent
abstention for phrase rank, phrase gate pattern, VAD uniqueness and ranks,
complete-source uniqueness and ranks, activity/primary disagreement, competing
activity, initial identity, alignment, native provenance, and protected source
labels. After a warning-clean build and complete CTest pass, the production C++
projector replays the frozen T123 and T111 producer packages at least twice.

Automation may verify immutable input hashes, deterministic output hashes,
source reconstruction, monotonic time, and raw change scope only. Every changed
complete conversation is then read chronologically and in reverse against
`test.txt`. That contextual semantic review alone retains or removes FR36 and
updates any manually derived ledger. A retained frozen replay is not a new
real-WebSocket result and cannot close the speaker business by itself.
