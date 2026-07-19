# FR39 Source-Leading Phrase-Tail Isolation Diagnosis (2026-07-19)

## Scope and authority

This frozen-evidence diagnosis addresses `ref-0518` after retained FR38. It
uses the source-hashed T111 and T123 typed producer packages on their common
sample clock and does not run audio, change a model, tune TOML, or score ASR
wording. `test/data/reference/test.txt` remains the authoritative human-
listened reference.

No compiled code, script, query, formula, metric, score operation, or interval
operation assigns correctness, aggregates accuracy, selects FR39, or issues a
product verdict. Shell tools only display immutable typed evidence. Complete
chronological and reverse contextual semantic review against `test.txt` alone
may retain or reject a resulting candidate.

## Reconciled conversational boundary

The historical 2026-07-16 closing-promotion review treated
`老师最有发言权` as a continuation of Tang Yunfeng's question. The later T135
complete forward/reverse reread corrected that omission, and its reconciliation
supersedes the earlier row judgment. The human reference exposes a direct
three-turn handoff:

- at `56:19`, Tang Yunfeng asks for opinions about how the US company should
  operate;
- at `56:26`, Zhu Jie answers `老师最有发言权。对。`; and
- at `56:34`, Tang Yunfeng resumes with `随哪边。` before Shi Yi continues.

The retained T123 projector assigns the complete middle answer to
`spk_1`/Tang Yunfeng with ordinary `sole_diar_support`. This is a critical
speaker-attribution defect under the reconciled complete context.

## Partition-stable typed evidence

The frozen identity map is `spk_0=朱杰`, `spk_1=唐云峰`,
`spk_2=徐子景`, and `spk_3=石一`. T111 and T123 use different ASR text and
alignment partitions around this answer, but expose the same acoustic
topology.

T123 `text_id=290` contains ten source characters. Its source-leading exact
punctuation phrase `[0,8)` covers `3384.780-3385.580` and the substantive
answer. The containing short business interval `[0,10)` extends to
`3386.060`; its tail `[8,10)` contains one positive-duration aligned visible
character followed by punctuation. That tail begins at `3385.980`, after a
pause exceeding checked-in `align_snap_pause_sec`, so it is independently
bounded from the exact phrase. T111 `text_id=257` has the same source ranges
and corresponding bounds `3384.760-3385.560` and `3384.760-3386.040`.

The component evidence is:

| View | T123 top-two pattern | T111 top-two pattern | Existing-gate state |
|---|---|---|---|
| Exact phrase session | Zhu `0.420548`, Tang `0.418677` | Zhu `0.486976`, Tang `0.447951` | Short score passes; margin abstains in both |
| Exact phrase robust | Zhu `0.425273`, Tang `0.406517` | Zhu `0.482548`, Tang `0.436944` | Short score passes; T123 margin abstains and T111 margin passes |
| Containing interval session | Tang `0.433662`, Zhu `0.400575` | Tang `0.425889`, Zhu `0.398584` | Short score passes; margin abstains |
| Containing interval robust | Tang `0.429741`, Zhu `0.407335` | Tang `0.426526`, Zhu `0.410514` | Short score passes; margin abstains |
| Containing VAD session | Zhu `0.385336`, Tang `0.376636` | Same immutable VAD | Regular score and margin abstain |
| Containing VAD robust | Tang `0.403887`, Zhu `0.388762` | Same immutable VAD | Regular score and margin abstain |
| Complete source | Zhu/Tang only; the robust order reverses | Zhu/Tang only; both views retain Zhu first | Regular score and margin abstain |

Activity diarization assigns one local slot to Tang over the complete interval,
and primary diarization assigns the same slot and identity over the exact
phrase. That local slot's first stable global identity is Zhu, however. Primary
diarization changes to a third identity only over the isolated aligned tail,
while activity remains Tang. Thus no single coarse view can safely own both
parts. The exact phrase is the only source/time scale on which both galleries
independently retain Zhu; appending the isolated response reverses both
interval galleries to Tang.

This is not a long-session identity drift and does not justify a global margin
reduction. It is a source-partition precedence defect: an independently
aligned tail changes wider acoustic evidence and causes the exact
source-leading phrase to abstain, after which the native slot paints the whole
answer as Tang.

## FR39 contract

FR39 may restore only the exact source-leading punctuation phrase. Every
condition is conjunctive:

1. The phrase starts at source zero, is embedding-backed and robust-complete,
   lies between the existing primary-consensus minimum and short maximum, and
   contains the configured minimum count of positive aligned units.
2. Every current phrase character remains unprotected native
   `sole_diar_support` for one identity B.
3. Exactly one activity local slot carrying B covers the phrase and its
   containing interval without competing activity. Exactly one primary segment
   from that same local slot carries B over the complete phrase. The slot's
   earliest stable identity is a different identity A.
4. Exactly one same-text short business interval starts with the phrase,
   extends to the complete source end, and has one source-contiguous tail. The
   tail contains exactly one positive-duration aligned visible character plus
   configured punctuation and starts after the existing alignment-pause gate.
5. Exactly one primary segment carrying a third identity C covers that aligned
   tail. A, B, and C are pairwise different, and no competing primary segment
   overlaps either exact component.
6. Both exact-phrase galleries rank A first and B second under the unchanged
   short score gate. At least one unchanged margin gate abstains, so the
   ordinary eligible phrase path has not already decided the range.
7. Both containing-interval galleries rank B first and A second, pass the
   unchanged short score gate, and abstain on the unchanged margin gate.
8. Exactly one robust-complete VAD contains the interval. Its session and
   robust top two are exactly A and B in opposite orders, and both views
   abstain on the unchanged duration-class score and margin gates.
9. Exactly one robust-complete complete-source view contains the interval and
   has full matching source bounds. Both galleries expose only A and B as the
   top two, at least one ranks A first, and both abstain on the unchanged
   duration-class score and margin gates.

The rule writes only the exact phrase source/alignment range to A. It does not
rewrite the isolated tail, the containing interval, preceding/following text,
timestamps, ASR content, producer tracks, or identities. It adds no TOML field,
threshold, fitted constant, transcript lookup, speaker name, reference label,
or known timestamp. Missing, duplicate, differently ranked, differently
gated, mixed, protected, unaligned, non-leading, non-punctuation, or
source/time-inconsistent evidence preserves ordinary behavior.

## Verification and decision boundary

Focused tests must prove the complete positive topology and independent
abstention for source shape, provenance, local-slot identity, phrase and tail
alignment, activity/primary uniqueness, all exact/broad gallery orders and
gate states, VAD/complete-source containment, and write-scope preservation.
A warning-clean build and complete CTest pass are engineering evidence only.

The production C++ projector then replays each frozen T123 and T111 package at
least twice. Automation may verify input/output hashes, source reconstruction,
time monotonicity, deterministic bytes, and raw change scope only. The reviewer
must read every changed complete conversation chronologically and in reverse
against `test.txt`. Only that semantic review may retain FR39 or update the
manual ledger. A retained frozen replay is not a new real-WebSocket result and
cannot close the speaker business by itself.
