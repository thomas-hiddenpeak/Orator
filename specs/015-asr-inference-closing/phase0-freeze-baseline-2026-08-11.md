# Spec 015 Phase 0 Freeze and Baseline (2026-08-11)

## Scope

This report closes Spec 015 T001-T006. It records source, configuration, model,
build, test, prior-product-evidence, and official-reference provenance only. No
audio candidate was produced, no transcript or speaker output was evaluated,
and no product accuracy or acceptance conclusion is made.

## Clean Checkpoint

| Item | Value |
|---|---|
| Runtime behavior baseline | `c235830254f9737f170429a1379d5cfd62657cae` |
| Spec 015 planning commit | `fa245261fb0dc8d38bad0bca5ef966fb96921917` |
| Frozen-control implementation | `1a49a91e6055cc2763ac23a6b7c2c2d4249a2ae5` |
| Git state during final manifest | clean, no changed or untracked path |
| Root TOML SHA-256 | `80ed328a94049ec267ebfbf563d80138669d43aa1d65ee83a6882569399ce4b0` |
| Server binary SHA-256 | `9408f957352063404486cdbca8ba1d886b451d45cddbe293cd1928a8d363add6` |
| Canonical audio SHA-256 | `b7c25d1c349b02d654b6a406bc29039749e4240a4109dda4fcc905285b14b18b` |
| Human reference SHA-256 | `35e8695057be82f3028877f7dc159f10ecfa0ab7f06c444cefa9a079b0e24a86` |

The gitignored final reproducibility manifest is
`artifacts/spec015/baseline/repro-manifest.json`. Its file SHA-256 is
`62c6026b80b7f68aa126da711df33a20b2d467143a72fb218a6ca5901c205d74`;
its internal canonical content SHA-256 is
`36b5f1d8227a777244e09d026ea3a143dac113e0bb06b01aff709d923c823705`.

## Frozen Control

The checked-in `frozen-control.json` records exact values for `[align]`,
`[timeline]`, `[speaker]`, `[speaker_fusion]`, `[vad]`, and `[diarizer]`, plus
hashes for pure aligner, VAD, diarization, speaker-identification, speaker-
fusion, comprehensive-timeline, and common-time-base source files. It also
records the ASR, aligner, Sortformer v2.1, TitaNet, and VAD model assets.

The manifest SHA-256 is
`b736d82b5c6fefcc42c020d3398abfa2db15eaa223c1dba9a505f3c3fe661fab`.
The complete guard, including all model assets and the official checkout,
passes. Two registered tests cover guard behavior and the current fast
source/config verification. The full large-model check remains an explicit
pre-candidate and pre-capture gate rather than adding multi-gigabyte hashing to
every CTest run.

Configured model digests from the clean reproducibility manifest are:

| Model | SHA-256 |
|---|---|
| Qwen3-ASR directory | `5d5911665ea78eb50ec6703a75c37002e80274398a8fdfa45c6b1ee319671f5b` |
| Forced aligner directory | `91f73de6a92eb279b14a52f0b13b1108f6cd104138c88a5687a29874ff52125d` |
| Sortformer v2.1 | `d036020b6b93977098929d417b1b106a952ec02cc38cafc9d3315ae0ec4d90b8` |
| TitaNet directory | `e771eb2e2cbfde9977fda1f537c662006aeba8daacb56e3abe0716182e867a9a` |
| Silero VAD | `13f1f0c5d61411445c4f0d75bc4ee1a6895ec2551edb0d1d60d692d97122d2c0` |

These are mechanical identity records, not model-quality measurements.

## Engineering Baseline

The exact clean checkpoint receives a clean-first complete build. The build log
contains no compiler `warning:` or `error:` diagnostic. GCC emits only its
pre-existing C++ ABI parameter-passing notes in frozen speaker files and
speaker probes; no source change is made for those notes.

- clean build log SHA-256:
  `d35ad3b76b970f7ef20fb90c68ca67b2317fd58e04c9ae9aeebccc21880ad80b`;
- complete CTest: `77/77` passed in `52.88` seconds; and
- CTest log SHA-256:
  `1a6b28ec60176a9e89a3414f88c64fff8eafc3f6e928a9442863afbd136ab4e6`.

CTest includes the production WebSocket mechanical contract, the existing
model-parity fixtures, frozen speaker tests, and both new freeze-guard tests.
No test assigns product correctness.

No `orator_ws`, `tegrastats`, Chromium, Chrome, or Firefox process remains after
the final checks.

## Prior Full ASR Evidence Freeze

Phase 3I remains the immutable product baseline; it is not rerun in Phase 0.

| Evidence | SHA-256 |
|---|---|
| Full run JSON | `3bb41068ada63e6bb107f51263f65a1b2f3316aeb72999a2791c56fb0fd33fb1` |
| Run manifest | `cf49f15d2b225738d816d7728ced51530ddbcb804deaeaccf2b7e89f7daef0c6` |
| Pre-run manifest | `f4a9027657f28ead2ce126d853c257f31acadb30ed7a69d73cd6e34efd2eab13` |
| Complete contextual report | `ab2133f7fa70b2472d5a3cd2cad42e5d4feb187b9ef6ccea6ff949a1e16aa86b` |

These hashes preserve the prior manually reviewed 70-79 percent baseline. This
report does not reproduce, recalculate, or reinterpret that verdict.

## Official Source Freeze

The local official checkout is clean at
`7c6daf77a2421100f5fb066495372c00129d39ff`.

| Pinned source | SHA-256 |
|---|---|
| `qwen_asr/core/vllm_backend/qwen3_asr.py` | `dad51cde09bef35fae5304ba1e5d4cc02a199845128d9bbb7717c4206584aa60` |
| `qwen_asr/inference/qwen3_asr.py` | `0b1770f8e907b6c5a0a1e9ebce037cb63f48555f3cd15eaf6ea2078e9df41a7b` |
| `qwen_asr/core/transformers_backend/modeling_qwen3_asr.py` | `2fb5d98da1933748f5117ee05ce4e7150c9ead8154fb8e25f7af3968b853adc7` |

## Phase Decision

T001-T006 are complete. The speaker and non-ASR controls now have an enforced
mechanical boundary, the current engineering baseline is clean, the prior full
ASR evidence is immutable, and the official source is pinned. No runtime
candidate is authorized. Phase 1 proceeds at T007 by restoring a usable
official numerical-oracle environment.
