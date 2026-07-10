# Spec 012 Diar Tail TOML Experiments - 2026-07-10

## Scope

This review continues from `speaker-recovery-phase1-findings-2026-07-10.md`.
The target is the high-confidence wrong-diar evidence class in late-session
windows, especially 3000-3120 s and the known 3270-3304 s hard failure.

All runtime parameter experiments were represented as temporary TOML files
copied from `orator.toml`. The repository `orator.toml` was not changed.

These are diagnostic experiments. No candidate in this document is accepted as
a runtime configuration change.

## Baseline

Baseline command:

```bash
./build/diar_evidence_probe /home/rm01/test/test.mp3 \
  /tmp/orator_diar_phase1_20260710
```

Baseline metadata:

- config: `orator.toml`;
- audio duration: 3615.12 s;
- frames: 45189;
- sessions: 1;
- diar segments: 759;
- compute: 26.8487 s.

## Candidate TOML Files

| Candidate | TOML changes | Result |
|---|---|---|
| `strict_onset` | `[diarizer].onset=0.55`, `offset=0.45` | Rejected |
| `min_dur_1p2` | `[diarizer].min_dur_on=1.2` | Rejected |
| `min_dur_2p0` | `[diarizer].min_dur_on=2.0` | Rejected |
| `left2` | `[diarizer].chunk_left_context=2` | Rejected as insufficient |
| `right0` | `[diarizer].chunk_right_context=0` | Rejected |
| `left2_right0` | left context 2, right context 0 | Rejected |

## Commands

```bash
ORATOR_CONFIG=/tmp/orator_diar_strict_onset_20260710.toml \
  ./build/diar_evidence_probe /home/rm01/test/test.mp3 \
  /tmp/orator_diar_strict_onset_20260710

ORATOR_CONFIG=/tmp/orator_diar_min_dur_1p2_20260710.toml \
  ./build/diar_evidence_probe /home/rm01/test/test.mp3 \
  /tmp/orator_diar_min_dur_1p2_20260710

ORATOR_CONFIG=/tmp/orator_diar_min_dur_2p0_20260710.toml \
  ./build/diar_evidence_probe /home/rm01/test/test.mp3 \
  /tmp/orator_diar_min_dur_2p0_20260710

ORATOR_CONFIG=/tmp/orator_diar_left2_20260710.toml \
  ./build/diar_evidence_probe /home/rm01/test/test.mp3 \
  /tmp/orator_diar_left2_20260710

ORATOR_CONFIG=/tmp/orator_diar_right0_20260710.toml \
  ./build/diar_evidence_probe /home/rm01/test/test.mp3 \
  /tmp/orator_diar_right0_20260710

ORATOR_CONFIG=/tmp/orator_diar_left2_right0_20260710.toml \
  ./build/diar_evidence_probe /home/rm01/test/test.mp3 \
  /tmp/orator_diar_left2_right0_20260710
```

## Segment Counts

| Candidate | Total segments |
|---|---:|
| baseline | 759 |
| strict_onset | 720 |
| min_dur_1p2 | 538 |
| min_dur_2p0 | 400 |
| left2 | 757 |
| right0 | 750 |
| left2_right0 | 763 |

The threshold and minimum-duration candidates mainly delete segments. They do
not recover the correct speaker in the target hard window.

## Late-Window Segment Evidence

### 3000-3120 s

| Candidate | Diar duration | spk0 | spk1 | spk2 | spk3 |
|---|---:|---:|---:|---:|---:|
| baseline | 127.7 s | 37.2 | 13.3 | 2.2 | 75.0 |
| strict_onset | 118.4 s | 34.2 | 12.2 | 1.6 | 70.4 |
| min_dur_1p2 | 114.9 s | 32.6 | 8.6 | 1.7 | 71.9 |
| min_dur_2p0 | 100.5 s | 26.8 | 5.6 | 0.0 | 68.1 |
| left2 | 124.0 s | 43.1 | 11.4 | 3.2 | 66.3 |
| right0 | 122.1 s | 41.4 | 9.3 | 5.9 | 65.5 |
| left2_right0 | 120.9 s | 20.5 | 14.4 | 13.2 | 72.8 |

`left2` and `right0` reduce `spk3` duration in this window, but not enough to
justify a runtime change. The combined candidate regresses this window.

### 3240-3360 s

| Candidate | Diar duration | spk0 | spk1 | spk2 | spk3 |
|---|---:|---:|---:|---:|---:|
| baseline | 65.5 s | 13.4 | 3.8 | 14.0 | 34.2 |
| strict_onset | 59.2 s | 12.5 | 3.8 | 12.9 | 30.0 |
| min_dur_1p2 | 59.4 s | 12.3 | 3.8 | 13.2 | 30.1 |
| min_dur_2p0 | 47.8 s | 10.3 | 3.8 | 8.3 | 25.4 |
| left2 | 62.8 s | 4.3 | 4.7 | 20.6 | 33.2 |
| right0 | 63.0 s | 4.4 | 4.7 | 21.9 | 32.0 |
| left2_right0 | 60.4 s | 13.1 | 3.8 | 6.2 | 37.2 |

None of the candidates materially fixes the known hard-window region. Several
candidates reduce total evidence rather than recover the correct speaker.

### 3360-3480 s

| Candidate | Diar duration | spk0 | spk1 | spk2 | spk3 |
|---|---:|---:|---:|---:|---:|
| baseline | 92.5 s | 36.0 | 8.8 | 13.4 | 34.2 |
| left2 | 95.7 s | 50.5 | 6.8 | 1.4 | 37.0 |
| right0 | 97.0 s | 47.2 | 6.6 | 8.6 | 34.5 |
| left2_right0 | 96.1 s | 51.7 | 6.9 | 0.6 | 36.9 |

Some context candidates shift duration toward `spk0`, but the effect is not
targeted and does not solve 3270-3304 s.

## 3270-3304 s Exact Check

Baseline:

```text
3270.799927-3277.839927 local=3 conf=0.786590 margin=0.671015
3284.079927-3285.839927 local=3 conf=0.902871 margin=0.888449
3293.199926-3294.079926 local=3 conf=0.533092 margin=0.503721
3296.239926-3298.319926 local=3 conf=0.893165 margin=0.860394
3299.759926-3300.559926 local=2 conf=0.598685 margin=0.348320
3301.199926-3304.319926 local=3 conf=0.914820 margin=0.888716
```

`left2_right0`:

```text
3270.799927-3274.159927 local=3 conf=0.868479 margin=0.842891
3275.919927-3277.439927 local=3 conf=0.671136 margin=0.556078
3284.079927-3285.839927 local=3 conf=0.885193 margin=0.862461
3293.119926-3294.159926 local=3 conf=0.593683 margin=0.556099
3296.239926-3298.399926 local=3 conf=0.838268 margin=0.775474
3299.759926-3304.399926 local=3 conf=0.793463 margin=0.724169
```

The combination candidate removes the small local-2 hint around 3299.76 s and
makes the end of the hard window more uniformly `local=3`. It is rejected.

## NeMo Full-Length Reference Check

Command:

```bash
tools/.venv-nemo/bin/python tools/reference/nemo_sortformer_ref.py \
  --audio /home/rm01/test/test.mp3 \
  --out /tmp/nemo_probs_testmp3_20260710.npy
```

Result:

- NeMo output shape: `(1, 45189, 4)`;
- frame count matches the C++ probe frame count.

NeMo frame-level summaries:

| Window | Active frames | Mean top1 | Mean margin | Top speaker frame counts |
|---|---:|---:|---:|---|
| 3000-3120 s | 94.5% | 0.848 | 0.673 | spk0=383, spk1=162, spk2=42, spk3=913 |
| 3240-3360 s | 60.7% | 0.555 | 0.513 | spk0=217, spk1=164, spk2=106, spk3=1013 |
| 3360-3480 s | 74.5% | 0.672 | 0.550 | spk0=804, spk1=89, spk2=27, spk3=580 |
| 3270-3304.5 s | 55.0% | 0.495 | 0.469 | spk0=33, spk1=67, spk2=18, spk3=313 |

This confirms that the hard-window `spk3` bias exists in NeMo's own full-length
output for the same audio. The C++ port is already covered by
`test_diar_stream`, which passed with exact equality against the stored NeMo
streaming oracle sample:

```text
diar streaming vs NeMo: frames=502 max_abs=0 mean_abs=0 (tol 1e-02)
test_diar_stream PASSED
```

## Conclusion

No tested low-cost TOML candidate is accepted.

The evidence now points away from a simple runtime parameter fix:

- threshold/min-duration changes delete evidence but do not recover the correct
  speaker;
- left/right context changes produce mixed shifts and do not solve 3270-3304 s;
- NeMo full-length reference shows the same hard-window bias toward `spk3`.

The next recovery path should not continue sweeping these same TOML parameters.
Viable next directions are:

1. collect stronger independent speaker evidence for late-session windows;
2. implement a separately validated semantic speaker-business policy for
   business-view correction;
3. replace or augment the diarization model for this failure mode.

Any of these requires full-session context-aware review before acceptance.
