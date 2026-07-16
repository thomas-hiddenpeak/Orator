# Adjacent Primary-Supported Prefix Review

**Date:** 2026-07-16
**Status:** Full frozen-context gate passed; runtime acceptance pending

## Evaluation Authority

No code, script, notebook, formula, query, metric, or algorithm judged speaker
correctness, aggregated accuracy, ranked a candidate, or issued this verdict.
Automation enumerated typed adjacent intervals, generated raw acoustic evidence,
checked source/time/hash contracts, replayed the production C++ projector, and
displayed the sole changed context. The retention decision below comes only from
manual reading of the complete surrounding conversation in both chronological
and reverse reference order.

## Frozen Evidence

- Typed adjacent queries: 67.
- Query manifest:
  `/tmp/orator-spec013/runtime-v21/fr16abl-run-b-evidence-manifest.json`.
- Frozen Run A registry SHA-256:
  `66461a77755984a08231d06306da7ce9e1eeac07be1927e91f8a772fc54c7b3f`.
- Augmented voiceprint TSV SHA-256:
  `252642472ffd2cf1b8cc10308f56914bc698bd31413d4886b597cd5a4082d357`.
- Both C++ frozen replays produced SHA-256:
  `281a74b57803ab32af6a36a0c75ed9dbf19f23c02b15904babe6858950998b1b`.
- Mechanical comparison exposed one changed business entry only.

The robust Run A prototype identities map `spk_1`, `spk_2`, and `spk_3` to
their same Run B identities; Run A `spk_4` maps to frozen Run B `spk_0`. This is
an evidence-identity translation, not a correctness decision. A separate
attempt to reconstruct the runtime epoch gallery from final thresholded diar
segments enrolled `spk_4` through `spk_8`; because that identity set differs
from the frozen four-identity Run B track, its output was rejected as a
mechanically invalid evidence source and was not used by the candidate.

## Complete Context Review

Before the changed interval, Xu Zijing explains that the two people approve one
another. Shi Yi then asks whether Xu means the finance or finance-responsible
role. Xu answers immediately afterward, and Tang Yunfeng follows with a separate
question about the two finance-responsible roles. The changed source is:

- `2982.520-2982.680` `spk_3`: `你`
- `2982.680-2983.320` `spk_3`: `说财务`
- `2983.560-2984.040` `spk_2`: `啊？`

The first two entries are one continuous Shi Yi question. The following reply
remains Xu Zijing, so the challenge repairs the source-initial prefix without
crossing the next speaker handoff. Reverse-order reading of `ref-0456` through
`ref-0466` reaches the same conclusion. The change is retained.

## Development Ledger

The complete manually maintained frozen ledger now contains 518 accepted and 38
incorrect reference contributions out of 556, approximately 93.2 percent. This
clears the 93 percent development margin. It is not a closeout verdict: the full
candidate still requires complete forward and reverse semantic rereading,
critical and fixed-block review, and two real full-length WebSocket runs.
