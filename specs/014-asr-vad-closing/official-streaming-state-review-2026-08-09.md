# Official Streaming State Review (2026-08-09)

- **Scope**: Spec 014 T069, source and state-transition audit only
- **Native control**: clean `master` at `81b6503`
- **Official source**: Qwen3-ASR commit
  `7c6daf77a2421100f5fb066495372c00129d39ff`
- **Product evaluation**: none

This report follows three rejected focused candidates. It records source-level
causality and the next bounded implementation contract. It does not evaluate a
transcript, calculate accuracy, rank output, or issue product acceptance.

## Why the Prior Window Candidate Failed

The 800-frame candidate correctly used one complete trained encoder-attention
window. Its numerical locality remained exact. However, it also reduced decode
continuation from one update per second to one update per eight seconds while
retaining the 32-token per-step generation budget. Complete contextual review
then found two long Finals cut off before critical clauses.

The official streaming example also uses `max_new_tokens=32`, but it performs a
continuation every two seconds. The budget and cadence are one state-machine
contract; moving only the acoustic append boundary invalidated that relationship.
Numerical encoder parity could not establish transcript completeness.

## Pinned Official Contract

`qwen_asr/inference/qwen3_asr.py` lines 584-829 and
`examples/example_qwen3_asr_vllm_streaming.py` lines 64-94 establish:

1. incoming mono 16 kHz PCM may arrive in arbitrary call sizes;
2. the state buffers until one complete two-second chunk is available;
3. every consumed chunk is appended to `audio_accum`;
4. every decode re-feeds all accumulated audio without padding;
5. chunk IDs zero and one use no prior transcript prefix;
6. later chunks tokenize the prior raw decode and remove five trailing tokens,
   extending rollback when decoding would split an invalid character;
7. generated continuation replaces the rolled-back suffix rather than being
   compared with another transcript;
8. finalization appends any residual tail without padding and decodes once more;
   and
9. the official streaming example uses deterministic generation with a
   32-token limit.

Official streaming is currently exposed only by its vLLM backend. Orator cannot
adopt vLLM because the runtime has zero third-party dependencies. The state
transition can nevertheless be implemented over the existing native mel,
encoder, tokenizer, and deterministic decoder, whose component contracts remain
covered by the retained numerical tests.

## Native Delta

Current `Qwen3Asr::StreamChunk` freezes independently encoded acoustic slices
into one persistent decoder cache. It never re-encodes accumulated segment
audio. `TranscribeWindow` already provides the needed native primitive: encode
all supplied PCM, append a provided committed text prefix to the prompt, decode
deterministically, and return only the new continuation.

The bounded correction adds an `accumulated_redecode` branch behind typed TOML.
It retains exact PCM only within the existing 24-second VAD segment, advances in
official two-second chunks, computes the rollback prefix from native tokenizer
IDs, and calls `TranscribeWindow` on the growing PCM prefix. Finalization handles
the residual tail. The existing `kv_append` behavior remains the default control
until all engineering gates pass.

## Controls

- no prompt, VAD, endpoint, segment-cap, alignment, diarization, speaker, time-
  base, or Web UI policy changes;
- no reference words, timestamps, or candidate-output inspection in runtime;
- no code-based transcript comparison, scoring, ranking, or selection;
- resolved configuration captures mode, chunk duration, and rollback counts;
- exact real-WebSocket evidence only after a clean control implementation commit;
  and
- immediate restoration of `kv_append` on performance or contextual failure.

## Inactive Implementation Checkpoint

The native implementation now carries `stream_mode`, `stream_chunk_ms`,
`stream_unfixed_chunks`, and `stream_unfixed_tokens` from `orator.toml` through
the resolved configuration and `AsrConfig`. `Qwen3Asr` validates those values
and implements the growing-PCM decode loop, rollback prefix, and unpadded tail
behind `accumulated_redecode`. The checked-in mode remains `kv_append`; therefore
this checkpoint changes capability but does not activate candidate output.

Focused `test_config`, `test_qwen3`, `test_asr_worker`, and `test_registration`
pass `4/4`. Complete CTest passes `75/75` in `53.14` seconds. A subsequent
clean-first build completes with no warning or error diagnostic; the observed
GCC ABI notes predate and do not arise from this change. Retained mel, encoder,
decoder, oracle-provenance, registration, WebSocket, and Web model gates are
included in that complete run. These are engineering and numerical contracts,
not transcript evaluation or product acceptance.

Inactive implementation commit `2acae3a` is pushed to `master`. The next
candidate changes the only output-affecting value from `stream_mode =
"kv_append"` to `stream_mode = "accumulated_redecode"`; configuration assertions
and SDD status change with it, while all other runtime values remain fixed.
With that candidate active, focused tests pass `4/4`, complete CTest passes
`75/75` in `50.92` seconds, and a clean-first build again emits no warning or
error. Real-WebSocket capture and contextual review have not yet occurred.

Phase 3E capture and complete contextual review are now recorded in
`official-accumulated-2s-review-2026-08-09.md`. The two-second candidate is
rejected and checked-in TOML returns to `kv_append`. Its long-segment behavior
supports retaining the implementation for a separately specified one-second
cadence trial; it does not support product promotion.
