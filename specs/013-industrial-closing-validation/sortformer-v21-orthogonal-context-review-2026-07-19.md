# Sortformer v2.1 Orthogonal Context Review (2026-07-19)

## Scope and authority

This record completes T193 and the T194 design decision for FR47. The sole
product authority is the human-listened
`test/data/reference/test.txt`. The reviewer traversed each of the 23 T192
worksheets in chronological order and then from the tail back to the start.
Each pass related the complete surrounding conversation to the frozen FR45
business view, ASR source, forced alignment, activity, primary, VAD, all four
Sortformer v2.1 posterior channels, the time-varying local-slot identity
epochs, and both TitaNet galleries.

No executable, script, query, formula, notebook, metric, or algorithm assigned
speaker correctness, grouped or counted a product result, ranked a topology,
selected the candidate below, or issued the decision. Tools only reproduced,
ordered, displayed, and hashed immutable evidence. All classifications and the
design boundary in this record were written manually from conversational
context.

The frozen identity map is `spk_0=Zhu Jie`, `spk_1=Tang Yunfeng`,
`spk_2=Xu Zijing`, and `spk_3=Shi Yi`.

## Complete contextual classification

### 0-600 seconds

- `ref-0049`: Tang's short response is present in the ASR source. Local slot 0
  rises above the existing frame-activity gate as a secondary channel at the
  aligned response while slot 3 remains primary. The accepted preceding Shi
  turn `ref-0048` also contains sustained slot-0 secondary activity, so raw
  secondary activity alone cannot authorize a write.
- `ref-0058`: Tang is sustained as a secondary slot-0 channel through the
  listener-verified response, but the ASR source contains a different numeric
  phrase and Shi remains primary. TitaNet does not independently recover Tang.
  This is useful overlap evidence without a safe text contribution.
- `ref-0066`: activity and primary expose a bounded Tang return inside Shi's
  longer turn. The ASR source preserves only Shi's first `we two can` phrase
  and has no second source range for Tang's overlapping repetition. The exact
  short identity evidence also conflicts. The speaker island is real, but the
  business projector cannot manufacture a missing contribution.

### 600-1200 seconds

- `ref-0099`: slot 3 is dominant throughout the complete retained clause.
  Activity, primary, containing VAD, and both exact primary-run galleries all
  identify Shi, while the final ordinary short voiceprint write changes only
  the middle clause to Zhu. This remains the complete later-fusion overwrite.
- `ref-0102`: Tang appears only as a subordinate slot-0 channel under a strong
  Shi slot-3 run. The activity view overlaps, but primary, VAD, and exact
  identity evidence select Shi. This is conflicting overlap evidence rather
  than another `ref-0099` topology.
- `ref-0118`: slot 0 becomes primary at the response boundary, but alignment
  splits the listener-verified Tang response into Shi `yes` and Tang's final
  particle. The useful return is temporally displaced across the source edge
  and is too short for an independent identity query.

### 1200-2400 seconds

- `ref-0252`: the listener-verified Zhu response is absent from all substantive
  Sortformer channels. The retained source and primary transition from Shi to
  Tang, and TitaNet supplies no Zhu identity. This is producer-wrong evidence.
- `ref-0313`: the listener-verified Shi response has no retained business
  source. Slot 0 is dominant and maps to Tang; slot 3 is inactive. This is
  source absence plus a wrong producer identity.
- `ref-0327`: the aligned response straddles a native handoff. Slot 3 is a
  strong primary micro-run over the latter and larger aligned portion, while
  the surrounding activity, VAD, and TitaNet query are dominated by Tang.
  Context identifies Shi, but this is an uncorroborated subminimum primary
  island and is not interchangeable with `ref-0099`.
- `ref-0331`: a slot-3 primary micro-run is present at the listener-verified
  Shi response, but the ASR/VAD source has already ended. It has no exact
  identity embedding and therefore remains a source-edge absence.
- `ref-0333`: the response token is appended to Tang's preceding source. Slots
  0 and 1 dominate and both map to Tang; slot 3 stays subordinate and TitaNet
  supplies no Shi identity. This is producer-wrong evidence.

### 2400-3000 seconds

- `ref-0354`: slot 0 supplies sustained secondary activity, but slot 1 remains
  dominant and both slots map to Tang in this epoch. The exact session and
  robust galleries are weak and nearly split between Zhu and Xu. The local
  evidence cannot uniquely identify Zhu.
- `ref-0375`: the retained ASR wording is wrong, but its bounded business,
  activity, and primary identity is already Xu. Under the current speaker-only
  scope this row needs no speaker rewrite; it remains an ASR/source-quality
  issue for the later complete ledger reconciliation.
- `ref-0390`: several channels converge at the boundary and the Tang primary
  return begins after the aligned response source. No exact TitaNet query is
  available. This is temporal displacement with no unique speaker evidence.
- `ref-0426`: Shi is visible as sustained secondary activity and becomes
  primary only near the end, while Tang and Xu dominate the preceding primary
  sequence and TitaNet mostly selects them. The useful Shi evidence is partial
  across a long listener turn and cannot support one exact source rewrite.
- `ref-0442`: Tang and Shi are active across the overlapping question, but no
  Sortformer slot or TitaNet view identifies Zhu. The listener phrase is also
  collapsed into different ASR wording. The required identity is absent.
- `ref-0444`: the duplicate-time Zhu question has only one retained price
  token. The raw channels are mixed among Tang, Xu, and Shi, and the phrase
  gallery selects Tang. Zhu is absent from the machine evidence.
- `ref-0461`: Tang remains a sustained secondary slot-1 channel over the
  listener statement, but Shi is primary and both TitaNet galleries identify
  Shi. This is genuine overlap contradicted by the independent identity model.

### 3000-3615 seconds

- `ref-0499`: slot 0 is primary for Shi's response but still carries Tang's
  old global identity until the `3243.280` epoch split. Moving that split
  backward is unsafe: the immediately preceding accepted `ref-0498` Tang turn
  uses the same slot, and the current galleries do not independently identify
  Shi on the disputed short span.
- `ref-0503`: the opening has brief Zhu evidence, but slots 0 and 3, both mapped
  to Shi in this region, dominate the substantive proposal. Successive VAD and
  TitaNet evidence also select Shi. This is a sustained upstream producer and
  gallery error.
- `ref-0505`: the matching source text is displaced into the preceding
  reference window. Slot 1 supports Tang only on the opening fragment; the
  substantive aligned clause is slot 3 and both galleries select Shi. Source
  continuity alone cannot overrule the contrary acoustic evidence.
- `ref-0507`: slot 1 is primary and active over Tang's complete retained
  phrase, but its global identity is still Zhu. The exact session gallery
  selects Shi while the robust gallery selects Tang. The same slot receives a
  strong Tang epoch at `3330.640`, exposing delayed local-slot identity rather
  than missing Sortformer activity.
- `ref-0509`: Shi remains primary over the merged two-response phrase. During
  the first positive-duration aligned response unit, slot 1 is consistently
  the second channel and crosses the existing `0.5` activity gate on one raw
  frame; during Shi's immediately following second response it does not cross
  that gate. Slot 1 receives the same strong Tang epoch at `3330.640`. The raw
  posterior therefore preserves a bounded Tang interruption that activity
  postprocessing and the current identity map omit.

## Reverse controls

The reverse pass retained the same surrounding speaker order as the forward
pass. In particular:

- `ref-0048` prevents treating every sustained secondary channel as an
  interruption, while `ref-0050`, `ref-0051`, `ref-0065`, and `ref-0067`
  preserve the real handoffs around the early overlap cases.
- `ref-0097`, `ref-0100`, and `ref-0101` show that the complete
  `ref-0099` agreement is locally exceptional; `ref-0326` and `ref-0328` show
  that the short `ref-0327` primary island is a different topology.
- `ref-0353`, `ref-0355`, `ref-0425`, `ref-0427`, `ref-0441`, `ref-0443`,
  `ref-0445`, `ref-0460`, and `ref-0462` preserve the surrounding rapid-turn
  order and reject broad primary or secondary-channel preference.
- `ref-0498` and `ref-0500` reject an earlier global slot-0 epoch boundary.
  `ref-0508` and `ref-0510` retain Shi because slot 1 never crosses the
  existing frame-activity gate in their aligned response units. `ref-0511`
  retains Tang after the strong slot-1 epoch has started.

## T194 decision

The raw posterior does not authorize a general secondary-channel rule, a
general primary veto, a wider identity-epoch backfill, or a source-continuity
rewrite. Those alternatives fail the controls above or fit only one residual.

It does establish one narrower reference-free topology shared by `ref-0507`
and `ref-0509`: an exact aligned phrase or unit is supported by one local slot
as top-1 or top-2 on every intersecting raw frame, that slot crosses the
existing TOML `speaker_fusion.frame_activity_threshold` on at least one frame,
and the first later stable epoch for the same local slot supplies a different
global identity within the existing TOML-owned identity backfill horizon. The
future epoch must retain the existing minimum duration and matching primary
support. The source range must have one uniform conflicting current identity;
multiple eligible slots, rank ties, missing frames, a missing threshold
crossing, an unchanged identity, or absent future primary support force
abstention.

This topology uses the posterior already deposited in
`ComprehensiveTimeline`; it does not lower a threshold or add a fitted
duration. Phrase and aligned-unit ranges remain forced-alignment owned. The
existing `0.5` frame gate, identity-stage `120 s` backfill horizon, minimum
embedding duration, phrase bounds, and categorical top-two precedent are
reused. A new TOML boolean may enable the experiment, but it introduces no new
numerical parameter.

T194 therefore authorizes a deterministic frozen T123 candidate and focused
engineering tests. It does not authorize retention, a root-TOML baseline
change, a new audio run, a ledger change, or a closure claim. Every mechanically
changed complete conversation must receive another forward and reverse
contextual review before the candidate can advance.
