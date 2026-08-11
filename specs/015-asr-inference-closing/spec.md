# Spec 015: ASR Inference and Product Closing

- **Status**: Planning; no runtime candidate is authorized yet
- **Created**: 2026-08-11
- **Baseline**: `c235830254f9737f170429a1379d5cfd62657cae`
- **Historical evidence**: Spec 014 through Phase 3I
- **Speaker control**: Sortformer v2.1 and the FR50 business-speaker path are
  frozen for the complete duration of this spec

## 1. Summary

This spec restarts ASR closing from model-integration evidence rather than from
another transcript parameter experiment. The current Qwen3-ASR weights are not
treated as a weak model. The working hypothesis is that the native streaming
port does not yet preserve the official inference contract across feature
normalization, audio-encoder context, text-decoder generation, and endpoint
state.

The work is intentionally isolated from the completed speaker work. No
Sortformer, TitaNet, speaker registry, speaker fusion, speaker configuration,
or speaker-view policy may change. The frozen speaker path remains active in
real WebSocket captures only as a regression guard.

## 2. Verified Starting State

1. The complete Phase 3I candidate processed all 3615.120 seconds of
   `test.mp3`. Complete chronological and reverse contextual review of all 556
   `test.txt` contributions and all 2,664 Live events manually placed final ASR
   semantics in the 70-79 percent band. Material errors occurred in every
   complete 600-second block.
2. Wrong text is already present in ASR partial and final output before forced
   alignment, speaker projection, comprehensive-timeline rendering, or the Web
   UI. Those downstream stages are not the source of the lexical errors.
3. A numerical probe proves that independently encoding and permanently
   appending 100-mel-frame blocks is not equivalent to encoding their complete
   trained context. The observed per-slice maximum absolute difference is
   `6.163e-02` to `1.759e-01`. Independent 800-frame controls match complete
   windowed-encoder slices exactly.
4. The official streaming source accumulates PCM and re-feeds all accumulated
   audio. The current checked-in path freezes one-second acoustic states. The
   rejected accumulated candidate removed that one defect but still used the
   native attention, decoder, prompt, and VAD-bounded session contract; it did
   not establish official end-to-end numerical parity.
5. Existing numerical fixtures cover one complete mel input, one encoder
   output, one decoder prefill argmax, and split-prefill behavior. They do not
   cover long-context window attention, repeated streaming decode, every
   generated token, final-tail behavior, or full prompt/token placement.
6. The checked-in control is `stream_mode = "kv_append"`, dormant
   `stream_chunk_ms = 2000`, `stream_window_mel_frames = 100`,
   `windowed_encoder = false`, and `ban_steps = 3`.
7. The local offline tool environments currently contain CPU-only PyTorch and
   no vLLM package. Restoring a trusted, pinned numerical oracle is therefore
   the first implementation prerequisite, not an optional follow-up.

## 3. Objective

Produce one maintainable native C++/CUDA ASR path that:

- is numerically validated against the pinned official Qwen3-ASR source from
  PCM through deterministic generated token IDs;
- preserves complete conversational meaning over canonical `test.mp3`;
- preserves critical numbers, negations, names, entities, decisions, and
  commitments;
- produces useful, convergent Live and Final text without substantive silence
  hallucination;
- maintains common-clock endpoint ownership without joining unrelated speech
  or truncating an utterance;
- meets the real-time, stability, telemetry, and terminal-latency gates; and
- causes no newly accepted regression in the frozen FR50 final business view.

## 4. Change Boundary

### 4.1 Authorized ownership

Only the following runtime areas may change under this spec:

- `feature/whisper_mel`: Qwen3-ASR feature extraction and normalization;
- `model/asr_audio_tower`: Qwen3-ASR audio-encoder numerical behavior;
- `model/asr_text_decoder`: Qwen3-ASR prompt, cache, logits, sampling, and
  deterministic generation behavior;
- `model/qwen3_asr`: ASR-only streaming state and finalization;
- `pipeline/asr_worker`: ASR-local use of existing VAD evidence and ASR
  endpoint/session ownership;
- ASR-only protocol publication and ASR Live/Final Web UI state, but only after
  final-text semantics pass; and
- typed `[asr]` TOML values and ASR-specific tests/oracles.

### 4.2 Frozen ownership

The following are immutable controls:

- Sortformer v2.1 model, weights, worker, profiles, and `[diar]` values;
- TitaNet, speaker database/registry, enrollment, recognition, and `[speaker]`
  values;
- FR50 fusion policy, business-speaker logic, and speaker rendering policy;
- the shared VAD model and `[vad]` values;
- forced-aligner implementation and `[align]` values;
- comprehensive-timeline speaker ownership and `[timeline]` values;
- the common time base and all non-ASR protocol schemas; and
- `test.mp3`, `test.txt`, speaker registries used by the frozen baseline, and
  every model file outside the ASR model directory.

ASR text changes will naturally change the text consumed by the unchanged
aligner and final business view. That data-flow consequence is permitted. A
change to their implementation, configuration, timing policy, or speaker
decision is not.

## 5. Functional Requirements

### FR1 - Speaker freeze manifest

Before ASR implementation begins, record the baseline commit, hashes of frozen
model files, frozen source paths, and resolved non-ASR TOML sections. Every
candidate build and capture MUST mechanically prove that this control surface
is unchanged. Mechanical hash and diff checks do not evaluate speaker quality.

### FR2 - Pinned numerical oracle

The project MUST use the exact pinned official Qwen3-ASR revision
`7c6daf77a2421100f5fb066495372c00129d39ff` and the checked-in model hashes.
Offline Python/PyTorch tooling may generate immutable tensors, logits, token
IDs, and provenance. It may not score or judge transcripts.

A CUDA-capable official oracle is preferred. If vLLM cannot run on the Jetson,
the fallback is a source-equivalent PyTorch oracle built from the pinned
official Transformers implementation plus the official vLLM attention-window
contract. CPU execution is acceptable for bounded fixtures. An unexplained
oracle delta blocks a runtime candidate.

### FR3 - PCM-to-token parity matrix

Numerical validation MUST cover, at minimum:

- exact PCM and Whisper feature extraction, including global-max
  normalization, retained tails, amplitude changes, and unpadded final tails;
- audio-encoder outputs at 2, 8, 16, and 24 seconds, including the official
  eight-second attention boundaries and partial final windows;
- complete prompt token IDs, special-token placement, language conditioning,
  audio embedding placement, position IDs, and cache positions;
- decoder logits and selected token ID at every greedy generation step;
- complete generated token sequences for fresh, accumulated, rollback, and
  final-tail states; and
- split-prefill/cache reuse equivalence wherever the native implementation
  claims equivalence.

Existing tolerances remain unchanged. New tolerances MUST be fixed from oracle
and precision evidence before implementation output is inspected. Tolerances
MUST NOT be widened to admit a candidate.

### FR4 - First-divergence repair

Work proceeds from the first mismatching stage only. A later stage cannot be
changed to compensate for an earlier mismatch. No ASR product candidate is
authorized until the complete parity matrix has no unexplained divergence and
multiple complete generated token sequences match the trusted deterministic
reference.

### FR5 - Official-equivalent native stream

The parity path MUST preserve exact accumulated PCM, official feature
normalization, official audio-encoder attention ownership, official prompt and
language construction, deterministic decoding, rollback, EOS handling, token
budget, and unpadded final-tail behavior. Any runtime-selectable value MUST be
a typed `[asr]` TOML field resolved through the normal configuration order.

The legacy `kv_append` path remains an inactive baseline until product
acceptance. It MUST NOT be mixed with the parity path inside one decoder
session or used to select between transcript candidates.

### FR6 - ASR-local endpoint isolation

The shared VAD output remains frozen. ASR may change only how its worker groups
that existing evidence into decoder sessions and acoustic lead/trail context.
Endpoint ownership MUST stay on the common time base. ASR MUST NOT read the
diarization or speaker tracks to decide an endpoint.

Endpoint work begins only after model-path parity is established. It must
separate acoustic padding from decoder semantic state so that padding does not
silently join unrelated contributions. All behavior remains TOML-owned and
reference-free.

### FR7 - Evaluation governance

No executable code, script, query, formula, notebook, metric, lexical rule, or
algorithm may evaluate ASR meaning, endpoint correctness, hallucination,
speaker attribution, or candidate quality. Automation may run the product,
capture immutable evidence, validate numerical parity, hashes, schemas, time,
transport, and telemetry, and display unjudged evidence only.

Focused, 120-second, 360-second, and 600-second runs are diagnostic or
mechanical checks only. They cannot rank candidates, select TOML values, assign
global accuracy, reject or accept a product candidate, or stop the required
full-length review.

### FR8 - Full canonical product decision

Every ASR product candidate MUST process all 3615.120 seconds of `test.mp3`
through the production WebSocket. The reviewer MUST read every `test.txt`
contribution and all relevant ASR Live, Final, endpoint, alignment, frozen
speaker, and comprehensive evidence chronologically and then again in reverse
contextual windows. Only the reconciled complete review may produce a product
accuracy band or verdict.

Run A starts with an empty isolated registry. A candidate that passes Run A is
restarted for Run B with only Run A's frozen registry. Both runs require
independent complete review. A candidate failing a complete full review is
restored to the checked-in baseline before another hypothesis is specified.

### FR9 - Silence, microphone, and presentation

After final-text semantics pass, the exact candidate MUST complete three
independent digital-silence runs and physical-input scenarios covering room
tone, short speech, continuous speech, pauses, interruption, overlap, and
ordinary voiced background noise. ASR publication, persistence, export, and
desktop/mobile Web UI Live/Final state MUST converge without stale or duplicate
text.

Physical voiced-input closure remains explicitly blocked when no functioning
capture device is available; fake-device playback cannot substitute for it.

### FR10 - Maintainable final state

After acceptance, remove or permanently disable obsolete experimental ASR
modes that could be selected accidentally, retain only evidence-producing
diagnostics that have an owner, and synchronize this spec, Spec 014 handoff,
Spec 013 status, `PROJECT_STATE.md`, TOML comments, and tests with the accepted
code.

## 6. Acceptance Gates

| Area | Required result |
|---|---|
| Numerical parity | No unexplained stage delta; deterministic full token sequences match trusted fixtures |
| Full ASR semantics | At least 90.0% by complete contextual semantic review |
| Fixed blocks | Every complete 600-second block at least 90.0%; final 15.12 seconds reported separately |
| Critical meaning | 100% preservation of critical numbers, negations, names, entities, decisions, and commitments |
| Silence | Zero substantive final transcripts in each of three reviewed runs |
| Endpoint | No meaning-changing truncation, duplication, or unrelated cross-turn join in complete context |
| Live/Final | Convergent IDs and content across runtime, terminal tracks, persistence, export, and UI |
| Speaker guard | Frozen implementation/configuration and no newly accepted final-view regression |
| Time base | Every active track reconciles to the exact input sample extent |
| Real time | Full stream at least `0.98x` at `1.0x` pacing; direct-end terminal result within 30 seconds |
| Stability | No crash, OOM, CUDA error, race finding, or unbounded backlog |
| Telemetry | Continuous runtime telemetry and `tegrastats` with required load fields |
| Repeatability | Full Run A and Run B independently pass all applicable gates |
| Engineering | Warning-clean build and complete registered test suite pass |

Every semantic percentage, label, comparison, and verdict is manually derived
and manually checked from the two complete contextual readings.

## 7. Non-Goals

- Improving, retuning, or reopening diarization, speaker registration,
  recognition, registry, or FR50 fusion.
- Changing shared VAD, forced alignment, comprehensive speaker ownership, or
  the common time base to compensate for ASR errors.
- Adding MOSS, Unified Audio, another ASR model, or a runtime Python/vLLM
  dependency.
- Transcript-specific hotwords, names, timestamps, or rules derived from
  `test.txt`.
- Parameter sweeps, automated transcript metrics, candidate ensembles, or
  choosing whichever transcript looks better.
- Performance optimization before correctness unless a proven mechanical
  limit prevents a complete real-time capture.

## 8. Constitution Check

- Runtime remains pure C++20/CUDA with the existing closed dependency carveout.
- Every changed model stage receives trusted numerical-oracle validation.
- All endpoints and published intervals retain the one common time base.
- Product validation uses the real incremental WebSocket path.
- Runtime behavior is typed through `orator.toml`.
- Product decisions use complete contextual semantic review only.
- The frozen speaker path is neither modified nor reopened by this spec.
