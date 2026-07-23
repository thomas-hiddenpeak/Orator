# FR53 Primary Source-Gap Activity Diagnosis (2026-07-23)

## Status

FR53 is stopped and its runtime implementation is removed. The broad candidate,
alignment-gap refinement, and source-wide-text refinement each expose an
unsupported speaker-activity entry in complete Run A chronological context.
No accuracy total, candidate ranking, or product verdict was produced by code.
FR50 remains the current real-WebSocket baseline.

The final diagnostic reading confirms that primary-speaker identity plus a
forced-alignment gap does not establish that speech exists. The next phase must
investigate upstream speech-presence evidence; final fusion may not add another
primary-only activity rule.

## Evaluation Authority

The rejections below come only from complete contextual-semantic reading against
`test/data/reference/test.txt`, together with the unaltered Run A producer and
business tracks. `jq` was used only to display raw entries. Candidate inventory,
hashes, and entry counts were not used to judge, select, or score the branch.

## Rejected Candidate

| Item | Value |
|---|---|
| Baseline | `a6f0d33730326b19a3831019b1aba21fd900f126` |
| Frozen source | `/tmp/orator-spec013/release-a6f0d33-fr50-precompute/run-full-a.json` |
| Disabled replay | `/tmp/orator-spec013/release-a6f0d33-fr53-primary-activity/full-a-disabled.json` |
| Broad candidate | `/tmp/orator-spec013/release-a6f0d33-fr53-primary-activity/full-a-candidate.json` |
| First changed interval | `3.199999928-4.239999905 s`, `text_id=0` |

The reference opens with Zhu Jie speaking continuously from `00:00:03`. The
ASR source is `我是比较理想化那个人嘛，其实是这样的，就是说，嗯，嗯。`.
Its forced alignment places ordinary positive-duration source units throughout
the candidate interval. Complete-source and business-interval TitaNet evidence
selects `spk_0`, and the retained text-bearing business entry assigns the source
to `spk_0` (Zhu Jie).

The primary track instead contains a local `spk_1` interval at
`3.199999928-4.239999905 s`. The broad FR53 rule appended that interval as a
text-free Tang Yunfeng activity merely because no Tang text entry overlapped
it. In complete conversational context this is a duplicate false contribution,
not omitted speech. One hard counterexample is sufficient to reject the broad
candidate; retention would have required every changed context to pass all four
readings.

The first alignment-gap refinement is frozen under
`/tmp/orator-spec013/release-a6f0d33-fr53-primary-activity-narrowed`. Its
disabled A/B replays are byte-identical to FR50 with SHA-256
`d36227140dc8fcba9e40f116946ed8f8222e1b0aad73777665478a0f9f14d88c`;
its isolated A/B candidate files are also byte-identical with SHA-256
`7eb69482e8c18674004019c9acd0955b779bfc1fcf96f4c8669a7609ce7f1029`.
Those hashes are mechanical evidence only.

Manual chronological reading again rejects the first changed conversation.
The reference has Zhu Jie speaking continuously from `00:03:10` to
`00:03:21`. The candidate appends Tang Yunfeng at
`191.439995721-191.759995714 s`, inside the alignment pause between `就是` and
`我`. The adjacent text-bearing intervals, complete-source TitaNet evidence,
and the human-listened reference all retain Zhu Jie. The short primary segment
has no embedding evidence. Crucially, Tang Yunfeng is already represented by
the complete `你百分之十五的话，我就可以拿到百分之七啊。` text interval earlier
in the same `text_id=14` source. A mere non-overlap check therefore mistakes a
local primary error for an omitted contribution.

## Source-Wide Refinement

The final refinement required all of the following structural conditions:

1. Its overlap with one contiguous unaligned interval inside the ASR source is
   at least the existing TOML `timeline.align_snap_pause_sec`.
2. Its total overlap with all positive-duration aligned units in that source is
   no greater than the existing TOML
   `timeline.align_boundary_split_tolerance_sec`.
3. No zero-duration aligned unit lies on or inside the candidate interval.
4. No text-bearing business entry anywhere in the same finalized ASR source
   carries the candidate global identity. Testing only temporal overlap is
   insufficient.

The first condition represents a material source-text gap. The second prevents
a long primary misclassification over ordinary aligned speech from qualifying
because it happens to cross a pause. The third prevents a collapsed lexical
unit from being treated as absent source. The fourth distinguishes an omitted
speaker contribution from a speaker who is already textually represented
elsewhere in the same ASR result. No new numeric parameter is added.

Its frozen artifacts are under
`/tmp/orator-spec013/release-a6f0d33-fr53-primary-activity-source-wide`.
Disabled A/B replay remains byte-identical to FR50 with SHA-256
`d36227140dc8fcba9e40f116946ed8f8222e1b0aad73777665478a0f9f14d88c`.
Isolated candidate A/B replay is mechanically identical with SHA-256
`35f04e6dc80077b1203b36f23fe9f0346d51013a2f0e729eea83f07cb6e8a88b`.

Complete Run A chronological diagnostic reading gives the following semantic
findings. The first unsupported entry rejects the candidate; the remaining
contexts were read only to determine whether a safer shared evidence topology
exists. A/B reverse and independent B semantic readings were therefore neither
needed nor used for retention.

| Interval / source | Complete-context finding |
|---|---|
| `235.199995-235.919995`, `text_id=16`, Shi | Unsupported. Tang Yunfeng owns the complete `没有什么不可以说的` contribution; the candidate begins after its aligned text and VAD tail, with no Shi Yi response in the human-listened context. |
| `478.639989-479.119989`, `text_id=42`, Tang | Material. Tang Yunfeng's overlapping `你们俩可以` is absent from ASR while Shi Yi's simultaneous phrase remains. |
| `1101.679975-1102.239975`, `text_id=95`, Xu | Unsupported. The gap lies inside Shi Yi's continuous `差不多……最好的情况……` contribution; Xu Zijing does not speak there. |
| `1673.679963-1674.319963`, `text_id=143`, Shi | Unsupported. Tang Yunfeng is finishing the `六百美金买顶帽子` contribution before Xu Zijing's reaction; Shi Yi does not contribute there. |
| `1889.759958-1890.239958`, `text_id=163`, Shi | Unsupported. The source is Tang Yunfeng's `就没事儿`; the next Shi Yi contribution occurs later and is not this gap. |
| `2189.279951-2189.679951`, `text_id=190`, Shi | Unsupported. The gap is inside Xu Zijing's uninterrupted explanation of the financing discussion schedule. |
| `2362.799947-2363.199947`, `text_id=206`, Shi | Material. Shi Yi's `对` follows Tang Yunfeng's `对吧` but is absent from ASR source text. |
| `3101.279931-3101.759931`, `text_id=258`, Tang | Unsupported. The gap is inside Shi Yi's continuous explanation of consolidated reporting and dividends. |

The two material contexts and the unsupported controls all satisfy the final
alignment-gap, source-wide text absence, unique global identity, and native-
label conditions. Internal versus trailing gap position, VAD containment,
primary duration/confidence, and available primary-run embedding do not form one
shared reference-free boundary for both material contexts. Adding another
condition from these examples would tune final fusion to the reference instead
of establishing speech presence.

## Decision And Next Gate

FR53 is rejected, the false-default implementation and TOML key are removed,
and T243 returns runtime behavior to FR50. The failed branch is not eligible for
a real-WebSocket run or frozen-ledger change.

The post-removal full build completes without a compiler warning or error; GCC
prints only its existing ABI parameter-passing notes. All `72/72` CTest entries
pass. These are engineering checks and do not evaluate product accuracy.

The next phase must audit speech-presence evidence before speaker identity:
unaltered audio, VAD pre-threshold evidence, VAD endpoint state, Sortformer
posterior/activity/primary evidence, and source/alignment bounds for the same
material and abstention contexts. Automation may capture and display those raw
signals only. Any new producer contract requires complete contextual-semantic
review and at least two material contexts separated from accepted controls by
evidence available at runtime. Otherwise the source-omission residuals remain
explicitly unresolved.
