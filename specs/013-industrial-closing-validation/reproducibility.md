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
  --reference test/data/audio/test.txt \
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
- commit and dirty paths observed by the client;
- client `ORATOR_*` values and invocation parameters;
- host, Jetson release, power mode, and clock state.

The static fixture manifest hash is copied into the review record for every run.
Any missing hash, missing `resolved_config`, dirty-path mismatch, or unrecorded
override invalidates an accuracy or performance comparison.
