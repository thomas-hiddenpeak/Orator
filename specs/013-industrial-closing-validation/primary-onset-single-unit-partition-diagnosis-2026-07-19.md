# FR41 Primary-Onset Single-Unit Partition Diagnosis (2026-07-19)

## Scope and authority

This diagnosis follows retained FR40 and examines the frozen T111/T123
partition difference at `ref-0268`. It uses immutable diarization, primary,
VAD, voiceprint, ASR, and forced-alignment producer evidence on the common
clock. It does not rerun audio, change a model, tune TOML, evaluate ASR
wording, or use `test.txt` as production input.

No compiled code, script, query, formula, metric, score operation, or interval
operation assigns correctness, aggregates accuracy, selects FR41, or issues a
product verdict. Shell tools only display frozen evidence. Complete
chronological and reverse contextual semantic review against the human-
listened `test.txt` is the sole authority that may retain or reject a replayed
candidate.

## Complete conversational boundary

Xu Zijing explains that one presentation page will state where the technology
originated. Zhu Jie gives one short `啊` response. Xu then resumes with a
second `啊` and explains the technology migration and dimensional advantage.
Reading forward from Zhu's preceding strategy question and backward from Xu's
continuation identifies an Xu-to-Zhu-to-Xu handoff, not one duplicated Xu
backchannel.

The current T123 business view merges `1932.316-1933.036` `啊啊，` under
`spk_2`/Xu. T111 already separates its first aligned `啊` under `spk_0`/Zhu
with the existing `primary_speaker_pause_onset_aligned_island_override`, then
returns the second `啊` and Xu's explanation to `spk_2`. T111 is partition
evidence only; it is not available to the runtime projector.

## Partition-stable producer evidence

The frozen identity map is `spk_0=朱杰`, `spk_1=唐云峰`,
`spk_2=徐子景`, and `spk_3=石一`. T111 and T123 have identical activity,
primary, and VAD tracks in this region:

| View | Immutable evidence |
|---|---|
| Activity A | `spk_2`/Xu covers the surrounding `1926.960-1944.160` explanation |
| Activity B | one `spk_0`/Zhu local slot covers `1932.240-1932.960` |
| Primary before | A ends at `1931.840`, leaving the configured pause before B |
| Primary B | one `spk_0` run covers `1932.240-1932.800` |
| Primary after | A resumes exactly at `1932.800` and continues beyond the existing minimum |
| Previous VAD | ends at `1931.740` |
| Containing VAD | starts at `1932.292`, continues through the A recovery, and ranks A first in both galleries |

T111 aligns a trailing source character from Xu's question inside the B
primary run, followed after configured punctuation by Zhu's first `啊`. The
existing rule sees that in-run source split and writes only the latter unit to
B.

T123 aligns the previous visible source character at `1931.356-1931.516`,
before the B run. Configured punctuation occupies the only intervening source
gap. The first `啊` is the sole positive aligned unit inside B at
`1932.316-1932.636`; the second `啊` is source-adjacent but begins at
`1932.876`, after A resumes. The existing rule requires two aligned units
inside B and therefore abstains even though every acoustic boundary is
unchanged. This is an alignment-partition defect in an already retained rule,
not evidence for broad primary precedence.

## FR41 contract

FR41 extends only the existing primary-onset aligned-island rule with a second
source-partition representation. The current in-run representation remains
unchanged. The new representation is eligible only when every condition is
true:

1. The candidate primary B run passes the existing primary-consensus minimum
   and short maximum and has the existing paused A-before/B/gapless-A-after
   topology.
2. Exactly one positive aligned unit from the current ASR source overlaps and
   lies wholly inside B. It maps one visible, non-whitespace,
   non-punctuation source character.
3. The nearest previous positive aligned unit belongs to the same ASR source,
   ends before B, and is separated from the candidate by at least the existing
   alignment-pause value.
4. The nonempty source gap between that previous unit and the candidate
   consists entirely of configured punctuation. Together the previous and
   candidate units satisfy the existing configured minimum aligned-unit count.
5. The nearest following positive aligned unit belongs to the same ASR source,
   is source-adjacent to the candidate, begins only after B ends, and therefore
   bounds the single-unit write before A's continuation.
6. Existing labels from the previous unit through the candidate are uniform,
   known, voiceprint-backed A and differ from B.
7. One B activity local slot covers the candidate; only A/B activity may
   overlap it. The existing containing-VAD onset, prior-VAD pause,
   continuation, completeness, and dual-gallery A ranking all remain required.

The write covers only the one aligned candidate character. It does not carry
punctuation, repaint the following source-adjacent unit, change text, alter a
timestamp, or mutate producer evidence. Missing, duplicate, multi-unit,
nonpunctuation-gap, short-gap, nonadjacent-following, in-run-following,
punctuation-target, insufficient-unit, mixed-label, competing-identity,
primary, activity, or VAD evidence preserves current behavior.

FR41 adds no TOML field, threshold, duration, score, margin, transcript lookup,
speaker name, known timestamp, reference datum, or fitted constant. It uses
only existing typed evidence and checked-in configuration.

## Verification and decision boundary

Focused tests must preserve the existing in-run partition and add the
single-unit boundary partition plus independent abstention for every new
condition. A warning-clean build and complete CTest pass are engineering
evidence only.

The production projector must then replay frozen T123 and T111 packages at
least twice. Automation may verify hashes, deterministic bytes, source
reconstruction, time order, and display raw change scope only. Every changed
complete conversation must be read chronologically and in reverse against
`test.txt`. Only that semantic review may retain FR41 or update a manual
ledger. A retained frozen replay is not a new real-WebSocket result and cannot
close the speaker business by itself.
