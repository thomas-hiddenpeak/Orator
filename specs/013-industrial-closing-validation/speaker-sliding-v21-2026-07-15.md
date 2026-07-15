# v2.1 Multi-Scale Voiceprint Screening - 2026-07-15

## Status

T059G is complete as a frozen-evidence screening experiment. The reference-free
candidate repairs five of the inherited v2.1 profile's natural turns without a
new contextual regression, raising the full 556-turn diagnostic from 413 to
418 correct (`75.1799%`). This is below the 93 percent implementation gate, so
the policy is not integrated into the runtime and receives no real-WebSocket
acceptance claim.

The candidate generator is reference-free and does not evaluate results. No
code, script, test, notebook, formula, query, metric, or algorithm may assign
correctness, calculate accuracy, compare/rank candidates, select the policy, or
issue a verdict. The historical judgments below were made by reading adjacent
conversational context and manually deriving the reported totals.

## Evidence Contract

The tool reads the captured v2.1 timeline, native TitaNet scores, and a dedicated
TOML policy. It does not read `test.txt`, real speaker names, known failure
timestamps, or manually assigned correctness. `test.txt` is read only afterward
by the display-only review tool and the contextual reviewer.

The candidate enforces these constraints:

- 3.0-second and 5.0-second native TitaNet windows advance by 1.0 second on the
  captured session's absolute clock;
- both scales must independently pass the configured score and margin gates and
  select the same identity;
- rankings are restricted to the four identities active in this captured
  session, rather than stale identities retained in the registry;
- known-speaker rewrites require a whole-turn TitaNet decision plus a sustained
  rolling run for the same identity;
- candidate-strength rewrites split only at forced-alignment pauses; strong
  evidence may preserve the captured business-span boundary;
- a short baseline identity surrounded by the selected identity on both sides
  is preserved to avoid absorbing a real interjection;
- every threshold, duration, scale, and boundary tolerance comes from
  `speaker-sliding-v21.toml`.

## Frozen Result

| Item | Result |
|---|---:|
| Audio extent | 3615.12 s at 16 kHz |
| Active identities | `spk_3`, `spk_1`, `spk_2`, `spk_4` |
| Native TitaNet spans | 7,224 |
| Dual-scale accepted points | 1,166 |
| Accepted continuous runs | 239 |
| Source business entries | 936 |
| Output business entries | 938 |
| Changed source entries | 9 |
| Reference rows affected | 11 |

The 936 decisions comprise 671 ineligible direct embeddings, 252 direct
agreements with the baseline, two unknown fills, seven known-speaker rewrites,
one rejected neighbour-sandwich rewrite, and three rejections for insufficient
rolling duration.

The display-only changed packet was read in adjacent conversational context.
All other rows retain the completed inherited-profile manual judgment because
their speaker assignment is unchanged. No code or executable automation assigned
a label, total, ranking, policy choice, or verdict.

| Reference row | Manual result | Reason |
|---|---|---|
| `ref-0001` | remains correct | Two early unknown spans become Zhu Jie; the turn was already correct from its substantive continuation. |
| `ref-0334` | repaired | The patent-location question changes from Tang Yunfeng to Zhu Jie through the aligned phrase boundary. |
| `ref-0356` | repaired | The substantive North America/Singapore proposal changes from Tang Yunfeng to Zhu Jie. |
| `ref-0357` | remains correct | The adjacent Xu Zijing reply is unchanged. |
| `ref-0379` | remains incorrect | Only the middle 3.96 seconds changes to Zhu Jie; substantial start and end clauses remain Tang Yunfeng. |
| `ref-0404` | remains correct | The changed evidence begins on the following Zhu Jie question; Tang Yunfeng's substantive turn stays intact. |
| `ref-0405` | repaired | The complete twelve-working-day question changes from Xu Zijing to Zhu Jie. |
| `ref-0450` | repaired | The substantive retained-capital clause changes from Tang Yunfeng to Zhu Jie. |
| `ref-0451` | remains correct | The following Shi Yi reply remains `spk_3`. |
| `ref-0515` | repaired | Both substantive halves of the long two-option summary change from Tang Yunfeng to Zhu Jie. |
| `ref-0516` | remains correct | The following Shi Yi turn remains `spk_3`. |

The final contextual diagnostic is therefore 418 correct, 137 incorrect, and
one ambiguous natural turn. It equals `418/556 = 75.1799%`, 14.8201 percentage
points below the 90 percent product floor and 17.8201 points below the 93
percent frozen-candidate implementation gate. It is not an exact speaker-time
score because the audible-boundary ledger remains unsigned.

## Reproducibility

The evidence build was repeated after implementation and produced a byte-
identical rolling-evidence JSON. Re-projecting from that evidence produced the
same 936 decisions and nine changed entries; only the recorded rolling-evidence
input path differed. Seven focused Python cases pass, including scale agreement,
active-identity filtering, neighbour-sandwich rejection, and alignment-boundary
splitting. The clean configured project suite passes 64/64 after removal of the
obsolete v2 model gate.

| Artifact | SHA-256 |
|---|---|
| Captured v2.1 real-WebSocket timeline | `653e366ba18ff61011b43d08ebd1da7f8f10c247a6b7482800a60eb4bcc4ea65` |
| TitaNet model | `59e1e3069a33bef8da8b9467a5a64a30a5de8c7ae2550b15d5323ef537acaad1` |
| Frozen registry | `d7e2b7ff7a5ba3f945b177cfa2888e3d02ef7b33f4d552d6e9f39e517ad47f38` |
| Sliding policy TOML | `fbe975c4d6f23206a4f28933f334ca26f6d0c57a30acc3c209db6bc928837e0f` |
| Sliding span TSV | `812249bb86d233711b2e478cd3b9610457fcd4a8195908758506f3461e6bab94` |
| Sliding TitaNet TSV | `725acc85641243824ca59012700a8f846a15940e972a888a0566c0af333d29e6` |
| Business-turn TitaNet TSV | `e8cace17e6d69199324d104901c4623431a2effc7783c11314cf4466c19d67d1` |
| Rolling evidence JSON | `dd84d3c2f343d050eee9abde5ea31d7cf8e6199b0d5b63fea620da4dfd88d2ba` |
| Projected candidate JSON | `035100cc6624a69d4376af71fdf739a89999a40d0375b2e3e9b286f18ecf742c` |
| Eleven-row review packet | `4317cc009ab23b05f75864be12342c7256e526f81b1c93577cff1a280031ece6` |

## Decision

The experiment proves that independent rolling TitaNet evidence has limited,
high-confidence value for complete Zhu Jie turns. It does not recover the many
remaining short turns and mixed spans because direct embeddings are unavailable
or conflicting for most of them. Per Spec 013 section 6.3, policy and threshold
tuning stops here. No offline-only model is introduced, and this frozen tool is
not presented as deployable functionality. A new model can be selected only if
it has a feasible native streaming path and exceeds the 93 percent frozen gate.
