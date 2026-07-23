# FR57 Speaker-Time Review

## Scope and authority

This record completes the remaining T102 speaker-time, per-speaker-time, and
source-time-offset audit for the accepted FR50 real-WebSocket baseline. The
frozen artifacts, hashes, identity map, review order, and decision boundary are
defined in `fr57-speaker-time-evidence-design-2026-07-23.md`.

The reviewer read the complete Run A and Run B conversations chronologically
and then independently from the final block to the first. The human-listened
`test/data/reference/test.txt` is the source-time and speaker authority. Runtime
decimal timestamps were used only to understand where evidence appeared; they
did not create finer reference truth.

No compiled code, script, notebook, spreadsheet formula, query, metric, or
algorithm assigned correctness, totalled judged time, calculated a result,
ranked the runs, or issued the decision below. Every judgment, total,
percentage, comparison, and verdict was made manually from complete
conversational context. Commands only displayed the frozen evidence or checked
its immutable provenance.

## Four independent readings

| Reading | Packet SHA-256 | Review result |
|---|---|---|
| Run A chronological | `730506a70dffeeef673051fdc9455fedb7e1e20bddd1f47e23a4beed63e13d69` | Complete; all source blocks and offsets reviewed |
| Run B chronological | `d9a580288aa745aa894e997d1e08911a29ab23b5cb893841b147c725044e17d7` | Complete; all source blocks and offsets reviewed independently |
| Run A reverse | `2e716af703b0045fadb43cd3804389be65f117add6fc961d858f5b74fc95adf6` | Complete; final block through first block, without importing forward duration labels |
| Run B reverse | `bb6fb4fff19e64d630a3bee5245d634bd9bb9c0e999072c0467b7b68a3032a4a` | Complete; final block through first block, without importing Run A duration labels |

All four readings lead to the same contextual time assignments recorded below.
That agreement is a manual review result, not an executable artifact
comparison.

## Source-time authority

The first human source mark is `00:00:03`; the last displayed source edge is
`01:00:15`. The applicable speaker-time denominator is therefore 3612 recorded
whole seconds. The first three audio seconds and the final 0.120 runtime seconds
have no human source-speaker truth and are not assigned to a speaker.

`ref-0160` is labelled Shi Yi in the source row, but the complete proposition
continues Tang Yunfeng's explanation about no longer attending the board. Its
five recorded seconds are therefore assigned to canonical Tang Yunfeng. This
is a contextual correction to the source speaker label, not a timestamp edit.

### Duplicate source timestamps

Every duplicate contribution remains in the 556 natural-turn denominator. At
the source's one-second precision, an earlier row at the same timestamp has no
independently measurable positive interval; the final row beginning at that
mark carries the following positive interval. No runtime decimal boundary was
used to expand an earlier duplicate row.

| Source mark | Duplicate contribution(s) | Positive source-time handling |
|---|---|---|
| `00:01:36` | `ref-0013` | `ref-0013` has no independently measurable positive seconds; `ref-0014` carries `96-98` |
| `00:02:49` | `ref-0021`, `ref-0022` | both have no independently measurable positive seconds; `ref-0023` carries `169-183` |
| `00:06:35` | `ref-0042` | no independently measurable positive seconds; `ref-0043` carries `395-398` |
| `00:07:47` | `ref-0061` | no independently measurable positive seconds; `ref-0062` carries `467-471` |
| `00:07:59` | `ref-0066` | no independently measurable positive seconds; `ref-0067` carries `479-488` |
| `00:08:13` | `ref-0071` | no independently measurable positive seconds; `ref-0072` carries `493-494` |
| `00:08:21` | `ref-0076` | no independently measurable positive seconds; `ref-0077` carries `501-505` |
| `00:08:26` | `ref-0079` | no independently measurable positive seconds; `ref-0080` carries `506-511` |
| `00:09:26` | `ref-0088` | no independently measurable positive seconds; `ref-0089` carries `566-567` |
| `00:13:27` | `ref-0118` | no independently measurable positive seconds; `ref-0119` carries `807-809` |
| `00:14:56` | `ref-0134`, `ref-0135` | both have no independently measurable positive seconds; `ref-0136` carries `896-902` |
| `00:19:01` | `ref-0170` | no independently measurable positive seconds; `ref-0171` carries `1141-1143` |
| `00:21:43` | `ref-0192` | no independently measurable positive seconds; `ref-0193` carries `1303-1313` |
| `00:39:15` | `ref-0329` | no independently measurable positive seconds; `ref-0330` carries `2355-2363` |
| `00:40:26` | `ref-0341` | no independently measurable positive seconds; `ref-0342` carries `2426-2428` |
| `00:43:50` | `ref-0371` | no independently measurable positive seconds; `ref-0372` carries `2630-2633` |
| `00:44:50` | `ref-0390` | no independently measurable positive seconds; `ref-0391` carries `2690-2692` |
| `00:46:35` | `ref-0414` | no independently measurable positive seconds; `ref-0415` carries `2795-2800` |
| `00:46:44` | `ref-0417` | no independently measurable positive seconds; `ref-0418` carries `2804-2808` |
| `00:46:49` | `ref-0420` | no independently measurable positive seconds; `ref-0421` carries `2809-2811` |
| `00:48:35` | `ref-0444` | no independently measurable positive seconds; `ref-0445` begins the following reconciled block |
| `00:59:05` | `ref-0543` | no independently measurable positive seconds; `ref-0544` carries `3545-3547` |

### Backward source timestamp

`ref-0445` is marked `2915-2933`, `ref-0446` is marked `2933-2932`, and
`ref-0447` is marked `2932-2935`. Complete context places Shi Yi's pricing
question before Tang Yunfeng's "five or six million" answer. To preserve one
source clock without overlap or double counting, the manual interpretation is:

| Contribution | Contextual source block |
|---|---|
| `ref-0445` Shi Yi | `2915-2932`, 17 seconds |
| `ref-0446` Xu Zijing | no independently measurable positive seconds |
| `ref-0447` Tang Yunfeng | `2932-2935`, 3 seconds |

The review did not sort these rows, take an absolute duration, interpolate a
boundary, or count any second twice.

### Fixed-block crossings

| Contribution | Speaker | Manual split at fixed boundary |
|---|---|---|
| `ref-0093` | Shi Yi | `573-600`: 27 seconds; `600-623`: 23 seconds |
| `ref-0177` | Shi Yi | `1178-1200`: 22 seconds; `1200-1207`: 7 seconds |
| `ref-0257` | Zhu Jie | `1786-1800`: 14 seconds; `1800-1802`: 2 seconds |
| `ref-0337` | Tang Yunfeng | `2393-2400`: 7 seconds; `2400-2406`: 6 seconds |
| `ref-0466` | Xu Zijing | `2998-3000`: 2 seconds; `3000-3013`: 13 seconds |
| `ref-0553` | Tang Yunfeng | `3586-3600`: 14 seconds; `3600-3601`: 1 second |

## Manual reference denominators

The reference blocks were first added in chronological order and then added
again from the reverse review record. Both manual additions agree.

| Source block | Zhu Jie | Tang Yunfeng | Xu Zijing | Shi Yi | Total source seconds |
|---|---:|---:|---:|---:|---:|
| `0-600` | 165 | 183 | 113 | 136 | 597 |
| `600-1200` | 9 | 198 | 9 | 384 | 600 |
| `1200-1800` | 23 | 130 | 80 | 367 | 600 |
| `1800-2400` | 142 | 207 | 83 | 168 | 600 |
| `2400-3000` | 82 | 215 | 32 | 271 | 600 |
| `3000-3600` | 124 | 181 | 47 | 248 | 600 |
| `3600-3615` (reported only) | 0 | 6 | 0 | 9 | 15 |
| **Full source extent** | **545** | **1120** | **364** | **1583** | **3612** |

## Contextual time adjustments

The following table contains every contribution whose time result is not a
straightforward full acceptance of its positive source block. It applies to
Run A and Run B after each was reviewed independently. A zero-second row still
retains its natural-turn judgment.

| Ref | Canonical speaker | Source seconds | Correct speaker seconds | Lost speaker seconds | Complete-context judgment |
|---|---|---:|---:|---:|---|
| `0049` | Tang | 1 | 0 | 1 | Tang's short agreement is absent; surrounding output remains Shi/Zhu |
| `0058` | Tang | 1 | 0 | 1 | the Tang value is not represented under Tang |
| `0063` | Shi | 1 | 0 | 1 | missing short Shi response |
| `0066` | Tang | 0 | 0 | 0 | wrong natural turn, but duplicate precision provides no positive time |
| `0099` | Shi | 3 | 2 | 1 | Shi is correct at `660-661` and `662-663`; `661-662` is assigned to Zhu |
| `0102` | Tang | 2 | 0 | 2 | repeated phrase is emitted under Shi rather than Tang |
| `0118` | Tang | 0 | 0 | 0 | wrong natural turn with no independently measurable positive time |
| `0135` | Shi | 0 | 0 | 0 | wrong natural turn with no independently measurable positive time |
| `0171` | Shi | 2 | 0 | 2 | source contribution is carried by Tang |
| `0221` | Xu | 1 | 0 | 1 | Xu confirmation is not represented under Xu |
| `0239` | Xu | 2 | 0 | 2 | Xu reaction is carried by Tang |
| `0241` | Xu | 3 | 0 | 3 | Xu question is carried by Tang |
| `0249` | Zhu | 5 | 2 | 3 | substantive Zhu question is retained, but the mixed preface is not Zhu-supported |
| `0252` | Zhu | 2 | 0 | 2 | Zhu response is carried by Tang |
| `0298` | Tang | 3 | 0 | 3 | Tang turn begins later in candidate evidence |
| `0313` | Shi | 1 | 0 | 1 | Shi confirmation is not represented under Shi |
| `0331` | Shi | 1 | 0 | 1 | short Shi response is absent |
| `0333` | Shi | 4 | 0 | 4 | Shi response is carried by Tang |
| `0341` | Tang | 0 | 0 | 0 | missing natural turn with no independently measurable positive time |
| `0354` | Zhu | 1 | 0 | 1 | Zhu confirmation is carried by Tang |
| `0390` | Tang | 0 | 0 | 0 | wrong natural turn with no independently measurable positive time |
| `0409` | Zhu | 2 | 0 | 2 | Zhu comment is missing |
| `0426` | Shi | 3 | 0 | 3 | Shi phrase is carried by Tang/Xu evidence |
| `0432` | Zhu | 2 | 1 | 1 | recognizable Zhu response occupies one whole-second block; unsupported Tang suffix occupies the next |
| `0442` | Zhu | 1 | 0 | 1 | Zhu price question is absent at the source edge |
| `0444` | Zhu | 0 | 0 | 0 | wrong natural turn with no independently measurable positive time |
| `0457` | Shi | 1 | 0 | 1 | Shi reaction is carried by Xu |
| `0461` | Tang | 2 | 0 | 2 | Tang explanation is carried by Shi |
| `0499` | Shi | 4 | 0 | 4 | no independent Shi evidence survives; candidate remains Zhu/Tang/unknown |
| `0503` | Zhu | 31 | 1 | 30 | only the opening whole-second block has Zhu evidence; later substantive clauses are Shi |
| `0505` | Tang | 6 | 0 | 6 | semantic clause appears earlier under Shi and is not valid Tang time here |
| `0506` | Tang | 3 | 0 | 3 | uncertain Tang source block has no reliable Tang attribution |
| `0537` | Tang | 1 | 0 | 1 | Tang label is displaced into Shi's longer output |

All other positive source blocks are fully speaker-correct under complete
context. This statement does not evaluate ASR wording.

## Attribution-affecting source offsets

| Context | Signed interpretation |
|---|---|
| `ref-0160` | source label says Shi; complete proposition is Tang, so all five seconds are counted to canonical Tang |
| `ref-0182` | isolated row appears edge-displaced, but neighbouring Xu context supports Xu at `1241-1242`; no speaker second is lost |
| `ref-0249` / `ref-0250` | Zhu's source block has a mixed preface and loses three seconds; Tang's "not necessarily" occurs just after the displayed edge and remains accepted at whole-second precision |
| `ref-0268` | rapid Zhu/Xu handoff at `1932-1933`; Zhu's reaction is recognizable and correctly attributed in its source second |
| `ref-0406` | a preceding Zhu confirmation reaches the displayed edge, while Tang owns the substantive `2745-2746` answer; the source second remains Tang-correct |
| `ref-0432` | Zhu response is followed by an unsupported Tang suffix; one of the two source seconds is retained and one is lost |
| `ref-0442` | Zhu question is absent at its one-second source edge; that second is lost |
| `ref-0446` / `ref-0447` | backward pair is reconciled at `2932` as documented above, without overlap or invented sub-second truth |
| `ref-0504` / `ref-0505` | `ref-0504` Tang phrase remains accepted; the `ref-0505` semantic clause appears early under Shi, so all six `ref-0505` seconds are lost without penalizing `ref-0504` twice |
| `ref-0517` | Tang question begins before the displayed `3379` edge, but context identifies Tang and contains no wrong-speaker substantive block; full source time is retained |
| `ref-0518` | Zhu response appears before the displayed `3386` edge and is correctly Zhu; full source time is retained |

No sub-second accuracy statistic is claimed from these observations.

## Independently checked manual totals

The chronological record and the reverse record were totalled separately for
each run. The four manual totals agree at 3529 correctly attributed source
seconds and 83 lost source seconds out of 3612 applicable seconds.

| Run and reading | Correct source seconds | Lost source seconds | Applicable source seconds | Manually calculated full result |
|---|---:|---:|---:|---:|
| Run A chronological | 3529 | 83 | 3612 | approximately 97.70% |
| Run A reverse | 3529 | 83 | 3612 | approximately 97.70% |
| Run B chronological | 3529 | 83 | 3612 | approximately 97.70% |
| Run B reverse | 3529 | 83 | 3612 | approximately 97.70% |

### Complete fixed blocks

Run A and Run B independently have the same manually reviewed block totals.
The final partial block is reported for completeness but is not one of the six
complete-block gates.

| Source block | Correct seconds | Lost seconds | Applicable seconds | Manually calculated result | Gate |
|---|---:|---:|---:|---:|---|
| `0-600` | 594 | 3 | 597 | approximately 99.50% | pass |
| `600-1200` | 595 | 5 | 600 | approximately 99.17% | pass |
| `1200-1800` | 589 | 11 | 600 | approximately 98.17% | pass |
| `1800-2400` | 591 | 9 | 600 | approximately 98.50% | pass |
| `2400-3000` | 589 | 11 | 600 | approximately 98.17% | pass |
| `3000-3600` | 556 | 44 | 600 | approximately 92.67% | pass |
| `3600-3615` | 15 | 0 | 15 | 100.00% | reported only |

### Canonical speakers

| Canonical speaker | Correct seconds | Lost seconds | Applicable seconds | Manually calculated result | Gate |
|---|---:|---:|---:|---:|---|
| Zhu Jie | 505 | 40 | 545 | approximately 92.66% | pass |
| Tang Yunfeng | 1101 | 19 | 1120 | approximately 98.30% | pass |
| Xu Zijing | 358 | 6 | 364 | approximately 98.35% | pass |
| Shi Yi | 1565 | 18 | 1583 | approximately 98.86% | pass |

The reverse additions were cross-checked by speaker and by fixed block. Zhu's
40 lost seconds are the reviewed `ref-0249`, `ref-0252`, `ref-0354`,
`ref-0409`, `ref-0432`, `ref-0442`, and `ref-0503` blocks. Tang's 19 are
`ref-0049`, `ref-0058`, `ref-0102`, `ref-0298`, `ref-0461`, `ref-0505`,
`ref-0506`, and `ref-0537`. Xu's six are `ref-0221`, `ref-0239`, and
`ref-0241`. Shi's 18 are `ref-0063`, `ref-0099`, `ref-0171`, `ref-0313`,
`ref-0331`, `ref-0333`, `ref-0426`, `ref-0457`, and `ref-0499`. Duplicate
timestamp turn errors contribute no independently measurable positive seconds.

## T102 decision

Both frozen FR50 runs pass the manually reviewed 90.0 percent full-session
speaker-time gate, every complete 600-second speaker-time gate, and every
canonical-speaker time gate. Previously signed FR50 fixed-block and per-speaker
natural-turn gates also remain passed. Every material source-time offset is
accounted for above. T102's required evidence breakdown is therefore complete.

This does **not** complete T084 or speaker-business closure. FR50 still has 19
critical residuals, 26 confidently wrong natural contributions, four missing
contributions, and one uncertain contribution. The critical-attribution and
confident-wrong-zero gates remain failed even though the duration gates pass.
ASR accuracy, independent holdout, physical microphone/browser evidence,
report review, release sign-off, and industrial readiness are not claimed.

No runtime code, TOML parameter, model, product artifact, natural-turn ledger,
or FR50 baseline changed during FR57.

The unchanged source tree also completes a warning-clean build and all `74/74`
configured CTest entries in `53.46 s`. This is an engineering consistency check
only; it does not evaluate or support the manual product verdict.
