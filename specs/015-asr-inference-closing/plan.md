# Spec 015: Implementation and Validation Plan

## 1. Execution Principle

This plan replaces transcript-led parameter experimentation with a causal
sequence:

1. freeze the speaker and non-ASR control surface;
2. restore a trusted official numerical oracle;
3. locate the first PCM-to-token divergence;
4. repair and validate one stage at a time;
5. construct one official-equivalent native ASR candidate;
6. run one complete canonical product evaluation; and
7. address ASR-local endpoint or presentation defects only when the complete
   review establishes that the numerically equivalent model path is not
   sufficient.

No transcript output is used to choose an implementation or TOML value before
the full product decision.

## 2. Baseline and Freeze

The planning baseline is clean `master` at
`c235830254f9737f170429a1379d5cfd62657cae`. Before implementation:

- record Git, worktree, root TOML, model, binary, device, and official-source
  provenance;
- create a speaker-freeze manifest covering Sortformer v2.1, TitaNet,
  speaker/registry/fusion code, and `[diar]`, `[speaker]`, `[vad]`, `[align]`,
  and `[timeline]` resolved values;
- retain the Phase 3I full artifact and its complete review as the immutable ASR
  product baseline;
- add a mechanical changed-path/config guard for every candidate; and
- make no claim that Spec 015 changes the independent canonical status of the
  FR50 speaker line.

The guard may hash and compare files and resolved values. It may not evaluate
speaker output.

## 3. Ownership Map

| Area | Disposition |
|---|---|
| Whisper mel and ASR feature state | May change after oracle exists |
| Qwen3-ASR audio encoder | May change one proven delta at a time |
| Qwen3-ASR text decoder | May change one proven delta at a time |
| Qwen3-ASR streaming state | May change after component parity |
| ASR worker VAD consumption | Deferred until model parity; ASR-local only |
| ASR protocol and Draft/Final UI | Deferred until final semantics pass |
| Shared VAD producer | Frozen |
| Forced aligner | Frozen |
| Sortformer/TitaNet/registry/fusion | Frozen |
| Comprehensive speaker timeline | Frozen |
| Common time base | Frozen |

Any need to cross a frozen boundary stops the phase and requires explicit owner
review. It is not handled as an incidental refactor.

## 4. Phase 0 - Reproducible Control

1. Freeze the source and configuration manifest.
2. Verify the current build is warning-clean and the complete CTest suite
   passes.
3. Verify the checked-in ASR control values and all frozen non-ASR values.
4. Verify no test server, client, browser capture, or `tegrastats` process is
   running.
5. Record that no runtime behavior changes in this phase.

Exit condition: a clean, mechanically enforceable baseline exists.

## 5. Phase 1 - Restore the ASR Oracle

The current `tools/.venv` and `tools/.venv-nemo` environments are CPU-only and
contain no vLLM package. The oracle is repaired before native model code.

### 5.1 Preferred path

Restore a CUDA-capable offline PyTorch environment on the Jetson and load the
exact official Transformers backend plus local Qwen3-ASR weights. Keep the
official checkout clean and pinned to
`7c6daf77a2421100f5fb066495372c00129d39ff`.

### 5.2 Bounded fallback

If official vLLM cannot execute on aarch64/Jetson:

- use the pinned official Transformers implementation for processor, mel,
  prompt, embeddings, decoder, and generation;
- implement the official vLLM eight-second `cu_seqlens` attention contract as
  an offline PyTorch oracle adapter without modifying the official checkout;
- run bounded fixtures on CPU if a CUDA PyTorch environment remains
  unavailable; and
- record the environment limitation and exact source derivation.

A compatible external CUDA host may generate immutable oracle artifacts only
when its hardware/software provenance and hashes are recorded. Imported
artifacts are numerical fixtures, never product transcripts or acceptance
evidence.

### 5.3 Oracle output

The oracle writes raw PCM, input features, encoder states, prompt IDs, position
IDs, per-step logits, selected token IDs, cache-state metadata, and full
generated token IDs under `models/reference/asr/spec015/`. It writes no
correctness label, score, ranking, or product verdict.

Exit condition: at least one complete fixture can be regenerated from pinned
source and its artifact hashes are stable.

## 6. Phase 2 - First-Divergence Matrix

Use two reference-free fixture families:

- deterministic synthetic PCM for silence, amplitude changes, boundary tails,
  and cache reproducibility; and
- fixed source-order PCM from the canonical audio, selected without reading or
  optimizing its transcript.

The minimum matrix is:

| Boundary | Cases |
|---|---|
| Feature extraction | full and incremental; 2/8/16/24 seconds; louder late audio; unpadded tail |
| Audio encoder | 100-frame diagnostic, 800-frame official window, 2/8/16/24 seconds, partial window |
| Prompt | empty context, configured language, exact official special-token order |
| Decoder prefill | fresh prefill, split prefill, audio embedding replacement, long cache positions |
| Generation | every logit/argmax step, EOS, 32-token cap, repeated-token guard |
| Streaming state | first two unfixed chunks, five-token rollback, invalid-character extension, final tail |

For each case:

1. compare the earliest available tensor or token boundary;
2. stop at the first unexplained delta;
3. fix only that owning stage;
4. rerun all earlier gates plus the changed gate; and
5. add the fixture to CTest before moving downstream.

The running Whisper maximum receives an explicit causality check. If a later
maximum changes the correct normalization of already frozen frames, the native
path must reprocess or retain enough state to restore exact accumulated-audio
features; comments claiming incremental exactness are not accepted as proof.

Exit condition: no unexplained delta exists from PCM through multiple complete
generated token sequences at 2, 8, 16, and 24 seconds.

## 7. Phase 3 - Native Parity Stream

After Phase 2, implement one inactive native mode with these properties:

- exact accumulated segment PCM;
- full accumulated feature recomputation or a numerically proven equivalent;
- official eight-second encoder attention ownership and partial-tail handling;
- exact official prompt, language, special-token, and position construction;
- exact deterministic greedy sampling without compensating EOS behavior;
- official unfixed-chunk, token rollback, and final-tail state;
- no selection between native transcripts; and
- bounded memory under the existing ASR segment cap.

Runtime selections remain typed `[asr]` TOML fields. The root TOML stays on the
existing baseline while the implementation is inactive. Tests cover disabled
behavior, invalid combinations, resolved configuration, state reset, common
clock extents, and repeated sessions.

Engineering gate:

- all new oracle fixtures pass;
- warning-clean build;
- complete CTest pass;
- no changed frozen file or resolved non-ASR value;
- no CUDA error or memory growth in repeated component sessions; and
- one short real-WebSocket mechanical capture completes with all track extents
  reconciled. Its transcript receives no product verdict.

Exit condition: a clean inactive implementation commit exists. A second clean
commit may activate the exact parity mode in an isolated candidate TOML.

## 8. Phase 4 - First Full Product Candidate

No 102-second transcript screening is performed. A 120-second and, if needed,
600-second run may verify transport, memory, throughput, terminal timing,
telemetry, and artifact completeness only.

The exact clean candidate then runs all 3615.120 seconds at 1.0x/100 ms through
the production WebSocket with:

- empty isolated registry for Run A;
- early and late observers;
- direct `end`;
- continuous runtime telemetry and `tegrastats`;
- immutable source/config/model/binary provenance; and
- exact common-time-base reconciliation.

The reviewer reads all 556 reference contributions with Final and endpoint
evidence in chronological order and again in reverse contextual windows. Every
Live event is also read in both directions for user-visible hallucination and
rewrite behavior. Frozen speaker and comprehensive output are reviewed only as
a regression guard.

Decision:

- If Run A satisfies every semantic and critical gate, proceed to Phase 6.
- If Run A fails, restore the root TOML and retain the immutable report. Use the
  complete manual review to identify the earliest owning class: model parity,
  ASR-local endpoint, prompt/context contract, or publication. A new candidate
  requires new source/numerical evidence; no parameter sweep is allowed.

## 9. Phase 5 - Conditional ASR Endpoint Work

This phase is entered only if the numerically equivalent full candidate shows
that model text is materially damaged by ASR session ownership.

The shared VAD producer remains byte/config identical. The design separates:

- speech-region evidence supplied by VAD;
- acoustic lead/trail samples retained for recognition; and
- the decoder session boundary that owns semantic history.

Potential implementation must be justified from complete Run A evidence and
common-clock traces, not from a timestamp score. It may reset or retain ASR
state only through an explicit typed ASR policy. It may not read diarization or
speaker identity, and it may not change forced alignment or final speaker
fusion.

After numerical and mechanical gates, the endpoint candidate returns directly
to a complete Phase 4 full run. Short runs remain diagnostic.

## 10. Phase 6 - Repeatability and Product Surface

After a Run A pass:

1. restart with only Run A's frozen registry and capture complete Run B;
2. repeat the full chronological and reverse contextual review independently;
3. run three independent digital-silence sessions and review each directly;
4. validate ASR partial/retract/final convergence in desktop and mobile
   Chromium, persistence, reload, reconnect, and export;
5. validate physical microphone room tone, short speech, continuous speech,
   pauses, interruption, overlap, and voiced background noise when functional
   hardware is available; and
6. confirm the frozen speaker path has no newly accepted final-view regression.

Only ASR event ownership and ASR Draft/Final presentation may change here.
Speaker rendering and comprehensive speaker policy stay frozen.

## 11. Phase 7 - Handoff

When both full runs and all available product gates pass:

- make the accepted values the checked-in `[asr]` TOML baseline;
- remove or permanently disable obsolete ASR experiment modes;
- retain reproducible oracle fixtures and their provenance;
- write the complete Spec 015 closing report;
- update Spec 014 as historical handoff, Spec 013 applicability,
  `PROJECT_STATE.md`, README/runtime configuration documentation, and tests;
- execute the locked holdout and report it separately; and
- create a release tag only after all applicable project gates are signed.

## 12. Failure and Stop Rules

- Three unsuccessful attempts to repair the same numerical boundary require a
  return to the last trusted fixture and a new causal analysis.
- An unexplained oracle delta blocks all product output.
- A frozen-path code/config delta blocks the candidate mechanically.
- A mechanical full-run failure may stop capture but produces no semantic
  verdict.
- A full semantic failure restores the root TOML before new work.
- No endpoint, prompt, cadence, token-budget, or rollback sweep is permitted.
- No later pipeline may compensate for text that ASR failed to produce.

## 13. Artifact Layout

```text
models/reference/asr/spec015/          # immutable numerical fixtures
artifacts/spec015/baseline/            # baseline manifests and checks
artifacts/spec015/parity/              # raw stage evidence
artifacts/spec015/candidates/<name>/    # exact product captures
artifacts/spec015/reviews/              # reviewer-authored full reports
```

`artifacts/` remains gitignored. Specs and final reports retain hashes and
provenance, not large runtime payloads.
