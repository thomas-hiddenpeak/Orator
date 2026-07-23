# FR58 Auxiliary Streaming-Context Review (2026-07-23)

## Status and authority

T264-T267 are complete. FR58 stops at the evidence gate. It authorizes no
auxiliary production worker, fusion rule, root-TOML value, model change,
product run, ledger change, baseline change, or closure claim. FR50 remains
the accepted real-WebSocket speaker baseline.

The sole product authority for this review is the complete human-listened
`test/data/reference/test.txt` conversation. No executable, script, query,
formula, metric, posterior value, or identity score assigned correctness,
classified a topology, selected a candidate, or issued the decision below.
Automation only ran the frozen model profile, preserved raw evidence, checked
mechanical and numerical contracts, and arranged unjudged display packets.

## T264 exact capture

Both captures used the exact 57,841,920-sample lossless stream wrapper, the
checked-in `340/40/40/300` high-latency TOML, and the same Sortformer v2.1
checkpoint as the accepted producer.

| Mechanical fact | Frozen result |
|---|---|
| Audit-start code | `880262695a52ae02d0e8b9f37930665e1ae5dec5` |
| Exact WAV SHA-256 | `a60af82d57958ddfaf5d0358820adc7536544c97d59aab9f55a3b2f53694a16e` |
| High-profile TOML SHA-256 | `1b7e26776ef3bb2dd8f33012d013d7f4516803fb1e11a72a2fd8d8b63091b897` |
| v2.1 weights SHA-256 | `d036020b6b93977098929d417b1b106a952ec02cc38cafc9d3315ae0ec4d90b8` |
| Raw frames | 45,189 rows, `0.000000-3615.039919 s`, `0.08 s` period |
| Onset/offset segments | 772 rows on one session clock |
| Run A / Run B component time | `27.2204 s` / `27.1220 s` |
| Frame SHA-256, both captures | `504d859c0c20fcf06bff8865abdb4ceeeda7420c4f547637c47b631a68040673` |
| Segment SHA-256, both captures | `57a01ad7ae1ef9fe34934d4cec2016238da12417893fe81bbdfcd6d239e20508` |
| Exact-profile numerical gate | `test_diar_async_stream_v21_high` passed |

The two captures are byte-identical. That establishes deterministic evidence
capture only. It is not a product comparison with the accepted profile. Old
July high-profile artifacts had 803 segments and different hashes, so no old
candidate interpretation was imported into this review.

## T265 identity and packets

The display-only packet tool now accepts paired auxiliary posterior and
segment files. `speaker_frozen_evidence.py` also exports the seven-column raw
diarization query schema consumed by `speaker_identity_replay_probe`. These
paths copy, validate, and display source data only.

The reference-free identity replay loaded the frozen four-speaker FR50 Run A
registry and queried all 772 auxiliary intervals, including overlap-excluded
gallery evidence and retained-reference provenance. It initially associated
local slots `0/1/2/3` with `spk_1/spk_0/spk_2/spk_3`, but the auxiliary segment
sequence later fragmented local slot 3 into `spk_4` and `spk_6`, local slot 2
into `spk_5`, and changed other local epochs. The replay ended with seven
enrolled identities. Those facts expose identity provenance and conflict; they
do not state which speaker is correct.

Independent packet trees preserve all 19 focus conversations and named
controls beside the exact FR50 source, alignment, VAD, accepted posterior,
identity epochs, TitaNet evidence, business output, auxiliary posterior,
auxiliary segments, raw query scores, and retained references:

| Packet | Content-manifest SHA-256 |
|---|---|
| Run A | `9493274d1a4689a3f184b7957c4d0dd9854acc3a4058db2d2323c1cdfe759711` |
| Run B | `ffd870ef242e62455e4d1221fa85c472f661ab4d2d524ea5ff78d8c67f49c7ca` |

## T266 Run A chronological reading

| Focus | Complete-context observation |
|---|---|
| `ref-0049` | Auxiliary slot 0 rises during Tang's response, but the same interval is covered by a longer slot-3 segment and the response remains fused into Shi's source. The segment query does not establish Tang independently. |
| `ref-0058` | The auxiliary Tang-slot island overlaps the misrecognized response, but the accepted producer already contains essentially the same slot-0 island. It extends into the following Shi material, so it supplies no new safe boundary. |
| `ref-0066` | The auxiliary view repeats the bounded Tang island already visible in the accepted producer. No separately writable source unit represents the repeated response. |
| `ref-0099` | Auxiliary slot 3 remains broad through the Shi clause, like the accepted broad evidence, while the nearby Tang controls are not preserved by the auxiliary primary view. It does not create a new short-write guard. |
| `ref-0102` | The auxiliary view keeps Shi and Xu activity through the repeated response and supplies no Tang interval. |
| `ref-0118` | The auxiliary Tang island covers both aligned characters, but its bounds are materially the same as the accepted slot-0 island. The defect remains projection across an existing primary boundary, not missing auxiliary evidence. |
| `ref-0252` | The auxiliary view contains Shi and Tang but no Zhu interval for the omitted leading response. |
| `ref-0313` | No auxiliary Shi activity exists at the missing source edge. Later Shi islands belong to other contextual responses. |
| `ref-0331` | A short Shi island is visible, but the accepted producer already contains the same island and no text-bearing unit survives at it. |
| `ref-0333` | The auxiliary view has no Shi interval over the response; Tang ends before the following Zhu turn. |
| `ref-0354` | The auxiliary producer assigns the retained phrase to Tang and therefore repeats the upstream conflict. |
| `ref-0390` | Auxiliary Tang activity starts after the aligned response point and overlaps the following Shi number phrase. It cannot safely move the zero-duration unit. |
| `ref-0426` | Xu remains broad while short Shi and Tang intervals alternate through the garbled clause. No uniform Shi boundary survives. |
| `ref-0442` | Auxiliary and accepted local-slot boundaries are nearly the same. The auxiliary frozen-registry epoch calls the short slot Zhu, but the first question is absent and the interval also covers the preceding Shi question edge. |
| `ref-0444` | The second auxiliary local-1 interval has Zhu identity evidence, but it ends before the only retained `price` token. The writable source remains displaced. |
| `ref-0461` | The auxiliary view contains Shi and Xu only; it supplies no Tang evidence for the altered clause. |
| `ref-0499` | This is the one material complementary activity case: auxiliary slot 3 appears over `no difference`, where the accepted producer lacks Shi. Its auxiliary identity is `spk_6`, direct scores conflict, and concurrent slot 0 remains visible, so stable Shi identity is not established. |
| `ref-0503` | Zhu appears only on the opening fragment; the substantive later clauses remain auxiliary slot 3, matching the accepted upstream error. |
| `ref-0505` | The matching earlier sentence remains auxiliary slot 3 rather than Tang, and no later source interval exists at the coarse reference row. |

## T266 Run B chronological reading

Run B was read from the start without importing Run A judgments.

| Focus | Independent observation |
|---|---|
| `ref-0049` | The short slot-0 overlap remains inseparable from the longer slot-3 source and has conflicting identity evidence. |
| `ref-0058` | The same slot-0 island is already present in the accepted producer and crosses into the next Shi context. |
| `ref-0066` | The repeated Tang island still has no writable repeated source. |
| `ref-0099` | Broad auxiliary Shi activity repeats evidence already available before the final short overwrite and does not preserve the Tang controls. |
| `ref-0102` | No auxiliary Tang activity owns the repeated response. |
| `ref-0118` | Both views already contain the Tang island; the aligned two-character response still crosses the accepted primary boundary. |
| `ref-0252` | Zhu remains absent from the auxiliary producer. |
| `ref-0313` | The missing Shi response has no auxiliary interval at its source edge. |
| `ref-0331` | The existing short Shi island remains source-less. |
| `ref-0333` | No auxiliary Shi evidence appears over the appended response. |
| `ref-0354` | Tang remains the auxiliary owner of the retained phrase. |
| `ref-0390` | The Tang auxiliary onset remains later than the aligned point and extends into the Shi control. |
| `ref-0426` | The clause remains a mixed Xu/Shi/Tang producer sequence. |
| `ref-0442` | A different frozen-registry epoch label does not recover the missing Zhu question or isolate it from Shi's preceding edge. |
| `ref-0444` | The Zhu-labelled auxiliary interval and the surviving price token remain temporally disjoint. |
| `ref-0461` | Tang remains absent from the auxiliary evidence. |
| `ref-0499` | Auxiliary slot 3 is complementary, but its global identity remains fragmented and it overlaps slot 0. |
| `ref-0503` | Only the opening fragment has Zhu; later content remains Shi-produced. |
| `ref-0505` | The matching sentence remains Shi-produced in the auxiliary view. |

## T266 Run A reverse reading

The reverse pass started at the tail and retained chronological order inside
each conversation.

| Focus | Reverse observation |
|---|---|
| `ref-0505` | No Tang auxiliary evidence appears over the displaced matching sentence. |
| `ref-0503` | Reversing the long focus confirms that only its opening has Zhu evidence. |
| `ref-0499` | The complementary slot-3 island remains globally unresolved and concurrent with Tang activity. |
| `ref-0461` | Shi/Xu evidence remains upstream of the final decision; Tang is absent. |
| `ref-0444` | The Zhu auxiliary island ends before the retained price token, so no source unit can receive it. |
| `ref-0442` | The first Zhu question remains missing and the auxiliary island still overlaps the Shi boundary. |
| `ref-0426` | Mixed activity and corrupted source remain the first defect. |
| `ref-0390` | The auxiliary Tang onset remains later than the aligned response point. |
| `ref-0354` | The auxiliary profile independently repeats Tang ownership. |
| `ref-0333` | The response remains appended to Tang without an auxiliary Shi interval. |
| `ref-0331` | The short Shi island remains visible but source-less in both views. |
| `ref-0313` | No auxiliary Shi island exists at the missing response edge. |
| `ref-0252` | The expected Zhu speaker remains absent from the producer. |
| `ref-0118` | The high-context island repeats rather than supplements the accepted Tang island. |
| `ref-0102` | The repeated Tang response remains unsupported by the auxiliary view. |
| `ref-0099` | Broad auxiliary Shi activity is not a second reusable short-overwrite topology. |
| `ref-0066` | The repeated Tang island still lacks a writable semantic unit. |
| `ref-0058` | The repeated slot-0 island crosses into following Shi material and does not add a safe source boundary. |
| `ref-0049` | Slot 0 and slot 3 remain simultaneous across the fused response source. |

## T266 Run B reverse reading

Run B was then reread independently from tail to start.

| Focus | Independent reverse observation |
|---|---|
| `ref-0505` | The displaced sentence again has only Shi auxiliary activity. |
| `ref-0503` | The auxiliary profile again keeps Zhu only on the opening fragment. |
| `ref-0499` | The one complementary slot remains identity-fragmented and overlapped. |
| `ref-0461` | No Tang producer evidence emerges in reverse context. |
| `ref-0444` | The Zhu-labelled interval and retained word remain on opposite sides of the source boundary. |
| `ref-0442` | The question remains absent, and its putative island is not isolated from Shi's control. |
| `ref-0426` | Xu, Shi, and Tang still alternate before the final mixed output. |
| `ref-0390` | The aligned point still precedes the auxiliary Tang return. |
| `ref-0354` | Tang remains the auxiliary producer over the focus. |
| `ref-0333` | No Shi interval owns the appended response. |
| `ref-0331` | Both profiles expose the same source-less Shi micro-island. |
| `ref-0313` | The missing response edge remains inactive for Shi. |
| `ref-0252` | Auxiliary evidence remains Shi/Tang rather than Zhu. |
| `ref-0118` | Existing and auxiliary slot-0 islands remain the same evidence topology. |
| `ref-0102` | The auxiliary profile still misses Tang's repeated response. |
| `ref-0099` | Broad auxiliary support remains useful context but not a second safe final-fusion case. |
| `ref-0066` | The real short island remains text-free. |
| `ref-0058` | The island still overlaps both the focus and following Shi material. |
| `ref-0049` | The fused source still contains unresolved simultaneous slots. |

## Controls and reconciliation

The four readings agree on three boundaries:

1. `ref-0058`, `ref-0066`, `ref-0118`, and `ref-0331` do not establish a new
   auxiliary topology. Their useful short islands already exist in the
   accepted v2.1 producer. `ref-0048`, `ref-0056`, `ref-0057`, `ref-0062`,
   `ref-0065`, `ref-0067`, and `ref-0116` through `ref-0120` show why an
   auxiliary-overlap preference would either repeat current behavior or cross
   a valid neighboring turn.
2. `ref-0442` and `ref-0444` expose a different auxiliary identity epoch, but
   not a usable business boundary. The first question is missing; the second
   surviving word occurs after the Zhu-labelled interval. `ref-0441`,
   `ref-0443`, and `ref-0445` require abstention. The mapping also comes from a
   replay that starts with the final frozen four-speaker registry, so it is not
   a causal empty-registry Run A identity contract.
3. `ref-0499` is the only critical residual with genuinely complementary
   auxiliary activity absent from the accepted producer. The auxiliary local
   slot lacks stable global identity and overlaps a conflicting slot.
   `ref-0498` and `ref-0500` reject a slot-only or continuity rewrite.

Consequently, no two material residuals satisfy the same complete contract of
new activity, writable boundary, stable global identity, and accepted-control
abstention. This is a contextual-semantic decision, not a profile score or an
automated comparison.

## T267 decision

T267 takes the stop branch. A second high-context production worker would add
memory, scheduling, identity reconciliation, and maintenance cost without an
authorized business repair. FR58 therefore lands only diagnostic tooling,
tests, and durable evidence records.

The next investigation should not rerun this profile or fit a rule to
`ref-0499`. It may reuse the frozen auxiliary capture to inspect whether the
one true complementary slot topology occurs in the remaining manually signed
noncritical residuals and controls. Any such phase needs a new frozen SDD
scope and must solve causal empty/frozen-registry identity before a candidate
can exist.

## Engineering validation

- `python3 -m py_compile` passes for both changed evidence utilities;
- focused evidence tests pass `8/8` and `12/12`;
- `cmake --build build -j` completes without warnings;
- full CTest passes `74/74`, including
  `test_diar_async_stream_v21_high` and both changed Python test targets.

These checks validate tooling and model-port contracts only. They do not
evaluate speaker correctness or alter the manual decision above.
