# FR41 Primary-Onset Single-Unit Frozen Replay Review (2026-07-19)

## Scope and decision authority

This review completes the frozen gate specified by
`primary-onset-single-unit-partition-diagnosis-2026-07-19.md`. It evaluates
only FR41's representation of the already retained primary-onset aligned-
island rule. It does not rerun audio, tune TOML, change a producer track,
evaluate ASR wording, or claim a new real-WebSocket result.

No compiled code, script, query, formula, metric, score operation, or interval
operation assigns correctness, aggregates accuracy, ranks the candidate, or
issues the retention decision. Automation is limited to build/test contracts,
immutable hashes, deterministic replay, source reconstruction, time order,
and display of the raw change scope. The retention decision below comes only
from complete chronological and reverse contextual semantic reading against
the human-listened `test.txt`.

## Engineering and replay evidence

The final source builds cleanly. The build log contains no warning or error
line beyond compiler ABI notes, and all 69 registered CTest entries pass.
Focused FR41 tests preserve the existing in-run shape, exercise the new
single-unit boundary shape, and independently abstain for missing or duplicate
neighbors, another source, invalid source ranges, empty or visible source gaps,
a short pause, nonadjacent or in-run continuation, punctuation, whitespace, or
nonpositive candidates, wide or multiple candidate units, and the configured
minimum-unit guard.

The final clean binary replays each frozen producer package twice:

| Frozen package | Replay A SHA-256 | Replay B SHA-256 | Mechanical result |
|---|---|---|---|
| T123 | `cbc67b85afce75bc2e40a19a5cb567309981cd433d1ccf4733fc3000415c65d2` | `cbc67b85afce75bc2e40a19a5cb567309981cd433d1ccf4733fc3000415c65d2` | byte-identical |
| T111 | `ad2abee782ab30ff67be1a86fa46f4ec0c16b5422ae18954107c398131157aa4` | `ad2abee782ab30ff67be1a86fa46f4ec0c16b5422ae18954107c398131157aa4` | byte-identical and unchanged from FR40 |

Supporting immutable evidence:

| Evidence | SHA-256 |
|---|---|
| warning-clean final build log | `d016687549d71b3dfced67b4196da2359a666d9076103b0ad06338a693acc99b` |
| complete final CTest log | `f03ab84c257983432a082f1ac4c8f89ecd228d31293897410b8fe034eaea4003` |
| clean frozen replay probe | `36a967b893a0d1f53613ea8bfb914cebd07969015714d945df45f6886603c950` |
| checked-in `orator.toml` | `c3c1acbc723fb3f0d600625025023edb51750fb1b51731342bc893dd2d3273b1` |
| human-listened `test.txt` | `35e8695057be82f3028877f7dc159f10ecfa0ab7f06c444cefa9a079b0e24a86` |

These are engineering and provenance facts only; hash equality is not a
product-accuracy judgment.

## Raw mechanical change scope

After omitting replay-only `turn_id`, T123 has one changed source interval.
The previous output was:

```text
1932.316-1933.036  啊啊，  spk_2  voiceprint_phrase_session
```

The FR41 output is:

```text
1932.316-1932.636  啊    spk_0  primary_speaker_pause_onset_aligned_island_override
1932.876-1933.036  啊，  spk_2  voiceprint_phrase_session
```

T123 entry cardinality changes from 1711 to 1712 because the formerly merged
source span is split. There is no other text, identity, reason, timestamp, or
source-coordinate change. T111 has no normalized change and remains at 1752
entries. These statements expose scope; they do not evaluate it.

## Complete contextual semantic review

The complete `31:17-33:23` conversation was read chronologically against
`test.txt`, then read again from the later continuation back through the
handoff. Xu Zijing first explains that one presentation page will state where
the technology originated. Zhu Jie gives the first short `啊` response. Xu
then gives the second `啊` and continues explaining the technology migration
and dimensional advantage. The surrounding turns establish an
Xu-to-Zhu-to-Xu handoff.

The reverse reading reaches the same interpretation: Xu's uninterrupted
technical continuation owns the second reaction, while Zhu's intervening
response is bounded by the local Zhu primary/activity island. Assigning both
reactions to Xu erases that response; assigning both to Zhu breaks Xu's
continuation. FR41 changes only the first reaction and leaves every neighboring
contribution unchanged.

## Manual decision and ledger

The complete forward and reverse contextual semantic review **retains FR41**.
Only current T123 `ref-0268` changes from a noncritical confident-wrong
speaker attribution to accepted. T111 was already correct and is partition
evidence only, so it is not counted again.

The manually reconciled frozen ledger is now `515/556`: 41 contributions
remain incorrect, comprising 34 confident-wrong, 6 missing, and 1 uncertain.
The seven fixed blocks are `87/93`, `79/84`, `74/80`, `75/80`, `117/129`,
`80/87`, and `3/3`; every complete 600-second block remains above its
natural-turn floor. The manually reviewed per-speaker natural-turn ledgers are
Zhu Jie `75/83`, Tang Yunfeng `174/189`, Xu Zijing `68/73`, and Shi Yi
`198/211`. All four per-speaker natural-turn floors now pass.

Speaker-business closure remains open. The critical-attribution and
confident-wrong gates still fail; speaker-time, per-speaker-time,
source-time-offset, full real-path repeatability, independent holdout,
report-review, and release-signing evidence remain unsigned. FR41 is therefore
a retained transitional frozen experiment, not an industrial-release verdict.
