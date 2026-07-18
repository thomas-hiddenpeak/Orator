# Current-Commit Full Speaker Review (2026-07-17)

## Scope and claim boundary

This record seals the speaker-attribution evidence produced by clean commit
`6dbc600e4eb55eb7ea3329ce1efac77470f8233e`. It covers two complete
`3615.120`-second runs of `test.mp3` through the production incremental
WebSocket path at 1.0x input pacing. Run A started with an empty isolated
speaker registry. Run B started a new server process with the registry frozen
from Run A.

The product-result review is manual and contextual. No executable, script,
query, formula, notebook, metric, or algorithm assigned correctness, aggregated
accuracy, ranked a candidate, or issued the result. Tools captured the runs,
verified structural contracts and hashes, and arranged unjudged evidence.

This report signs only the full natural-business-turn speaker-attribution gate.
It does not sign the other conjunctive gates in Spec 013 and therefore does not
close T084 or Spec 013.

## Frozen inputs and build

| Evidence | SHA-256 |
|---|---|
| Source commit | `6dbc600e4eb55eb7ea3329ce1efac77470f8233e` |
| `build/orator_ws` | `63af598d1b8db458ecc54b1674fb849b89ed9ed50b846878f786b985b2728643` |
| `orator.toml` | `0d690eea6482518cfd866efb08215a8ffd29addc80530bc53d30035665bfb9a8` |
| `test.mp3` | `b7c25d1c349b02d654b6a406bc29039749e4240a4109dda4fcc905285b14b18b` |
| `test.txt` | `35e8695057be82f3028877f7dc159f10ecfa0ab7f06c444cefa9a079b0e24a86` |
| Complete CTest log | `ef5aedb52bdb4262ae339cca4ce890d5534fd799b4de9189ad834eb6104a6c62` |

The warning-clean build succeeded and the complete registered suite passed
`68/68`. The checked-in TOML selected streaming Sortformer v2.1 with the
`340/1/188/188` profile. Configuration, binary, source, commit, and worktree
identity remained stable before and after each run.

## Full-run evidence

| Evidence | Run A: empty registry | Run B: frozen registry |
|---|---|---|
| Timeline SHA-256 | `b5292303b974ee0374954e294228877441fd55cc9c7f8b6b6b182e139fb92968` | `3a2ced22bbb588e618e51860047de21dd107032a85180af94c6f9297c785c254` |
| Manifest SHA-256 | `8df3cbb7f165a52abe668a78406fe753c1e3fa700e55c8311d87d072ebfcc76c` | `de1d50262de146555dced5dc0a908aaeaef9dc7920df0011d6c1c0f90a0278a1` |
| Artifact ID | `orator-20260717T085317Z-6dbc600e4eb5-3615.120s` | `orator-20260717T095645Z-6dbc600e4eb5-3615.120s` |
| Push wall time | `3615.2 s` | `3615.2 s` |
| Total wall time | `3677.656 s` | `3677.666 s` |
| Stream RTF | `0.983x` | `0.983x` |
| Terminal tracks | diar 755, ASR 275, align 275, business 1754 | diar 755, ASR 275, align 275, business 1752 |
| Frozen registry SHA-256 | `66461a77755984a08231d06306da7ce9e1eeac07be1927e91f8a772fc54c7b3f` | unchanged |

Both manifests report no structural contract issue, exact time-base
reconciliation, complete required telemetry fields, and producer/early
observer/late observer convergence. Runtime telemetry cadence exceeded 95
percent in both runs. Continuous `tegrastats` recorded GPU utilization, memory,
temperature, GPU rail power, and total system power. No server error, CUDA
error, allocation failure, crash, or unexpected producer acceptance occurred.

These are mechanical execution facts, not product-accuracy judgments.

## Context-review evidence

Every reference row was retained in the forward packets, and the complete
session was also arranged in reverse fixed-block order. Unchanged speaker
sequences inherited their prior manual judgments only after source identity and
surrounding handoff context were reconciled. Every changed speaker sequence was
read again in its complete forward and reverse conversational context.

| Packet | SHA-256 |
|---|---|
| Run A forward, 556 references | `9d999d59ab0eeb580b0d411ee21eb2b1ae5046479958e4a7599491a4557dd4bc` |
| Run A reverse blocks | `86b75c00cd8164aa3b8259c27109cf096d6242ac0a790a470d404ec7d7bebf9b` |
| Run B forward, 556 references | `187d8a0effd12a4b26c3a1076f1e2add2962b4e16b50b957b2f6ce264bf3a274` |
| Run B reverse blocks | `788990a58ff26e206ceb3c13f12ff4c2c471115b0a5d8d3c3641276a358e4a94` |
| Run A changed-context display | `9dce6a546a7de8fd40edaf389f6e86f01857065a3d6a3446776191f03d0eb42d` |
| Run B changed-context display | `63e91e92aa46bcddda7c2716ca9029d2dd0f239c583fb26d683083e40ee04531` |

The changed-context displays contain no correctness field and supplied no
judgment or score.

## Manual natural-turn result

### Run A

The current output retains the earlier `ref-0221` error and introduces one new
incorrect contribution at `ref-0261`: the middle of Xu Zijing's contribution
`他能来定期监管我们的财务` is attributed to Tang Yunfeng. The other changed
contexts preserve the correct speaker or a valid natural multi-speaker handoff.

The 42 manually confirmed incorrect contributions are:

`ref-0009`, `ref-0024`, `ref-0045`, `ref-0049`, `ref-0058`, `ref-0061`,
`ref-0071`, `ref-0090`, `ref-0118`, `ref-0135`, `ref-0160`, `ref-0182`,
`ref-0221`, `ref-0241`, `ref-0249`, `ref-0250`, `ref-0252`, `ref-0253`,
`ref-0261`, `ref-0296`, `ref-0298`, `ref-0313`, `ref-0327`, `ref-0331`,
`ref-0333`, `ref-0338`, `ref-0341`, `ref-0354`, `ref-0375`, `ref-0390`,
`ref-0417`, `ref-0442`, `ref-0444`, `ref-0457`, `ref-0461`, `ref-0499`,
`ref-0504`, `ref-0505`, `ref-0506`, `ref-0507`, `ref-0509`, and `ref-0537`.

The reviewer manually established 514 accepted and 42 incorrect natural
contributions and manually calculated `514 / 556`, approximately `92.45%`.

### Run B

The current output repairs prior errors at `ref-0215` and `ref-0429`. It
introduces two errors: `ref-0102`, where Tang Yunfeng's `就不用开会了` is
attributed to Shi Yi, and `ref-0514`, where Shi Yi's critical negation `不行`
is mostly unknown. Uncertain output does not count as correct under Spec 013.

The 43 manually confirmed incorrect contributions are:

`ref-0009`, `ref-0024`, `ref-0045`, `ref-0049`, `ref-0058`, `ref-0061`,
`ref-0071`, `ref-0090`, `ref-0102`, `ref-0118`, `ref-0135`, `ref-0160`,
`ref-0182`, `ref-0221`, `ref-0241`, `ref-0249`, `ref-0252`, `ref-0253`,
`ref-0296`, `ref-0298`, `ref-0313`, `ref-0327`, `ref-0331`, `ref-0333`,
`ref-0338`, `ref-0341`, `ref-0375`, `ref-0390`, `ref-0417`, `ref-0432`,
`ref-0436`, `ref-0442`, `ref-0444`, `ref-0457`, `ref-0461`, `ref-0499`,
`ref-0504`, `ref-0505`, `ref-0506`, `ref-0507`, `ref-0509`, `ref-0514`, and
`ref-0537`.

The reviewer manually established 513 accepted and 43 incorrect natural
contributions and manually calculated `513 / 556`, approximately `92.27%`.

Both runs therefore pass the full natural-business-turn speaker gate. No
whole-session speaker permutation was found. This is not an overall Spec 013
acceptance verdict.

## T084 conjunctive-gate audit

| Gate | Current status | Evidence boundary |
|---|---|---|
| Full natural-turn speaker accuracy | Passed for A and B | Complete contextual semantic review above |
| Full speaker-time accuracy | Open | No manual source-time-block total from the completed contextual judgments |
| Fixed 600 s speaker blocks | Open | No signed per-block semantic totals |
| Per-speaker time and turn recall | Open | No signed per-speaker ledger totals |
| Critical speaker turns | Open | Criticality has not been signed for all 556 rows |
| Confident wrong attribution | Open | Confidence class has not been signed for all incorrect rows |
| Source-time offsets | Open | Attribution-affecting offsets have not been manually annotated at `test.txt`'s whole-second precision |
| ASR and silence gates | Open | Outside this speaker-only review |
| Forced alignment structure | Passed mechanically for A and B | 275 final ASR IDs and 275 aligned groups in each run; no manifest issue |
| Common time base | Passed mechanically for A and B | All seven terminal extents reconcile to `3615.120 s` |
| Live/terminal convergence | Server WS passed; full browser gate open | Producer and both observers agree; no full-run browser capture |
| Real-time input pacing | Passed mechanically for A and B | Both runs report `0.983x` stream RTF |
| Terminal timeline within 30 s | Open | The client waits for `flush` and then `end`; the manifest records only their combined wait |
| Stability and telemetry | Passed mechanically for A and B | No runtime error; required telemetry coverage and cadence pass |
| Engineering | Passed | Warning-clean build and `68/68` CTest |
| Repeatability and documentation | Open | Conjunctive product gates above remain unsigned |

The observed total post-push wall interval is about 62.46 seconds in each run,
but it combines two terminal requests. It cannot prove or disprove the
single-terminal `<= 30 s` requirement. The harness must record `flush` and
`end` waits separately before that gate can be signed.

## Current permitted statement

On clean commit `6dbc600e4eb5`, both required full-length canonical A/B runs
pass the Spec 013 natural-business-turn speaker-attribution threshold under
complete contextual semantic review. Full canonical closure, release sign-off,
ASR closure, and general industrial readiness remain open until every
conjunctive gate above is signed.
