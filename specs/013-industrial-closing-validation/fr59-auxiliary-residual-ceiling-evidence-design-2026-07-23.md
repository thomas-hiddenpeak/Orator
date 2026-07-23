# FR59 Auxiliary Residual-Ceiling Evidence Design (2026-07-23)

## Status

T268-T271 are complete. The four required contextual-semantic readings fail
the candidate gate, so FR59 stops without product code, a model run, runtime
worker, root-TOML change, result ledger change, baseline change, or closure
claim. See `fr59-auxiliary-residual-ceiling-review-2026-07-23.md`.

## Question

FR58 found one genuinely complementary critical activity case: the
high-context v2.1 view exposes auxiliary slot 3 over `ref-0499`, where the
accepted producer has no Shi activity. FR58 correctly stopped because that
slot lacked stable global identity and no second material critical residual
shared the complete topology.

FR59 asks whether that one producer complement has a second independent
occurrence among the remaining manually signed noncritical wrong, missing, and
uncertain contributions. It does not reopen the 18 other FR58 critical
contexts, compare profile accuracy, or search parameters. The exact FR58 model
output is reused without rerunning audio.

## Evaluation authority

`test/data/reference/test.txt` is the human-listened product authority. No
compiled code, script, notebook, shell command, query, formula, metric,
spreadsheet, posterior statistic, similarity score, or algorithm may assign
speaker correctness, classify a topology, count repairs, rank evidence,
select a candidate, or issue an acceptance verdict.

Automation may copy, order, hash, schema-check, and display raw evidence. Only
complete contextual-semantic reading of every focus and named control may
decide whether a second occurrence exists.

## Frozen sources

| Item | Frozen value |
|---|---|
| Audit-start code | `a675bda936c5ca32f51beda3ecde6fdb6eb06803` |
| Accepted runtime baseline | `a6f0d33730326b19a3831019b1aba21fd900f126` |
| FR50 Run A SHA-256 | `33482f741d2467f28a436a6ffd90bdd0dd708e0e93af7cf814ef29c62d4781da` |
| FR50 Run B SHA-256 | `7ba17a7caacbe39bafd747fefd8fade4600f509e6c6372a03d1af7c7d14be65c` |
| Human reference SHA-256 | `35e8695057be82f3028877f7dc159f10ecfa0ab7f06c444cefa9a079b0e24a86` |
| Auxiliary frame SHA-256 | `504d859c0c20fcf06bff8865abdb4ceeeda7420c4f547637c47b631a68040673` |
| Auxiliary segment SHA-256 | `57a01ad7ae1ef9fe34934d4cec2016238da12417893fe81bbdfcd6d239e20508` |
| Full residual context source SHA-256 | `e9f9d3bae46cb509fed303d1ebfcf22153cde6d33e1a2d120565403cdfbfe356` |

The exact scope is
`fr59-auxiliary-residual-ceiling-contexts-2026-07-23.tsv`. It contains the
eight manually signed noncritical confidently wrong contributions
(`ref-0135`, `ref-0171`, `ref-0221`, `ref-0239`, `ref-0241`, `ref-0298`,
`ref-0457`, and `ref-0537`), three previously unaudited missing contributions
(`ref-0063`, `ref-0341`, and `ref-0409`), the uncertain `ref-0506`, and
`ref-0499` as the frozen complementary-activity anchor. Named controls come
unchanged from the previously reviewed full-residual context table.

## Evidence contract

Separate Run A and Run B packet trees must display, for every focus and
control:

- the complete human conversation;
- exact FR50 business output, ASR source, forced alignment, VAD, accepted
  Sortformer posterior/segments, primary speaker, identity epochs, and TitaNet
  evidence;
- exact FR58 auxiliary posterior and segments;
- raw auxiliary identity query scores, retained-reference provenance, and all
  conflicting or unmapped identities.

No packet field may contain an expected identity, correctness label, repair
class, aggregate, profile winner, or verdict.

## Manual gate

The reviewer must read all 13 packet contexts in this order:

1. Run A chronological;
2. Run B chronological without inheriting Run A judgments;
3. Run A reverse;
4. Run B reverse without inheriting earlier judgments.

A later candidate may be specified only when a second independent material
residual, in addition to `ref-0499`, has all of the following:

- auxiliary activity genuinely absent from the accepted producer, rather
  than a slightly shifted duplicate;
- a writable ASR/alignment/business boundary on the common clock;
- one reference-free global identity that remains causal for empty-registry
  Run A and restarted frozen-registry Run B;
- explicit abstention in every superficially matching accepted control.

The final frozen FR50 registry used by the FR58 diagnostic replay is future
session state for empty-registry Run A. It may display evidence but cannot by
itself satisfy the causal identity requirement. Same-slot assumptions across
profiles, manually inferred names, reference timestamps, source wording, or a
single additional focus are insufficient.

If the gate fails, FR59 stops without product code or another model run. If it
passes, T271 may update SDD for one false-by-default candidate before any
implementation begins.
