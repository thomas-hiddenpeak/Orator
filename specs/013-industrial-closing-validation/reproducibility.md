# Reproducibility Manifest and Artifact Names

## Artifact names

Closing artifacts use UTC and the exact source commit:

```text
orator-<stage>-<YYYYMMDDThhmmssZ>-<commit12>-<duration>-<run>.json
```

`stage` is one of `baseline`, `candidate`, `acceptance-a`, or `acceptance-b`;
`duration` is `120s`, `360s`, `600s`, or `full`; `run` is a monotonically
increasing two-digit number. A WebSocket capture always has a sibling
`.manifest.json` produced by `ws_unified_test.py`.

## Static fixture manifest

Before a baseline or acceptance series, freeze the shared fixture:

```bash
python3 tools/verify/py/repro_manifest.py \
  --repo . \
  --config orator.toml \
  --audio test/data/audio/test.mp3 \
  --reference test/data/reference/test.txt \
  --executable build/orator_ws \
  --registry /path/to/frozen-speakers.bin \
  --out /path/to/orator-fixture-<timestamp>-<commit12>.json
```

The acceptance command must not use `--skip-model-hashes`. The manifest records
the commit and dirty paths, SHA-256 for configuration, audio, reference,
executable, registry, every model file, and aggregate model directories. It also
records environment overrides, device identity, power mode, clock state, and
tool versions.

## Per-run manifest

The real WebSocket server places the complete post-precedence
`resolved_config` object in every terminal timeline. The unified client writes
the capture first, hashes it, and then writes `<capture>.manifest.json` with:

- capture, source-container, and streamed-PCM SHA-256 values;
- the complete resolved configuration and its canonical SHA-256;
- configuration file, Git commit/worktree-content, and server-binary SHA-256
  snapshots taken before streaming and again after the terminal timeline; the
  worktree digest includes tracked diffs and every unignored untracked file, so
  changing an already-dirty file is detectable;
- an explicit consistency result requiring the resolved configuration path and
  all three pre/post snapshots to match;
- client `ORATOR_*` values and invocation parameters;
- host, Jetson release, power mode, and clock state.

For 1x runs of at least 120 seconds, the same client requires at least 95
percent coverage for GPU utilization, memory, system power, CPU, RAM, and
temperature fields and for both runtime/device sample cadence. Field presence
does not compensate for missing time samples.

Manual runs pass the same files used to launch the server:

```bash
python3 tools/verify/py/ws_unified_test.py \
  --config-path orator.toml --server-binary build/orator_ws \
  --pcm test/data/audio/test.mp3 --duration 3615.12 --rate 1.0 \
  --require-telemetry \
  --out /path/orator-baseline-<timestamp>-<commit12>-full-01.json
```

The static fixture manifest hash is copied into the review record for every run.
Any missing hash, missing `resolved_config`, pre/post source drift, dirty-path
mismatch, or unrecorded override invalidates an accuracy or performance
comparison.

Reproducibility automation establishes artifact identity and execution
conditions only. No manifest, WebSocket client, validator, script, test,
notebook, formula, query, or algorithm may evaluate semantic/speaker results,
calculate accuracy, rank candidates, or issue an acceptance verdict. Those
results require complete contextual semantic review plus manual tally and
manual independent verification under Constitution 1.7.0 Article VI.
