# FR42 Isolated-VAD Single-Character Alignment-Gap Frozen Replay Review (2026-07-19)

## Scope and decision authority

This review completes the frozen gate specified by
`isolated-vad-single-character-alignment-gap-diagnosis-2026-07-19.md`. It
evaluates only FR42's representation of one zero-duration source character
inside the retained isolated-VAD aligned-island rule. It does not rerun audio,
tune TOML, change a producer track, evaluate ASR wording, or claim a new
real-WebSocket result.

No compiled code, test, script, query, formula, metric, score operation, or
interval operation assigns correctness, aggregates accuracy, ranks FR42, or
issues the retention decision. Automation is limited to build/test contracts,
immutable hashes, deterministic replay, source reconstruction, time order, and
display of the raw change scope. The decision below comes only from complete
chronological and reverse contextual semantic reading against the human-
listened `test.txt`.

## Engineering and replay evidence

The final source builds without a `warning:` or `error:` diagnostic, and all 69
registered CTest entries pass. Focused coverage preserves the existing
source-contiguous shape and exercises the new one-character dropout shape.
Independent abstention cases cover punctuation and whitespace dropouts, a
wide or repeated source gap, a pause-sized or overlapping time gap,
multi-character and extra positive units, invalid source bounds, a different
configured minimum, missing punctuation configuration, and bounded write
scope.

The final binary replays each frozen producer package twice:

| Frozen package | Replay A SHA-256 | Replay B SHA-256 | Mechanical result |
|---|---|---|---|
| T123 | `00499131119515f91a5a6592a49b37190c1462a7804f45c76190f2c011efe6c7` | `00499131119515f91a5a6592a49b37190c1462a7804f45c76190f2c011efe6c7` | byte-identical |
| T111 | `ad2abee782ab30ff67be1a86fa46f4ec0c16b5422ae18954107c398131157aa4` | `ad2abee782ab30ff67be1a86fa46f4ec0c16b5422ae18954107c398131157aa4` | byte-identical and unchanged from FR41 |

Supporting immutable evidence:

| Evidence | SHA-256 |
|---|---|
| warning-clean build log | `7e1a39e8fbed68460151f69719b66afa8b2ae274aa4974117d0f77409f524ed5` |
| complete CTest log | `9657248d0e7871c62dd79ae9648563a5e79a2c935060d9f036f78907b20982d7` |
| frozen replay probe | `5a1787d15d622265b7561a247f4a5508422ba857dce2b5242a0040bf169466e5` |
| checked-in `orator.toml` | `c3c1acbc723fb3f0d600625025023edb51750fb1b51731342bc893dd2d3273b1` |
| human-listened `test.txt` | `35e8695057be82f3028877f7dc159f10ecfa0ab7f06c444cefa9a079b0e24a86` |

These hashes establish engineering provenance and determinism only. They do
not support the semantic decision.

## Raw mechanical change scope

After omitting replay-only `turn_id`, T123 has one changed source interval. The
FR41 output was:

```text
2851.196-2851.596  什么意思？  spk_1  voiceprint_direct_regular
```

The FR42 output is:

```text
2851.196-2851.596  什么意  spk_0  voiceprint_vad_isolated_aligned_island_override
2851.596-2853.436  思？    spk_1  voiceprint_direct_regular
```

The T123 record count changes from 1712 to 1713 because the source span is
split. There is no other text, identity, reason, timestamp, or source-
coordinate change. T111 remains byte-identical at 1752 records. These facts
only expose the candidate's boundary.

## Complete contextual semantic review

The complete `45:47-49:42` conversation was read chronologically against
`test.txt`, then read again from the later ownership and transfer-price
discussion back through the valuation exchange. Tang Yunfeng asks how to value
the companies, asks Xu Zijing for the Hangzhou valuation, addresses Zhu Jie as
`朱总`, and asks for the Chengdu valuation. Zhu gives the short substantive
answer `两个亿吧`. Tang then resumes with the condition that the company has
not reached that stage.

FR42 assigns the positive aligned response island and its one internal
zero-duration character to `spk_0`/Zhu. Its ASR wording `什么意` is distorted,
but ASR wording is outside this speaker-only decision. The isolated target VAD,
its two independent gallery views, and the complete question-answer context all
bound the recognizable Zhu response. T111 independently preserves the same
handoff with its differently partitioned `那是硬` response under Zhu.

The retained `思？` suffix requires an explicit boundary finding. It does not
form an independent natural contribution or express a separate business
proposition in either reading direction; it only completes the incorrect ASR
word `什么意思`. Tang's recognizable next contribution begins with `你没...`.
The `思` character has zero alignment duration at `2852.636`, after the target
VAD ends at `2851.868` and after the following VAD begins at `2852.580`.
FR42 therefore does not extend Zhu's write through that character. The suffix
remains a visible source-time residual for later speaker-time review rather
than being hidden or repainted without supporting evidence.

Reading backward from Tang's condition reaches Zhu's bounded answer, then
Tang's direct question. Continuing backward reaches the Hangzhou valuation and
the broader pricing discussion. The same Tang-to-Zhu-to-Tang handoff remains
coherent, and no neighboring contribution changes.

## Manual decision and ledger

The complete forward and reverse contextual semantic review **retains FR42**.
Only current T123 `ref-0432` changes from a critical missing Zhu contribution
to accepted. T111 was already accepted and is partition evidence only, so it is
not counted again.

The manually reconciled frozen ledger is now `516/556`: 40 contributions remain
incorrect, comprising 34 confident-wrong, five missing, and one uncertain.
Twenty-five confident-wrong and one missing contribution remain critical. The
seven blocks are `87/93`, `79/84`, `74/80`, `75/80`, `118/129`, `80/87`, and
`3/3`; every complete 600-second natural-turn block remains above its floor.
The manually reviewed per-speaker natural-turn ledgers are Zhu Jie `76/83`,
Tang Yunfeng `174/189`, Xu Zijing `68/73`, and Shi Yi `198/211`. All four
per-speaker natural-turn floors remain passed.

Speaker-business closure remains open. Critical attribution and confident-
wrong attribution still fail, and speaker-time, per-speaker time, source-time-
offset, independent full real-path repeatability, locked holdout, report review,
and release signing remain unsigned. FR42 is a retained transitional frozen
experiment, not an industrial-release verdict.
