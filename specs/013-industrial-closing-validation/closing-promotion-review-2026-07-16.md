# Closing Promotion Review - 2026-07-16

> **Historical evidence.** The current clean-commit evidence and conjunctive-
> gate audit are recorded in
> [current-commit-full-review-2026-07-17.md](current-commit-full-review-2026-07-17.md).
> That report supersedes this document's current-source totals and promotion
> wording.
> The later T135 complete forward/reverse reconciliation also supersedes the
> historical `ref-0426`, `ref-0503`, and `ref-0518` acceptance rows below; see
> [speaker-baseline-reconciliation-2026-07-19.md](speaker-baseline-reconciliation-2026-07-19.md).

## Evaluation boundary

This record separates mechanical verification from product-result evaluation.
Programs ran the production pipeline, verified source, time, hash, transport,
telemetry, and deterministic-replay contracts, and arranged unjudged evidence
for reading. No compiled code, script, query, formula, notebook, metric, or
algorithm assigned correctness, counted accepted contributions, calculated an
accuracy result, ranked a candidate, or issued a product verdict.

Every product judgment below was made by reading the complete conversational
context in chronological order and then rereading fixed reverse blocks. A
natural handoff with only an edge character assigned to the adjacent speaker is
accepted. A complete substantive contribution attributed to the wrong speaker
is incorrect.

## Frozen replay changes

The initial-slot corroboration replay was run twice from the same retained Run A
typed tracks. Both outputs had SHA256
`a36e3a7ec642f84b207552513e2362d209bcaa709ff6893481112dd08c5c19a0`.
Manual review of every changed context found repairs for Zhu Jie at
`ref-0350`, `ref-0352`, and `ref-0363`; the adjacent `ref-0351` context retained
its prior unresolved boundary and did not become a new substantive regression.

The guarded four-view replay was then run twice on that retained candidate.
Both outputs had SHA256
`5c10cfff80999fb6b61632b3914491f497a2b94bde703b1aea1d5d6db9ef99bf`.
Its only changed context, Tang Yunfeng's `ref-0308` question, was repaired. The
five changed contexts therefore contain four clear contextual repairs, one
unchanged unresolved boundary, and no reviewed correct-to-incorrect transition.

The transport serializer now retains nine decimal places for live and terminal
common-clock seconds. Focused tests establish that the one-sample interval
`1.000000000-1.000062500` remains positive after JSON serialization and that
typed in-memory evidence does not change. This is a mechanical time-base
contract and did not participate in the judgments above.

## 120-second gate

| Item | Value |
|---|---|
| Artifact | `/tmp/orator-spec013/runtime-v21/closing-gate-120-ws.json` |
| Artifact SHA256 | `3911c7170211fc9940993df3a55f6258f3d3d2df91e08c0089d8301de4187141` |
| Audio | `test/data/audio/test.mp3`, first 120.000 seconds |
| Runtime path | real WebSocket, 1.0x input rate |
| Model | `models/sortformer_4spk_v2.1.safetensors` |
| Configuration | checked-in `orator.toml` |
| Mechanical result | source, time, hash, terminal, and telemetry contracts passed |

The reviewer read all 18 reference contributions intersecting the audible
prefix, then reread `0-120` as one reverse block. `ref-0018` was judged only on
its audible portion before the cutoff.

| Ref | Judgment | Note |
|---|---|---|
| 0001 | Accepted | Zhu main opening contribution |
| 0002 | Accepted | Zhu continuation |
| 0003 | Accepted | Xu question |
| 0004 | Accepted | Zhu reply |
| 0005 | Incorrect | Shi short interjection split across Xu and Zhu identities |
| 0006 | Accepted | Zhu continuation after the interjection |
| 0007 | Accepted | Xu question |
| 0008 | Accepted | Tang clause visible in local-slot context; only edge characters split |
| 0009 | Accepted | Zhu `然后呢` with one edge character split |
| 0010 | Accepted | Tang proposal |
| 0011 | Accepted | Zhu interruption and continuation |
| 0012 | Accepted | Tang `就这么定了` |
| 0013 | Accepted | Zhu `不是` |
| 0014 | Accepted | Tang `不能犹豫` |
| 0015 | Accepted | Zhu interruption |
| 0016 | Accepted | Tang response |
| 0017 | Accepted | Zhu reply |
| 0018 | Accepted | Audible Tang portion stable through the cutoff |

Manual conclusion: 17 accepted and 1 incorrect contribution. The reviewer
manually calculated `17 / 18 = 94.44%`; the 120-second semantic gate passed.

## 360-second observer gate

The first three observer attempts failed after the default libwebsockets
validity boundary because the raw receive-only client ignored WebSocket PING.
Two attempted server-side buffering changes did not alter that boundary and
were fully reverted. The authoritative raw client now sends a serialized,
client-masked PONG with the identical payload and excludes control frames from
application evidence. Socket-level coverage passed.

| Item | Value |
|---|---|
| Artifact | `/tmp/orator-spec013/runtime-v21/closing-gate-360-rfc-pong-ws.json` |
| Artifact SHA256 | `f6b4549c7c97d232400e49337448fc0ad57b2117e0ce1487856b45d42e14f435` |
| Audio | `test/data/audio/test.mp3`, first 360.000 seconds |
| Runtime path | real WebSocket, 1.0x input rate |
| Model | `models/sortformer_4spk_v2.1.safetensors` |
| Configuration | checked-in `orator.toml` |
| Mechanical result | producer plus early/late observers remained valid; ordered live and terminal, time, hash, and telemetry contracts passed |

The reviewer read all 39 reference contributions intersecting the audible
prefix in chronological order and then reread `240-360`, `120-240`, and
`0-120` in reverse block order. `ref-0039` was judged only on its audible
portion before the cutoff.

| Ref | Judgment | Note |
|---|---|---|
| 0001 | Accepted | Zhu opening contribution |
| 0002 | Accepted | Zhu continuation |
| 0003 | Accepted | Xu question |
| 0004 | Accepted | Zhu reply |
| 0005 | Incorrect | Shi short interjection split across Xu and Zhu identities |
| 0006 | Accepted | Zhu continuation |
| 0007 | Accepted | Xu question |
| 0008 | Accepted | Tang clause visible in local-slot context; edge characters split |
| 0009 | Accepted | Zhu `然后呢` with an edge split |
| 0010 | Accepted | Tang proposal |
| 0011 | Accepted | Zhu interruption |
| 0012 | Accepted | Tang decision |
| 0013 | Accepted | Zhu `不是` |
| 0014 | Accepted | Tang `不能犹豫` |
| 0015 | Accepted | Zhu interruption |
| 0016 | Accepted | Tang response |
| 0017 | Accepted | Zhu reply |
| 0018 | Accepted | Tang long turn |
| 0019 | Accepted | Zhu long turn |
| 0020 | Accepted | Shi contribution visible in the local-slot context |
| 0021 | Accepted | Tang contribution visible in the duplicate-time exchange |
| 0022 | Accepted | Shi numerical contribution visible in the duplicate-time exchange |
| 0023 | Accepted | Tang main contribution |
| 0024 | Accepted | Zhu `啊?` restored |
| 0025 | Incorrect | Xu `嗯?` merged into Zhu's identity |
| 0026 | Accepted | Zhu question |
| 0027 | Accepted | Tang response |
| 0028 | Accepted | Zhu long turn |
| 0029 | Accepted | Tang interruption; internal edge character only |
| 0030 | Accepted | Tang long turn |
| 0031 | Accepted | Zhu long turn |
| 0032 | Accepted | Tang question; trailing handoff belongs to Zhu |
| 0033 | Accepted | Zhu answer |
| 0034 | Accepted | Tang response |
| 0035 | Incorrect | Zhu `我就铺垫错了嘛` attributed mainly to Tang |
| 0036 | Accepted | Tang substantive reply remains correctly attributed |
| 0037 | Incorrect | Tang `不能再等了` attributed to Zhu |
| 0038 | Accepted | Xu long turn |
| 0039 | Accepted | Audible Xu portion stable through the cutoff |

Manual conclusion: 35 accepted and 4 incorrect contributions. The reviewer
manually calculated `35 / 39 = 89.74%`; the 360-second semantic promotion gate
failed and no 600-second promotion is claimed from this state.

This run reused `/tmp/orator/storage/spec013-speakers.bin` after several
diagnostic sessions had already updated it. The registry was not an isolated
acceptance fixture, so this failed result is retained as evidence but cannot be
used to diagnose a model or rule regression. The next promotion attempt must
start from a backed-up and then empty isolated registry; its resulting fixture
must be frozen before the restarted-registry run.

## Clean 600-second gate

The diagnostic registry above was backed up and removed before this run. The
server started with no `spec013-speakers.bin`; the registry produced by this
single 600-second session was frozen with SHA256
`a4fcc26f7da1a707e3463195b73ec34b1f291ad49a1fc5b05061327c02e52798`.

| Item | Value |
|---|---|
| Artifact | `/tmp/orator-spec013/runtime-v21/closing-gate-600-clean-ws.json` |
| Artifact SHA256 | `b8ba1946cc42c8f6f5711e05288a1c8e19be3e685984cc0ed71c57ca348952d1` |
| Audio | `test/data/audio/test.mp3`, first 600.000 seconds |
| Runtime path | real WebSocket, 1.0x input rate, producer plus early/late observers |
| Model | `models/sortformer_4spk_v2.1.safetensors` |
| Config SHA256 | `ab7e6420cdbda53b457f82130256fb0f6a8e3c4867d227b657b0f11d54f93a04` |
| Binary SHA256 | `cb4b4a665f15944b4e90a41654713c7c4c1237c61d6714920d6b8c651c9e9f6f` |
| Wall time / stream RTF | 606.49 seconds / 0.989x |
| Terminal tracks | diar 98, ASR 46 |
| Mechanical result | all observer, terminal, source, config, binary, time, hash, and telemetry contracts passed |

The reviewer read every one of the 93 reference contributions intersecting the
600-second run in chronological order, then reread `480-600`, `360-480`,
`240-360`, `120-240`, and `0-120` in reverse block order. `ref-0093` was judged
only on its audible portion through the cutoff.

| Ref | Judgment | Note |
|---|---|---|
| 0001 | Accepted | Zhu opening contribution stable from clean startup |
| 0002 | Accepted | Zhu continuation |
| 0003 | Accepted | Xu question |
| 0004 | Accepted | Zhu reply |
| 0005 | Accepted | Shi `就是杭州嘛` restored as `spk_3` |
| 0006 | Accepted | Zhu continuation |
| 0007 | Accepted | Xu question |
| 0008 | Accepted | Tang main clause; leading edge belongs to Shi |
| 0009 | Accepted | Zhu `然后呢`; first character is an edge slip |
| 0010 | Accepted | Tang main proposal |
| 0011 | Accepted | Zhu interruption; leading edge remains with Tang |
| 0012 | Accepted | Tang `就这么定了` |
| 0013 | Accepted | Zhu `不是` |
| 0014 | Accepted | Tang `不能犹豫` |
| 0015 | Accepted | Zhu interruption |
| 0016 | Accepted | Tang long response |
| 0017 | Accepted | Zhu reply; one leading handoff character |
| 0018 | Accepted | Tang long turn |
| 0019 | Accepted | Zhu long turn |
| 0020 | Accepted | Shi response |
| 0021 | Accepted | Tang contribution visible in duplicate-time context |
| 0022 | Accepted | Shi numerical contribution visible in duplicate-time context |
| 0023 | Accepted | Tang main contribution |
| 0024 | Incorrect | Zhu `啊?` attributed to Xu |
| 0025 | Accepted | Xu `嗯?` |
| 0026 | Accepted | Zhu question |
| 0027 | Accepted | Tang response |
| 0028 | Accepted | Zhu long turn |
| 0029 | Accepted | Tang interruption; short internal boundary noise only |
| 0030 | Accepted | Tang long turn |
| 0031 | Accepted | Zhu long turn |
| 0032 | Accepted | Tang statement; trailing handoff belongs to Zhu |
| 0033 | Accepted | Zhu answer |
| 0034 | Accepted | Tang response |
| 0035 | Accepted | Zhu main clause; trailing handoff fragment |
| 0036 | Accepted | Tang long turn |
| 0037 | Accepted | Tang `不能再等了` restored |
| 0038 | Accepted | Xu long turn |
| 0039 | Accepted | Xu continuation |
| 0040 | Accepted | Xu continuation |
| 0041 | Accepted | Tang main clause; final character edge slip |
| 0042 | Accepted | Xu short acknowledgment |
| 0043 | Accepted | Tang response |
| 0044 | Accepted | Xu response |
| 0045 | Incorrect | Shi `对` attributed to Xu |
| 0046 | Accepted | Xu acknowledgment |
| 0047 | Accepted | Xu main proposal; terminal handoff edge |
| 0048 | Accepted | Shi response |
| 0049 | Incorrect | Tang `对` attributed to Shi |
| 0050 | Accepted | Zhu response |
| 0051 | Accepted | Tang response |
| 0052 | Accepted | Shi response |
| 0053 | Accepted | Zhu acknowledgment |
| 0054 | Accepted | Shi continuation |
| 0055 | Accepted | Shi numerical statement |
| 0056 | Accepted | Tang `对` |
| 0057 | Accepted | Shi numerical contribution |
| 0058 | Incorrect | Tang `相差0.7` attributed mainly to Shi |
| 0059 | Accepted | Shi continuation |
| 0060 | Accepted | Xu question |
| 0061 | Incorrect | Shi `我说` attributed to Tang |
| 0062 | Accepted | Tang response |
| 0063 | Accepted | Shi `对` |
| 0064 | Accepted | Tang main clause |
| 0065 | Accepted | Shi contribution; terminal handoff fragment |
| 0066 | Accepted | Tang phrase restored except first character |
| 0067 | Accepted | Shi numerical turn |
| 0068 | Accepted | Tang question |
| 0069 | Accepted | Shi response |
| 0070 | Accepted | Tang contribution in shared ASR phrase |
| 0071 | Incorrect | Shi `才44 45` attributed to Tang |
| 0072 | Accepted | Tang `那你可以否决了` |
| 0073 | Accepted | Shi response; first two characters are an edge slip |
| 0074 | Accepted | Tang question |
| 0075 | Accepted | Shi reply |
| 0076 | Accepted | Tang acknowledgment |
| 0077 | Accepted | Shi calculation |
| 0078 | Accepted | Tang `可以啊` |
| 0079 | Accepted | Shi `倒是可以` |
| 0080 | Accepted | Tang `合理的, 没问题` |
| 0081 | Accepted | Xu objection; following handoff belongs to Shi |
| 0082 | Accepted | Shi long proposal |
| 0083 | Accepted | Tang question |
| 0084 | Accepted | Shi reply |
| 0085 | Accepted | Tang question |
| 0086 | Accepted | Shi long turn |
| 0087 | Accepted | Shi numerical explanation |
| 0088 | Accepted | Xu `对` |
| 0089 | Accepted | Shi response |
| 0090 | Incorrect | Xu `嗯, 对, 对` attributed to Shi |
| 0091 | Accepted | Shi continuation |
| 0092 | Accepted | Xu contribution visible inside the numerical exchange |
| 0093 | Accepted | Shi audible portion through 600 seconds |

The chronological and reverse-block reviews agree on 86 accepted and 7
incorrect contributions. The reviewer manually calculated
`86 / 93 = 92.47%`. The clean 600-second business-speaker result passes the
90-percent industrial floor. It does not by itself replace the independently
terminated 120-second and 360-second promotion runs, and it is not a full-file
acceptance result.

## Observed-gallery promotion

The checked-in profile changes only
`speaker_fusion.minimum_gallery_size = 3`. Each run below started as a new
server process with the same frozen registry fixture, then terminated at its
declared duration. The profile, source, and binary remained unchanged during
all three streams.

| Item | Value |
|---|---|
| Resolved config SHA256 | `e9f07bd5e2cab535d1617d7530e853b5c256db4fa380eee8cc1700a5a399415a` |
| `orator.toml` SHA256 | `0d690eea6482518cfd866efb08215a8ffd29addc80530bc53d30035665bfb9a8` |
| Binary SHA256 | `cb4b4a665f15944b4e90a41654713c7c4c1237c61d6714920d6b8c651c9e9f6f` |
| Runtime path | real WebSocket, 1.0x producer, early observer, late observer |
| Model | `models/sortformer_4spk_v2.1.safetensors` |

| Duration | Artifact SHA256 | Wall / stream RTF | Terminal evidence | Mechanical contracts |
|---|---|---|---|---|
| 120 s | `f908962e7856f271ded1382e86e857836e1c0eaad576cdcdc0161692a793ed1e` | 120.805 s / 0.993x | diar 23, ASR 10, voiceprint 0, business 33 | Passed |
| 360 s | `d9573d19218da49786579d106d1fe8712f2f8c2efa311ddbea936643c4f5481c` | 364.392 s / 0.988x | diar 54, ASR 28, voiceprint 1479, business 106 | Passed |
| 600 s | `357a9bff3ed39e9d5adf541c16bbb65009ade9cb22594f8bfe69eb01ffcbb219` | 606.499 s / 0.989x | diar 98, ASR 46, voiceprint 2475, business 199 | Passed |

The 120-second terminal state had only two stable identities, so an empty
voiceprint track is expected under the three-identity minimum. Complete
chronological review of all 18 contributions and reverse review of the full
interval found only `ref-0005` incorrect. The reviewer manually calculated
`17 / 18 = 94.44%`.

The 360-second terminal state had three stable identities and emitted final
voiceprint evidence. Complete chronological review of all 39 contributions,
followed by reverse review of `240-360`, `120-240`, and `0-120`, found
`ref-0005`, `ref-0009`, and `ref-0025` incorrect. The reviewer manually
calculated `36 / 39 = 92.31%`. Relative to the independently terminated
four-identity-gate run, `ref-0035` and `ref-0037` were repaired after the
observed gallery became available.

The 600-second terminal state was reviewed across all 93 contributions in
chronological order and then reread in reverse fixed blocks. The only incorrect
contributions were `ref-0024`, `ref-0045`, `ref-0049`, `ref-0058`, `ref-0061`,
`ref-0071`, and `ref-0090`; the reviewer manually calculated
`86 / 93 = 92.47%`. The mature terminal gallery also retroactively restored
the early contexts that were unresolved in the shorter terminal states. No
whole-speaker swap or long-range identity drift was found in the reverse pass.

All three promotion levels exceed the 90-percent industrial floor under the
complete contextual semantic review rule. This promotes the profile to the two
full-length acceptance runs; it is not itself a full-file acceptance verdict.

## Full-length acceptance Run A

Run A started a new server process with an empty isolated speaker registry.
The producer streamed the complete 3615.120-second `test.mp3` through the real
WebSocket path at 1.0x input rate while early and late observers remained
connected. The registry created by this run was frozen before any restart.

| Item | Value |
|---|---|
| Artifact | `/tmp/orator-spec013/runtime-v21/closing-full-final-a-gallery3-ws.json` |
| Artifact SHA256 | `caa683381a7cd3e10c5556caeca1ab8998ffeb3dc73144b9cbd8383fd8b4c563` |
| Frozen registry | `/tmp/orator-spec013/runtime-v21/registry-diagnostics/full-run-a-gallery3.bin` |
| Frozen registry SHA256 | `66461a77755984a08231d06306da7ce9e1eeac07be1927e91f8a772fc54c7b3f` |
| Audio | `test/data/audio/test.mp3`, complete 3615.120 seconds |
| Runtime path | real WebSocket, 1.0x producer, early observer, late observer |
| Model | `models/sortformer_4spk_v2.1.safetensors` |
| Resolved config SHA256 | `e9f07bd5e2cab535d1617d7530e853b5c256db4fa380eee8cc1700a5a399415a` |
| `orator.toml` SHA256 | `0d690eea6482518cfd866efb08215a8ffd29addc80530bc53d30035665bfb9a8` |
| Binary SHA256 | `cb4b4a665f15944b4e90a41654713c7c4c1237c61d6714920d6b8c651c9e9f6f` |
| Wall time / stream RTF | 3654.861 seconds / 0.989x |
| Terminal tracks | diar 755, ASR 275 |
| Terminal timeline SHA256 | `79f2a507ca0eb345efbd03746a3d7cf1ec8102ff484599385499aeb46369c0b3e` |
| Mechanical result | all observer, terminal, source, config, binary, common-time-base, extent, hash, and telemetry contracts passed |

The producer, early observer, and late observer ended with the same terminal
timeline hash. All seven worker extents reached 57,841,920 samples with zero
gap. Source, config, binary, and worktree hashes remained unchanged during the
stream, and telemetry coverage exceeded the required 95 percent. These are
mechanical observations only and did not contribute to the accuracy verdict.

The reviewer read all 556 reference contributions and their candidate business
view in chronological order. The reviewer then independently reread the six
fixed reverse blocks `50:00-60:15`, `40:00-50:00`, `30:00-40:00`,
`20:00-30:00`, `10:00-20:00`, and `00:00-10:00`. No executable mechanism
assigned a row judgment, counted a result, estimated accuracy, or issued the
verdict.

The complete manual ledger is expressed as the following 55 incorrect
contributions; every other reference contribution from `ref-0001` through
`ref-0556` is accepted:

`ref-0009`, `ref-0024`, `ref-0045`, `ref-0049`, `ref-0058`, `ref-0061`,
`ref-0071`, `ref-0090`, `ref-0118`, `ref-0135`, `ref-0154`, `ref-0160`,
`ref-0182`, `ref-0194`, `ref-0241`, `ref-0249`, `ref-0250`, `ref-0252`,
`ref-0253`, `ref-0268`, `ref-0280`, `ref-0296`, `ref-0298`, `ref-0313`,
`ref-0322`, `ref-0327`, `ref-0331`, `ref-0333`, `ref-0338`, `ref-0341`,
`ref-0354`, `ref-0375`, `ref-0382`, `ref-0390`, `ref-0396`, `ref-0409`,
`ref-0417`, `ref-0420`, `ref-0432`, `ref-0442`, `ref-0444`, `ref-0457`,
`ref-0459`, `ref-0461`, `ref-0472`, `ref-0478`, `ref-0499`, `ref-0500`,
`ref-0504`, `ref-0505`, `ref-0506`, `ref-0507`, `ref-0509`, `ref-0537`, and
`ref-0548`.

The reverse pass changed seven first-pass judgments after the surrounding
conversation made the source turn structure explicit:

| Ref | Final judgment | Contextual reason |
|---|---|---|
| 0155 | Accepted | Tang carries the substantive middle clause; only adjacent Shi handoff fragments remain at the two edges |
| 0248 | Accepted | Shi opens the invitation, Tang supplements it, Shi asks, and Tang urges; the reference row combines the alternating exchange |
| 0263 | Accepted | Shi opens, Tang completes `我们自己别吼`, Shi confirms, and Tang continues in the next source turn |
| 0426 | Superseded by T135 | The complete reconciled review finds no usable Shi identity for the packaging proposal |
| 0503 | Superseded by T135 | Most of Zhu's sustained nominee-ownership proposal remains assigned to Shi |
| 0518 | Superseded by T135 | `老师最有发言权` is Zhu's answer and was assigned to Tang |
| 0521 | Accepted | Zhu and Tang perform a natural two-person handoff inside one merged reference row |

Short content alone was not grounds for acceptance. Complete contributions such
as `ref-0049` and `ref-0061` remain incorrect because their entire business
attribution is wrong; the edge allowance applies only when the substantive
contribution remains with the correct speaker.

The reviewer manually established 501 accepted and 55 incorrect contributions,
then manually calculated `501 / 556 = 90.11%`. Run A passes the 90-percent
industrial business-speaker floor. This is one acceptance run, not the final
closing verdict; the independently restarted frozen-registry Run B and its own
complete forward and reverse semantic review remain mandatory.

## Historical current-source promotion evidence

After FR16ABL entered the production projector, the current binary repeated the
entire acceptance protocol. Empty-registry Run A and restarted frozen-registry
Run B each streamed all `3615.120` seconds at 1.0x through the real WebSocket
path with early and late observers. Both passed source, configuration, binary,
worktree, common-clock, extent, observer, terminal-hash, telemetry, and
single-producer contracts. Run A produced the same frozen registry SHA-256
`66461a77755984a08231d06306da7ce9e1eeac07be1927e91f8a772fc54c7b3f`;
Run B preserved it.

Complete contextual semantic reconciliation over all 556 contributions and
reverse blocks manually established:

| Run | Accepted | Incorrect | Manual result |
|---|---:|---:|---:|
| Current A, empty registry | 515 | 41 | approximately 92.63% |
| Current B, frozen registry | 513 | 43 | approximately 92.27% |

No program assigned correctness, produced these totals, calculated these
percentages, or issued the verdict. The complete current ledgers, artifact
hashes, reconciliation notes, residual cluster, and claim boundary are recorded
in `final-full-context-review-2026-07-16.md`.

Both runs exceeded the 90-percent natural-business-turn speaker floor in this
historical review. The 2026-07-17 clean-commit report supersedes these totals
and records that the natural-turn gate alone does not close T084, the complete
canonical scene, ASR accuracy, or industrial readiness.
