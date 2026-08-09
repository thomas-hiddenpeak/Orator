# Official Greedy Termination Review (2026-08-10)

- **Scope**: Spec 014 T086-T087
- **Candidate commit**: `a9ebea78508ab0240c331094f12c5e026cdfbaa0`
- **Candidate**: `accumulated_redecode`, 1000 ms, `ban_steps = 0`
- **Human reference**: `test/data/reference/test.txt`, lines 445-463
- **Classification**: focused diagnostic only; no product verdict
- **Superseded decision**: the former rejection and decision to stop longer
  evaluation are invalid under Constitution 1.8.0
- **Full-length resolution**: Phase 3I later rejects the exact candidate after
  complete 3615.120-second contextual review; see
  `official-greedy-full-context-review-2026-08-10.md`

This report covers only 102 seconds of a 3615.120-second source. Its local
semantic observations remain valid causal evidence, but they cannot establish
global accuracy, overall improvement/regression, candidate acceptance or
rejection, or whether full evaluation should proceed. Phase 3I therefore
requires the exact candidate to receive a full real-WebSocket run and complete
chronological/reverse review before any product decision.

No code, script, query, formula, metric, or algorithm evaluated transcript
correctness, compared transcript candidates, assigned an accuracy result, or
issued this decision. Automation captured raw events and checked only source,
configuration, transport, timing, time-base, observer, persistence, telemetry,
and schema contracts. The reviewer read all 81 Live events and eight Final
records chronologically and in reverse order, then read all 28 comprehensive
contributions chronologically and in reverse order against the complete human
conversation.

## Exact Evidence

- artifact:
  `artifacts/spec014/candidates/asr-official-greedy-no-eos-ban/focused-legal-context/`;
- source: the continuous `1536-1638 s` span of `test.mp3`, 102.000 seconds,
  1,632,000 mono 16 kHz samples;
- WAV SHA-256:
  `5c806e0e3dd6839cf9657804b639381f07abe8a4f3c5a5befee9d565d88f0cdc`;
- streamed PCM SHA-256:
  `49b86debff7b300281886d0680e112d300a3cf8b287f5fe6039fe11620b23d10`;
- isolated TOML SHA-256:
  `269e2d9ca6f79196f3bf76535279b3c3b02c57983bfa1468ecaea625f9459b90`;
- resolved configuration SHA-256:
  `58a1f4276ece3778ab26eff78acaf5c65fce7d537f896488d047b48baf8698bf`;
- server binary SHA-256:
  `4c1e41d5e61f1a41310d64dcea9c88f4976f0ca1c7336e0768f93f5e5ca79fd1`;
- run SHA-256:
  `715e1354de8c9e716186f1652d390f0a0579c253abf5e84392b519ab14ca0e74`;
- manifest SHA-256:
  `fa573ef7be3c395ff51a4c683bae37b736c9d31bdc291bb4389891e2dd97792b`;
- source commit and worktree remained clean and unchanged from client start
  through terminal capture; and
- the isolated TOML differs from the checked-in candidate only by WebSocket/UI
  ports and artifact-local registry/storage/session paths.

The real-WebSocket run completes in 103.332 seconds at reported `0.987x` and
returns its terminal state 1.330 seconds after direct `end`. Diarization and ASR
compute totals are 2.151 and 52.044 seconds. Terminal output contains 22
diarization entries, eight Final ASR records, 33 VAD segments, eight alignment
groups, and 28 comprehensive contributions.

All seven declared pipeline extents close at exactly 1,632,000 samples with no
gap; wall-clock, time-base, and reconciliation flags are true. Producer, early
observer, and late observer terminal hashes match; early event and telemetry
streams match, and there is no unexpected observer error. Runtime telemetry has
101 samples and `tegrastats` has 103 samples, with complete required GPU,
memory, system-power, CPU, RAM, and temperature coverage. These are mechanical
capture facts only and do not evaluate transcript or speaker correctness.

## Complete Live and Final Reading

### Segment 0

The second Live state correctly says `雷总也不说话了`. Final still changes it
to `雷总也是出汗了`. Removing the first-three-token EOS ban therefore does not
prevent the complete accumulated-audio Final from replacing the human negation
with a false action.

### Segment 1

Final preserves `他过完年之后再没找过我`. This previously preserved statement
remains intact but does not offset failures in the critical controls.

### Segment 2

The Live trajectory moves through `嗯` and `是知道` to `签了这个项目`, then
locks `签了总项目`. Neither Live nor Final ever contains the human critical term
`独家`. The reply remains `没他说` instead of `没跟他说`.

### Segment 3

An early Live briefly ends in `语音`, then changes to `一直听`; Final repeats
`一直听的人` instead of the legal relationship `一致行动的人`. It also retains
`零起量界` instead of `灵启疆界` and `刘先河直接吃` instead of
`有限合伙人直接持`. The later 10-percent option-pool question is carried
forward, so these are critical lexical and ownership substitutions rather than
a missing tail.

### Segment 4

Live still exposes `我觉得挺幸福的时候了`, then changes to the unsupported
`十五天`. Final retains that addition, uses `资源` for the employee `期权`, and
changes `3万4` to `三到四`. The candidate therefore fails the legal, equity,
and numeric-unit controls.

### Segment 5

The founders' rationale and `我不可能给股份` commitment remain preserved
through Final. The candidate does not regress this important control.

### Segment 6

The first Live says `故事就是这么`; Final still adds words and publishes
`故事就是这么多故事` instead of `故事就是这么个故事`.

### Segment 7

Live again exposes the unsupported instruction `让老刘投钱`, then revises it
away. Final preserves the effort and holding-ratio goal plus `好嘞`, but the
material false Live assertion remains part of the user-visible history.

The complete reverse reading reaches the same interpretation. In particular,
the preserved ending does not retroactively repair the earlier false investment
instruction, and the preserved no-equity commitment does not change the legal
and numeric failures that precede it.

## Complete Comprehensive-View Reading

All 28 contributions were read in chronological order and then from contribution
27 back to contribution 0. Forced alignment and the frozen FR50 speaker view
split the eight Final records into speaker-bound fragments, but their combined
text preserves every material Final substitution listed above:

- `雷总也是出汗了` remains the terminal business assertion;
- the exclusive-signing question remains `签了总项目`;
- `一直听的人`, `零起量界`, and `刘先河直接吃` remain in the legal passage;
- `十五天`, employee `资源`, and `三到四` remain in the following passage;
- the no-equity commitment remains preserved; and
- the final effort and holding-ratio goal remain preserved.

No diarization, speaker identity, forced-alignment, or comprehensive-view record
contains independent text evidence that can recover the missing negation,
exclusive-signing term, legal relationship, company name, ownership phrase,
equity term, or numeric unit. The comprehensive view therefore cannot repair
this candidate after ASR Final publication. Its speaker fragmentation remains
inside the frozen conditional FR50 boundary and is not promoted by this run.

## Historical Decision (Superseded)

The candidate was authorized because native decoding suppressed EOS during its
first three argmax positions while the pinned official vLLM sampler did not.
The exact product-path test removes that suppression without changing the
accumulated state, VAD, prompt, segment, model, alignment, speaker, time-base, or
publication values. Complete contextual review shows no repair in any focused
critical control and retains the same material Live hallucination.

The original review used these local findings to reject Phase 3H, stop the
longer gates, restore `kv_append`, and close the accumulated decoder branch.
That product decision exceeded the evidence scope and is superseded by
Constitution 1.8.0. The restoration remains a historical engineering action,
not a valid semantic verdict.

## Full-Length Resolution

This report itself remains diagnostic only. Phase 3I subsequently runs the
exact candidate through complete `test.mp3` and reads all 556 `test.txt`
contributions and all 2,664 Live events chronologically and again in reverse
fixed windows. That independent full-length review manually places the
candidate in the 70-79% semantic band, finds every complete 600-second block
below closure, and rejects it. The local findings above are confirmed as part
of the broader session-wide pattern; they did not independently produce that
verdict.
