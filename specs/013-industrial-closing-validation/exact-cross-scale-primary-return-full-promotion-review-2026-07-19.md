# FR32 Full Promotion Review (2026-07-19)

## Scope and authority

This report completes T142 for transitional experimental commit
`72d81c8084757b4c4210ba90ac14b5d1c1155e89`. The complete
`test/data/audio/test.mp3` recording was streamed through the production
WebSocket path with streaming Sortformer v2.1 and the checked-in
`340/1/188/188` profile. Run A started from an empty registry. Run B restarted
the server and used the registry frozen after Run A.

`test/data/reference/test.txt` is the authoritative human-listened reference.
Every one of its 556 contributions was read independently for each full run in
chronological order and again in reverse 600-second block order. ASR wording
was used only to locate each contribution; this review judges the final
speaker-business attribution in complete conversational context.

No compiled code, script, notebook, query, formula, metric, or algorithm
assigned correctness, aggregated an accuracy result, compared an accuracy
gate, ranked the candidate, or issued the verdict. Tools only executed and
captured the real stream, verified mechanical contracts, hashed immutable
artifacts, compared byte-normalized tracks, and arranged unjudged reference
and runtime evidence for reading. All product judgments and totals below were
derived manually from the two complete forward and reverse reviews.

## Promotion ladder

| Gate | Mechanical evidence | Complete contextual finding |
|---|---|---|
| Silence | One 30-second real-WebSocket run; zero diarization, ASR, or final business entries; direct-end wait `0.259 s` | No substantive transcript, false speaker, or hallucinated business contribution |
| Repeated 120 seconds | Independent runs terminate in `1.222 s` and `1.212 s`; their normalized seven-track bundle is byte-identical at `3f784df72ef191d89a350153ef102978587486b6e946092555db4d3aac6ddd1c` | Complete forward and reverse reading of `ref-0001` through `ref-0018` finds no new natural-turn regression; known cold-start, micro-turn, and boundary defects remain |
| 600 seconds | All active tracks close at `9,600,000` samples; wall time `603.334 s`; direct-end wait `3.334 s`; normalized seven-track bundle equals retained T128 at `824e782685c3cede34f68b8e98cf6cfcd691e135dec58bbb1310d2e7f15104bf` | Complete forward and reverse reading of all 93 contributions finds no new natural-turn regression |
| Full Run A | Empty registry; `3615.120 s` audio; wall time `3631.968 s`; `0.995x`; direct-end wait `16.768 s` | Independent complete review records `506/556`; conjunctive closing gates fail |
| Full Run B | Restarted frozen registry; `3615.120 s` audio; wall time `3632.081 s`; `0.995x`; direct-end wait `16.684 s` | Independent complete review also records `506/556`; the same conjunctive closing gates fail |

The lower ladder permits interpretation of the two full runs; it does not
itself establish product accuracy. Each accuracy statement above comes from
the complete contextual reading named in the final column.

## Frozen evidence

| Item | SHA-256 |
|---|---|
| Source commit workspace | `5edc6db52db2c52cf4491cc1c0545e4205e40c1ed41805b7b7b47b244680ce19` |
| Server binary | `8f4e7cb96f72f8fa1440ff28c0d1558dfbbd3d3f20f3ed01642552b695655092` |
| `test.mp3` container | `b7c25d1c349b02d654b6a406bc29039749e4240a4109dda4fcc905285b14b18b` |
| Streamed PCM | `17f0edda49989f3ceada60170885091023eeb9d67faae0d6dd67bb585b8857fe` |
| Human reference | `35e8695057be82f3028877f7dc159f10ecfa0ab7f06c444cefa9a079b0e24a86` |
| Sortformer v2.1 | `d036020b6b93977098929d417b1b106a952ec02cc38cafc9d3315ae0ec4d90b8` |
| Silence artifact / manifest | `d4f8d2ec1d223cc1528b99d95c85925da5981996eec3fe09584d6dcd18abec02` / `cb3fc0cad05d99fa81989b9e7c03c5daa39121cb64cc95d706e622170713e0` |
| 120 A artifact / manifest | `c5ef27edf8d5ba57c872c4c6491ba0ee6ada0a4557d5c8e345c529460d21575e` / `f064b23b0e018f340e0be699493fe95d3ee70cfcdd1c096caaafbdb8bdcd7eb4` |
| 120 B artifact / manifest | `205ed3bef3d69e63c397d892f7cc5d2e6925385d01544af99c8cab2d00961dcb` / `3f198afbb4ba34e4549664b5d42649593bfc1dae5e7ec8abb413d9a9ae22fca4` |
| 600 artifact / manifest | `a429315644d53c77f729a3553d019f9127a0678268a4864e23dbdd0097693657` / `29b831c3aaf0a5bafbbc7dd6d8c4792fe7e9430a2e3ba169d32bb8b0a9188ddf` |
| Full A artifact / manifest | `11864fb8f39a0832f4c30f7224539f70113328a3c1f076bd22b2cf7207f7c9b9` / `11637db44946c1033772cae642cfb927303b72bff5a5ec3307261b896d8c75ca` |
| Full B artifact / manifest | `8ce7c18a1e476993bb5805128bbc73003123ec00f69b6dd94c66ccd6ccee8e43` / `cfe721535d86400fb94e118fa6ce1ad04c21a213f34a94a235cafd4fff7870d4` |
| Full A / B TOML | `4e5a9e1e6b8033af3a0c23ce9c553578b1c553b9c9ce4a4380ad0958cf3efec2` / `e257651e46a3bfb10b0c6212942d814599ac129f819b5c9d92172127d3f92098` |
| Frozen and post-Run-B registry | `66461a77755984a08231d06306da7ce9e1eeac07be1927e91f8a772fc54c7b3f` |
| Run A forward / reverse packets | `68805a26e43238f5124047e6cf277085acc691f91decd2cdb9bda8f665c79f07` / `239801e6e89fd4ea95f11e0442301a66a12b3883994b1b6e2034922f446ae0fe` |
| Run B forward / reverse packets | `2a6b7d0af9c83ca1aaa4d9391cf467e61bac79b266562e8299b41b3bf861d721` / `81999e214220d09cc87ab53c9d5ee077f599d08ae109ce74dbb49c0bc3931c1f` |

Both full artifacts contain 755 diarization, 1,348 primary-speaker, 308 ASR,
972 VAD, 308 forced-alignment, 16,103 voiceprint, and 1,707 final business
entries. Every active track closes at `57,841,920` samples with zero gap;
time-base, wall-clock, observer convergence, provenance, and required telemetry
contracts pass. Run A and Run B have the same normalized seven-track SHA-256,
`82a10a38de6614e977f5d484ddaebd1de368489cc0871d81db0144631f8dc6bd`,
and the same comprehensive-view SHA-256,
`732fe29a3dbff817407dda7ab3620ab3a33e1ad5971fae59e64ef47792f96db6`.
These are repeatability facts, not accuracy evidence.

The six upstream tracks also remain byte-identical to frozen T123 at
`bacc74400c0afb6224a20495f60b464cdb26ac522ca53b67bf1f3e34f6ad8a1b`.
Only the final business view changes around `ref-0154`, exactly where FR32
acts. Registry initialization therefore does not explain the full result, and
FR32 does not alter producer evidence.

## Complete contextual result

The two independent reviews reach the same contribution-level findings. FR32
repairs `ref-0154`: Shi Yi asks whether the three rounds include the current
round, and Tang Yunfeng's following `不含` answer now remains with Tang. No new
natural-turn regression or long-session identity swap appears in either
forward or reverse reading.

Each run has 506 accepted and 50 incorrect contributions, manually derived as
43 confident-wrong (`CW`), six missing (`M`), and one uncertain (`U`). Thirty-
one `CW` and two `M` contributions carry critical business meaning. The same
50 incorrect reference IDs occur in both runs:

`0025, 0049, 0058, 0061, 0063, 0066, 0071, 0099, 0102, 0118, 0135,
0171, 0192, 0194, 0221, 0239, 0241, 0252, 0268, 0298, 0313, 0327,
0331, 0333, 0341, 0350, 0354, 0375, 0390, 0406, 0409, 0417, 0420,
0426, 0432, 0442, 0444, 0457, 0461, 0478, 0499, 0503, 0504, 0505,
0506, 0507, 0509, 0517, 0518, 0537`.

### Fixed blocks

| Block | Run A | Run B | Gate |
|---|---:|---:|---|
| 0-600 | 86 / 93 | 86 / 93 | Pass |
| 600-1200 | 79 / 84 | 79 / 84 | Pass |
| 1200-1800 | 74 / 80 | 74 / 80 | Pass |
| 1800-2400 | 74 / 80 | 74 / 80 | Pass |
| 2400-3000 | 114 / 129 | 114 / 129 | Fail |
| 3000-3600 | 76 / 87 | 76 / 87 | Fail |
| 3600-3615.12 | 3 / 3 | 3 / 3 | Reported only |

### Canonical speakers

| Speaker | Run A | Run B | Gate |
|---|---:|---:|---|
| 朱杰 | 70 / 83 | 70 / 83 | Fail |
| 唐云峰 | 171 / 189 | 171 / 189 | Pass |
| 徐子景 | 67 / 73 | 67 / 73 | Pass |
| 石一 | 198 / 211 | 198 / 211 | Pass |

The manually derived full-session natural-turn result is `506/556`, about
`91.01%`, for each run. The standalone full average clears 90 percent, but the
2400-3000 and 3000-3600 fixed blocks, 朱杰 recall, critical-turn gate, and
confident-wrong gate fail. Per-speaker time and source-time-offset gates remain
unsigned. Because Spec 013 gates are conjunctive, neither run closes the
speaker business.

## Decision and next evidence step

FR32 remains checked in as a bounded, repeatable repair for `ref-0154`; it is
not promoted to a closing result. The full real path confirms that the frozen
T123 repair generalizes without a new regression, but it improves only one of
the 51 previously reconciled T123 errors. T111 remains the best frozen
comparison at `514/556`, while neither T111 nor FR32 satisfies all closing
gates.

No further audio capture is justified until the frozen T111/T123 evidence
upper bound is exhausted. FR33 will test a source-free, partition-invariant
cross-scale abstention rule for the `ref-0517` evidence topology. It may use
only typed activity, primary, alignment, business-interval, VAD, and both
existing voiceprint galleries under existing TOML gates. It must add no fitted
threshold or reference-specific input, preserve specialized challenge
precedence, replay T111 and T123 deterministically, and receive complete
forward and reverse contextual review of every changed conversation before
retention. A later exact-phrase/VAD precedence candidate remains separate and
will be attempted only if FR33 establishes a safe frozen result.
