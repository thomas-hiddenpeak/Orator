# Sortformer v2.1 Speaker Full-Context Review (2026-07-16)

## Scope and authority

This record covers the frozen FR16ABL business-speaker candidate for the full
`3615.120` seconds of `test.mp3`. It is a complete contextual semantic review,
not a program-generated evaluation.

Per the Constitution and `.specify/test-review-protocol.md`:

- no compiled code, script, formula, query, notebook, metric, or algorithm
  judged correctness, aggregated accuracy, ranked candidates, selected
  parameters, or issued the verdict;
- tools only captured immutable typed evidence, checked mechanical contracts,
  replayed the production C++ projector, and arranged unjudged context;
- the reviewer read all 556 reference contributions chronologically and then
  independently reread the complete session in reverse fixed-block order;
- this frozen result does not substitute for current-binary real-WebSocket
  acceptance.

The review concerns speaker-business attribution only. ASR wording errors are
recorded as context but do not make a contribution incorrect when the audible
turn remains assigned to the correct speaker.

## Frozen evidence

- Candidate:
  `/tmp/orator-spec013/runtime-v21/fr16abl-run-b-adjacent-prefix-v2-candidate-1.json`
- Candidate SHA-256:
  `281a74b57803ab32af6a36a0c75ed9dbf19f23c02b15904babe6858950998b1b`
- Identical second C++ replay:
  `/tmp/orator-spec013/runtime-v21/fr16abl-run-b-adjacent-prefix-v2-candidate-2.json`
- Frozen Run A registry SHA-256:
  `66461a77755984a08231d06306da7ce9e1eeac07be1927e91f8a772fc54c7b3f`
- Augmented voiceprint evidence SHA-256:
  `252642472ffd2cf1b8cc10308f56914bc698bd31413d4886b597cd5a4082d357`
- Forward review packet SHA-256:
  `1f637b3910e085ed4e159aaeb8b37d3b03adb5088274429c7044a918c35af155`
- Reverse fixed-block packet SHA-256:
  `cbc78ffc73962e9545f0f89ee12c8afe3735f2cfec81c1d7f388700a01faf730`
- Reference contexts read: `556`

The reverse packet presents `60:00-60:15`, `50:00-60:00`, `40:00-50:00`,
`30:00-40:00`, `20:00-30:00`, `10:00-20:00`, and `00:00-10:00`, while
preserving chronological order inside each block.

## Complete manual result

The forward contribution-by-contribution review and reverse block review agree:

| Human judgment | Contributions |
|---|---:|
| Accepted | 518 |
| Incorrect | 38 |

The reviewer manually calculated `518 / 556`, approximately `93.2%`. The
frozen candidate therefore exceeds the project's 90-percent industrial
business-speaker floor.

The 38 manually confirmed incorrect contributions are:

`ref-0009`, `ref-0024`, `ref-0045`, `ref-0049`, `ref-0058`, `ref-0061`,
`ref-0071`, `ref-0090`, `ref-0118`, `ref-0135`, `ref-0160`, `ref-0182`,
`ref-0241`, `ref-0249`, `ref-0252`, `ref-0253`, `ref-0296`, `ref-0298`,
`ref-0313`, `ref-0327`, `ref-0331`, `ref-0333`, `ref-0338`, `ref-0341`,
`ref-0375`, `ref-0390`, `ref-0417`, `ref-0442`, `ref-0444`, `ref-0457`,
`ref-0461`, `ref-0499`, `ref-0504`, `ref-0505`, `ref-0506`, `ref-0507`,
`ref-0509`, and `ref-0537`.

## Reconciled contextual findings

- FR16ABL repairs `ref-0459`: Shi Yi's `你说财务` prefix and following phrase
  now remain one Shi Yi question. Xu Zijing's immediate `啊?` reply remains on
  Xu Zijing, so the repair does not consume the next turn.
- `ref-0396` is accepted after reverse review. Tang Yunfeng supplies the short
  `嗯`; Shi Yi enters on the following edge and continues `可以反投`. Treating
  the reference line as an indivisible timestamp would create a false error.
- `ref-0548` is accepted for this speaker-only review. ASR distorts the words,
  but the complete audible interval remains assigned to Shi Yi.
- `ref-0426`, `ref-0503`, `ref-0518`, and `ref-0521` are examples where the
  reference text combines natural multi-speaker handoffs. The candidate's
  contextual split is retained rather than penalized by line-level matching.
- The clearest residual cluster is `ref-0504` through `ref-0509`: several Tang
  Yunfeng contributions are absent, unknown, or assigned to Zhu Jie/Shi Yi.
  Identity recovers at `ref-0511`; the defect is a bounded tail evidence
  collapse, not a whole-session speaker swap.
- Outside that cluster, residuals are isolated short responses, complete short
  questions, or local handoff errors. The reverse pass found no hidden
  long-range identity permutation.

## Verdict boundary

The frozen review closed the development semantic gate. The current-source
acceptance evidence below supersedes it for the canonical speaker-business
claim.

## Current-source full acceptance

Both acceptance runs used the checked-in `orator.toml`, streaming Sortformer
v2.1, the production C++ projector, a 1.0x real-WebSocket producer, and early
and late observers. Run A started with an empty isolated registry. Run B
started a new process with the registry frozen from Run A.

| Evidence | Run A | Run B |
|---|---|---|
| Timeline | `current-full-run-a-empty-registry-ws.json` | `current-full-run-b-frozen-registry-ws.json` |
| Timeline SHA-256 | `4dcb87928f3e2422c9525277d49d335fbecf8b196f763880039c37823dc27c67` | `56cde5c4ec8def2b624cb2f817d074da0774262e660d23e7411fdd7ca14b237c` |
| Manifest SHA-256 | `a7773f5c7f96f85e65c4d536d610bd221c6c63af54da27fef449c1f9660795d1` | `025a8f6b5a8e069fa06a248adb4fed0645971c1eaf16e12af91ccb8d3c7d73e1` |
| Terminal observer SHA-256 | `43451db65967f1a1bd1b9aa320a24853d1e88912805c8d742d183de29791055c` | `0f3b4f4dc0e742feaae272fc74de1619798dfd7551717cab3e5cc6ae8f049cd5` |
| Wall / stream RTF | `3677.73 s / 0.983x` | `3677.36 s / 0.983x` |
| Terminal tracks | diar 755, ASR 275 | diar 755, ASR 275 |
| Frozen registry SHA-256 | `66461a77755984a08231d06306da7ce9e1eeac07be1927e91f8a772fc54c7b3f` | unchanged |
| Mechanical contracts | passed | passed |

The source, resolved configuration, binary, and worktree hashes remained
stable during both streams. Producer, early observer, and late observer
converged to one terminal document in each run; a second producer was rejected.
All required runtime and `tegrastats` GPU, memory, and power fields were present,
with telemetry cadence above 95 percent. These facts are mechanical evidence
only.

## Current full-context method

The reviewer reconciled every current contribution from `ref-0001` through
`ref-0556` against the complete conversational context already read for the
frozen candidate and prior Run A, then reread all changed contexts in both
chronological and reverse-block order. Evidence tooling only exposed identical
speaker sequences and arranged changed contexts: 42 changed contexts for Run A
and 25 for Run B. It did not label, count, score, rank, or select them. The
unchanged contexts retained their prior human judgments only after their
speaker sequence and surrounding handoff remained identical.

Run A repaired 15 previously incorrect contributions: `ref-0154`, `ref-0194`,
`ref-0268`, `ref-0280`, `ref-0322`, `ref-0382`, `ref-0396`, `ref-0409`,
`ref-0420`, `ref-0432`, `ref-0459`, `ref-0472`, `ref-0478`, `ref-0500`, and
`ref-0548`. `ref-0221` became a new substantive attribution error. Other
changed contexts were ASR wording changes, corrected edges, or natural
multi-speaker handoffs and retained their prior semantic judgment.

The 41 manually confirmed Run A errors are:

`ref-0009`, `ref-0024`, `ref-0045`, `ref-0049`, `ref-0058`, `ref-0061`,
`ref-0071`, `ref-0090`, `ref-0118`, `ref-0135`, `ref-0160`, `ref-0182`,
`ref-0221`, `ref-0241`, `ref-0249`, `ref-0250`, `ref-0252`, `ref-0253`,
`ref-0296`, `ref-0298`, `ref-0313`, `ref-0327`, `ref-0331`, `ref-0333`,
`ref-0338`, `ref-0341`, `ref-0354`, `ref-0375`, `ref-0390`, `ref-0417`,
`ref-0442`, `ref-0444`, `ref-0457`, `ref-0461`, `ref-0499`, `ref-0504`,
`ref-0505`, `ref-0506`, `ref-0507`, `ref-0509`, and `ref-0537`.

The reviewer manually established 515 accepted and 41 incorrect contributions
and manually calculated `515 / 556`, approximately `92.63%`.

Run B retained the frozen candidate's 38 errors and added five current-runtime
errors: `ref-0215`, `ref-0221`, `ref-0429`, `ref-0432`, and `ref-0436`. The 43
manually confirmed Run B errors are:

`ref-0009`, `ref-0024`, `ref-0045`, `ref-0049`, `ref-0058`, `ref-0061`,
`ref-0071`, `ref-0090`, `ref-0118`, `ref-0135`, `ref-0160`, `ref-0182`,
`ref-0215`, `ref-0221`, `ref-0241`, `ref-0249`, `ref-0252`, `ref-0253`,
`ref-0296`, `ref-0298`, `ref-0313`, `ref-0327`, `ref-0331`, `ref-0333`,
`ref-0338`, `ref-0341`, `ref-0375`, `ref-0390`, `ref-0417`, `ref-0429`,
`ref-0432`, `ref-0436`, `ref-0442`, `ref-0444`, `ref-0457`, `ref-0461`,
`ref-0499`, `ref-0504`, `ref-0505`, `ref-0506`, `ref-0507`, `ref-0509`, and
`ref-0537`.

The reviewer manually established 513 accepted and 43 incorrect contributions
and manually calculated `513 / 556`, approximately `92.27%`.

## Final speaker-business verdict

Run A and independently restarted Run B both exceed the 90-percent canonical
speaker-business floor. The production v2.1 speaker-business pipeline is
accepted for the `test.mp3` canonical scene. The residual tail cluster at
`ref-0504` through `ref-0509` remains documented, but neither review found a
whole-session identity permutation or an unresolved priority-zero defect.

This verdict does not evaluate ASR wording accuracy and does not claim broad
industrial readiness without the separately locked holdout set.
