# FR42 Isolated-VAD Single-Character Alignment-Gap Diagnosis (2026-07-19)

## Scope and authority

This diagnosis follows retained FR41 and examines the frozen T111/T123
partition difference at `ref-0432`. It uses immutable diarization, primary,
VAD, voiceprint, ASR, and forced-alignment producer evidence on the common
clock. It does not rerun audio, change a model, tune TOML, evaluate ASR wording,
or use `test.txt` as production input.

No compiled code, script, query, formula, metric, score operation, or interval
operation assigns correctness, aggregates accuracy, selects FR42, or issues a
product verdict. Shell tools only display frozen evidence. Complete
chronological and reverse contextual semantic review against the human-
listened `test.txt` is the sole authority that may retain or reject a replayed
candidate.

## Complete conversational boundary

The complete `45:47-49:42` context establishes an explicit valuation exchange.
Tang Yunfeng asks how to value the companies, asks Xu Zijing for the Hangzhou
valuation, then addresses Zhu Jie as `朱总` and asks for the Chengdu valuation.
Zhu gives the short substantive answer `两个亿吧`. Tang immediately continues
with the condition that the company has not yet reached that stage. Reading
the later ownership and transfer-price discussion back toward the exchange
reaches the same Tang-question, Zhu-answer, Tang-continuation sequence.

The current T123 business view retains recognizable speech at
`2851.196-2851.596`, but assigns the ASR wording `什么意思？` to
`spk_1`/Tang. The manually reconciled frozen ledger therefore continues to
treat `ref-0432` as a missing critical Zhu answer. T111 already assigns its
corresponding recognizable span `2851.280-2851.600` to `spk_0`/Zhu with the
existing `voiceprint_vad_isolated_aligned_island_override`. T111 is partition
evidence only; it is not an input to the runtime projector.

## Partition-stable producer evidence

The frozen identity map is `spk_0=朱杰`, `spk_1=唐云峰`,
`spk_2=徐子景`, and `spk_3=石一`. T111 and T123 have identical activity,
primary, and VAD tracks in this region:

| View | Immutable evidence |
|---|---|
| Activity | coarse `spk_1`/Tang activity covers `2847.600-2856.000` |
| Primary | `spk_1`/Tang covers the answer at `2851.040-2852.000` |
| Previous VAD | `2848.996-2850.620`, leaving more than the existing alignment-pause value |
| Target VAD | isolated `2851.172-2851.868`, containing only the answer island |
| Following VAD | begins at `2852.580`, again beyond the existing alignment-pause value |
| Target session gallery | uniquely ranks `spk_0`/Zhu first under the existing short-VAD gate |
| Target robust gallery | independently ranks `spk_0`/Zhu first under the same gate |

T111 exposes three positive, source-contiguous one-character aligned units
inside the target VAD. The retained isolated-VAD rule rewrites exactly their
source span from the coarse direct Tang identity to Zhu.

T123 exposes two positive one-character units inside the same VAD. Their
source ranges are `14-15` and `16-17`; the visible source character at index
15 lies between them but has zero alignment duration and is consequently not
present in typed positive aligned-unit evidence. The first unit ends at
`2851.356`, the second begins at `2851.516`, and their gap is below the existing
`timeline.align_snap_pause_sec = 0.25`. The current isolated-VAD rule requires
strict source contiguity and therefore abstains. Every independent acoustic,
identity, isolation, and common-clock condition is otherwise unchanged.

## FR42 contract

FR42 extends only the retained isolated-VAD aligned-island rule with one
alignment-dropout representation. The current source-contiguous representation
remains unchanged. The new representation is eligible only when every
condition is true:

1. The VAD passes the existing positive-duration, embedding, robust-gallery,
   dual-gallery identity, score, and margin gates.
2. The VAD has unique nonoverlapping previous and following VADs, and both
   gaps meet the existing alignment-pause value.
3. Every positive aligned unit overlapping the VAD belongs to the current ASR
   source and is fully contained by the VAD.
4. The positive units equal the existing configured minimum count. Every unit
   maps exactly one source character, remains temporally ordered, and has a
   valid source range.
5. Exactly one neighboring unit pair has a source gap, and that gap contains
   exactly one visible, non-whitespace, non-configured-punctuation character.
   Every other pair is source-contiguous.
6. The temporal gap across the missing source character is strictly below the
   existing alignment-pause value. Missing, equal-or-longer, overlapping, or
   multiply gapped timing preserves current behavior.
7. Existing labels across the complete bounded source span, including the
   missing character, remain uniform, known, and `voiceprint_direct_*` backed.
8. The existing single-current-slot activity coverage, absence of competing
   activity, unique covering primary, and primary/current-identity agreement
   remain required.

The write covers only the source span from the first positive unit through the
last positive unit, including the one internal missing character. It does not
carry following punctuation, repaint another phrase, change text, alter a
timestamp, or mutate producer evidence. Missing, duplicate, extra-unit,
multi-character-unit, invalid-source, punctuation-gap, whitespace-gap,
multi-character-gap, long-time-gap, mixed-label, activity, primary, VAD, or
gallery evidence preserves current behavior.

FR42 adds no TOML field, threshold, duration, score, margin, transcript lookup,
speaker name, known timestamp, reference datum, or fitted constant. It uses
only existing typed evidence and checked-in configuration.

## Verification and decision boundary

Focused tests must preserve the existing contiguous representation and add the
single-character dropout representation plus independent abstention for every
new condition. A warning-clean build and complete CTest pass are engineering
evidence only.

The production projector must then replay frozen T123 and T111 packages at
least twice. Automation may verify hashes, deterministic bytes, source
reconstruction, time order, and display raw change scope only. Every changed
complete conversation must be read chronologically and in reverse against
`test.txt`. Only that semantic review may retain FR42 or update a manual
ledger. A retained frozen replay is not a new real-WebSocket result and cannot
close the speaker business by itself.
