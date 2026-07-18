# Native Handoff Full Promotion Review (2026-07-18)

## Scope and claim boundary

This record completes T106 for FR16ABM at clean commit
`1a475e6b7473603f443803e6dcab2285ec62804c`. The candidate passed the
warning-clean build and all 68 registered CTest entries before it was promoted
through 120-second, 600-second, and full-length production WebSocket runs. Run
A used an empty isolated speaker registry. Run B restarted the server with the
registry produced by Run A frozen at SHA-256
`66461a77755984a08231d06306da7ce9e1eeac07be1927e91f8a772fc54c7b3f`.

Every runtime value remained TOML-owned. The full runs streamed all
`3615.120` seconds of `test.mp3` at 1.0x input pacing and sent `end` directly
after the final audio frame without a priming `flush`.

The product-result review below is manual and contextual. No executable,
script, query, formula, notebook, metric, or algorithm assigned correctness,
aggregated accuracy, ranked the runs, or issued the result. Tools captured the
runs, verified mechanical contracts and hashes, and arranged unjudged evidence.
The reviewer read all 556 contributions in chronological order for each run,
then independently reread each complete session in six reverse fixed windows.

This report signs T106 and the natural-business-turn gate for these two runs.
It does not include audible boundary listening and does not sign speaker-time,
fixed-block, per-speaker, criticality, confidence, or boundary-offset gates.
T102, T084, canonical closure, and industrial readiness remain open.

## Frozen inputs and build

| Evidence | SHA-256 |
|---|---|
| Source commit | `1a475e6b7473603f443803e6dcab2285ec62804c` |
| `build/orator_ws` | `452e95d11aea4d748cc5b295cb33175f43ff462661f5d8ca73bba0ab063a61be` |
| `test.mp3` | `b7c25d1c349b02d654b6a406bc29039749e4240a4109dda4fcc905285b14b18b` |
| `test.txt` | `35e8695057be82f3028877f7dc159f10ecfa0ab7f06c444cefa9a079b0e24a86` |
| Sortformer v2.1 | `d036020b6b93977098929d417b1b106a952ec02cc38cafc9d3315ae0ec4d90b8` |
| Run A TOML | `ee9259d34e84536b45270ac6ea86e4cb2b082dac8bd515e85259431c1c105941` |
| Run B TOML | `f2ceffbe4114a55991a1ca456cc32ff78f9ca5603161ba785ff5afdfedd7e060` |
| Frozen registry | `66461a77755984a08231d06306da7ce9e1eeac07be1927e91f8a772fc54c7b3f` |

The Run A and Run B TOML files differ only in isolated storage and registry
paths. Both resolve the checked-in streaming v2.1 `340/1/188/188` policy. The
source commit, worktree, binary, configuration, audio, and registry provenance
remained stable throughout each run.

## Promotion ladder

The same clean binary first passed the real incremental WebSocket path at the
two required promotion lengths:

| Duration | Artifact SHA-256 | Manifest SHA-256 | Direct `end` wait |
|---|---|---|---|
| `120.000 s` | `264438c9ed596ead0663c7f09917e89bc4ea6917b384d42c1d5fc2737e58ae49` | `cbe19d27623eccf24d7021e2ba7c431e6990058e98d990ee303b944e2ddaa43c` | `0.808 s` |
| `600.000 s` | `7f09470dfddab24e57c0bdd1cc3e523a23d3d38aae82822042df7f00a99e25a0` | `8058aaf9786827cb156d1e401109087a9aaf4e1075c9050d9a8156f421305acc` | `4.590 s` |

Both runs had exact seven-track common-clock extents, no structural contract
issue, matching producer/observer terminal payloads, and complete required
telemetry coverage. These are mechanical promotion facts, not accuracy results.

## Full-run mechanical evidence

| Evidence | Run A: empty registry | Run B: frozen registry |
|---|---|---|
| Timeline SHA-256 | `419379df5a2a49c1ca83e25c9a5606d10cbddad8080d4138e9df251e0e43c166` | `bf9c78e5b89eaf039fce860562938413422bd1fc5bb0da29e8188c56c7899584` |
| Manifest SHA-256 | `3396b745d1bb445bef69f7356bedc8aba7c76f4431bdf4143a175cbb44d6a5e3` | `633eba2f613aec471a26d39e1b7405de4a2a43d49d6862118328876492c30e25` |
| Fixture manifest SHA-256 | `5afca47b48a332372e34f092e0ccc43aeb62ec3e8c7b337cfb58785c469277aa` | `9c48cadf586ad77ef32d907726e71915c4eb38edafe51d04a7aa8e90bd6fda42` |
| Artifact ID | `orator-20260717T215241Z-1a475e6b7473-3615.120s` | `orator-20260717T225516Z-1a475e6b7473-3615.120s` |
| Total wall time | `3641.927 s` | `3641.385 s` |
| Stream real-time factor | `0.993x` | `0.993x` |
| Direct `end` wait | `26.503 s` | `26.185 s` |
| Diar / primary / ASR / VAD / align | `755 / 1348 / 275 / 972 / 275` | `755 / 1348 / 275 / 972 / 275` |
| Voiceprint / business | `16070 / 1753` | `16096 / 1753` |
| Terminal observer SHA-256 | `c17b41fadd9f264793a08d34c4511755b8b4c9006e49bdb25cea4cece24f5cd2` | `32b4be2c0115da873e136fb61a6631e72f23b7e9e08613852bcb4b4cf3c53c1d` |
| Runtime / `tegrastats` samples | `3442 / 3631` | `3443 / 3631` |
| Runtime / `tegrastats` cadence | `95.214% / 100%` | `95.242% / 100%` |

Both artifacts report no contract issue. Every registered track ends at
`57,841,920` common-clock samples with zero gap; `timebase_ok`,
`timebase_reconciled`, and `wall_clock_ok` are true. Producer, early observer,
and late observer terminal payloads agree. GPU utilization, GPU memory, system
power, CPU, RAM, and temperature coverage are complete. No server, CUDA,
allocation, stability, or producer-ownership error occurred.

## Context-review evidence

| Packet | SHA-256 |
|---|---|
| Run A forward, all 556 references | `99a1912fabe91b973bcd95a6493dde549e6dca9de85b62884de987cd179c5366` |
| Run A reverse fixed windows | `c0479234d208fc4812a69720721a3a32b3605d0b232cbbfe0fcc34dbb5ac2c70` |
| Run B forward, all 556 references | `15db7c1c72b0b6a0e9411c9440c7d9a2bf81c689e2a7849ea1954028a9947886` |
| Run B reverse fixed windows | `a49676193f8004983a4e55d414cd7552c29507e266aecccc23249c89a20881f7` |

The review used the contextual mapping `spk_0 = Zhu Jie`, `spk_1 = Tang
Yunfeng`, `spk_2 = Xu Zijing`, and `spk_3 = Shi Yi`. ASR wording errors were
not treated as speaker errors when the natural speaker attribution remained
clear. Conversely, a semantically recognizable turn assigned to the wrong
speaker remained a speaker error even when its words were correct.

The full reread also corrects one prior row-boundary judgment. The
`ref-0250` contribution `不一定啊` is emitted at `1769.10-1769.82` by
`spk_1`, Tang Yunfeng, immediately after the provisional reference interval
ends at `1769.00`. Complete conversation context makes this a correct
attribution in both runs. Rejecting it because it falls 0.10 seconds beyond a
provisional row boundary would be a mechanical-window judgment and is not
permitted by the contextual semantic protocol.

## Manual natural-turn result

### Run A

FR16ABM preserves Tang Yunfeng's lead-in and restores Shi Yi's substantive
`四十四十五` contribution at `ref-0071`; adjacent `ref-0070`, `ref-0072`, and
`ref-0073` remain naturally attributed. Tang Yunfeng's long `ref-0310`
valuation turn also remains intact. The forward and reverse reads found no new
contextual regression and no whole-session speaker permutation.

The 42 manually confirmed incorrect contributions are:

`ref-0009`, `ref-0024`, `ref-0045`, `ref-0049`, `ref-0058`, `ref-0061`,
`ref-0090`, `ref-0102`, `ref-0118`, `ref-0135`, `ref-0160`, `ref-0182`,
`ref-0192`, `ref-0194`, `ref-0221`, `ref-0241`, `ref-0249`, `ref-0252`,
`ref-0253`, `ref-0296`, `ref-0298`, `ref-0313`, `ref-0327`, `ref-0331`,
`ref-0333`, `ref-0338`, `ref-0341`, `ref-0354`, `ref-0375`, `ref-0390`,
`ref-0417`, `ref-0442`, `ref-0444`, `ref-0457`, `ref-0461`, `ref-0499`,
`ref-0504`, `ref-0505`, `ref-0506`, `ref-0507`, `ref-0509`, and `ref-0537`.

The reviewer manually established 514 accepted and 42 incorrect natural
contributions and manually calculated `514 / 556`, approximately `92.45%`.

### Run B

Run B independently preserves the FR16ABM repair at `ref-0071`. Its frozen
registry also retains usable Zhu Jie evidence at `ref-0192` and Xu Zijing
evidence at `ref-0194`. It has one additional local error at `ref-0215`, where
Xu Zijing's `你说光远吧` contribution is attributed to Zhu Jie. No tail-wide
identity swap or long-range speaker drift appears in the complete reverse read.

The 41 manually confirmed incorrect contributions are:

`ref-0009`, `ref-0024`, `ref-0045`, `ref-0049`, `ref-0058`, `ref-0061`,
`ref-0090`, `ref-0102`, `ref-0118`, `ref-0135`, `ref-0160`, `ref-0182`,
`ref-0215`, `ref-0221`, `ref-0241`, `ref-0249`, `ref-0252`, `ref-0253`,
`ref-0296`, `ref-0298`, `ref-0313`, `ref-0327`, `ref-0331`, `ref-0333`,
`ref-0338`, `ref-0341`, `ref-0354`, `ref-0375`, `ref-0390`, `ref-0417`,
`ref-0442`, `ref-0444`, `ref-0457`, `ref-0461`, `ref-0499`, `ref-0504`,
`ref-0505`, `ref-0506`, `ref-0507`, `ref-0509`, and `ref-0537`.

The reviewer manually established 515 accepted and 41 incorrect natural
contributions and manually calculated `515 / 556`, approximately `92.63%`.

Both full runs pass the 90 percent natural-business-turn speaker-attribution
floor. The remaining errors are localized short handoffs, absent evidence, and
alignment-placement defects rather than a session-wide identity permutation.
This is not an overall Spec 013 acceptance verdict.

## Promotion decision and next boundary

FR16ABM is retained in the production fusion policy and T106 is complete. Its
bounded abstention fixed the target native handoff on both full real paths
without changing TOML parameters or introducing a contextual regression.

T107-T109 subsequently traced the independent forced-alignment/VAD placement
defect around `ref-0090`, implemented FR16ABN, and retained it through frozen
A/B replay and complete changed-context review; see
`delayed-alignment-clause-review-2026-07-18.md`. FR16ABN subsequently passed its
own real-WebSocket promotion and superseded this report as the current
natural-turn production evidence; see
`delayed-alignment-full-promotion-review-2026-07-18.md`. This report remains the
FR16ABM baseline and causality record. T102 still requires audible review and
manual signing of all 556 boundaries, overlaps, criticality, and confidence
classes before the remaining speaker-time and block gates can be derived. That
work may not reuse this natural-turn result as a substitute.
