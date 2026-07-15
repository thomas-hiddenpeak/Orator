# Spec 012 Speaker-Uncertain Business View - 2026-07-10

> **Evaluation governance:** Under Constitution 1.7.0, no code or executable
> automation may assign correctness, calculate accuracy, rank/select a
> candidate, or issue a verdict. Automated values below are mechanical evidence
> only; product results require complete contextual semantic review and manual
> result verification.

## Purpose

Phase 1 found two late-session failure classes:

- sparse wrong evidence, where support diagnostics can identify weak or missing
  selected-speaker evidence;
- confident wrong evidence, where raw diarization strongly supports the wrong
  speaker and support diagnostics alone cannot detect the error.

This change addresses only the first class as a safety improvement. It adds an
explicit machine-readable business-view field:

```json
"speaker_uncertain": true
```

The raw `speaker`, `speaker_id`, `speaker_support`, and diar support metrics are
preserved. The field does not rewrite the speaker and is not an accepted
speaker-accuracy recovery.

## Rule

For every comprehensive entry:

- `speaker_support == "strong"` => `speaker_uncertain = false`
- `speaker_support == "weak"` => `speaker_uncertain = true`
- `speaker_support == "none"` => `speaker_uncertain = true`

The rule is intentionally derived from the already TOML-gated support
diagnostics. It introduces no additional runtime parameter.

## Runtime Surface

The field is emitted in:

- live comprehensive revision JSON;
- final `timeline.comprehensive` JSON;
- Web UI normalized model state.

The Web UI already marks weak/none support visually through the speaker-support
badge. The new field is for downstream business consumers that need a stable
boolean instead of interpreting the support string.

## Validation

Commands run:

```bash
git diff --check
node --check web/js/model.js
node --check web/js/render/timeline.js
node --check web/js/render/transcript.js
cmake --build build -j
./build/test/test_comprehensive_timeline
./build/test/test_config
./build/test/test_json
cmake --build build -j 2>&1 | rg "warning:|error:"
cd build && ctest --output-on-failure
ORATOR_CONFIG=orator.toml ./build/orator_ws
python3 tools/verify/py/ws_unified_test.py \
  --pcm /home/rm01/test/test.mp3 \
  --duration 30 --rate 0 --port 8765 \
  --out /tmp/orator_uncertain_30s_20260710.json
```

Observed:

- build passed;
- `test_comprehensive_timeline` passed;
- `test_config` passed;
- `test_json` passed;
- CTest passed: 47/47;
- JS syntax checks produced no output;
- warning/error grep produced no output;
- 30 s real WebSocket run succeeded: audio 30.00 s, wall 14.58 s, stream RT
  2.058x, diar 6 segments, ASR 12 utterances;
- final comprehensive entries: 12;
- missing `speaker_uncertain`: 0;
- support counts: `strong=9`, `weak=2`, `none=1`;
- uncertain counts: `false=9`, `true=3`;
- support/uncertain mismatch: 0.

## Acceptance Boundary

Accepted as a business-view safety field.

Not accepted as speaker-accuracy recovery. It does not solve high-confidence
wrong diar evidence, including 3000-3120 s and parts of 3350-3615 s.
