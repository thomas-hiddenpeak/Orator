# FR43 Complete-Source Local-Pair Tie Frozen Replay Review (2026-07-19)

## Scope and decision authority

This record completes the frozen gate specified by
`complete-source-local-pair-tie-diagnosis-2026-07-19.md`. It evaluates only
FR43's representation of one raw zero-duration aligned character and one
locally unresolved phrase inside the retained complete-source aligned-VAD
closure. It does not rerun audio, tune TOML, change a producer track, evaluate
ASR accuracy, or claim a new real-WebSocket result.

No compiled code, test, script, query, formula, metric, score operation, or
interval operation assigns correctness, aggregates accuracy, ranks FR43, or
issues the retention decision. Automation is limited to build/test contracts,
immutable hashes, deterministic replay, source reconstruction, time order, and
display of the complete raw change scope. The decision below comes only from
complete chronological and reverse contextual semantic reading against the
human-listened `test.txt`.

## Engineering and replay evidence

The clean source builds without a `warning:` or `error:` diagnostic, and all
69 registered CTest entries pass. Focused coverage preserves the existing
fully anchored representation and exercises both the zero-duration plus
phrase-top-two and zero-duration plus local-pair-tie representations.
Independent abstention cases cover raw zero-unit absence, punctuation and
whitespace dropouts, missing and duplicate adjacent units, a positive unit on
the claimed missing character, a multi-character unit, invalid unit and
interval source bounds, multiple unanchored intervals, overlapping and pause-
sized unit gaps, bridge exclusion, disabled pause/punctuation configuration,
phrase-top disagreement, a locally supported phrase top, a locally decisive
session or robust view, missing and duplicate local scores, and nonlocal
activity or primary coverage.

The final binary replays each frozen producer package twice:

| Frozen package | Replay A SHA-256 | Replay B SHA-256 | Mechanical result |
|---|---|---|---|
| T123 | `ed9744a6d8a4b9bfb0bfdffd56abe62f91c4ee14b38cb8a0de1c2952e6a02bcc` | `ed9744a6d8a4b9bfb0bfdffd56abe62f91c4ee14b38cb8a0de1c2952e6a02bcc` | byte-identical |
| T111 | `ad2abee782ab30ff67be1a86fa46f4ec0c16b5422ae18954107c398131157aa4` | `ad2abee782ab30ff67be1a86fa46f4ec0c16b5422ae18954107c398131157aa4` | byte-identical and unchanged from FR42 |

Supporting immutable evidence:

| Evidence | SHA-256 |
|---|---|
| warning-clean build log | `0078d81ec76971ef35f6355d627450196e7ef58fd1c1551065ad7aa20530acee` |
| complete CTest log | `b8b8f3f9d3b735bbc260f093a1ddebaa39ad1b0794544aa035097b14c7de64ee` |
| frozen replay probe | `4a02f4ada38355fd4954e415bd7c78d77b6f2625aee47c5a214cc059142003e5` |
| checked-in `orator.toml` | `c3c1acbc723fb3f0d600625025023edb51750fb1b51731342bc893dd2d3273b1` |
| human-listened `test.txt` | `35e8695057be82f3028877f7dc159f10ecfa0ab7f06c444cefa9a079b0e24a86` |
| T123 source timeline | `61119ab2eb4f66ed08be85652df44619001227b4986fa6b770239a840b26a9f0` |
| T111 source timeline | `d5c97db9ff91b41da4ccd5414d5f2bca4966592e60fb2717058fee2e600132e9` |

These hashes establish engineering provenance and determinism only. They do
not support the semantic decision.

## Raw mechanical change scope

After omitting replay-only `turn_id`, T123 has one changed source interval. The
FR42 output was:

```text
1313.196-1313.957985  那老有点小，还有  spk_1  competing_diar_interval_policy
1313.957985-1314.156  点                spk_1  primary_speaker_tie_break
1314.156-1314.236     小。              spk_2  primary_speaker_tie_break
```

The FR43 output is:

```text
1313.196-1314.236  那老有点小，还有点小。  spk_2
                   voiceprint_complete_source_aligned_vad_closure
```

The T123 record count changes from 1713 to 1711 because the complete source is
merged. There is no other text, identity, reason, timestamp, or source-
coordinate change. T111 remains byte-identical at 1752 records. These facts
only expose the candidate boundary.

## Complete contextual semantic review

The complete `20:07-22:56` conversation was read chronologically against
`test.txt`. Tang Yunfeng asks whether the proposed allocation is acceptable.
Shi Yi says he has no objection; Zhu Jie also says he has no objection. Shi
then asks the group again to state any objection while the conversation is
being recorded. Xu Zijing answers that Tang's share is still a little low.
Tang immediately responds that this is reasonable and starts explaining his
own objective and future return.

FR43 assigns the complete `那老有点小，还有点小。` source to `spk_2`/Xu. Its
ASR wording substitutes `小` for the reference's `少`, but ASR wording is
outside this speaker-only decision. The contribution is the complete bounded
response to Shi's prompt and refers to Tang in the third person. It does not
belong to Tang. The preceding prompt remains under Shi and the following
explanation remains under Tang; FR43 changes neither neighboring contribution.

The same interval was then read in reverse from the later dilution discussion
back through Tang's explanation, Xu's response, Shi's prompt, and Tang's
original request for opinions. Tang's later explanation is a response to the
assessment that his share is low. Reading farther backward reaches the explicit
request for opinions and the two no-objection replies. The reverse context
therefore preserves the same Shi-to-Xu-to-Tang sequence and the same source
boundaries.

## Manual decision and ledger

The complete forward and reverse contextual semantic review **retains FR43**.
Only current T123 `ref-0194` changes from a critical confident-wrong Tang
attribution to accepted Xu attribution. T111 was already accepted and is
partition evidence only, so it is not counted again.

The manually reconciled frozen ledger is now `517/556`: 39 contributions
remain incorrect, comprising 33 confident-wrong, five missing, and one
uncertain. Twenty-four confident-wrong and one missing contribution remain
critical. The seven blocks are `87/93`, `79/84`, `75/80`, `75/80`, `118/129`,
`80/87`, and `3/3`; every complete 600-second natural-turn block remains above
its floor. The manually reviewed per-speaker natural-turn ledgers are Zhu Jie
`76/83`, Tang Yunfeng `174/189`, Xu Zijing `69/73`, and Shi Yi `198/211`. All
four per-speaker natural-turn floors remain passed.

Speaker-business closure remains open. Critical attribution and confident-
wrong attribution still fail, and speaker-time, per-speaker time, source-time-
offset, independent full real-path repeatability, locked holdout, report
review, and release signing remain unsigned. FR43 is a retained transitional
frozen experiment, not an industrial-release verdict.
