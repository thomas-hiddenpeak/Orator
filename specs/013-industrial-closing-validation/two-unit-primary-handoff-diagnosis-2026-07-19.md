# FR40 Two-Unit Primary Handoff Diagnosis (2026-07-19)

## Scope and authority

This frozen-evidence diagnosis follows retained FR39 and addresses the
partition-sensitive `ref-0024`/`ref-0025` handoff. It uses the immutable T111
and T123 typed producer packages on their common sample clock. It does not run
audio, change a model, tune TOML, or evaluate ASR wording.
`test/data/reference/test.txt` remains the authoritative human-listened
reference.

No compiled code, script, query, formula, metric, score operation, or interval
operation assigns correctness, aggregates accuracy, selects FR40, or issues a
product verdict. Shell tools only display immutable typed evidence. Complete
chronological and reverse contextual semantic review against `test.txt` alone
may retain or reject a resulting candidate.

## Complete conversational boundary

The human reference exposes three consecutive reactions after Tang Yunfeng
states that Zhu Jie can receive seven percent:

- Zhu Jie reacts `啊？`;
- Xu Zijing responds `嗯？`; and
- Zhu Jie asks `为啥是7呢。`

The complete forward context identifies the first and third contributions as
one speaker's surprise and follow-up question. Reverse reading from Tang's
answer reaches Zhu's question, Xu's intervening acknowledgement, and Zhu's
initial reaction in the same order. These are two real speaker handoffs, not
one interchangeable backchannel.

## Partition-stable typed evidence

The frozen identity map is `spk_0=朱杰`, `spk_1=唐云峰`,
`spk_2=徐子景`, and `spk_3=石一`. Both packages expose the same activity,
primary, VAD, and common-clock alignment topology, while ASR partitions the
two reactions differently.

T111 places both reactions at the tail of `text_id=12`. Source `[107,108)` is
aligned to `183.740-183.900` and is followed by question punctuation. Source
`[109,110)` is aligned to `184.220-184.300` and is followed by terminal
punctuation. The existing projector paints the complete `啊？嗯。` interval as
`spk_2` with `voiceprint_direct_short`.

T123 places the first reaction at the tail of `text_id=13`: source
`[116,117)` is aligned to `183.756-183.836` and followed only by punctuation.
The second reaction starts `text_id=14`: source `[0,1)` is aligned to
`184.240-184.320` and followed by punctuation before Zhu's separately aligned
question at `185.120`. The existing projector retains the first unit as
`spk_0`, but a wider `text_id=14` direct interval paints `嗯，` as `spk_0`.

The package-independent producer evidence is:

| View | Immutable evidence |
|---|---|
| Activity | One `spk_0`/Zhu local slot covers both aligned units without competing activity |
| Primary left | One `spk_0` run `183.520-184.080` covers only the first aligned unit |
| Primary right | One `spk_2` run `184.240-184.480` covers only the second aligned unit |
| T111 alignment | The two one-character units are ordered, non-overlapping, and separated by `0.320 s` |
| T123 alignment | The same units cross an ASR boundary and are separated by `0.404 s`; subtracting the checked-in `0.080 s` boundary tolerance yields the same subminimum topology |
| VAD | One robust-complete VAD `183.620-184.476` contains exactly these two positive units |
| VAD session | Xu `0.360992`, Zhu `0.286897`; unchanged short score and margin gates pass |
| VAD robust | Xu `0.385988`, Zhu `0.314870`; unchanged short score and margin gates pass |

The coarse views are therefore complementary rather than interchangeable.
Activity preserves the surrounding Zhu turn, VAD identifies the short Xu
response, primary provides the exact A-to-B boundary, and forced alignment
maps that boundary back to source characters. T111 loses the handoff by
painting both units from one short voiceprint interval; T123 loses it by
painting the source-leading Xu unit from a wider Zhu interval. The common
clock preserves the same two-unit handoff in both partitions.

This is not evidence for lowering a global threshold or for trusting primary
on arbitrary subminimum islands. It is a coarse direct-write precedence defect
at one fully corroborated two-unit handoff.

## FR40 contract

FR40 may replace only one exact aligned unit and its immediately following
configured punctuation with that unit's primary identity. Every condition is
conjunctive:

1. The candidate is a positive-duration, no-embedding `aligned_unit` with one
   visible source character and valid source bounds.
2. Its current labels are uniform, known, voiceprint-backed
   `voiceprint_direct_*`, and differ from the proposed primary identity.
3. Exactly one robust-complete VAD contains the candidate. Exactly two
   positive-duration aligned units from any ASR partition lie wholly inside
   that VAD; no third aligned unit overlaps it.
4. Both units contain exactly one visible source character followed by at
   least one configured punctuation character before the next visible source
   character or source end.
5. The units are ordered without overlap. Their raw alignment gap is at least
   the existing alignment-pause value and, after subtracting at most the
   existing alignment-boundary tolerance, remains below the existing primary-
   consensus minimum; no new duration constant is introduced.
6. Exactly one primary run covers each unit within the existing alignment
   boundary tolerance. The runs are ordered, non-overlapping, and name two
   different identities A then B. No competing primary run overlaps either
   unit.
7. Exactly one activity local slot carrying A covers both units and no
   competing activity overlaps either unit.
8. Both VAD galleries rank B first and A second and pass the unchanged short
   score and margin gates. No identity outside A/B may occupy either top-two
   pair.
9. The proposed identity for the candidate is its exact primary identity and
   must be A or B. Existing labels already matching that identity remain
   untouched.

The write extends only across configured punctuation immediately following
the selected visible character. It does not repaint the other unit, a later
visible character, neighboring ASR text, timestamps, producer tracks, or
identity records. Missing, duplicate, differently ordered, differently
ranked, differently gated, embedded, mixed, protected, unaligned, additional-
unit, competing-primary, competing-activity, or source/time-inconsistent
evidence preserves ordinary behavior.

FR40 adds no TOML field, threshold, score, margin, duration, transcript lookup,
speaker name, known timestamp, reference label, or fitted constant. It uses
only existing typed producer evidence and checked-in configuration.

## Verification and decision boundary

Focused tests must prove both partition shapes and independently remove every
source, punctuation, label provenance, containment, unit-count, timing,
primary, activity, VAD rank, gate, and write-scope condition. A warning-clean
build and complete CTest pass are engineering evidence only.

The production C++ projector then replays each frozen T123 and T111 package at
least twice. Automation may verify input/output hashes, source reconstruction,
time monotonicity, deterministic bytes, and raw change scope only. The reviewer
must read every changed complete conversation chronologically and in reverse
against `test.txt`. Only that semantic review may retain FR40 or update the
manual ledger. A retained frozen replay is not a new real-WebSocket result and
cannot close the speaker business by itself.
