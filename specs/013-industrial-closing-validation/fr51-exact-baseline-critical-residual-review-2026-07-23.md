# FR51 Exact-Baseline Critical-Residual Review

## Authority and scope

This record completes T233-T235. It is a contextual-semantic review of the 19
business-critical residuals manually retained after FR50. It does not evaluate
ASR wording as a separate product, calculate a score, rank configurations, or
change the accepted ledger. Every product observation below was written by
reading the complete local conversation against the human-listened
`test/data/reference/test.txt` context. No executable comparison assigned an
earliest-loss layer or selected the decision at the end of this document.

The review used four independent passes:

1. exact FR50 Run A from `ref-0049` to `ref-0505`;
2. exact FR50 Run B in the same direction without inheriting Run A labels;
3. Run A from `ref-0505` back to `ref-0049`;
4. Run B in the same reverse order.

Every pass reread the complete reference section, all named controls, final
business pieces and decision audit, ASR source, forced alignment, VAD,
Sortformer activity and primary evidence, TitaNet evidence, and local identity
epochs. Historical FR49 findings were provenance controls only.

## T233 mechanical capture

These are source-integrity facts, not product judgments:

| Fixture | Frozen value |
|---|---|
| Code | `a6f0d33730326b19a3831019b1aba21fd900f126`, clean `master` artifact manifests |
| Checked-in TOML SHA-256 | `d00150ae376d802af0fcf8c0a89aa3fae1e0abb2bf5d10601c55cefd570a40db` |
| Resolved-config SHA-256 | `5453fec307fbc9a55976074666cf28505366b103e8244f87a058f2a491e3dc20` |
| Sortformer v2.1 weights SHA-256 | `d036020b6b93977098929d417b1b106a952ec02cc38cafc9d3315ae0ec4d90b8` |
| Stream PCM SHA-256 | `17f0edda49989f3ceada60170885091023eeb9d67faae0d6dd67bb585b8857fe` |
| Exact WAV wrapper SHA-256 | `a60af82d57958ddfaf5d0358820adc7536544c97d59aab9f55a3b2f53694a16e` |
| Human reference SHA-256 | `35e8695057be82f3028877f7dc159f10ecfa0ab7f06c444cefa9a079b0e24a86` |
| FR51 context-table SHA-256 | `386fe47a3ecf7819942678b0d6ae8d8493b0b060996c6b77086d311d6fe0c191` |
| Probe binary SHA-256 | `ee7402366668c8a55492e3cc152c5793f833dce3f586c131b0786b905c6661fc` |
| Full Run A artifact SHA-256 | `33482f741d2467f28a436a6ffd90bdd0dd708e0e93af7cf814ef29c62d4781da` |
| Full Run B artifact SHA-256 | `7ba17a7caacbe39bafd747fefd8fade4600f509e6c6372a03d1af7c7d14be65c` |

`diar_evidence_probe` was run twice over the exact lossless stream under the
checked-in TOML. The independent compute durations were `36.4363 s` and
`37.2591 s`. Each capture contains 45,189 four-channel frames over
`0.000000-3615.039919 s` with an `0.08 s` frame period and 755 onset/offset
segments. Both raw-frame files have SHA-256
`79fd2c416ac76a0af477f98bf8d848f6e604b2d94c5c4445e653978afd6c7e41`;
both segment files have SHA-256
`94a2a758ef9771e1646d27eb56a6257421ae620870e3bf467eb9fed9976264c0`.
Equality establishes repeatable capture only.

The existing display-only packet tool produced independent worksheet trees:

| Packet | Content-manifest SHA-256 | Mechanical result |
|---|---|---|
| A | `d7c0cf0ad8287b259d3189da94f1ecfa01c5f56faecfbc1a21f6455b685b45a0` | 19 context directories; all 116 listed payloads pass `sha256sum -c` |
| B | `a80922ef1906cfe52c0c39f8ca8b95c38739bf014a91872b8939cc257b750fca` | 19 context directories; all 116 listed payloads pass `sha256sum -c` |

Each packet mechanically reproduces the artifact's 1,348 primary runs with
the configured `0.5` activity threshold, unchanged local slots and ordering,
and a `1e-6` serialization tolerance. This check validates producer
provenance only. It does not compare a speaker with the reference.

## T234 Run A chronological reading

| Focus | Complete-context observation | Earliest unusable layer |
|---|---|---|
| `ref-0049` | Tang's `对` survives only as a zero-duration aligned point inside Shi's `对，嗯`; no Tang interval owns the point. | Forced alignment/source partition |
| `ref-0058` | `相差0.7` is replaced by a different numeric sequence; a Tang secondary island cannot be attached to the missing semantic unit. | ASR source |
| `ref-0066` | The second `你们俩可以` has no separate ASR unit; the short Tang island is detached from writable text. | ASR source |
| `ref-0099` | Shi owns the retained clause in activity, primary, VAD, primary-run, and complete-source evidence; `661.276-662.076 s` alone is rewritten to Zhu by `voiceprint_direct_short`. | Final fusion overwrite |
| `ref-0102` | Tang's repeated response is retained, but primary, secondary activity, and short identity views disagree before final selection. | Raw speaker-evidence conflict |
| `ref-0118` | Tang activity covers the response while primary changes from Shi to Tang inside the two-codepoint aligned phrase, yielding Shi `对` and Tang `呀`. | Alignment/native-boundary conflict |
| `ref-0252` | Zhu's leading `对啊` is absent; the remaining phrase crosses Shi and Tang evidence without Zhu support. | ASR source, then raw identity |
| `ref-0313` | Shi's `对` is absent even though a short Shi primary island remains between ASR sources. | ASR source boundary |
| `ref-0331` | Shi's `对` is absent between two sources; the `2362.800-2363.200 s` Shi primary island has no token. | ASR source boundary |
| `ref-0333` | `对` is appended to Tang's preceding source, and local diarization, primary, and source-scale identity support Tang. | Source partition and raw identity |
| `ref-0354` | The text is retained, but both diarization slots and primary resolve to Tang; the short identity view does not establish Zhu. | Sortformer/identity evidence |
| `ref-0390` | Tang's `对` is a zero-duration point at the end of Shi's phrase, before the later Tang primary return. | Forced alignment displacement |
| `ref-0426` | The B/C clause is garbled and its retained fragments alternate among Tang, Xu, and Shi in both raw and final views. | ASR source and raw identity |
| `ref-0442` | Zhu's `什么价格` is absent; nearby source text and speaker evidence belong to other turns. | ASR source |
| `ref-0444` | Only `价格` survives, while primary and short identity evidence disagree and neither establishes Zhu. | ASR source and raw identity |
| `ref-0461` | The clause is altered to `你们俩财务的人`; its retained interval is supported as Shi, not Tang. | ASR source and raw identity |
| `ref-0499` | `哦，没区别` is retained, but its raw intervals move Zhu to Tang to no support and expose no Shi evidence. | Sortformer/identity evidence |
| `ref-0503` | The opening fragment supports Zhu; later independent clauses predominantly support Shi, with one Xu island and gaps, despite the reference's continuous Zhu turn. | Raw speaker producer/identity evidence |
| `ref-0505` | The semantic clause occurs near `3301-3304 s`, earlier than the coarse reference row, and primary plus multi-scale identity evidence support Shi. | Reference/source placement and raw identity |

## T234 Run B chronological reading

Run B was read independently before consulting the Run A notes.

| Focus | Independent complete-context observation | Earliest unusable layer |
|---|---|---|
| `ref-0049` | The response point remains inside Shi's aligned source; the later Zhu and Tang controls are separately preserved. | Forced alignment/source partition |
| `ref-0058` | The expected decimal comparison has no source token to receive Tang's overlapping activity. | ASR source |
| `ref-0066` | The short Tang primary return exists, but only Shi's earlier wording is retained as source. | ASR source |
| `ref-0099` | Broad and primary-run evidence retain Shi, while the exact short interval alone writes Zhu into the middle of one Shi sentence. | Final fusion overwrite |
| `ref-0102` | The response sits under sustained Shi primary evidence with subordinate Tang activity and weak, split identity evidence. | Raw speaker-evidence conflict |
| `ref-0118` | One aligned response crosses the Shi-to-Tang primary boundary, so the two characters cannot be recovered by a uniform interval rule. | Alignment/native-boundary conflict |
| `ref-0252` | The omitted leading response removes the only clear Zhu semantic unit; retained text follows the Shi-to-Tang transition. | ASR source, then raw identity |
| `ref-0313` | No Shi response token remains after Tang's `对吧`; the nearby source ends before the orphan evidence can be projected. | ASR source boundary |
| `ref-0331` | A Shi primary micro-island is visible, but neither adjacent ASR source contains its response. | ASR source boundary |
| `ref-0333` | The response is fused into Tang's complete source and every usable local identity view supports Tang. | Source partition and raw identity |
| `ref-0354` | Retained text lies inside a sustained Tang primary run; no independent Zhu identity evidence survives. | Sortformer/identity evidence |
| `ref-0390` | The zero-duration response precedes rather than overlaps the later Tang primary island. | Forced alignment displacement |
| `ref-0426` | Both source wording and local speaker ownership alternate through the focus clause. | ASR source and raw identity |
| `ref-0442` | The Zhu question is not present in the source or any writable aligned unit. | ASR source |
| `ref-0444` | The surviving noun is placed in the next source and carries Xu/Tang rather than Zhu evidence. | ASR source and raw identity |
| `ref-0461` | The altered phrase is acoustically and finally owned by Shi before the Xu control begins. | ASR source and raw identity |
| `ref-0499` | Three fragments expose Zhu, Tang, and unsupported ownership; no broader Shi view can safely rewrite them. | Sortformer/identity evidence |
| `ref-0503` | Zhu support is local to the first fragment; later source, primary, and identity views repeatedly support Shi. | Raw speaker producer/identity evidence |
| `ref-0505` | The matching sentence is earlier than the reference timestamp and all usable evidence over it supports Shi. | Reference/source placement and raw identity |

## T235 Run A reverse reading

The reverse pass started at the disputed tail and retained local conversational
order inside each worksheet.

| Focus | Reverse observation |
|---|---|
| `ref-0505` | Reading backward first exposes the timestamp displacement; no Tang acoustic support appears over the matching sentence. |
| `ref-0503` | The last and middle clauses independently keep Shi evidence, so a Zhu continuity rewrite is not supported by the packet. |
| `ref-0499` | The retained response remains fragmented across Zhu, Tang, and no support before the accepted Tang control. |
| `ref-0461` | The Shi-supported altered source precedes a distinct Xu response; final fusion did not erase a Tang consensus. |
| `ref-0444` | The surviving `价格` belongs to the next source boundary and has no Zhu consensus. |
| `ref-0442` | The first Zhu question remains absent rather than overwritten. |
| `ref-0426` | Source corruption and alternating raw identities precede the final mixed output. |
| `ref-0390` | The response point remains earlier than the Tang primary return. |
| `ref-0354` | Tang remains the sustained primary/identity owner across the retained text. |
| `ref-0333` | The response remains attached to Tang's complete source. |
| `ref-0331` | The orphan Shi primary island remains visible without a source token. |
| `ref-0313` | The expected Shi token remains absent at the ASR boundary. |
| `ref-0252` | Zhu's leading unit remains missing and the surviving phrase remains a Shi/Tang transition. |
| `ref-0118` | Reversing context still places the primary transition inside one two-character response. |
| `ref-0102` | Tang activity and Shi primary evidence remain contradictory before fusion. |
| `ref-0099` | The short Zhu write remains surrounded by one retained Shi sentence and broader Shi evidence. |
| `ref-0066` | The Tang island remains real but has no second source phrase. |
| `ref-0058` | The expected semantic unit remains absent from the numeric ASR source. |
| `ref-0049` | The zero-duration response remains inside Shi's source before the later Zhu/Tang controls. |

## T235 Run B reverse reading

Run B was then reread independently from tail to start.

| Focus | Independent reverse observation |
|---|---|
| `ref-0505` | The matching sentence again precedes the coarse reference row and remains Shi-supported. |
| `ref-0503` | Only the opening focus fragment supplies Zhu evidence; separate later clauses retain non-Zhu producer evidence. |
| `ref-0499` | No Shi producer view emerges when the response is approached from the following controls. |
| `ref-0461` | The retained phrase remains altered and Shi-owned before Xu's reply. |
| `ref-0444` | The surviving source noun remains outside a usable Zhu interval. |
| `ref-0442` | The missing question cannot be reconstructed from neighboring source text. |
| `ref-0426` | Mixed source and mixed raw identities remain the earliest defect. |
| `ref-0390` | The aligned point and Tang primary island remain temporally separated. |
| `ref-0354` | Tang remains the only sustained producer identity over the focus. |
| `ref-0333` | Tang remains the source and acoustic owner of the appended response. |
| `ref-0331` | The short Shi island remains source-less. |
| `ref-0313` | The expected Shi response remains absent between ASR sources. |
| `ref-0252` | The omitted Zhu response and later Shi/Tang source remain distinct defects. |
| `ref-0118` | Activity supports Tang, but primary changes identity inside the aligned phrase. |
| `ref-0102` | Raw evidence remains genuinely split rather than later overwritten. |
| `ref-0099` | All broader views still support Shi and only the short direct write differs. |
| `ref-0066` | No writable Tang repetition appears despite the short primary island. |
| `ref-0058` | The missing decimal phrase remains the first loss. |
| `ref-0049` | No Tang-owned aligned interval appears at the response point. |

## Four-pass reconciliation

All four readings agree on the causal boundary. `ref-0099` is the only exact
FR50 critical residual in which usable, mutually corroborating speaker evidence
survives through the source, alignment, activity, primary, VAD, and broad
identity views and is then overwritten by final fusion. `ref-0118` is visibly
split during final projection, but its primary transition lies inside the
aligned phrase and therefore supplies a different, genuinely contradictory
upstream topology. Every other focus loses the semantic unit, displaces it
from the useful speaker interval, or presents conflicting/wrong producer
identity before the final comprehensive view chooses a label.

The complete historical direct-short conflict review remains an abstention
control. It found many accepted contexts where short direct identity evidence
correctly repairs the native label and only `ref-0099` with this exact broad-
consensus contradiction. FR51's exact-baseline reread does not create a second
material occurrence. A production guard fitted to this one reference would
therefore repeat the single-context policy pattern that FR48 explicitly
rejected.

## T235 decision and next producer boundary

T235 stops the final-fusion branch. It authorizes no code, model, TOML, replay,
new audio run, ledger, or closure change. FR50 remains the accepted real-path
speaker baseline.

The next SDD phase must investigate the upstream **short-response source-edge
provenance** contract before any candidate exists. The manually related
contexts are:

- retained but zero-duration/displaced response points: `ref-0049`,
  `ref-0390`;
- short speaker islands without a separately writable semantic unit:
  `ref-0066`, `ref-0313`, `ref-0331`;
- one short response whose aligned characters straddle a primary boundary:
  `ref-0118`.

The shared question is whether the current ASR-source and forced-alignment
boundary representation discards speaker provenance that already exists on
the common clock. The phase may preserve and display existing producer values;
it must not invent missing words, infer correctness in code, or use reference
timestamps at runtime. It must first distinguish an ASR-source omission from
an alignment displacement and from a genuinely contradictory speaker island.
Only a complete contextual review may then decide whether a reusable producer
contract exists. Long-turn identity conflicts such as `ref-0503` remain a
separate later phase and may not be rewritten by conversational continuity.
