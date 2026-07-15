# Engineering Closing Gates - 2026-07-15

## Scope

This record closes Spec 013 T042 for clean build, warnings, complete registered
tests, JavaScript, selected host sanitizers, and selected CUDA dynamic checks.
It establishes engineering integrity only. It does not score speaker or ASR
accuracy and does not replace the full real-WebSocket and manual-review gates.
No automated gate, compiled test, script, metric, formula, query, or algorithm
may convert this evidence into product accuracy, candidate ranking, or a product
verdict; only complete contextual semantic review may do so.

The checked source was clean `master` at `ce388a7`. All evidence is retained
outside Git under `/tmp/orator-spec013/closing-gates-ce388a7/`.

## Release Gates

The Release build completed without a compiler `warning:` or `error:` record.
The complete configured suite passed 64/64 in 60.95 seconds, including:

- all C++ and CUDA unit/numerical tests;
- both official v2.1 asynchronous profile gates;
- the real-WebSocket speech/silence and multi-observer integration gate;
- the dependency-free Node Web UI model test;
- reproducibility, frozen-evidence, and review-packet tooling tests.

| Evidence | SHA-256 |
|---|---|
| Release build | `ba864cd0cca3b521582432ad57c9f641593794aa8773f8633b37172855d48093` |
| Empty warning/error result | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` |
| Release CTest 64/64 | `0dbf67c5587da2d3f005b1debf31caec2bb474c73582e2b62e3181545934d0ac` |

## ASan and UBSan

An independent Debug build used GCC AddressSanitizer and UndefinedBehaviorSanitizer
with frame pointers. Leak detection was disabled because the selected binaries
link the CUDA runtime; all other ASan and UBSan failures were configured to stop
the test immediately. The selected 25/25 host, ownership, threading, protocol,
transport, timeline, speaker-identity, and business-fusion tests passed in 3.90
seconds without a sanitizer report.

The ASan/UBSan CTest log SHA-256 is
`2264fdbca2301772fbec65cda2134715cbec427e391f4c9f64b1ae6e9a1e80b6`.

## CUDA Dynamic Checks

Compute Sanitizer 2026.2 executed the following selected checks:

| Path | Tool | Result | Evidence SHA-256 |
|---|---|---|---|
| Inherited v2.1 async FIFO/cache, 1502 frames | memcheck + full leak check | 0 errors, 0 leaked bytes | `056aa230405be8f9e6d75a2fa2fb901d4c0f7dd75849853d829af7b496a9135e` |
| Inherited v2.1 async FIFO/cache, 1502 frames | initcheck | 0 errors | `cb3e4933193a9c7143fa4e136242cde628d8ea93e94ba12f785faeee3eca76b1` |
| Public CUDA kernels, 13/13 | racecheck | 0 hazards, errors, or warnings | `48faa9e4a55be129ea82f07ce1d09d7dd0345cb0e052e4371da1ad163135e349` |
| Public CUDA kernels, 13/13 | memcheck + full leak check | 0 errors, 0 leaked bytes | `d9c23022012206cf8de983449f0f60fedb2af89a78d8ccfb046e2c157516670f` |
| Public CUDA kernels, 13/13 | synccheck | 0 errors | `46ea4ba2743e2fc7cd8a03553227d78a2b31dbe67280228a3aa087532a45a24b` |
| Batched SGEMM production shapes | memcheck + full leak check | 0 errors, 0 leaked bytes | `6d107c8c668e2cda97de80b3d74b757e47a1e38dc2c3cdc0e952c157fc284969` |
| ASR GEMM, 7/7 | memcheck + full leak check | 0 errors, 0 leaked bytes | `139ea9a423b65c889fc88976c206988cac9bda5eebc3b9c63a97d91ece3c2495` |

The full inherited v2.1 memcheck and initcheck retained numerical parity at
`max_abs=1.19209e-6`, `mean_abs=1.20224e-7`, and no significant argmax
mismatch under the `1e-5` gate.

## Resource-Bounded Check

A full 1502-frame Sortformer racecheck was attempted but stopped after its
instrumentation process reached approximately 79 GiB host memory and continued
to grow. It emitted no diagnostic before termination. This is neither a pass nor
a product failure. T042 relies on the complete model memcheck/initcheck and the
public-kernel racecheck/synccheck above. The aborted log is retained at SHA-256
`2dfc8cd82875b0a5d9a276583dc032c231abac1031f1f51b26607db86746ed9b`.

## Decision

T042 is complete. No sanitizer finding requires a code change. Product closing
remains open on the signed audible ledger, contextual accuracy, physical
microphone acceptance, candidate promotion, and two full acceptance runs.
