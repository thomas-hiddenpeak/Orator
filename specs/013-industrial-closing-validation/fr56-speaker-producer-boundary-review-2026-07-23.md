# FR56 Speaker Producer-Boundary Review

## Authority and scope

This record completes T253-T257. It reviews the exact FR50 producer evidence
at the boundary between streaming Sortformer local slots and TitaNet identity
epochs. It does not evaluate ASR wording as a separate product, calculate a
score, rank a configuration, or change the accepted speaker ledger.

Every observation below was written from complete conversational-semantic
reading against the human-listened `test/data/reference/test.txt`. No
executable comparison assigned a speaker, classified a cause, grouped a
topology, selected a policy, or issued the decision. Automation only arranged
and hashed immutable evidence.

The four independent passes were:

1. Run A from `ref-0102` through `ref-0505`;
2. Run B in the same direction without importing Run A judgments;
3. Run A from `ref-0505` back through `ref-0102`;
4. Run B in the same reverse order.

Each pass read the complete reference section, neighbouring controls, current
business output, ASR source and forced alignment, VAD, Sortformer posterior,
activity and primary views, local identity epochs, retained-reference
provenance, and available TitaNet query evidence.

## Mechanical evidence

These are provenance and structural facts only:

| Evidence | Frozen value |
|---|---|
| FR50 code | `a6f0d33730326b19a3831019b1aba21fd900f126` |
| Run A artifact | `33482f741d2467f28a436a6ffd90bdd0dd708e0e93af7cf814ef29c62d4781da` |
| Run B artifact | `7ba17a7caacbe39bafd747fefd8fade4600f509e6c6372a03d1af7c7d14be65c` |
| Stream PCM | `17f0edda49989f3ceada60170885091023eeb9d67faae0d6dd67bb585b8857fe` |
| Checked-in TOML | `d00150ae376d802af0fcf8c0a89aa3fae1e0abb2bf5d10601c55cefd570a40db` |
| Sortformer v2.1 | `d036020b6b93977098929d417b1b106a952ec02cc38cafc9d3315ae0ec4d90b8` |
| TitaNet | `59e1e3069a33bef8da8b9467a5a64a30a5de8c7ae2550b15d5323ef537acaad1` |
| Human reference | `35e8695057be82f3028877f7dc159f10ecfa0ab7f06c444cefa9a079b0e24a86` |
| FR56 context table | `19c9e53de90a74ccc937b2af821db96f4e929140daf6d24b4d1d2c3f93d5f38c` |
| A packet content manifest | `c18c82e05b0583114a61cce672d136e08e3daae0313444741402893c68c093f7` |
| B packet content manifest | `43a1797ca14bdd34d2aae79d59d9d2da7c63310378bf714ba40bfadcc8dea6e5` |

Every file listed by both packet content manifests passes `sha256sum -c`.
Run A and Run B expose the same final local identity epochs. With the
checked-in no-reset profile, local slot 0 changes from `spk_1` to `spk_3` at
`3243.279927507 s`; local slot 1 changes from `spk_0` to `spk_1` at
`2136.639952242 s`, returns to `spk_0` at `3108.479930520 s`, and changes to
`spk_1` at `3330.639925554 s`. Slots 2 and 3 remain `spk_2` and `spk_3`.
These values describe captured output; they do not judge a speaker.

## Run A chronological reading

| Context | Complete-context producer observation |
|---|---|
| `ref-0102` | Tang repeats `就不用开会了`. Local slot 0 retains Tang as subordinate activity, while local slot 3 remains Shi and primary across the response. No identity epoch changes nearby, and the short TitaNet views do not independently establish Tang. |
| `ref-0252` | Zhu's leading response is absent from the retained source and substantive speaker evidence. The available Sortformer sequence changes from Shi to Tang; no Zhu slot or TitaNet identity is available for the missing contribution. |
| `ref-0333` | Shi's short `对` is appended to Tang's source. Local slots 0 and 1 both map to Tang in the active epochs; the nearby Shi slot does not own the focus and no independent Shi query establishes it. |
| `ref-0354` | Zhu's brief confirmation is carried by local slots 0 and 1 while both map to Tang. Longer Zhu controls on either side can be recovered by their own TitaNet evidence, but the short focus does not supply the same evidence. No epoch boundary occurs in the context. |
| `ref-0426` | The retained wording and speaker evidence alternate among Tang, Xu, and Shi. Shi appears only in partial secondary/late-primary evidence; no single source range or identity epoch represents the complete listener turn. |
| `ref-0444` | Only the later `价格` survives. Its raw channels and short identity evidence point to Xu/Tang rather than Zhu; the Zhu question has no usable producer identity. |
| `ref-0461` | The altered retained clause is primary and voiceprint-supported as Shi. Tang appears only as subordinate activity at the boundary and does not supply an independent identity for the clause. |
| `ref-0499` | This is the only focus adjacent to an identity epoch change. Shi's response is split among local slot 1 mapped to Zhu, local slot 0 still mapped to Tang, and an unsupported interval before slot 0 changes to Shi at `3243.279927507 s`. The preceding accepted Tang turn uses the same slot 0, and the short focus TitaNet evidence does not establish Shi. |
| `ref-0503` | The opening interval maps to Zhu, but later substantive clauses are assigned to local slots 0 and 3 mapped to Shi. The known contaminated retained reference weakens independence but removing it still leaves non-overlapping upstream evidence without a reusable Zhu decision. |
| `ref-0505` | The matching semantic clause occurs earlier than the coarse reference row. Its substantive interval is local slot 3 mapped to Shi; Tang evidence is confined to the opening fragment. There is no identity transition that can restore the complete clause. |

## Run B chronological reading

Run B was read independently. It retains the same distinctions:

- `ref-0102`, `ref-0333`, `ref-0354`, `ref-0426`, `ref-0444`, and
  `ref-0461` remain within stable epochs whose raw channels are incomplete or
  contradictory for the listener-verified speaker.
- `ref-0252` still lacks a Zhu source and producer identity.
- `ref-0499` remains the sole focus adjacent to an epoch change; its preceding
  Tang control and absent independent Shi query remain unchanged.
- `ref-0503` remains dominated by Shi-mapped channels after its Zhu opening,
  and `ref-0505` remains source-displaced with substantive Shi evidence.

The frozen Run B registry therefore does not convert these local-slot errors
into a different reusable identity topology.

## Run A reverse reading

Starting from the tail first makes the boundary constraint explicit.
`ref-0505` and most of `ref-0503` remain Shi-supported after the slot-0 epoch
has already changed, so a wider Tang or Zhu continuity rule is not supported.
At `ref-0499`, moving the Shi epoch backward would cross the accepted Tang
`ref-0498`, while keeping the exact boundary leaves the short response without
independent Shi evidence. The accepted Tang `ref-0500` also demonstrates that
final TitaNet evidence may legitimately disagree with the newly changed raw
slot identity.

Reading backward through `ref-0461`, `ref-0444`, `ref-0426`, `ref-0354`,
`ref-0333`, `ref-0252`, and `ref-0102` exposes no additional epoch boundary.
Their missing or conflicting target identities already exist in the
Sortformer/source producer evidence.

## Run B reverse reading

The independent Run B reverse pass reaches the same boundary without importing
Run A labels. The tail does not provide a second safe identity-epoch backfill
case. The middle contexts remain stable-epoch producer conflicts with different
target speakers and source conditions, while the early contexts retain the
same dominant/secondary disagreement or missing target identity.

## T254 and T255 decision

The existing evidence is sufficient to decide the FR56 observability question.
An identity-transition trace could explain which internal drift branch created
the exact `3243.279927507 s` boundary, but it cannot supply independent Shi
evidence before that boundary or remove the accepted Tang control. No other
FR56 focus has a nearby epoch change. T255 therefore requires no implementation:
no diagnostic API, runtime consumer, TOML key, or model behavior is added.

## T257 reconciliation

The four readings identify only one epoch-adjacent material context,
`ref-0499`, and its controls reject a wider or earlier epoch assignment. The
other nine contexts do not share that mechanism: their target speaker is
absent, subordinate, source-displaced, or contradicted inside an otherwise
stable local identity epoch.

The required two independent contexts with one reference-free producer
topology do not exist. FR56 stops without a runtime, TOML, model, product-run,
ledger, baseline, or closure change. FR50 remains the current real-path
speaker baseline. The next closing work should sign the still-open T102
speaker-time, per-speaker-time, and source-time-offset evidence before another
producer experiment is considered.
