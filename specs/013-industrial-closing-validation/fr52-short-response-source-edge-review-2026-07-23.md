# FR52 Short-Response Source-Edge Review (2026-07-23)

## Status

FR52 T236-T238 are complete. The exact FR50 Run A and Run B evidence was read
in full local conversational context in chronological order and independently
in reverse order. All four manual readings support one bounded experiment:
preserve identity-resolved primary-speaker activity in the final business view
when no text-bearing business interval represents that speaker. The experiment
must not create, copy, or infer a word.

FR50 remains the current real-WebSocket baseline. FR52 changes no code, TOML,
model, frozen ledger, or product result. FR53 is authorized only as a
false-default frozen-evidence experiment.

## Evaluation Authority

Only complete contextual-semantic reading against
`test/data/reference/test.txt` made the findings and decision below. The packet
tool only copied source evidence into reviewable worksheets. Hash, schema,
ordering, bound, and raw-copy checks are mechanical provenance evidence; they
did not label a context, rank evidence, select this contract, or issue a product
verdict.

## Frozen Evidence

| Item | Value |
|---|---|
| Runtime baseline | `a6f0d33730326b19a3831019b1aba21fd900f126` |
| Run A source | `/tmp/orator-spec013/release-a6f0d33-fr50-precompute/run-full-a.json` |
| Run B source | `/tmp/orator-spec013/release-a6f0d33-fr50-precompute/run-full-b.json` |
| Run A worksheet | `/tmp/orator-spec013/release-a6f0d33-fr52-source-edge/run-full-a-worksheets` |
| Run B worksheet | `/tmp/orator-spec013/release-a6f0d33-fr52-source-edge/run-full-b-worksheets` |
| Run A content manifest SHA-256 | `c690871443d5b3f7b18303680d44c5645872624f817f800457bdfce5a385f7e7` |
| Run B content manifest SHA-256 | `a46f824a0c6c1f9d0ae3bd2c0e9c56445d26305937050e640257bd2ea9ba3989` |
| Context table SHA-256 | `eb4ab38a332f88121c958d67a30f0fbf466aeed316b3185718ac33743a385382` |

Both manifests pass their listed payload checks. Each packet preserves the
complete ASR source, per-codepoint alignment, VAD, Sortformer posterior,
activity and primary tracks, voiceprint evidence, local identity epochs,
business decision audit, and final business pieces for all six contexts.

## Four Independent Readings

1. Run A chronological: every focus and named control was read in conversation
   order from `ref-0049` through `ref-0390`.
2. Run B chronological: the same contexts were read from the independently
   restarted full artifact without importing Run A judgments.
3. Run A reverse: every context and control was re-read from `ref-0390` back to
   `ref-0049`.
4. Run B reverse: the reverse reading was repeated independently on Run B.

The four readings agree on every finding below.

## Manual Context Findings

| Context | Complete-context finding | FR53 disposition |
|---|---|---|
| `ref-0049` | ASR retains the response, but forced alignment places both `亿` and `对` at `432.732` with zero duration. Tang has strong secondary posterior activity while Shi remains primary. No Tang primary interval survives. | Abstain. Secondary posterior alone cannot create a final speaker-activity record in this experiment. |
| `ref-0066` | ASR retains Shi's `我我们俩可以` and omits Tang's overlapping `你们俩可以`. A distinct Tang primary interval survives at `478.639989302-479.119989291`; its global identity is unique within the source span. The surrounding Shi and Tang controls remain coherent. | Material positive. Preserve the Tang interval as speaker activity with empty text. |
| `ref-0118` | ASR retains both `对呀` responses. The first response crosses the Shi-to-Tang primary boundary and the final view splits its two characters. Tang is already represented by a text-bearing interval, while `ref-0066` proves that a generic phrase-tail rewrite would steal words from a simultaneous Shi phrase. | Abstain from both duplicate activity and text rewriting. This needs a separate phrase-ownership contract. |
| `ref-0313` | ASR retains Tang's `对吧` and omits Shi's following `对`. A short primary island survives, but local slots 0 and 1 both resolve to Tang in this source span; the candidate island has no voiceprint embedding. The available identity contradicts the reference response. | Abstain on native-slot alias conflict. Do not guess Shi. |
| `ref-0331` | ASR ends Tang's question at `2362.124` and the next source begins at `2364.556`. Between them, a distinct Shi primary interval survives at `2362.799947187-2363.199947178`; its global identity is unique within the source span. No source word exists for the response. | Material positive. Preserve the Shi interval as speaker activity with empty text. |
| `ref-0390` | ASR retains a zero-duration `对` at `2690.380`. The right edge first selects Xu and later Tang, while local slots 0 and 1 both map to Tang inside the same ASR source. Neither short primary run has a voiceprint embedding. | Abstain on competing identities and native-slot alias conflict. No nearest-neighbour or slot-sum rewrite is authorized. |

## Authorized Reference-Free Contract

FR53 may append a `speaker_activity` business entry only when all of these
conditions hold from runtime evidence alone:

1. The immutable primary interval lies wholly inside one finalized ASR source
   span and carries a non-empty global `speaker_id`.
2. No text-bearing business entry of the same global identity overlaps that
   primary interval. Existing text and its ownership remain unchanged.
3. Across diarization and primary evidence intersecting that ASR source span,
   the candidate global identity maps to exactly one native speaker label.
   Multiple native labels for the same global identity force abstention.
4. The output reuses the primary interval's exact common-clock start, end,
   native label, global identity, and confidence. It does not move a boundary.
5. The appended entry has the same `text_id`, an explicit
   `content_kind=speaker_activity`, empty `text`, primary-evidence provenance,
   and no inferred lexical content.

The policy has one false-default TOML enable flag and introduces no numerical
parameter. With the flag disabled, terminal and revision output must remain
byte-identical to FR50 behavior.

## Required Abstentions

- No primary interval: `ref-0049` remains unchanged.
- Same speaker already represented by text: `ref-0118` remains unchanged.
- One global identity mapped from multiple native labels in the source span:
  `ref-0313` and `ref-0390` remain unchanged.
- Empty or unknown global identity, a primary interval crossing an ASR source
  boundary, or any need to invent text always abstains.

## Next Gate

FR53 must implement the contract behind a false-default TOML flag, prove the
disabled path unchanged, and replay exact frozen Run A and Run B once with the
flag enabled. Automation may list raw changed entries and arrange complete
contexts only. Every changed context and its controls must then receive Run A
chronological/reverse and Run B chronological/reverse semantic review before
the branch may be retained or enabled for a real-WebSocket ladder.
