# FR54 VAD Evidence Capture Design (2026-07-23)

## Status

T244-T247 are complete. FR54's context-only table and exact source hashes are
frozen. The current terminal timeline stores finalized VAD segments and typed
endpoint frontiers, but not the Silero probability for each 512-sample window.
The evidence-only probe and packet extensions capture that missing raw evidence
without changing runtime behavior. Four independent contextual readings reject
a runtime experiment because the two material contexts do not share one safe
speech-presence topology.

The required evidence can be captured without modifying `IVad`, `GpuVad`,
`AuditoryStream`, `ComprehensiveTimeline`, any runtime decision, or any TOML
value. Only the existing offline `vad_segment_probe` requires an optional raw-
evidence output mode.

## Frozen Sources

| Item | SHA-256 |
|---|---|
| Exact FR50 Run A | `33482f741d2467f28a436a6ffd90bdd0dd708e0e93af7cf814ef29c62d4781da` |
| Exact FR50 Run B | `7ba17a7caacbe39bafd747fefd8fade4600f509e6c6372a03d1af7c7d14be65c` |
| Exact WAV wrapper | `a60af82d57958ddfaf5d0358820adc7536544c97d59aab9f55a3b2f53694a16e` |
| Exact stream PCM | `17f0edda49989f3ceada60170885091023eeb9d67faae0d6dd67bb585b8857fe` |
| Checked-in `orator.toml` | `d00150ae376d802af0fcf8c0a89aa3fae1e0abb2bf5d10601c55cefd570a40db` |
| Silero VAD weights | `13f1f0c5d61411445c4f0d75bc4ee1a6895ec2551edb0d1d60d692d97122d2c0` |
| Sortformer v2.1 weights | `d036020b6b93977098929d417b1b106a952ec02cc38cafc9d3315ae0ec4d90b8` |
| Human-listened `test.txt` | `35e8695057be82f3028877f7dc159f10ecfa0ab7f06c444cefa9a079b0e24a86` |
| FR54 context table | `2c5e4a64c3d4466b75c3d471b102f5e1e3ceacf95eebf5b68f811e9b238bbf7c` |
| FR54 review-context table | `b5e4df43208c00b968f4e16e1f86f6d70783609add1cea9b77bcfcbdd584f5f3` |

The context table is
`fr54-short-primary-speech-presence-contexts-2026-07-23.tsv`. It contains only
IDs, focus/control roles, exact focus and complete-context common-clock bounds,
and source/link IDs. It has no expected speaker, speech label, correctness
value, candidate condition, or verdict.

The companion
`fr54-short-primary-speech-presence-review-contexts-2026-07-23.tsv` maps each
location to a complete display window and named neighbouring reference blocks.
It uses the already governed `speaker_residual_evidence_packet.py` schema and
also contains no expected output, correctness label, or verdict.

## Existing Contract

`GpuVad::DrainSegments` computes one Silero probability for every 512 input
samples, applies the TOML threshold and endpoint counters, and emits finalized
sample-index segments. `GpuVad::state()` exposes the typed observed, active,
stable-active, and stable-silence frontiers. `GpuVad::DebugWindowProbs` already
returns the raw probability vector and is numerically covered against the CPU
reference by `test_vad` at the recorded `2e-3` tolerance.

The production terminal artifact retains finalized VAD segments, not the raw
probability vector. Reconstructing probabilities from segments would discard
the evidence FR54 needs and is not allowed.

## Evidence-Only Probe Extension

Preserve the current invocation:

```text
vad_segment_probe <audio> <config.toml> <segments.tsv>
```

Add one optional pair of outputs:

```text
vad_segment_probe <audio> <config.toml> <segments.tsv> \
  <window_probabilities.tsv> <endpoint_states.tsv>
```

When the optional pair is present:

1. One fresh `GpuVad` instance calls `DebugWindowProbs` over the exact audio and
   writes one raw row per complete 512-sample window: evidence ID, exact sample
   bounds, exact common-clock bounds, and probability.
2. A separate fresh `GpuVad` instance follows the existing probe's production-
   style one-second `Push`/`DrainSegments` cadence. After each drain and final
   flush, it copies `VadStateResult` into the state TSV with exact sample/time
   frontiers.
3. The original finalized segment output remains byte-compatible in schema and
   continues to validate monotonic sample bounds.
4. All VAD model, threshold, endpoint, pad, and sample-rate values come only
   from the supplied TOML. The probe adds no parameter and does not classify a
   window, summarize probabilities, choose a threshold, rank evidence, or
   issue a product judgment.

The probe output is diagnostic evidence under `tools/`; it is not a runtime
dependency or a new product track.

Extend `speaker_residual_evidence_packet.py` with optional raw VAD window and
endpoint-state TSV inputs. The existing invocation and packet schema remain
valid when they are absent. When present, the tool verifies the exact headers,
monotonic bounds/frontiers, and source hashes, then copies only rows intersecting
each provided complete-context window. This is display-only range selection;
the packet tool must not compare probabilities, apply a threshold, or label a
context.

## Mechanical Verification

- Build `vad_segment_probe` and `test_vad` warning-clean.
- Run `test_vad` to preserve GPU/CPU probability parity and endpoint contracts.
- Run the extended probe twice on a short exact-audio prefix and require
  byte-identical outputs, valid headers, monotonic sample bounds, one
  probability row per complete 512-sample window, and state frontiers bounded
  by observed samples.
- Run once on the exact full WAV and record output hashes. These checks establish
  provenance and determinism only; they do not evaluate speech or speakers.

The extended probe passes its focused build and `test_vad`; GPU/CPU probability
parity remains within the existing `2e-3` numerical tolerance. Two exact-prefix
captures are byte-identical. The exact full capture records:

| Output | SHA-256 |
|---|---|
| Finalized segments | `dc94ff46a463a1a3317e6642d8c374c4ef059b1e35d5cff10dd7fd686f170390` |
| Raw 512-sample probabilities | `0d1bc0f7d4a5905330f649080afd56dc2756fe6bd7c4e003a3b6ff9d956a0b93` |
| Production-cadence endpoint states | `c2e3cf145dcf44e973c5f783da1c4e026fcdea7a05141f99f95ebcfd451a5b01` |

The full segment rows mechanically match both frozen terminal VAD tracks at
their serialized bounds. The independent A/B worksheet content manifests are
`d8b309de64ef877d16d7e17ccf7263fa90e2116888fb0c95c2da5cb9253f2e73`
and `08c0d5a036f7dd408189ed5319e6267d26cb7360bfae9d3147c1f7b77e45c72d`;
every listed payload passes its hash check. These are provenance statements,
not product judgments.

## Constitutional Review Gate

Complete raw evidence was read for every FR54 context in Run A chronological,
Run B chronological, Run A reverse, and Run B reverse order. The manual result
is recorded in
`fr54-short-primary-speech-presence-review-2026-07-23.md`. No executable
labelled a probability sequence, compared a material/control result,
aggregated a score, or selected a producer rule. T247 stops the branch; the
capture tooling remains available for future evidence audits.
