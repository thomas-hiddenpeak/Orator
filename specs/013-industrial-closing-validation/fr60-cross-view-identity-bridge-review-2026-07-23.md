# FR60 Cross-View Identity-Bridge Review (2026-07-23)

## Status and authority

T272-T275 are complete. T275 takes the stop branch. FR60 authorizes no
auxiliary runtime worker, identity mapping, fusion rule, product code,
root-TOML change, model run, product run, result-ledger change, baseline
change, or closure claim. FR50 remains the accepted real-WebSocket speaker
baseline.

The sole product authority for this review is the complete human-listened
`test/data/reference/test.txt` conversation. No executable, script, query,
formula, metric, posterior value, similarity score, or algorithm assigned
correctness, derived an identity, classified a bridge, ranked evidence,
selected a candidate, or issued the decision below. Automation only copied
frozen source evidence, checked schemas, hashes, ordering, and common-clock
interval arithmetic, and displayed every raw pairwise intersection or explicit
absence.

## Frozen evidence

FR60 reused the exact FR50 Run A/B product captures and FR58 auxiliary Run A/B
captures. It did not run audio or either model again.

| Mechanical fact | Frozen value |
|---|---|
| Audit-start code | `78bcb7ef28b631cf36edb08466805cfd237927b9` |
| FR50 Run A SHA-256 | `33482f741d2467f28a436a6ffd90bdd0dd708e0e93af7cf814ef29c62d4781da` |
| FR50 Run B SHA-256 | `7ba17a7caacbe39bafd747fefd8fade4600f509e6c6372a03d1af7c7d14be65c` |
| Human reference SHA-256 | `35e8695057be82f3028877f7dc159f10ecfa0ab7f06c444cefa9a079b0e24a86` |
| Auxiliary A/B frame SHA-256 | `504d859c0c20fcf06bff8865abdb4ceeeda7420c4f547637c47b631a68040673` |
| Auxiliary A/B segment SHA-256 | `57a01ad7ae1ef9fe34934d4cec2016238da12417893fe81bbdfcd6d239e20508` |
| Run A packet manifest SHA-256 | `7bb8e52c34120d8f738e630027c6e1b5229d23e2d7001fdfe9ba7cac5a923ad4` |
| Run B packet manifest SHA-256 | `cf8e3d3da5ac43ec92866e4db19c8e113c039afca1a11855f1e0880293b6d730` |
| Run A packet content-list SHA-256 | `55edb397502373381ef2c8a935b40d7ba7cfb69ca1bcd5d20648d5904cf5a534` |
| Run B packet content-list SHA-256 | `925ad1a34d14b9999ef47af0774bab7bbf25f951c2730d1e14d7eb24ab5607fa` |

Each independent packet contains six fixed blocks covering
`0.000-3615.120 s` without a gap. Every pass read the complete reference
conversation, final business view, accepted diarization and primary-speaker
rows, every auxiliary local slot, and every raw intersection row. Named focus
and control IDs only oriented the reading; they did not limit the evidence.

## Run A chronological reading

| Full-session block | Complete-context observation |
|---|---|
| `0-600` | At `ref-0063`, accepted diarization, primary-speaker, and auxiliary evidence all expose Shi activity around the short response. The final business view retains no text-bearing unit at that source span. Cross-view identity agreement therefore cannot perform a speaker rewrite. |
| `600-1200` | `ref-0135` is a zero-duration contribution at a duplicate timestamp and has no independent writable business unit. Around `ref-0171`, accepted rows alternate between Tang and Shi while multiple auxiliary slots span the same exchange. The complete conversation does not expose a bounded identity bridge for the retained Tang-owned phrase. |
| `1200-1800` | `ref-0221` already has accepted Xu activity and gains only conflicting auxiliary activity. At `ref-0239`, auxiliary local 2 intersects accepted globally identified Tang rows over the writable `1675.516-1675.676` unit. The later Xu anchor beginning near `1693.839` belongs to a separate event. `ref-0241` also shows one auxiliary slot crossing the Xu-to-Tang handoff. |
| `1800-2400` | At `ref-0298`, the retained business source is Xu-owned while accepted and auxiliary focus activity identifies Shi. The later Tang segment begins as a separate continuation and cannot establish Tang ownership retrospectively at the focus. |
| `2400-3000` | At `ref-0341`, no Tang auxiliary return isolates the interjection. At `ref-0409`, the auxiliary island intersects an accepted Tang identity rather than establishing the reference Zhu response, and no text-bearing business unit survives. At `ref-0457`, a tiny accepted Shi primary island sits inside broad Xu evidence without an independent auxiliary Shi slot. |
| `3000-3615.120` | At `ref-0499`, accepted global rows expose Zhu and then Tang at the focus; the complementary auxiliary activity has no Shi anchor. Later Shi activity belongs to a separate event. `ref-0506` has no supporting activity, while `ref-0537` remains inside broad Shi evidence despite the human Tang interjection. These controls reject generic same-slot propagation. |

## Run B chronological reading

Run B was read independently from the beginning without importing Run A
judgments.

| Full-session block | Independent observation |
|---|---|
| `0-600` | The Shi activity at `ref-0063` is again present in all producer views, but the final business view again has no writable response. |
| `600-1200` | The duplicate timestamp at `ref-0135` still provides no separable business unit. At `ref-0171`, alternating accepted identities and overlapping auxiliary slots still fail to bind a distinct Shi phrase without using reference content. |
| `1200-1800` | `ref-0221` remains conflicting rather than complementary. At `ref-0239`, the short auxiliary island again intersects accepted Tang rows, not a causal Xu continuity span. The later Xu event and the slot-crossing handoff at `ref-0241` independently prevent a local-slot identity rule. |
| `1800-2400` | `ref-0298` again has no Tang identity at the focus; the later Tang continuation is temporally distinct. |
| `2400-3000` | `ref-0341` remains inside Shi auxiliary activity, `ref-0409` remains source-less and accepted as Tang at the island, and `ref-0457` remains a tiny primary-only Shi island absorbed by broad Xu auxiliary evidence. |
| `3000-3615.120` | `ref-0499` again has complementary activity but no accepted Shi identity at the focus. The inactive `ref-0506` control and broad-Shi `ref-0537` control again provide no structural abstention for a generic propagation rule. |

## Run A reverse reading

The reverse pass started with the tail block and retained chronological order
inside each complete block.

| Full-session block | Reverse-context observation |
|---|---|
| `3000-3615.120` | Reading back from later clear Shi activity does not attach that identity to `ref-0499`; the intervening source context establishes separate events. `ref-0506` and `ref-0537` retain their contradictory control topologies. |
| `2400-3000` | The later identities do not supply missing writable content at `ref-0409` or turn its accepted Tang island into Zhu. The broad evidence at `ref-0341` and `ref-0457` remains unable to isolate the human short turns. |
| `1800-2400` | The later Tang segment remains a new event rather than a backward identity bridge for `ref-0298`. |
| `1200-1800` | Reading back from the later Xu anchor confirms a temporal separation before `ref-0239`. The focus island remains tied to accepted Tang rows, and `ref-0241` still demonstrates local-slot reuse across two speakers. |
| `600-1200` | Reverse context preserves the duplicate unit boundary at `ref-0135` and the mutually overlapping accepted and auxiliary identities at `ref-0171`; neither becomes a reference-free rewrite. |
| `0-600` | The earliest focus again has matching Shi activity but no retained text-bearing business unit. |

## Run B reverse reading

Run B was then reread independently from the tail to the start.

| Full-session block | Independent reverse observation |
|---|---|
| `3000-3615.120` | Later Shi context again fails to bridge across the separate Zhu/Tang focus event at `ref-0499`. The two later controls remain incompatible with generic same-slot or nearest-anchor propagation. |
| `2400-3000` | No later context creates a Zhu identity or writable unit at `ref-0409`. The `ref-0341` and `ref-0457` short responses remain unbounded within broader producer activity. |
| `1800-2400` | The next Tang event again cannot be used as retrospective focus ownership at `ref-0298`. |
| `1200-1800` | The later Xu anchor again begins after a separate Tang continuation. The `ref-0239` focus has no same-continuity Xu identity, and the neighboring handoff again rejects a slot-number mapping. |
| `600-1200` | `ref-0135` still has no independent writable source and `ref-0171` still contains contradictory identities inside any plausible local continuity span. |
| `0-600` | Existing Shi producer evidence remains unable to rewrite an absent business unit at `ref-0063`. |

## Identity-bridge reconciliation

The four complete readings agree on the following boundaries:

1. `ref-0239` has a genuine auxiliary activity complement and a writable
   aligned unit, but that auxiliary island intersects accepted Tang identities.
   The nearest clear Xu anchor starts in a separate later event. No bounded,
   same-continuity Xu bridge exists in the timeline evidence.
2. `ref-0499` has a genuine auxiliary activity complement, but accepted global
   tracks at the focus identify Zhu and Tang rather than Shi. The later Shi
   continuation is a separate event. No bounded Shi bridge exists.
3. One local-slot or nearest-anchor propagation rule cannot express either
   desired identity without reference IDs, reference timestamps, text, or
   future registry state. `ref-0241`, `ref-0506`, and `ref-0537` expose the
   corresponding false-propagation cases rather than structural abstentions.
4. `ref-0063` and `ref-0409` show that correct or useful producer activity is
   insufficient when the final business view has no writable text-bearing
   unit. `ref-0135` shows the same boundary at a duplicate zero-duration
   contribution.
5. `ref-0171`, `ref-0298`, `ref-0341`, and `ref-0457` retain contradictory or
   unbounded identity evidence. They do not provide a safer alternate bridge.
6. Empty-registry Run A and restarted frozen-registry Run B expose the same
   evidence boundary under independent chronological and reverse reading.
   Their agreement does not satisfy the failed identity and control clauses.

This is a complete human contextual-semantic decision derived from the full
conversation and full-session evidence blocks. It is not a scripted
comparison, a calculated product metric, or a new speaker-accuracy result.

## T275 decision

T274 fails the conjunctive manual gate because neither required focus has a
noncontradictory timeline-causal identity bridge, no single reference-free
rule can express both, and matching controls do not provide safe structural
abstention. T275 therefore takes the required stop branch and does not specify
or implement a candidate.

The raw common-clock intersections remain useful diagnostic evidence, but
promoting them to a business rewrite would convert unresolved activity into an
unsupported global identity. A future phase requires a new causal identity
source or a separately specified writable-unit synthesis contract before this
evidence ceiling can be revisited. Neither is authorized by FR60.

## Engineering validation

`python3 test/integration/py/test_speaker_residual_evidence_packet.py` passes
all 14 focused mechanical tests, and both changed Python files pass
`python3 -m py_compile`. Both packet content lists pass `sha256sum --check`.
`cmake --build build -j` completes without a warning or error, and full CTest
passes `74/74`, including both Sortformer v2.1 profile gates, the WebSocket
contract, and the registered residual-evidence packet test. These checks
validate repository consistency and the raw evidence path only; they do not
evaluate product correctness or change the manual decision.
