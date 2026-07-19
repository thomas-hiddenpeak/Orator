# FR47 Real-Path Promotion Review (2026-07-19)

## Scope and authority

This record covers the clean real-WebSocket promotion of the source-bounded
FR47 speaker-business candidate after the terminal JSON repair. It covers two
independent 120-second runs, one 600-second run, full empty-registry Run A, and
full restarted Run B using Run A's frozen registry. Every run used the same
committed binary and the checked-in `orator.toml` without a behavioral command-
line override.

`test/data/reference/test.txt` is the authoritative human-listened reference.
No code, script, query, formula, notebook, metric, hash, or equality check
assigned speaker correctness, classified business meaning, ranked the
candidate, aggregated an accuracy result, or issued the product decision.
Automation executed and captured the real stream, verified mechanical
contracts, and arranged immutable reference/candidate evidence for reading.
The speaker judgments and ledgers below were transcribed only after complete
contextual reading in chronological order and again in reversed fixed windows.

This report promotes FR47 from frozen replay evidence to repeatable real-path
evidence. It does not close Spec 013 or establish industrial readiness.

FR48 subsequently repeated the complete A/B forward and reverse review with
speaker ownership separated explicitly from ASR wording. That review corrects
only `ref-0375`: the final view already uses `spk_2` (徐子景), although its
decoded words are wrong. The corrected speaker-only ledger below supersedes
the original `521/556` transcription; the captured artifacts and all
mechanical promotion evidence are unchanged.

## Frozen revision and inputs

| Item | Frozen value |
|---|---|
| Git commit | `70f1186d2b9e0b1b12808ebc644a164d1e21983c` |
| Worktree during every run | Clean |
| `build/orator_ws` SHA-256 | `d5870872af3241b1efb8466af2e21550ce0c1a0bc2fcfe963e3e90c29f001d3b` |
| `orator.toml` SHA-256 | `94ef9bff2ff649c12ee66b4e38e1bd5aee05ea2283c30bb3a9981947d569cb13` |
| Resolved-config SHA-256 | `7c76bb207527efb1b46325436e89612ea15be9e8b97f26990b7b26d6cc0e7011` |
| `test.mp3` container SHA-256 | `b7c25d1c349b02d654b6a406bc29039749e4240a4109dda4fcc905285b14b18b` |
| Full decoded PCM SHA-256 | `17f0edda49989f3ceada60170885091023eeb9d67faae0d6dd67bb585b8857fe` |
| Full decoded input | 57,841,920 samples at 16,000 Hz (`3615.120 s`) |
| `test.txt` reference SHA-256 | `35e8695057be82f3028877f7dc159f10ecfa0ab7f06c444cefa9a079b0e24a86` |
| Diarizer | Sortformer v2.1 from checked-in TOML |
| Speaker posterior correction | `[speaker_fusion].posterior_future_epoch_enable = true` |

The build preceding these runs was warning-clean and all `70/70` CTest entries
passed. Commit `70f1186` changes only variable-length speaker-voiceprint JSON
serialization and its exact long-record regression relative to the retained
FR47 commit; it changes no model, speaker assignment policy, time code, or TOML
parameter.

## Promotion ladder

All artifacts and manifests are retained under
`/tmp/orator-spec013/release-70f1186-fr47-jsonfix-real/` on the validation host.

| Stage | Artifact ID | Artifact SHA-256 | Manifest SHA-256 | Direct-end wait |
|---|---|---|---|---|
| 120 A | `orator-20260719T140756Z-70f1186d2b9e-120.000s` | `3d0f66e629c8e2351c216ff9c0e5ad076dacf9cba381a5d7bfe25c3c628151e5` | `e46babfab7b391116bcc87eb382d971f02f138a5455b730b4814e1ad5bce0894` | `1.208 s` |
| 120 B | `orator-20260719T141037Z-70f1186d2b9e-120.000s` | `9e5d49e37d6ff1897f666434ea9250c72fc97f7eff9739170b5f7ade84994e3b` | `b6ce10f47552d56d2fe3462b2fcff6c3a855d6207d425496872874f607cd63d1` | `1.223 s` |
| 600 | `orator-20260719T142348Z-70f1186d2b9e-600.000s` | `4f692bad281296e9776eba8f8fd80af31c73c5f21f676cd773c644fd7641ab04` | `036057977430cc862983abe76eb96c41f258afeeeab2b9fc74bf69c62cfe5afe` | `3.465 s` |
| Full A | `orator-20260719T152859Z-70f1186d2b9e-3615.120s` | `a43288a4145b1ad3f92a0599020a46346bde9bf81ad6f18572136b4bc380ff9e` | `e57c816baca50dc06fd4e736f537535cba94984eb403fc3a79af67d94f109aa8` | `17.559 s` |
| Full B | `orator-20260719T163259Z-70f1186d2b9e-3615.120s` | `c2d45c2e87e21f8ef9d98094f933f09badd593edf0c779968cab9289ee64f3a1` | `7f3666ffd6d527f5b84c7e9140d68193f68562c5371d43bb21aa68183833d0de` | `17.392 s` |

The two independent 120-second runs close all seven product tracks at
1,920,000 samples. Their normalized product-entry bundles are mechanically
identical at SHA-256
`e8613dfbdffbbb3394d5e80955eb73d30e7f200c4ffb4b3058df3a1b805928b8`.
Complete forward and reverse reading of `ref-0001` through `ref-0018` finds no
new contextual speaker regression. The known cold-start and rapid short-turn
defects remain.

The clean 600-second run closes all seven tracks at 9,600,000 samples. Its
track counts are 98 diarization, 166 primary-speaker, 52 ASR, 159 VAD, 52
alignment, 2,473 speaker-voiceprint, and 196 business entries. Complete forward
and reverse reading of `ref-0001` through `ref-0093` preserves the frozen FR47
decisions, including the known cold-start, micro-turn, and word-tail failures,
without a new long-running identity drift. This manual finding authorizes the
full A/B stage; structural equality does not.

## Full-run mechanical evidence

Run A starts with an isolated empty registry. Run B starts after a server
restart with only Run A's frozen registry copied into an otherwise empty
storage directory. The registry remains byte-identical after Run B at SHA-256
`66461a77755984a08231d06306da7ce9e1eeac07be1927e91f8a772fc54c7b3f`.

Both full runs consume the exact 57,841,920 PCM samples at requested `rate=1`.
Run A takes `3633.001 s` wall time with `3615.200 s` push time; Run B takes
`3632.708 s` wall time with `3615.200 s` push time. Each run reports `0.995x`
stream real-time factor. Both direct-end waits are below the frozen 30-second
terminal limit.

Each full terminal document parses as complete JSON. Diarization, primary,
ASR, VAD, alignment, speaker-voiceprint, and business extents all close at
57,841,920 samples with zero declared extent gap; common-time-base, reconciled-
time-base, and wall-clock flags are true. Each run contains 755 diarization,
1,348 primary-speaker, 308 ASR, 972 VAD, 308 alignment, 16,104 speaker-
voiceprint, and 1,716 business entries. Producer, early observer, and late
observer terminal documents match within each run, and no unexpected observer
error is present.

Run A records 3,615 runtime telemetry samples and 3,623 `tegrastats` samples;
Run B records 3,615 and 3,622 respectively. Required GPU utilization, GPU
memory, system power, CPU, RAM, temperature, and cadence fields have complete
mechanical coverage in both artifacts. These resource and protocol checks do
not imply speaker correctness.

After capture, the isolated A/B storage directories were archived with the
artifacts. The pre-existing business storage was restored at
`/tmp/orator/storage`, including its speaker registry at the same
`66461a77755984a08231d06306da7ce9e1eeac07be1927e91f8a772fc54c7b3f`
SHA-256. No validation server or client process remains active.

## Complete contextual review

The review uses fixed windows `0-600`, `600-1200`, `1200-1800`, `1800-2400`,
`2400-3000`, `3000-3600`, and `3600-3615.12`. Every full artifact is read once
from start to end and independently again from the last window to the first.
The packets show `test.txt` context and the final business view without a
correctness field:

| Packet | SHA-256 |
|---|---|
| Run A chronological | `9edf3d8f9bb2c88d5c74f1e2ac2127c76b5bf36c28f03d22e1f5a124c59c313d` |
| Run A reverse windows | `e5e1b67deea6046ca07623a873946cb1d4e84630767f5a2141e6a087f06aaa84` |
| Run B chronological | `6f96e0f74f2d3041f6d50d4826757f0f6d658d243d59ceeb654097c33f242170` |
| Run B reverse windows | `8038ab491534822a5e4dfa01c4c5b56cee39c3ca83d205e1a1c0dfcbf90532c2` |

The contextual identity mapping is `spk_0 = 朱杰`, `spk_1 = 唐云峰`,
`spk_2 = 徐子景`, and `spk_3 = 石一`. ASR wording differences are not treated
as speaker errors when the complete natural turn and its owner remain clear;
wrong ownership, absent material contribution, and genuinely indeterminate
ownership retain their separate manual classes.

Both complete A and B reviews, after the FR48 speaker-only reconciliation,
produce the same corrected signed ledger:

- natural-turn ledger: `522/556`, approximately `93.9%` as a presentation of
  the manually signed judgments;
- fixed-window ledgers: `88/93`, `79/84`, `76/80`, `75/80`, `119/129`,
  `82/87`, and `3/3`;
- per-speaker ledgers: 朱杰 `77/83`, 唐云峰 `176/189`, 徐子景 `70/73`, 石一
  `199/211`;
- residual classes: 28 confident-wrong, five missing, and one uncertain;
- business-critical residuals: 20.

The 20 critical residuals are `0049`, `0058`, `0066`, `0099`, `0102`, `0118`,
`0252`, `0313`, `0327`, `0331`, `0333`, `0354`, `0390`, `0426`,
`0442` (missing), `0444`, `0461`, `0499`, `0503`, and `0505`. The nine
additional confident-wrong residuals are `0061`, `0135`, `0171`, `0221`,
`0239`, `0241`, `0298`, `0457`, and `0537`. The remaining missing residuals
are `0063`, `0341`, `0409`, and `0417`; `0506` remains uncertain.

FR47's `ref-0507` and `ref-0509` repairs are present in both real runs. Tang
Yunfeng owns the `5.6个亿` expectation and first `对`; Shi Yi retains the
following strategy explanation and second `对，一点影响都没有`. Early mapping
at `ref-0037`, `ref-0071`, and `ref-0073`, and the terminal sequence through
`ref-0553`-`ref-0556`, remains contextually stable. Neither reading finds a
whole-session speaker permutation or a late accumulating identity drift.
Residuals remain distributed across every complete 600-second block and are
concentrated at overlap, zero-duration or very short interjections, source-
boundary fragments, and cases where the upstream evidence itself is absent or
wrong. The remaining defect is therefore not tail-only.

The complete correction and the stopped hierarchical-consensus diagnosis are
recorded in
`fr48-speaker-only-reconciliation-and-consensus-diagnosis-2026-07-19.md`.

## Decision and remaining gates

FR47 is now the best repeatable real-WebSocket speaker-business candidate and
replaces its frozen-only status. It satisfies the full natural-turn percentage,
every complete fixed-window natural-turn floor, every per-speaker natural-turn
floor, real-path repeatability, direct-end latency, time-base, observer,
provenance, and telemetry gates represented by this ladder.

It does not satisfy critical-attribution-zero or confident-wrong-zero. The
speaker-time, per-speaker-time, source-time-offset, independent locked holdout,
final report review, release signing, ASR, and physical microphone/browser
workstreams also remain open. Accordingly, Spec 013, T102, T084, and the
industrial-closing claim remain open.
