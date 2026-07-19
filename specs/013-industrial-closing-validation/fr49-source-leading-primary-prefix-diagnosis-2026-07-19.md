# FR49 Source-Leading Primary-Prefix Diagnosis (2026-07-19)

## Scope and authority

This record completes the frozen FR49 short-primary investigation, bounded
implementation, and candidate review. It uses the exact FR47 full A/B
timelines, capture-faithful Sortformer v2.1 posterior, session-wide primary-run
TitaNet evidence, identity epochs, and checked-in TOML. It does not rerun audio,
change a numeric parameter or model, or alter a producer track.

`test/data/reference/test.txt` is the authoritative human-listened reference.
No executable code, script, query, formula, metric, or algorithm assigned
correctness, counted an accepted result, grouped or ranked a topology, selected
the candidate below, or issued its verdict. Automation only copied and ordered
raw evidence, rendered complete context, checked schemas and hashes, and
confirmed A/B structural equality. Every product finding and ledger correction
below comes from complete forward and reverse contextual semantic reading.

## Frozen evidence

The reference-free exporter emits every short primary run that intersects a
positive aligned unit, a zero-duration unit, or an alignment gap and conflicts
with a mechanically associated current business piece. Each record contains
raw four-channel posterior frames, activity, primary, VAD, source/alignment,
voiceprint scores and completeness, identity epochs, current business pieces,
and immediate controls. It contains no reference ID, expected speaker,
correctness field, candidate score, aggregate, or verdict.

| Artifact | Result |
|---|---|
| Run A inventory | 271 records |
| Run B inventory | 271 records |
| A/B records SHA-256 | `3acf4a1bbbdf84498929d0c7ae348b0e016417f0a882ba8dd533c066e81e60e0` |
| Run A inventory JSON SHA-256 | `ac4d77a8d8332085c230c02fdcf8776ef93f9bced1cb717c063ea2783f518649` |
| Run B inventory JSON SHA-256 | `d7384feddde9e4ff4279f5df39e7b1d56cb5e8aaeb3988383de14d31011b2dba` |
| Run A chronological worksheet | `abf882f46989d38690fe2921e0f443f2aae4714feb5aec27411cf647908899b3` |
| Run A reverse-window worksheet | `abad0cb5cef5aff2c3d10d5c79e0ac339215f35f62872a884f0fee9a9e4a423c` |
| Run B chronological worksheet | `c9a5c4e6f6e315961be5276ece4ebc8466cb274c7206716fa6f11e314b94951c` |
| Run B reverse-window worksheet | `e632858cf2314342cc375e3f536ba2e31d0e01a89961bf7b93e21ad3e2724594` |

The worksheet bodies are identical between A and B; only the displayed source
timeline path differs. All 271 evidence records, all previously signed
residuals, and accepted neighboring controls were read in chronological and
reverse conversational order.

## Ledger erratum

The full review found one prior manual omission. `ref-0121` is 石一 saying
`你不低于...` between 唐云峰's preceding explanation and his continuation
`我不是，三轮融资之后...`. The current business view assigns the complete
`817.692-820.572` source phrase `你不低于我不是三轮融资之后，` to 唐云峰.
The first four aligned characters are therefore materially misattributed.

Before any candidate change, the corrected frozen speaker-only ledger is
`521/556`: 29 confident-wrong, five missing, and one uncertain. The unchanged
20 critical residuals remain open. This is a manually transcribed and
cross-checked correction, not a code-produced total. The newly complete
noncritical confident-wrong list adds `ref-0121` to `ref-0061`, `ref-0135`,
`ref-0171`, `ref-0221`, `ref-0239`, `ref-0241`, `ref-0298`, `ref-0457`, and
`ref-0537`.

## Two independent material contexts

`ref-0061` and `ref-0121` share one exact source-free topology:

| Evidence | `ref-0061` | `ref-0121` |
|---|---|---|
| Short primary identity | 石一, `467.440-467.920` | 石一, `817.600-818.560` |
| Short-primary duration | `0.480 s` | `0.960 s` |
| Candidate activity | 石一 fully covers the primary | 石一 fully covers the primary |
| Existing source prefix | `我`, `467.564-467.644` | `你不低于`, `817.692-818.412` |
| Positive right gap | `467.644-467.884` | `818.412-818.652` |
| Following primary | 唐云峰, starts `467.920` | 唐云峰, starts `818.560` |
| Following business source | `在看绝对通过的问题` | `我不是三轮融资之后` |
| Current absorbed identity | 唐云峰 | 唐云峰 |
| Primary-run TitaNet | session/robust both rank 石一 first | session/robust both rank 石一 first |

Both short primary runs meet the existing `0.4 s` embedding floor and remain
below the existing `1.5 s` short ceiling. Both source prefixes are complete
positive-duration business intervals wholly contained by the candidate
primary. Both are followed by a positive `0.24 s` alignment gap, within the
existing `0.25 s` pause gate. The next primary begins exactly at the candidate
end, has a different identity, lasts beyond the same `0.4 s` floor, and agrees
with the currently absorbed identity and the source-adjacent continuation.

The primary-run TitaNet evidence is independent of source text. In both
contexts the complete session and robust galleries rank 石一 first and both
configured score gates pass. At least one configured `0.04` margin gate passes
in each context. No new score, duration, or tolerance is fitted.

## Abstention boundary

Complete contextual review rejects superficially similar controls:

| Control | Required abstention |
|---|---|
| `short_primary:644` | The current business identity differs from the following primary identity. |
| `short_primary:713` | The candidate lies inside a longer business source interval and has no primary-run embedding. |
| `short_primary:753` | No complete positive aligned unit is contained by the short primary. |
| `short_primary:849` | The primary is only `0.24 s`, below the existing embedding floor. |
| `short_primary:1014` | Candidate activity is absent. |
| `short_primary:1249` | The following primary is subminimum and both TitaNet galleries prefer the incumbent, not the candidate. |
| `short_primary:1265` | There is no contiguous following primary or source-adjacent continuation. |
| `short_primary:748` / `ref-0304` | The short-primary identity already owns positive-duration source inside the same primary before the proposed prefix; extending it into 唐云峰's next contribution is a candidate-tail leak, not a swallowed source-leading repair. |

`ref-0118` remains diagnostic because its `0.32 s` primary is below the
existing floor. `ref-0417` lacks candidate activity. `ref-0066` and other
source-absent contributions cannot be repaired because no immutable source
characters exist to receive an attribution. Zero-duration-only and gap-only
representations likewise remain unchanged.

## Authorized implementation

One false-by-default TOML switch,
`speaker_fusion.source_leading_primary_prefix_enable`, controls both evidence
production and policy. When enabled, `SpeakerEvidenceStage` emits a
source-independent `primary_run:N` TitaNet query for each sorted primary run.
`SpeakerFusionPolicy` may restore a source prefix only when every FR49
condition is true: candidate duration, complete candidate/following activity,
contiguous longer following primary, complete dual-gallery candidate rank,
configured score/margin gates, exact source-leading business interval,
positive aligned characters, bounded positive alignment gap, uniform absorbed
identity, source-adjacent continuation, no immediately preceding source owned
by the following identity, no earlier candidate-owned positive source inside
the same short primary, and no third activity over the prefix.

Only the existing prefix characters may change identity. Text, source indices,
producer intervals, clocks, all non-prefix labels, and every source-absent row
remain immutable. Frozen A/B replay, disabled control, raw changed-scope
display, complete changed-context review, clean build, and complete CTest are
required before the candidate can be retained. A new audio run remains
unauthorized at this stage.

## First frozen replay correction

The first implementation replay is rejected as an intermediate candidate. A/B
and repeat hashes are stable, and the disabled control exactly reproduces the
FR48 replay, but the raw changed scope contains three writes rather than the
two authorized contexts. The additional `2177.260-2177.500` write assigns
`我跟` to 徐子景 inside `ref-0304`. Complete forward and reverse context in
`test.txt` identifies 唐云峰 as the owner of the whole contribution
`反正你就切开试一下吧`; the preceding 徐子景 contribution ends before it.

The raw evidence explains the false trigger: primary/activity `spk_2` already
owns positive-duration source inside the same short primary and then leaks
across the boundary before contiguous primary/activity `spk_1` takes over.
`ref-0061` has no earlier candidate-owned source inside its short primary.
`ref-0121` has an earlier 石一 source character, but that character ends before
the short primary begins. The corrected contract therefore abstains only when
candidate-owned positive source already exists earlier inside the same short
primary. No threshold, model output, source coordinate, or reference-derived
runtime label is added. Focused regression and a fresh complete frozen replay
are required.

## Final frozen replay

The corrected policy was replayed twice against each frozen A/B input. The
candidate outputs are byte-identical across all four replays. The independently
generated disabled controls are byte-identical to one another and exactly
reproduce the FR48 baseline. These are mechanical reproducibility findings; no
hash or program assigns a product judgment.

| Artifact | Mechanical result |
|---|---|
| Combined primary evidence A/B SHA-256 | `aa9ec569db37e5341be9b79c16015a2509e205692dcb702d94cb5c0796ee6462` |
| Four-channel frame evidence A/B SHA-256 | `79fd2c416ac76a0af477f98bf8d848f6e604b2d94c5c4445e653978afd6c7e41` |
| Candidate A1/A2/B1/B2 | 1,718 business entries; SHA-256 `91e1e7ab08f6c593b73762b158b5c4ee9c58eaf68ea59eb8f9ee34c21f747c30` |
| Disabled A/B control | 1,716 business entries; SHA-256 `75fc0b39fdf4530ec98a54f8e6ac113e8eef1aee00839c3d9c6577adafb8302e` |

The raw normalized changed-scope display contains exactly two existing source
ranges:

- `467.564-467.644` `我` changes from 唐云峰 to 石一; the continuation beginning
  at `467.884` remains 唐云峰.
- `817.692-818.412` `你不低于` changes from 唐云峰 to 石一; the continuation
  beginning at `818.652` remains 唐云峰.

No text, source index, producer interval, clock value, or other speaker label
changes. In particular, `2177.260-2178.220` remains wholly 唐云峰 in
`ref-0304`, so the rejected candidate-tail leak is absent.

## Complete contextual semantic review

The changed-context packets contain `ref-0061`, `ref-0062`, `ref-0121`, and
`ref-0122`. Complete context establishes that 石一 says `我说` before 唐云峰's
`他在看绝对通过的问题`, and that 石一 says `你不低于...` before 唐云峰's
`我不是，三轮融资之后...`. The two restored prefixes are correct and both
neighboring continuations remain correct.

That local review was not used as a substitute for a full result. Every one of
the 556 `test.txt` contributions was then read in complete chronological order
and again in reverse fixed-window order for candidate A, followed by a separate
complete chronological and reverse reading for candidate B. No packet was
truncated. The evidence packets are frozen as follows:

| Packet | SHA-256 |
|---|---|
| Candidate A chronological | `6173e3ad73622d4faa1f27d70cd5002632bc4f592a7e12e2e8877cd1170ec496` |
| Candidate A reverse windows | `046f858d0e90e9804e09b3de8e331dbf0eb935fcfa45fd7e9699e0037fe27c0c` |
| Candidate B chronological | `d3cc4c357488a6d58d1aac7b865d6f498b6b28ba897bcf0f5aa1984f75edd445` |
| Candidate B reverse windows | `22e6762027906bd080891d8c762f68819baa17096d30643bdf69599390814ba2` |
| Candidate A changed contexts | `5f3caa324999a06853042e308696c053ca15fdaefe3b62d478b5ba40e72b8786` |
| Candidate B changed contexts | `d5ddc3e34d1f75baf30f014c0c1b0b9d7bd47aff846048cf5e1235ac37dafc41` |

All four complete readings produce the same manual speaker-only decisions.
Relative to the corrected pre-candidate ledger, only `ref-0061` and
`ref-0121` move from confident-wrong to accepted. The manually transcribed
candidate ledger is therefore `523/556`, with 27 confident-wrong, five missing,
and one uncertain contribution. The seven manually cross-checked fixed-window
ledgers are `89/93`, `79/84`, `76/80`, `75/80`, `119/129`, `82/87`, and `3/3`.
The per-speaker ledgers are 朱杰 `77/83`, 唐云峰 `176/189`, 徐子景 `70/73`, and
石一 `200/211`.

The 20 business-critical residuals remain unchanged:

`0049`, `0058`, `0066`, `0099`, `0102`, `0118`, `0252`, `0313`, `0327`,
`0331`, `0333`, `0354`, `0390`, `0426`, `0442`, `0444`, `0461`, `0499`,
`0503`, and `0505`.

The remaining noncritical residuals are:

| Class | References |
|---|---|
| Confident-wrong | `0135`, `0171`, `0221`, `0239`, `0241`, `0298`, `0457`, `0537` |
| Missing | `0063`, `0341`, `0409`, `0417` |
| Uncertain | `0506` |

## Engineering verification

The final `--clean-first` build completes without a `warning:` or `error:`
diagnostic. The complete registered suite passes `71/71` CTest entries in
`53.22 s`, including the new exporter regression and focused evidence/fusion
cases. Both new Python files also pass `python3 -m py_compile`, and
`git diff --check` reports no whitespace error. These checks validate software
and evidence contracts only; they do not assign or confirm the product result.
After the clean build, two fresh A and two fresh B candidate replays reproduce
the recorded candidate SHA byte for byte; fresh disabled A/B controls likewise
reproduce the recorded disabled SHA. The reviewed artifacts therefore remain
exactly representative of the final binary.

## Decision

FR49 is retained as a bounded frozen candidate. It repairs two independent
noncritical source-leading contributions and preserves every other manually
reviewed contribution in both A and B. It is not yet a real-WebSocket result
and is not speaker-business closure: all 20 critical failures remain, the
confident-wrong-zero gate remains open, and speaker-time, per-speaker time,
source-time offset, holdout, ASR, browser/microphone, final-report, and release
gates remain unsigned. No new audio result or industrial-readiness claim is
authorized by this record.
