# Spec 012 Speaker-Support Diagnostics - 2026-07-09

## Purpose

The tail evidence review concluded that 3270-3304 s is not safely recoverable by
another acoustic-only speaker override using the currently captured evidence.
This change therefore does not force a new speaker decision. It exposes the
strength of diarization support behind each runtime comprehensive entry so weak
business ownership can be inspected explicitly in JSON and in the Web UI.

## Runtime change

Each `ComprehensiveTimeline::Entry` now carries these derived diagnostics for
the selected speaker over the exact entry interval:

- `diar_overlap_sec`
- `diar_total_overlap_sec`
- `diar_coverage_ratio`
- `diar_total_coverage_ratio`
- `diar_max_gap_sec`
- `diar_island_count`
- `speaker_support`: `none`, `weak`, or `strong`

The diagnostics are serialized in both live `revision` messages and final
`timeline.comprehensive` entries. They do not alter speaker attribution, ASR
text, diarization segments, VAD segments, or forced-alignment units.

## Configuration

The support label thresholds are TOML-gated under `[timeline]`:

```toml
speaker_support_min_coverage_ratio = 0.50
speaker_support_max_gap_sec = 1.00
speaker_support_max_islands = 1
```

An entry is labelled `weak` when the selected speaker has too little interval
coverage, has a selected-speaker evidence gap above the configured maximum, or
is assembled from too many separated selected-speaker islands. Unknown/no
selected-speaker evidence is labelled `none`; otherwise the entry is `strong`.

## Web UI

The modular Web UI preserves these fields in live revisions and final timelines.
Transcript rows with `speaker_support = "weak"` show a compact marker on the
speaker badge and expose the support details in the badge tooltip. The raw JSON
timeline view also includes the fields when rendering live state before a final
flush/end document arrives.

## Validation

Commands run on 2026-07-09:

```bash
cmake --build build -j
./build/test/test_comprehensive_timeline
./build/test/test_config
cd build && ctest --output-on-failure
cmake --build build -j 2>&1 | rg "warning:|error:" || true
node --check web/js/model.js
node --check web/js/render/transcript.js
node --check web/js/render/timeline.js
ORATOR_CONFIG=orator.toml ./build/orator_ws
python3 tools/verify/py/ws_unified_test.py --pcm /home/rm01/test/test.mp3 \
  --duration 30 --rate 0 --port 8765 \
  --out /tmp/orator_support_diag_30s_20260709.json
```

Observed result:

- Build passed.
- `test_comprehensive_timeline` passed, including sparse same-speaker island
  support labelled `weak` and complete coverage labelled `strong`.
- `test_config` passed, including the new `[timeline]` support thresholds.
- CTest passed: 47/47.
- Warning/error grep produced no output.
- JS syntax checks produced no output.
- Short real WebSocket field-propagation check passed: 30.0 s audio,
  14.65 s wall, stream RT 2.048x, 12 final comprehensive entries. The final
  timeline entries all included the support fields; support levels were
  `weak=2`, `strong=9`, `none=1`.

## Acceptance boundary

This is an evidence-visibility improvement, not an accepted accuracy recovery.
Before claiming speaker-business accuracy improvement, a full-length real
WebSocket run with `test.mp3` and a context-aware review under
`speaker-business-method.md` are still required.
