# Delayed Alignment Full Promotion Review (2026-07-18)

> **Reconciled 2026-07-19.** T135's new complete A/B forward-and-reverse
> reread adds five omitted errors and corrects T111 to `514/556`. The corrected
> list and conclusion below supersede the original `519/556` claim. See
> `speaker-baseline-reconciliation-2026-07-19.md`.

## Scope and claim boundary

This record completes T110 and T111 for FR16ABN at clean transitional
experimental commit
`6b1cb79fa4f5e942c310ea5a449225c33ccb7302`. The source passed a
warning-clean build and all 68 registered CTest entries before the candidate
advanced through 120-second, 600-second, and full-length production WebSocket
runs. Full Run A used an empty isolated speaker registry. Full Run B restarted
the server with Run A's registry frozen at SHA-256
`66461a77755984a08231d06306da7ce9e1eeac07be1927e91f8a772fc54c7b3f`.

Every behavioral value remained TOML-owned. The run-specific TOML files differ
only in isolated registry and storage paths. Both full runs streamed all
`3615.120` seconds of `test.mp3` at 1.0x input pacing and sent `end` directly
after the final audio frame without a priming `flush`.

The product-result review below is manual and contextual. No executable,
script, query, formula, notebook, metric, or algorithm assigned correctness,
aggregated accuracy, ranked a candidate, or issued the result. Tools captured
the runs, verified mechanical contracts and hashes, and displayed unjudged
evidence. The reviewer read all 556 contributions in chronological order for
each retained full run, then reread each complete session in six reverse fixed
windows and reconciled the two passes.

This report signs only the standalone 90 percent full-session natural-turn
floor for these two exact runs. `test.txt` is the human-listened reference and
all 556 rows were already read twice against each run; no duplicate audio
transcription or boundary-listening pass is required. T135 later signs and
fails one fixed block, 朱杰 recall, criticality, and confidence. Speaker-time,
per-speaker time, and source-time-offset breakdowns remain unsigned. T102,
T084, canonical closure, release sign-off, and industrial readiness remain
open.

## Frozen inputs and build

| Evidence | SHA-256 |
|---|---|
| Source commit | `6b1cb79fa4f5e942c310ea5a449225c33ccb7302` |
| `build/orator_ws` | `6812ba5f04ddc6df44a8eaf04654901b2237af98dffe430040d36e3dc058d8ab` |
| Checked-in `orator.toml` | `5e3ab154a1d337361e099fe2907587820981ccc8e33d2082faeb790aa00e6218` |
| `test.mp3` | `b7c25d1c349b02d654b6a406bc29039749e4240a4109dda4fcc905285b14b18b` |
| `test.txt` | `35e8695057be82f3028877f7dc159f10ecfa0ab7f06c444cefa9a079b0e24a86` |
| Sortformer v2.1 | `d036020b6b93977098929d417b1b106a952ec02cc38cafc9d3315ae0ec4d90b8` |
| Run A TOML | `2b87ecac5739d2441285fcf5df6422af564682d9fdbc66ec70ad5d0d7a841ecc` |
| Accepted Run B TOML | `d2cb3257279b1102f6ba22cf14bf6120f3c792b9266486dc56c57a566797e766` |
| Frozen registry | `66461a77755984a08231d06306da7ce9e1eeac07be1927e91f8a772fc54c7b3f` |

The source commit, worktree, binary, configuration, audio, and registry
provenance remained stable throughout each retained run. No environment
override changed the resolved runtime behavior.

## T110 promotion ladder

The same clean binary first passed the real incremental WebSocket path at the
two required promotion lengths:

| Duration | Artifact SHA-256 | Manifest SHA-256 | Direct `end` wait |
|---|---|---|---|
| `120.000 s` | `476aaad75e08633f99ed1e72985d68f7dd962cbe989d9c4c4ac1754511f5fd46` | `5ba05d9842f892f1eb2fb7a213eb16a21fa115c67e6bc33ec4c90f546b0f59bb` | `0.803 s` |
| `600.000 s` | `1390e74c04376f7e806f56a5d9e8c31512d76e22211daab89b6dcd7dd5533e0b` | `8c816f0a38601fed69e538f17926aef7aefdd2f9af5746f23112aeab3fd36816` | `4.607 s` |

Both promotion artifacts had exact seven-track common-clock extents, matching
producer/observer terminal payloads, no structural contract issue, and complete
required telemetry coverage. The complete 120-second forward/reverse context
read found no changed speaker sequence. The complete 600-second read found only
the intended `ref-0090` short-confirmation repair and retained it without a
changed-context regression. Mechanical checks did not make that judgment.

The immutable review packets are:

| Packet | SHA-256 |
|---|---|
| 120 s forward | `df5a6db9525afdd9e7baf1e8377530b98ea24622ee00400403c04727ea07e0e6` |
| 120 s reverse | `ac5563d06bac62fced159ddedb1c6f2f0a6a960bc167e8ae8313600306a13586` |
| 120 s changed sequence | `6f038e276f0013c6ab66864476f55b0e59e85be9c0df7da026e9e887425ff370` |
| 600 s forward | `ef5243d703a933891fddf454967b4c3fea5a851b5acbb0e6d63948bb73618bec` |
| 600 s reverse | `76464013995e11486e4451ec5253d9399d856ee3531a50083de7b18f68848f7d` |
| 600 s changed sequence | `7b7267d31196ad556732c54404034ca754924ff17b7f3aaedd835ad27e5b489b` |

## Full-run mechanical evidence

| Evidence | Run A: empty registry | Run B: frozen registry retry |
|---|---|---|
| Timeline SHA-256 | `d5c97db9ff91b41da4ccd5414d5f2bca4966592e60fb2717058fee2e600132e9` | `b5dfefc8c30ec9458bbe70a8f7e789a6997d082c9bcce7d834b1df12e6c725f4` |
| Manifest SHA-256 | `5b10f1f93217bf1d086aa481f470ddb8da2a48f950bc0f7c0d5e1560db07032f` | `2c204394f67707a3cd5db1f77923a0fa4661feeca8d8d8a8b8e6475771dcb6de` |
| Fixture manifest SHA-256 | `01eca9af7d015f18672289d713940e1f481e63d0597913704685ff6bd4b30672` | `41ced657697f8f3d9033c4dc39d25199c66ea288115f73ed7caaf0eb6f6ce1df` |
| Artifact ID | `orator-20260718T005920Z-6b1cb79fa4f5-3615.120s` | `orator-20260718T030524Z-6b1cb79fa4f5-3615.120s` |
| Total wall time | `3641.156 s` | `3641.008 s` |
| Stream real-time factor | `0.993x` | `0.993x` |
| Direct `end` wait | `25.849 s` | `25.585 s` |
| Diar / primary / ASR / VAD / align | `755 / 1348 / 275 / 972 / 275` | `755 / 1348 / 274 / 972 / 274` |
| Voiceprint / business | `16083 / 1750` | `16086 / 1748` |
| Terminal observer SHA-256 | `5e32d1f0fb23dfd92d4fb694342fcf1a67b61addf0367e126fa24103169f41e4` | `64c95b000eaaa51b2523c07cbb63711b281841cf677f073a5f9433dde8f0e746` |
| Runtime / `tegrastats` samples | `3438 / 3631` | `3442 / 3631` |
| Runtime / `tegrastats` cadence | `95.104% / 100%` | `95.214% / 100%` |

Both retained artifacts report no contract issue. Every registered track ends
at `57,841,920` common-clock samples with zero gap; `timebase_ok`,
`timebase_reconciled`, and `wall_clock_ok` are true. Producer, early observer,
and late observer terminal payloads agree. Required GPU utilization, GPU
memory, system power, CPU, RAM, and temperature fields have complete coverage.
No server, CUDA, allocation, stability, or producer-ownership error occurred.

### Excluded first Run B attempt

The first restarted Run B produced a complete terminal artifact at SHA-256
`3892fb1d45d8d4eba6a5d8db242c5e42e8b4ef3b8ba47fd97417fdfdaf366660`
with manifest SHA-256
`91dfa701854c6cd7860c9d146ad84fadd0b913d3134fda6c18a23c5831b764c0`.
It reached the terminal timeline in `26.139 s`, reconciled every track, and
preserved observer convergence, but its 3,433 runtime telemetry samples covered
only `94.965%` of the required cadence. It therefore failed the mechanical
telemetry contract and is excluded from product review and acceptance evidence.

One controlled retry used the same source, binary, behavioral TOML values,
audio, and frozen registry; only isolated storage paths changed. The accepted
retry is the Run B artifact in the table above. The observed cadence risk comes
from the telemetry loop waiting one full configured interval and then running
the probe, so probe latency accumulates. This is retained as an engineering
follow-up; the retry does not erase the failed artifact or relax the 95 percent
threshold.

## Context-review evidence

| Packet | SHA-256 |
|---|---|
| Run A forward, all 556 references | `6a70b4e979599b42b348375154e7ae225386be33b73a93a58ccc70f14a6e6da0` |
| Run A reverse fixed windows | `80afedbb71292862d832a96ee3cf9fe9a155ec05e2a85806eb0f0293a47af9db` |
| Run A changed from T106 | `b3e1ce339a4358753b7a9663e9a2917c5ee312ee451b910c1ce211e99298edfe` |
| Run B forward, all 556 references | `38ba63459d3aea6aed9b3ac3e5b8100488bfa3d509d3d25b52954401f0da9c4f` |
| Run B reverse fixed windows | `4d571aede4d5498d867fea011f29e88145a18682dbde7597599c52f00648e4d5` |
| Run B changed from T106 | `b91666ccf936f1bf1ce406847b0df8d9f746c25c74eb6ce2aa90271cf03baae3` |
| Accepted retry changed from excluded attempt | `e4b5ac1d4e9a169670628020a2a508b3b4f27f26a05a5a9d812dee132b65a18b` |

The contextual identity mapping is `spk_0 = Zhu Jie`, `spk_1 = Tang
Yunfeng`, `spk_2 = Xu Zijing`, and `spk_3 = Shi Yi`. ASR wording errors were
not treated as speaker errors when the natural speaker attribution remained
clear. Conversely, a recognizable contribution assigned to the wrong speaker
remained a speaker error even when its words were correct. Whole-second source-
time display edges did not override the conversational handoff.

## Manual natural-turn result

FR16ABN repairs `ref-0090` in both retained full runs: the short Xu Zijing
confirmation at `569.26-569.42` is assigned to `spk_2`, while Shi Yi's
following substantive calculation remains `spk_3`. The complete forward and
reverse reads found no session-wide identity permutation and no tail-wide
drift.

Run A also contains correct Zhu Jie and Xu Zijing contributions at
`ref-0192` and `ref-0194`. Run B contains a correct Xu Zijing contribution at
`ref-0215`. Those are run-specific captured outcomes and are not attributed to
the FR16ABN rule.

The initial T111 pass listed the same 39 contributions as incorrect in each
retained run. The T102 breakdown reread then reconciled `ref-0160`: its source
line says 石一, but the surrounding board-attendance exchange unambiguously
identifies 唐云峰, and both runs assign it to `spk_1`. It also reconciled
`ref-0182`: complete conversation context contains the recognizable 徐子景
contribution under `spk_2`, despite imperfect wording and a shifted display
edge. T135's uniform material-fragment reread adds `ref-0099`, `ref-0239`,
`ref-0426`, `ref-0503`, and `ref-0518`. The final 42 incorrect
contributions in each run are:

`ref-0009`, `ref-0024`, `ref-0045`, `ref-0049`, `ref-0058`, `ref-0061`,
`ref-0099`, `ref-0102`, `ref-0118`, `ref-0135`, `ref-0221`, `ref-0239`,
`ref-0241`, `ref-0249`, `ref-0252`, `ref-0253`, `ref-0296`, `ref-0298`,
`ref-0313`, `ref-0327`, `ref-0331`, `ref-0333`, `ref-0338`, `ref-0341`,
`ref-0354`, `ref-0375`, `ref-0390`, `ref-0417`, `ref-0426`, `ref-0442`,
`ref-0444`, `ref-0457`, `ref-0461`, `ref-0499`, `ref-0503`, `ref-0504`,
`ref-0505`, `ref-0506`, `ref-0507`, `ref-0509`, `ref-0518`, and `ref-0537`.

For each run, the reconciled review manually establishes 514 accepted and 42
incorrect natural contributions and manually calculates `514 / 556`,
approximately `92.45%`. Both retained full runs pass only the standalone 90
percent full-session natural-turn floor. The 3000-3600 fixed block, 朱杰
recall, criticality, and confidence gates fail. Details are in
`speaker-gate-breakdown-review-2026-07-18.md`.

## Promotion decision and next boundary

FR16ABN is retained in the production fusion policy. T110 and T111 are
complete, and this report supersedes the frozen-replay-only status in
`delayed-alignment-clause-review-2026-07-18.md`. The result is a transitional
experimental checkpoint, not a release or complete speaker-business closure.

T135 completes the corrected natural-turn, fixed-block, per-speaker,
criticality, and confident-wrong reconciliation. T102 remains open only for the
manual speaker-time, per-speaker time, and source-time-offset breakdowns for
Run A and Run B independently. Source-time review uses `test.txt` at its
recorded whole-second precision and must not invent sub-second reference truth.
T112 separately closed the telemetry scheduling follow-up. No follow-up may
reinterpret the natural-turn result above or use code to assign a product
verdict.
