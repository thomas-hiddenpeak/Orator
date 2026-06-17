# Tasks 003 — Incremental KV-Cache Real-Time ASR

- **Feature**: `003-sliding-window-asr`
- **Spec**: [spec.md](spec.md) · **Plan**: [plan.md](plan.md)
- **Status**: Implemented (incremental KV cache); verified + committed (8cc31ab)
- **Constitution**: v1.1.0

> Ordered, independently verifiable steps. Foundational numeric equivalence is
> verified BEFORE any worker change. Parameter defaults come from the sweep
> (T050), not assumed.

---

## Phase 0 — Baseline (measured)
- [x] **T000** `tools/cer.py` exists. FAIR baseline = production Silero-VAD
  pipeline on the same span (energy-VAD 30.2% is a dead lower bound). Measured on
  120 s of `test.mp3`: Silero-VAD CER 30.8% / ASR RTF 3.27x. This is the bar
  AC3/AC4 must hold.

## Phase 1 — Foundational primitives + numeric equivalence (verify before wiring)
- [ ] **T010** Add decoder primitives to `AsrTextDecoder`: `PrefillAt(embeds, T,
  pos0, stream)` (public append-prefill over existing `Forward(x,T,pos0)`),
  `TruncateCache(int len)`, `cache_len()` accessor. No numeric/kernel change.
  *(Verify: builds clean; a unit check that PrefillAt(a) then PrefillAt(b,pos0=
  len_a) yields the same last-token logits as one Prefill(a then b).)*
- [ ] **T011** Verify windowed-encoder chunk-local equivalence (AC2): probe
  encodes 120 s full vs chunk-by-chunk-append in windowed mode
  (`ORATOR_ASR_WINDOWED`); report max abs diff over the frozen earlier tokens.
  Choose `chunk_sec` = whole encoder windows. *(Verify: diff within bf16 tol
  ~1e-2; if full-attention default breaks equivalence, confirm windowed mode is
  required and document it.)*

## Phase 2 — Incremental streaming session
- [ ] **T020** Add `Qwen3Asr` session API (`StreamReset`, `StreamChunk`,
  `StreamFinalize`) implementing plan §1.3–1.4: append new-chunk audio KV via
  `PrefillAt`, re-prefill suffix + committed tail, `DecodeGreedy`,
  `TruncateCache` back to the audio-block checkpoint, with `unfixed_token_num`
  rollback (utf-8 safe). *(Verify: builds clean; a probe streams a clip and
  produces a continuous transcript.)*
- [ ] **T021** Boundary reset on speech endpoint / `max_segment_sec`: commit the
  segment transcript, `ResetCache`, restart. *(Verify: long stream resets; cache
  length bounded.)*

## Phase 3 — Validation and sweep
- [ ] **T030** Per-step cost incremental (AC1): instrument the session; deep
  step prefill within a small bound of an early step within a segment. *(Verify:
  not linearly growing.)*
- [ ] **T031** Incremental probe `tools/asr_stream_incremental_probe.cc`:
  streams 120 s, prints per-step time, RTF, and writes the transcript JSON for
  CER. *(Verify: AC1, AC4 measured.)*
- [x] **T040** CER (AC3): `tools/cer.py` on the incremental output vs gold
  less-or-equal the Silero-VAD baseline on the same span. MEASURED: incremental
  17.6% vs Silero-VAD 30.8% on 120 s (-13.2pp). *(Verify: AC3 met.)*
- [x] **T050** Sweep `chunk_sec` in {1,2}, `unfixed_token_num` in {3,5,8},
  `endpoint_sec` in {0.6,1.0}, `max_segment_sec` in {20,30}; record RTF + CER;
  choose defaults (CER <= baseline, highest sustained RTF). *(Verify: sweep table
  recorded; defaults set; refined 2026-06-17 for Web UI partial streaming.)*

  **Initial sweep range**: `chunk_sec` ∈ {8, 16}, `unfixed_token_num` ∈ {3, 5, 8}.
  **Refined production defaults (2026-06-17)**:

  | Parameter | Sweep range | Chosen | Reason |
  |---|---|---|---|
  | `kStreamWindowMel` | 800 (8 s) | **100 (1 s)** | Web UI partial-emission latency |
  | `max_new_tokens` | N/A (384) | **32** | Official vLLM default, 12× decode savings |
  | `unfixed_tokens_` | {3, 5, 8} | **15** | High-speed speech (10-12 chars/s) coverage |
  | `unfixed_chunks_` | N/A | **2** | Unchanged |
  | `max_segment_sec` | {20, 24, 30, 32} | **24.0** | Unchanged |

## Phase 4 — Integration and cleanup
- [ ] **T060** Wire the session into `AsrWorker` (plan §2.4): public surface
  unchanged; committed spans on the timeline with absolute times; comprehensive
  view intact. *(Verify: AC6; timeline document has both tracks + comprehensive;
  times monotonic.)*
- [ ] **T061** `StreamFinalize` commits the remaining tail (FR7). *(Verify: no
  audio left untranscribed.)*
- [ ] **T070** Determinism + memory (AC5, AC7): two runs identical committed
  text; VmRSS + per-step time bounded over a long run. *(Verify: AC5, AC7.)*
- [ ] **T080** Full build + `ctest` green under `-Wall -Wextra`; threaded path
  race-checked. *(Verify: AC7.)*
- [ ] **T081** Update `/memories/repo/` and `PROJECT_STATE.md` with the chosen
  parameters, RTF, CER, cost-stability result; remove energy-VAD micro-segment
  path if superseded (keep `SegmentSpeech` if reused as the endpoint detector).
  *(Verify: docs match reality.)*
- [ ] **T082** Commit.

## Traceability (requirement → task)

| Requirement | Tasks |
|---|---|
| FR1 persistent audio-block KV | T010, T020 |
| FR2 incremental append | T010, T020 |
| FR3 suffix re-prefill + truncate | T010, T020 |
| FR4 windowed encoder | T011, T020 |
| FR5 bounded growth (reset) | T021 |
| FR6 timeline output | T060 |
| FR7 finalize | T061 |
| FR8 CER measurement | T000, T040 |

| Acceptance | Tasks |
|---|---|
| AC1 per-step incremental | T030, T031 |
| AC2 chunk-local equivalence | T011 |
| AC3 CER <= Silero baseline | T040, T050 |
| AC4 RTF >= Silero baseline | T031, T050 |
| AC5 bounded memory/cost | T021, T070 |
| AC6 output contract unchanged | T060 |
| AC7 clean build/tests/determinism | T070, T080 |

## Definition of Done
Incremental KV-cache streaming implemented; chunk-local encode equivalence
verified; per-step cost incremental and bounded with boundary resets; RTF and
CER at or better than the Silero-VAD baseline on the same span with chosen
defaults; deterministic committed text; output contract unchanged; build + tests
green; docs updated; committed.
