# FR58 Auxiliary Streaming-Context Evidence Design (2026-07-23)

## Status

T263-T267 are complete. Four independent complete-context readings stop FR58
at its evidence gate and authorize no product candidate. The accepted FR50
real-WebSocket result, root `orator.toml`, models, speaker registry sequence,
business output, manual ledger, and closure status remain unchanged. See
`fr58-auxiliary-streaming-context-review-2026-07-23.md`.

## Question

FR51 already read every accepted pipeline for all 19 critical residuals.
FR45 and FR53-FR56 then exhausted the current alignment-gap, source-free
activity, gallery-overlap, and local-epoch hypotheses. FR58 asks one different
question: can another deployment-valid streaming context of the same
Sortformer v2.1 checkpoint preserve useful raw speaker evidence that the
accepted `340/1/188/188` context loses or contradicts?

The auxiliary source is NVIDIA's checked-in high-latency streaming profile,
`340/40/40/300` for chunk/right-context/FIFO/update-period. It uses the same
v2.1 weights as the accepted producer. It is therefore a correlated context
view, not an orthogonal model, offline oracle, or candidate replacement. Its
historical standalone result was rejected and cannot be reused as an accuracy
claim. Historical complete-context reading that exposed complementary evidence
at `ref-0499` is only the reason to test the hypothesis.

## Evaluation Authority

`test/data/reference/test.txt` is the human-listened product authority. No
compiled code, script, notebook, shell command, JavaScript, formula, query,
metric, spreadsheet, or algorithm may assign speaker correctness, map an
auxiliary local slot from the reference, classify a topology, count or
aggregate a result, rank either context profile, select a candidate, or issue
an acceptance verdict.

Automation may run the exact model profile; capture and display raw values;
verify numerical-oracle parity, hashes, schemas, row order, deterministic
bytes, model/config identity, and common-clock bounds; and arrange unjudged
evidence. Only complete contextual-semantic reading of every focus and control,
in all four required directions, may decide whether the hypothesis survives.

## Frozen Sources

| Item | Frozen value |
|---|---|
| Code at audit start | `880262695a52ae02d0e8b9f37930665e1ae5dec5` |
| Accepted runtime baseline | `a6f0d33730326b19a3831019b1aba21fd900f126` |
| FR50 Run A | SHA-256 `33482f741d2467f28a436a6ffd90bdd0dd708e0e93af7cf814ef29c62d4781da` |
| FR50 Run B | SHA-256 `7ba17a7caacbe39bafd747fefd8fade4600f509e6c6372a03d1af7c7d14be65c` |
| Exact streamed PCM | SHA-256 `17f0edda49989f3ceada60170885091023eeb9d67faae0d6dd67bb585b8857fe` |
| Exact lossless WAV wrapper | SHA-256 `a60af82d57958ddfaf5d0358820adc7536544c97d59aab9f55a3b2f53694a16e` |
| Human reference | SHA-256 `35e8695057be82f3028877f7dc159f10ecfa0ab7f06c444cefa9a079b0e24a86` |
| Root runtime TOML | SHA-256 `d00150ae376d802af0fcf8c0a89aa3fae1e0abb2bf5d10601c55cefd570a40db` |
| Auxiliary high-latency TOML | SHA-256 `1b7e26776ef3bb2dd8f33012d013d7f4516803fb1e11a72a2fd8d8b63091b897` |
| Sortformer v2.1 weights | SHA-256 `d036020b6b93977098929d417b1b106a952ec02cc38cafc9d3315ae0ec4d90b8` |
| TitaNet-Large weights | SHA-256 `59e1e3069a33bef8da8b9467a5a64a30a5de8c7ae2550b15d5323ef537acaad1` |
| Frozen four-speaker registry | SHA-256 `66461a77755984a08231d06306da7ce9e1eeac07be1927e91f8a772fc54c7b3f` |

The context scope is the exact 19-critical table already frozen for FR51:
`fr51-critical-residual-contexts-2026-07-23.tsv`. Its focus rows and named
controls are immutable for FR58. No reference-derived interval may be added to
the runtime or used to filter auxiliary output.

## Evidence Capture

1. Run `diar_evidence_probe` twice over the exact WAV wrapper with
   `ORATOR_CONFIG` set to the checked-in high-latency TOML.
2. Preserve all raw 80 ms posterior rows and all onset/offset segments without
   reference lookup, threshold search, relabelling, or result filtering.
3. Verify both outputs span the exact session clock, retain deterministic row
   order and bytes, and use the frozen model/config/audio identities.
4. Run the already registered v2.1 high-profile NeMo numerical test. It proves
   C++ parity for the configured model path only; it does not establish product
   quality.
5. If a canonical identity is attached to an auxiliary local slot, display the
   complete reference-free TitaNet input spans, registry source, session and
   robust scores, ambiguity, and epoch bounds. A local slot remains unassigned
   when that evidence is incomplete or conflicting.

## Display Contract

Run A and Run B receive separate packet trees. Every context must contain the
complete `test.txt` conversation and unmodified intersecting evidence from:

- FR50 ASR source and forced alignment;
- VAD probabilities/segments;
- accepted Sortformer frames, activity, primary track, and identity epochs;
- TitaNet interval, phrase, VAD, source, and primary-run evidence;
- final business pieces and decision audit;
- auxiliary high-context frames, onset/offset segments, and any explicitly
  sourced local-slot identity evidence.

The packet may state paths, hashes, row counts, clocks, and schema validity. It
must contain no expected identity, correctness field, candidate label, profile
winner, repair count, aggregate, or verdict.

## Manual Gate

The reviewer must read all focus and control conversations in this order:

1. Run A chronological;
2. Run B chronological without inheriting Run A judgments;
3. Run A reverse;
4. Run B reverse without inheriting the prior passes.

A production experiment is authorized only if at least two material residuals
show one reusable, reference-free auxiliary-evidence topology and every
superficially matching accepted control gives an explicit abstention. The
topology must explain activity, boundary, and stable global identity on the
common clock. Agreement caused only by the same weights, a manually inferred
slot name, source wording, a known reference timestamp, or one isolated focus
is insufficient.

## Conditional Runtime Boundary

If the manual gate passes, a later reviewed SDD update may define one
false-by-default TOML candidate. The auxiliary implementation must:

- remain an incremental streaming v2.1 worker, never an offline model;
- receive the immutable session `TimeBase` from the ingest owner;
- own its model state, cursor, and buffers;
- publish an additive typed track to `ComprehensiveTimeline`;
- avoid direct access to any other pipeline's state or output;
- expose deterministic global-identity reconciliation and abstain on incomplete
  or conflicting evidence;
- preserve every accepted track and final label when disabled;
- pass focused concurrency/time-base tests, both relevant NeMo profile gates,
  warning-clean build, complete CTest, frozen replay and semantic review, then
  the 120/600/full A/B real-WebSocket ladder.

If the manual gate fails, FR58 stops at evidence capture. No production code,
root-TOML value, model, product run, ledger, baseline, T084 state, or closure
claim changes.
