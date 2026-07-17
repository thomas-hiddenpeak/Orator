# Direct-End Full Speaker Review (2026-07-18)

## Scope and claim boundary

This record seals the full-length direct-end evidence produced by clean commit
`588bfbe635558e256c823ee1e58e4b13f4a8e18e`. Run A started with an empty
isolated speaker registry. Run B started a new server process with the registry
frozen from Run A. Both runs streamed all `3615.120` seconds of `test.mp3`
through the production incremental WebSocket path at 1.0x input pacing and sent
`end` directly after the final audio frame. Neither run sent `flush`.

The product-result review is manual and contextual. No executable, script,
query, formula, notebook, metric, or algorithm assigned correctness, aggregated
accuracy, ranked a candidate, or issued the result. Tools captured the runs,
verified structural contracts and hashes, and arranged unjudged evidence.

This report signs the full natural-business-turn speaker-attribution gate and
the mechanical direct-end latency gate only. It does not sign the remaining
conjunctive gates in Spec 013 and therefore does not close T084 or Spec 013.

This is the pre-FR16ABM direct-end baseline. The later clean-commit promotion,
full A/B recapture, and complete contextual review are recorded in
`native-handoff-full-promotion-review-2026-07-18.md`; this historical report is
not rewritten with the later result.

## Frozen inputs and build

| Evidence | SHA-256 |
|---|---|
| Source commit | `588bfbe635558e256c823ee1e58e4b13f4a8e18e` |
| `build/orator_ws` | `913661613d47bb7c93edd34abc9ddc6f69e5965c2ac7248240c04a6723e19e02` |
| Acceptance TOML | `d6056d75dc1ac72569a11b1d96b78edb6b8f7bf5fa310c9f6f07cbd49f439538` |
| `test.mp3` | `b7c25d1c349b02d654b6a406bc29039749e4240a4109dda4fcc905285b14b18b` |
| `test.txt` | `35e8695057be82f3028877f7dc159f10ecfa0ab7f06c444cefa9a079b0e24a86` |
| Sortformer v2.1 | `d036020b6b93977098929d417b1b106a952ec02cc38cafc9d3315ae0ec4d90b8` |
| Frozen registry | `66461a77755984a08231d06306da7ce9e1eeac07be1927e91f8a772fc54c7b3f` |

The acceptance TOML differs from the checked-in `orator.toml` only in the
isolated speaker-registry and storage paths. All behavioral values are
unchanged. It selects streaming Sortformer v2.1 with the `340/1/188/188`
profile and expresses every runtime setting in TOML. The exact commit had
already passed a warning-clean build and all 68 registered CTest entries.
Configuration, binary, source, commit, and clean-worktree identity remained
stable throughout both runs.

## Full-run mechanical evidence

| Evidence | Run A: empty registry | Run B: frozen registry |
|---|---|---|
| Timeline SHA-256 | `15892a4d66e91f44e7f5de3c9f452ee43df0d741dad2be18dd1f5c55f000d4f0` | `75f74c8d5889efd6536314908b7c5fc3fbb20b1d778b1a48c2928f82241a5729` |
| Manifest SHA-256 | `deaed9fb6e6f564750ff767ab27834f8e1ca2f44a6a1a776e8088c9d21d84d76` | `93b8932434699b8c72ede4d14d34e074377e06b0d7d732bfb02fd9ba158ddff0` |
| Fixture manifest SHA-256 | `c304f080caae7422f4091e99f1adf95339fb17cff030489d5cbffa00d3dba639` | `386fefbdac33d35441d341963b484da8f11b2c576193309e1c0b0d6cccb7a8f4` |
| Artifact ID | `orator-20260717T175046Z-588bfbe63555-3615.120s` | `orator-20260717T195136Z-588bfbe63555-3615.120s` |
| Push wall time | `3615.2 s` | `3615.2 s` |
| Total wall time | `3641.028 s` | `3641.505 s` |
| Stream real-time factor | `0.993x` | `0.993x` |
| Direct `end` wait | `25.597 s` | `26.305 s` |
| Diar / ASR / align | `755 / 275 / 275` | `755 / 275 / 275` |
| Voiceprint / business | `16072 / 1747` | `16083 / 1757` |
| Terminal observer SHA-256 | `60a4108764515347c5e500507aad5acf5655013b1506a2ab51799f4302e352bc` | `f8f086697bf838f7eef705dbb46246ebf8fb4e16b7963b5100b39dac3dc7b9bb` |

Both artifacts report no structural contract issue. All seven track extents
reconcile to `57,841,920` common-clock samples with zero gap, and time-base and
wall-clock checks pass. Producer, early observer, and late observer received
the same terminal payload. Required GPU utilization, GPU memory, system power,
CPU, RAM, and temperature fields have complete coverage. Runtime sample cadence
was `95.214%` for Run A and `95.519%` for Run B; continuous `tegrastats`
cadence was complete. No server error, CUDA error, allocation failure, crash,
or unexpected producer acceptance occurred.

An earlier Run B orchestration attempt was externally terminated before the
audio completed and produced no terminal artifact. It is excluded from all
evidence and judgments. The restarted Run B above is the only signed B run;
its registry remained byte-identical before and after the run.

These are mechanical execution facts, not product-accuracy judgments.

## Context-review evidence

Every reference row is present in each forward packet, and each complete
session is also arranged in reverse fixed-block order. Source audio, reference,
speaker mapping, policy, and frozen registry match the prior complete manual
review at commit `6dbc600e4eb5`. An unchanged speaker sequence inherited its
prior manual judgment only after its source identity and surrounding handoff
context were reconciled. All 17 changed Run A sequences and all 27 changed Run
B sequences, including adjacent fused and duplicate-timestamp contributions,
were read again in complete forward and reverse conversational context.

| Packet | SHA-256 |
|---|---|
| Run A forward, 556 references | `953c08cd118e3aab01f67860a04c9c8c2ec641a175a3c89178e0e86e66cdfd9e` |
| Run A reverse blocks | `fbe815b76c85fdfd1c8c3360fd3c5b258e64bf7c6610c1545e2f25abb4e1a3c1` |
| Run B forward, 556 references | `660071591613e3abd7721bad356a0ae8633a31ba6673d93cce18b9f765e92afc` |
| Run B reverse blocks | `32f6163c3901d780ac15d991851085f9920cd371718fc33f3d1b24e34ef763b2` |
| Run A changed-sequence display | `5c525601473621291ef183107e0e2bb33f331803ee0fe60a7cdace93d2ca11f2` |
| Run A changed-context windows | `044c1b0ac6bdddf7c2122d58c304fa891ecf089a160bdf9611a1828dbc228634` |
| Run B changed-sequence display | `69af4cae8613e8c5dc35967678c518a938f2c634501159509308f06b5ac33ac7` |
| Run B changed-context windows | `35e2e3f87f6d63dd93662915286b97814afba9dc0773702094c0b7cdb2a3a5aa` |

The displays contain no correctness field and supplied no judgment or score.

## Manual natural-turn result

### Run A

Run A repairs `ref-0261`: Xu Zijing's complete financial-supervision
contribution is now attributed to `spk_2`. It introduces three errors:

- `ref-0102`: Tang Yunfeng's repeated `just do not hold the meeting` turn is
  attributed to Shi Yi.
- `ref-0192`: Zhu Jie's short `no objection` insertion has no `spk_0` evidence
  and is absorbed into Shi Yi's surrounding turn.
- `ref-0194`: Xu Zijing's complete `Lao Tang has a little too little`
  contribution is attributed to Tang Yunfeng.

The other changed contexts preserve the correct natural speaker or a valid
multi-speaker handoff. The 44 manually confirmed incorrect contributions are:

`ref-0009`, `ref-0024`, `ref-0045`, `ref-0049`, `ref-0058`, `ref-0061`,
`ref-0071`, `ref-0090`, `ref-0102`, `ref-0118`, `ref-0135`, `ref-0160`,
`ref-0182`, `ref-0192`, `ref-0194`, `ref-0221`, `ref-0241`, `ref-0249`,
`ref-0250`, `ref-0252`, `ref-0253`, `ref-0296`, `ref-0298`, `ref-0313`,
`ref-0327`, `ref-0331`, `ref-0333`, `ref-0338`, `ref-0341`, `ref-0354`,
`ref-0375`, `ref-0390`, `ref-0417`, `ref-0442`, `ref-0444`, `ref-0457`,
`ref-0461`, `ref-0499`, `ref-0504`, `ref-0505`, `ref-0506`, `ref-0507`,
`ref-0509`, and `ref-0537`.

The reviewer manually established 512 accepted and 44 incorrect natural
contributions and manually calculated `512 / 556`, approximately `92.09%`.

### Run B

Run B repairs `ref-0514`: Shi Yi now owns both the lead-in and the aligned
evidence at the critical-negation location. The ASR words at that location are
still wrong, but lexical quality is outside this speaker-only judgment. It also
repairs the Shi Yi portion at `ref-0135`; the fused `I just, right?` output is
now wholly `spk_3`, so Tang Yunfeng's adjacent duplicate-timestamp `ref-0134`
becomes incorrect instead. That boundary movement has no net effect on the
count.

Run B introduces five further errors:

- `ref-0116`: the middle of Tang Yunfeng's question is attributed to Xu Zijing.
- `ref-0127`: Shi Yi's short acknowledgment is attributed to Tang Yunfeng.
- `ref-0192`: Zhu Jie's short `no objection` insertion is absorbed into Shi
  Yi's surrounding turn.
- `ref-0194`: Xu Zijing's complete contribution is attributed to Tang Yunfeng.
- `ref-0515`: the first substantive half of Zhu Jie's two-option summary is
  attributed to Tang Yunfeng.

The 47 manually confirmed incorrect contributions are:

`ref-0009`, `ref-0024`, `ref-0045`, `ref-0049`, `ref-0058`, `ref-0061`,
`ref-0071`, `ref-0090`, `ref-0102`, `ref-0116`, `ref-0118`, `ref-0127`,
`ref-0134`, `ref-0160`, `ref-0182`, `ref-0192`, `ref-0194`, `ref-0221`,
`ref-0241`, `ref-0249`, `ref-0252`, `ref-0253`, `ref-0296`, `ref-0298`,
`ref-0313`, `ref-0327`, `ref-0331`, `ref-0333`, `ref-0338`, `ref-0341`,
`ref-0375`, `ref-0390`, `ref-0417`, `ref-0432`, `ref-0436`, `ref-0442`,
`ref-0444`, `ref-0457`, `ref-0461`, `ref-0499`, `ref-0504`, `ref-0505`,
`ref-0506`, `ref-0507`, `ref-0509`, `ref-0515`, and `ref-0537`.

The reviewer manually established 509 accepted and 47 incorrect natural
contributions and manually calculated `509 / 556`, approximately `91.55%`.

Both runs pass the full natural-business-turn speaker gate. No whole-session
speaker permutation was found. Run B is lower than the prior current-commit
seal, but remains above the 90 percent product floor. This is not an overall
Spec 013 acceptance verdict.

## T084 conjunctive-gate audit

| Gate | Current status | Evidence boundary |
|---|---|---|
| Full natural-turn speaker accuracy | Passed for A and B | Complete contextual semantic review above |
| Full speaker-time accuracy | Open | No signed audible-boundary ledger or manual duration total |
| Fixed 600 s speaker blocks | Open | No signed per-block semantic totals |
| Per-speaker time and turn recall | Open | No signed per-speaker ledger totals |
| Critical speaker turns | Open | Criticality has not been signed for all 556 rows |
| Confident wrong attribution | Open | Confidence class has not been signed for all incorrect rows |
| Boundary offsets | Open | Reference timestamps are provisional; audible starts and ends are unsigned |
| ASR and silence gates | Open | Outside this speaker-only review |
| Forced alignment structure | Passed mechanically for A and B | 275 final ASR IDs and 275 aligned groups in each run; no manifest issue |
| Common time base | Passed mechanically for A and B | All seven terminal extents reconcile to `3615.120 s` |
| Live/terminal convergence | Server WS passed; full browser gate open | Producer and both observers agree; no full-run browser capture |
| Real-time input pacing | Passed mechanically for A and B | Both runs report `0.993x` stream real-time factor |
| Terminal timeline within 30 s | Passed mechanically for A and B | Direct `end` waits are `25.597 s` and `26.305 s`; no priming `flush` |
| Stability and telemetry | Passed mechanically for A and B | No runtime error; required telemetry coverage and cadence pass |
| Engineering | Passed | Warning-clean build and `68/68` CTest on the exact commit |
| Repeatability and documentation | Open | Conjunctive product gates above remain unsigned |

## Current permitted statement

On clean commit `588bfbe63555`, both required full-length canonical A/B runs
pass the Spec 013 natural-business-turn speaker-attribution threshold under
complete contextual semantic review. Both also satisfy the direct-end
30-second terminal-latency requirement mechanically. Full speaker closure,
canonical closure, release sign-off, ASR closure, and general industrial
readiness remain open until every conjunctive gate is signed.
