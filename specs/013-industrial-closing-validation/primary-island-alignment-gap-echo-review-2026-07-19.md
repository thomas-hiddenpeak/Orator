# Primary-Island Alignment-Gap Echo Review (2026-07-19)

## Scope and authority

This review covers the bounded FR45 frozen candidate authorized by
`primary-island-alignment-gap-evidence-diagnosis-2026-07-19.md`. The product
authority is the human-listened `test/data/reference/test.txt`. No executable
tool assigned correctness, aggregated accuracy, ranked the candidate, or
issued the retention decision. Tools were limited to build/test execution,
frozen projection, hashing, and raw changed-scope display.

FR45 is evaluated only as a frozen T123 speaker-projector repair. It is not a
new real-WebSocket result, does not alter ASR, forced alignment, VAD,
diarization, primary speaker, or a common-clock coordinate, and does not close
any remaining conjunctive acceptance gate.

## Implemented boundary

The typed evidence snapshot now carries the immutable primary-speaker track,
and voiceprint evidence carries explicit session-gallery completeness. The
producer emits `primary_alignment_gap_echo` only for one source-contiguous
strict-suffix phrase pair, one exact short primary island inside one positive-
character alignment gap, and one same-outer primary bracket within the
existing configured alignment-boundary tolerance. Multiple islands or phrase
mappings cause global abstention for that text. The acoustic query uses the
exact middle primary interval; the repeated following phrase supplies only its
source range.

The final fusion pass independently reconstructs source suffix, alignment gap,
primary bracket, and activity coverage. It requires complete equal session and
robust identity sets covering every active identity, independent passage of
the existing short score/margin gates, gallery agreement with the middle
primary identity, full local activity support, and a uniform ordinary outer
incumbent. Any missing, duplicate, tied, incomplete, ambiguous, specialized,
or inconsistent input abstains. A retained write changes only speaker fields
and speaker-decision audit metadata. No TOML value, threshold, fitted constant,
model, source text, alignment, or time coordinate was added or changed.

## Engineering evidence

The final worktree binary passes a complete build with no `warning:` or
`error:` diagnostic and all `70/70` registered CTest entries. Focused tests
cover the typed primary snapshot, explicit gallery completeness, evidence
producer positive case, multiple-island and multiple-phrase ambiguity,
suffix/alignment/primary/duration/boundary/query-scope abstention, the positive
fusion write, every independent gallery/topology/provenance gate, duplicate
typed evidence, and speaker-only write preservation.

| Artifact | SHA-256 |
|---|---|
| Final build log | `bc3616a6b2219d7238f7f2c6a12afbbd9f5b3a0bb1082a64d1c19ea9d954915c` |
| Frozen T123 evidence with FR45 row | `ff728295327df74ce22d1e84d3b1cad9207d9f12cac3ca8b8fc0c1fc8dc63fa0` |
| Repeated final T123 projection | `5a595ca1aa5816612b2603062d8467ee60bc3a342219cf5eda066cfddc3bb61a` |
| Repeated final T111 projection | `ad2abee782ab30ff67be1a86fa46f4ec0c16b5422ae18954107c398131157aa4` |

Both final T123 replays contain 1,715 business entries and are byte-identical.
Both final T111 replays contain 1,752 entries, are byte-identical, and are
unchanged from retained FR44. This is repeatability evidence, not product
accuracy evidence.

## Complete mechanical change display

The complete T123 raw display changes one prior `text_id=111` business turn:

- Before FR45, `1304.508-1305.468` source `没有意见，赶紧说，快！` is one
  `spk_3` turn with reason `voiceprint_direct_short`.
- After FR45, source `没有意见，` is an `spk_0` turn at the existing forced-
  alignment coordinate `1304.508-1304.588`, with reason
  `voiceprint_primary_alignment_gap_echo_override`.
- The remaining source `赶紧说，快！` stays `spk_3` at its existing
  `1305.308-1305.468` coordinate and retains
  `voiceprint_direct_short`.

The identity query itself uses the exact acoustic primary island
`1303.359970868-1303.999970853`. FR45 intentionally does not rewrite the
business time from forced alignment to that acoustic interval. Consequently
this frozen result cannot sign speaker-time or source-time-offset gates.

No other T123 business entry changes, and no T111 business entry changes. This
statement describes the complete raw projection scope only; it is not the
semantic decision.

## Forward contextual semantic review

The complete reference context was read from `20:33` through `22:42`, including
the lead-in valuation discussion and the full continuation after the disputed
phrase. At `21:38`, Tang Yunfeng asks whether the others have objections. At
`21:40`, Shi Yi answers `没问题。嗯，我没有我没有意见。`. At `21:43`, Zhu
Jie independently adds `没有意见。`. Shi Yi then immediately says
`有意见赶紧说哦，快！...` before Xu Zijing and Tang Yunfeng continue the
discussion.

The retained candidate separates only Zhu Jie's independent short response
from Shi Yi's following request. It preserves the forward conversational order
Tang-Shi-Zhu-Shi. The neighboring source remains visible exactly as before;
known neighboring attribution defects are not treated as repaired by FR45.

## Reverse contextual semantic review

The same context was then read backward from Tang Yunfeng's `22:07` explanation
through Shi Yi's follow-up, Xu Zijing's objection, Shi Yi's request, Zhu Jie's
short insertion, Shi Yi's preceding answer, and Tang Yunfeng's question. That
reverse reading independently requires Zhu Jie's `没有意见` between Shi Yi's
answer and Shi Yi's request. Returning the short phrase to `spk_0` preserves
that sequence; leaving it with `spk_3` merges two distinct adjacent speakers.

## Manual decision and remaining gates

Complete forward and reverse contextual semantic review retains FR45. Only
current T123 `ref-0192` advances the manually reconciled frozen ledger, from
`518/556` to `519/556`. The 1200-1800 natural-turn block advances from `75/80`
to `76/80`, and Zhu Jie advances from `76/83` to `77/83`. Source partitioning
inside the same contribution is not counted as another repair, and T111 is not
double-counted.

The result remains a transitional frozen experiment. Critical attribution,
confident-wrong attribution, speaker-time, per-speaker time, source-time
offsets, new real-path repeatability, independent holdout, ASR, microphone,
report review, release signing, and full speaker-business closure remain open.
