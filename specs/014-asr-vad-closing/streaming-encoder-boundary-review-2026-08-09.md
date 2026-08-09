# Streaming Encoder Boundary Review (2026-08-09)

- **Scope**: Spec 014 T061-T063, native ASR integration only
- **Product evaluation**: none
- **Runtime baseline**: clean `master` at `04c564d`
- **Official source**: Qwen3-ASR commit
  `7c6daf77a2421100f5fb066495372c00129d39ff`
- **Canonical input SHA-256**:
  `b7c25d1c349b02d654b6a406bc29039749e4240a4109dda4fcc905285b14b18b`

This report records numerical tensors and source-contract inspection. It does
not compare transcript meaning, calculate accuracy, rank output, or issue a
product verdict.

## Oracle Repair and Provenance

`tools/reference/asr_oracle.py` previously resolved the model below the
nonexistent `tools/models/` tree, referred to a moved `tools/test.mp3`, assumed
an unverified `/tmp` checkout, imported Torch before path checks, and described
its transcript as ground truth. T061 repairs those contracts:

- project model, audio, TOML, and artifact paths resolve from the repository
  root;
- the official checkout location is explicit and its Git revision must match
  the pinned revision above;
- production language, system prompt, and token limit come from `orator.toml`;
- `--check-only` verifies provenance without loading Torch; and
- the tool explicitly limits its output to numerical and raw evidence.

The new standard-library contract test covers project defaults, required
official files, the accepted revision, and rejection of a mismatched revision.
The repaired check resolves the current model, canonical audio, checked-in
TOML, and exact official commit successfully. This host currently exposes only
CPU builds of Torch in both project tool environments, so a new official GPU
forward pass is unavailable. No new official transcript or tensor claim is
made from that environment.

## Encoder Locality Experiment

The existing T011 C++/CUDA probe was extended to compare any convolution-
aligned slice that divides or contains the model's 800-mel-frame attention
window. Both runs use the first 16 seconds of the same decoded audio, one full
1600-frame mel tensor, one full windowed encoder result, the same local weights,
and then independent slice encodes.

### Eight-second control

Command shape:

```text
asr_encoder_chunk_probe test.mp3 Qwen3-ASR-1.7B 16 8
```

The full encode contains 208 audio tokens. Each 800-frame standalone control
contains 104 tokens. Both complete attention windows match their corresponding
full-encode slices exactly; the overall maximum absolute tensor difference is
`0.000e+00`.

Artifact SHA-256:
`b420acc57d1679e70844d0af3a215587d5f35248cc616c25dfbbd51306aa9784`.

### One-second production append unit

Command shape:

```text
asr_encoder_chunk_probe test.mp3 Qwen3-ASR-1.7B 16 1
```

Each standalone production slice contains 100 mel frames and 13 audio tokens.
All 16 slices differ from their corresponding positions in the same complete
windowed encode. Per-slice maximum absolute differences range from
`6.163e-02` to `1.759e-01`; the overall maximum is `1.759e-01`, above T011's
`5.000e-02` numerical contract threshold.

Artifact SHA-256:
`a21140857d031be23b9c05de9904666e972da93333f931b338a4afde14396176`.

The probe therefore rejects only the implementation claim that a 100-frame
encode can be frozen and appended as though it were a complete trained
attention window. It says nothing about transcript correctness.

## Source Trace

History identifies commit `d7010a5218d2e83234e0a67c9949819ea3f1e59d` as
the change from `kStreamWindowMel=800` to `100`. The retained T011 probe still
requires complete 800-frame windows. The later Spec 003 statement that any
100-frame boundary remains equivalent was not backed by a corresponding
numerical run and is contradicted by the experiment above.

Pinned official and native source inspection shows:

| Boundary | Official implementation | Current native implementation | Finding |
|---|---|---|---|
| Prompt structure | system, user/audio, assistant, optional language + `<asr_text>` | same ordered structure | no demonstrated structural mismatch |
| Normal stream rollback | empty prefix for first N chunks, then drop K tokenizer IDs with invalid-character guard | same policy | no demonstrated broad mismatch |
| Decode | deterministic temperature-zero generation | greedy argmax | same deterministic intent; prior decoder fixture remains the numerical gate |
| Audio update | append PCM, then re-feed all accumulated audio to the model | independently encode and permanently append each completed 100-frame block | demonstrated encoder-context mismatch |
| Final tail | re-feed all accumulated audio including the short tail | append only a standalone residual encoder block | inherits the demonstrated context mismatch |

The official default uses two-second input chunks, five unfixed tokens, and
accumulated-audio re-encoding. The current source uses one-second acoustic
appends and five unfixed tokens; historical Spec 003 text claiming 15 runtime
tokens is stale.

## Bounded Correction

The first correction is `asr-final-full-context-decode`. It does not replace
Live streaming or alter its publication cadence. Instead:

1. `AsrWorker` retains exactly the PCM already admitted to each active decoder
   segment, bounded by the existing TOML segment cap;
2. Live continues to use the current incremental path as provisional text;
3. at an endpoint or safety cap, the worker closes the provisional stream and,
   when `[asr].final_full_context_decode=true`, invokes a model-agnostic final-
   segment interface on the retained PCM;
4. Qwen3-ASR runs its existing full mel, full encoder, empty-prefix decoder path
   with a separate TOML `final_max_new_tokens` limit;
5. the complete-context result becomes the only Final text supplied to forced
   alignment and the comprehensive business view; and
6. an empty complete-context result retracts an exposed provisional partial
   instead of leaving stale Live text.

The candidate inherits `384` final tokens from the original full-segment path;
it is not selected from transcript output. The Live limit remains `32`. VAD,
prompt, time boundaries, alignment, Sortformer v2.1, FR50 speaker fusion, and
all reference vocabulary remain unchanged.

Engineering tests must prove exact PCM retention, feature-disable baseline
behavior, full-context Final replacement, empty-Final retraction, TOML loading,
and resolved-config capture. Model and pipeline builds must remain warning-clean
and the complete CTest suite must pass before any product output is reviewed.

The implemented candidate satisfies that engineering gate. The complete build
contains no `warning:` or `error:` diagnostic, and all `75/75` registered tests
pass in `53.95` seconds. This includes the retained mel/encoder/decoder numerical
fixtures, the new oracle provenance contract, exact segment-PCM retention,
feature-disabled controls, full-context Final replacement, empty-Final Live
retraction, TOML loading, resolved-config capture, and the registered real-
WebSocket contract.

Build log SHA-256:
`aab4323edcade0a6b66ffdac96e01c7be65128528e14d9179b75a4cd75372cb8`.

CTest log SHA-256:
`7069a924d087fab77d0c6e28eb79741b0e87066b47a623c771b8648cf2cf5c80`.

After those gates, one focused real-WebSocket context and neighboring controls
receive complete chronological and reverse contextual semantic review. Only
that review may decide whether this correction returns to T048.
