# FR60 Cross-View Identity-Bridge Evidence Design (2026-07-23)

## Status

T272-T275 are complete. The four required contextual-semantic readings fail
the identity-bridge gate, so FR60 stops without an auxiliary runtime worker,
identity mapping, fusion candidate, product code, root-TOML change, model run,
product run, result-ledger change, baseline change, or closure claim. See
`fr60-cross-view-identity-bridge-review-2026-07-23.md`.

## Question

FR59 found two genuinely complementary auxiliary activity cases across the
complete residual scope: noncritical `ref-0239` and critical `ref-0499`. Both
stop before product use because the short auxiliary segment has no causal
global identity. Direct segment TitaNet evidence is weak or conflicting, the
frozen final FR50 registry is future state for empty-registry Run A, and local
slot numbers are reused or confused elsewhere.

The accepted producer already publishes globally identified diarization and
primary-speaker tracks on the same session clock. The comprehensive timeline
can retain both independent views and a derived pipeline may revise its own
business track after later evidence arrives. FR60 asks whether raw common-clock
intersections with the accepted globally identified producer establish a
reference-free, decision-time identity bridge for both complementary islands
without assuming that an auxiliary slot has one identity for the full session.

## Constitutional boundary

The auxiliary producer and accepted producer remain independent. A possible
future auxiliary worker would deposit a distinct typed track into
`ComprehensiveTimeline`; a derived business pipeline could read both tracks
only from that store. No direct callback, pointer, shared flag, or pipeline
read is permitted. All source and intersection bounds remain on the ingest-
owned common time base.

`test/data/reference/test.txt` is the sole product authority. No compiled
code, script, notebook, shell command, query, formula, metric, spreadsheet,
posterior statistic, similarity score, or algorithm may assign speaker
correctness, derive an identity mapping, classify a bridge, count repairs,
rank evidence, select a candidate, or issue an acceptance verdict.

Automation may validate and display every raw pairwise interval intersection.
It must emit all intersecting accepted rows and an explicit no-intersection row
when none exists. It may not aggregate overlap by identity, choose a dominant
identity, derive an epoch, suppress a conflict, or add expected names or
correctness fields.

## Frozen sources

| Item | Frozen value |
|---|---|
| Audit-start code | `78bcb7ef28b631cf36edb08466805cfd237927b9` |
| Accepted runtime baseline | `a6f0d33730326b19a3831019b1aba21fd900f126` |
| FR50 Run A SHA-256 | `33482f741d2467f28a436a6ffd90bdd0dd708e0e93af7cf814ef29c62d4781da` |
| FR50 Run B SHA-256 | `7ba17a7caacbe39bafd747fefd8fade4600f509e6c6372a03d1af7c7d14be65c` |
| Run A manifest SHA-256 | `caf59d530b380b73752110e773eb3189764dab69033c766092176cbaa88c1075` |
| Run B manifest SHA-256 | `a102c418960a3e9bbadf743e37501d70be9da8137cdd83986f809cedcfab221c` |
| Human reference SHA-256 | `35e8695057be82f3028877f7dc159f10ecfa0ab7f06c444cefa9a079b0e24a86` |
| Auxiliary A/B frame SHA-256 | `504d859c0c20fcf06bff8865abdb4ceeeda7420c4f547637c47b631a68040673` |
| Auxiliary A/B segment SHA-256 | `57a01ad7ae1ef9fe34934d4cec2016238da12417893fe81bbdfcd6d239e20508` |

No audio, model, posterior, segment, accepted track, registry, or product view
is rerun or recalculated. The exact six fixed full-session blocks are frozen in
`fr60-cross-view-identity-bridge-contexts-2026-07-23.tsv`. They cover
`0.000-3615.120 s` without a gap and retain every human reference block; the
named focus and control rows only orient reading and do not limit evidence.

## Raw intersection schema

For every auxiliary onset/offset segment, the display packet must preserve:

- its source row index, absolute start/end, session, local slot, confidence,
  and mean margin;
- every intersecting accepted `diarization` row and every intersecting accepted
  `primary_speaker` row, including source index, absolute start/end, local slot,
  global speaker ID, and confidence;
- the exact pairwise intersection start/end and duration;
- one explicit row per accepted track when that auxiliary segment intersects
  no row in that track.

Rows are ordered only by source coordinates, source indexes, and track name.
The schema contains no mapping, selected identity, expected identity, bridge
status, winner, aggregate, repair label, or verdict.

The display utility validates every accepted source row before testing an
intersection, preserves auxiliary and accepted-track source indexes, and
emits a distinct absence row for each accepted track. Fourteen focused tests
and the registered `test_speaker_residual_evidence_packet` CTest target pass.

## Manual review gate

The reviewer must read all six full-session blocks in this order:

1. Run A chronological;
2. Run B chronological without inheriting Run A judgments;
3. Run A reverse;
4. Run B reverse without inheriting earlier judgments.

Every pass reads the complete `test.txt` conversation, auxiliary segments,
accepted diarization and primary tracks, raw pairwise intersections, and final
business output. The review must inspect all auxiliary local slots, not only
local 2 and local 3, so slot reuse and contradictory anchors remain visible.

A later candidate may be specified only if the complete contextual readings
establish all of the following directly:

1. `ref-0239` and `ref-0499` each have surrounding accepted global-identity
   anchors on the same auxiliary local slot, with source horizons available
   through the comprehensive timeline before the proposed final rewrite.
2. No contradictory accepted global-identity anchor occurs inside the exact
   local continuity span used for either focus. A full-session slot-number
   mapping is forbidden.
3. One reference-free rule can express both bridges using only typed track
   rows, common-clock bounds, source order, and identities already published by
   the accepted producer. Human names, reference IDs, reference timestamps,
   text content, final-registry state, and hardcoded focus times are forbidden.
4. Every superficially matching accepted contribution and every FR59 negative
   control has an explicit structural abstention.
5. Empty-registry Run A and restarted frozen-registry Run B expose the same
   bridge and abstention contract under independent reading.

If any clause fails, FR60 stops without product code or another model run. If
all clauses pass, T275 may freeze one false-by-default candidate architecture,
typed timeline contract, delayed-revision semantics, resource budget,
numerical gates, and constitutional real-WebSocket promotion ladder before
implementation begins.

## Constitution check

- **Article I/II**: no runtime dependency, model, precision, or accuracy trade
  is introduced by the evidence phase.
- **Article III**: evidence is related only by absolute common-clock intervals;
  any future pipeline contract must communicate through
  `ComprehensiveTimeline` and keep each producer track immutable.
- **Article IV**: no streaming or performance claim is made. A passing future
  candidate would still require the full real-WebSocket ladder.
- **Article V/VI**: the utility exposes complete raw rows and tests mechanical
  invariants only. Four complete contextual-semantic readings own every
  product judgment.
- **Article VIII/IX**: state documents change with the audit. FR60 adds no
  runtime setting; any later behavioral parameter must be typed and configured
  through `orator.toml`.
