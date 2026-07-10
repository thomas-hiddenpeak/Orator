# Spec 012 TitaNet Tail Evidence Review - 2026-07-10

## Scope

This review tests whether the orthogonal TitaNet speaker-embedding model can
provide corrective evidence for the late-session speaker-business failures,
especially the 3270-3304 s hard window.

This is an evidence review only. It does not accept a runtime speaker override
or TOML change.

## Inputs

- Audio: `/home/rm01/test/test.mp3`;
- Reference transcript for constitutional review context:
  `/home/rm01/test/test.txt`;
- Frozen full-session WebSocket capture:
  `/tmp/orator_support_diag_full_20260710.json`;
- Diar span TSV:
  `/tmp/orator_diar_spans_20260710.tsv`;
- Speaker embedding weights:
  `models/speaker/titanet_large.safetensors`.

The diar span TSV was generated from the frozen diarization track:

```bash
jq -r '.timeline.tracks[] | select(.kind=="diarization") | .entries[] |
  [.start,.end,.speaker,(.speaker_id // "null"),(.confidence // 0)] | @tsv' \
  /tmp/orator_support_diag_full_20260710.json \
  > /tmp/orator_diar_spans_20260710.tsv
```

Result:

```text
773 /tmp/orator_diar_spans_20260710.tsv
```

## Commands

```bash
cmake --build build --target speaker_embedding_probe -j

./build/speaker_embedding_probe /home/rm01/test/test.mp3 \
  models/speaker/titanet_large.safetensors \
  /tmp/orator_diar_spans_20260710.tsv \
  600 3 4 \
  > /tmp/orator_titanet_bucket600_20260710.txt

./build/speaker_embedding_probe /home/rm01/test/test.mp3 \
  models/speaker/titanet_large.safetensors \
  /tmp/orator_diar_spans_20260710.tsv \
  60 1 5 \
  > /tmp/orator_titanet_bucket60_min1_20260710.txt

./build/speaker_embedding_probe /home/rm01/test/test.mp3 \
  models/speaker/titanet_large.safetensors \
  /tmp/orator_diar_spans_20260710.tsv \
  30 1 5 \
  > /tmp/orator_titanet_bucket30_min1_20260710.txt
```

## 600 s Bucket Result

Metadata:

```text
clean spans=153  groups=25  bucket=600.0s min_sec=3.0
```

The late-session `L3@3000-3600` centroid remains strongly similar to earlier
L3 buckets:

| Pair | Cosine |
|---|---:|
| `L3@3000-3600` vs `L3@0000-0600` | 0.849 |
| `L3@3000-3600` vs `L3@0600-1200` | 0.875 |
| `L3@3000-3600` vs `L3@1200-1800` | 0.874 |
| `L3@3000-3600` vs `L3@1800-2400` | 0.811 |
| `L3@3000-3600` vs `L3@2400-3000` | 0.846 |

Same-window comparisons do not support overriding L3 to another local speaker:

| Pair | Cosine |
|---|---:|
| `L3@3000-3600` vs `L0@3000-3600` | 0.526 |
| `L3@3000-3600` vs `L1@3000-3600` | 0.470 |
| `L3@3000-3600` vs `L2@3000-3600` | 0.444 |

## 60 s Bucket Result

Metadata:

```text
clean spans=295  groups=133  bucket=60.0s min_sec=1.0
```

Target tail groups:

```text
L3@3240-3300 refs=5 best=[3299.76-3304.40] q=3.707 id=spk_3
L3@3300-3360 refs=2 best=[3317.84-3331.04] q=12.434 id=spk_3
L3@3360-3420 refs=2 best=[3373.36-3377.52] q=3.557 id=spk_3
```

For `L3@3240-3300`, the highest matches are historical L3 buckets:

| Rank | Match | Cosine |
|---:|---|---:|
| 1 | `L3@1380-1440` | 0.741 |
| 2 | `L3@2580-2640` | 0.720 |
| 3 | `L3@1680-1740` | 0.717 |
| 4 | `L3@1260-1320` | 0.714 |
| 5 | `L3@0900-0960` | 0.696 |

Best non-L3 alternatives for `L3@3240-3300` are much lower:

| Local | Best match | Cosine |
|---|---|---:|
| L0 | `L0@0780-0840` | 0.440 |
| L1 | `L1@0300-0360` | 0.383 |
| L2 | `L2@3300-3360` | 0.331 |

For `L3@3300-3360`, the same pattern holds:

| Rank | Match | Cosine |
|---:|---|---:|
| 1 | `L3@1320-1380` | 0.766 |
| 2 | `L3@1380-1440` | 0.751 |
| 3 | `L3@1260-1320` | 0.744 |
| 4 | `L3@1740-1800` | 0.706 |
| 5 | `L3@1080-1140` | 0.699 |

Best non-L3 alternatives for `L3@3300-3360` are:

| Local | Best match | Cosine |
|---|---|---:|
| L0 | `L0@3600-3660` | 0.395 |
| L1 | `L1@0300-0360` | 0.451 |
| L2 | `L2@0240-0300` | 0.331 |

## 30 s Bucket Result

Metadata:

```text
clean spans=295  groups=196  bucket=30.0s min_sec=1.0
```

The 30 s bucket gives the closest available voiceprint view for the exact hard
region. There is no `L3@3240-3270` group after clean-span filtering, but
`L3@3270-3300` is present.

For `L3@3270-3300`, the top matches remain L3:

| Rank | Match | Cosine |
|---:|---|---:|
| 1 | `L3@3300-3330` | 0.762 |
| 2 | `L3@1770-1800` | 0.724 |
| 3 | `L3@2580-2610` | 0.723 |
| 4 | `L3@1410-1440` | 0.709 |
| 5 | `L3@1260-1290` | 0.704 |

Best non-L3 alternatives for `L3@3270-3300`:

| Local | Best match | Cosine |
|---|---|---:|
| L0 | `L0@0780-0810` | 0.440 |
| L1 | `L1@1440-1470` | 0.424 |
| L2 | `L2@1020-1050` | 0.321 |

For `L3@3300-3330`, the top matches are also L3:

| Rank | Match | Cosine |
|---:|---|---:|
| 1 | `L3@1320-1350` | 0.793 |
| 2 | `L3@1410-1440` | 0.790 |
| 3 | `L3@3270-3300` | 0.762 |
| 4 | `L3@1260-1290` | 0.749 |
| 5 | `L3@2580-2610` | 0.726 |

## Interpretation

TitaNet is useful as an orthogonal evidence source because it tests speaker
identity through voiceprint similarity rather than Sortformer frame ownership.
However, in this specific late-session failure window, TitaNet does not
contradict the Sortformer `L3` attribution.

The evidence is not safe for an automatic speaker override:

- coarse 600 s buckets say late L3 is still close to historical L3;
- 60 s buckets preserve the same conclusion around 3240-3360 s;
- 30 s buckets preserve the same conclusion around 3270-3330 s;
- best non-L3 alternatives are consistently lower than best historical L3
  matches.

## Conclusion

Do not implement a runtime TitaNet override for 3270-3304 s.

The voiceprint stage remains valuable for uncertainty reporting, future
cross-session identity work, and candidate review, but this test rejects it as
an accepted accuracy fix for the current tail failure. The next recovery path
should focus on explicit business-view policy or model-level augmentation, and
any accepted change still requires full-length context-aware review under
`speaker-business-method.md`.
