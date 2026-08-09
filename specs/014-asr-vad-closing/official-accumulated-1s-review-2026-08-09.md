# Official Accumulated 1-Second Review (2026-08-09)

- **Scope**: Spec 014 T076-T078
- **Candidate commit**: `244006e6fa0df083e9a236029e7e4f3934b6a099`
- **Candidate**: `accumulated_redecode`, 1000 ms, 2 unfixed chunks, 5 rollback tokens
- **Human reference**: `test/data/reference/test.txt`, lines 445-463
- **Decision**: rejected; `kv_append` and the dormant 2000 ms value restored

No code, script, query, formula, metric, or algorithm evaluated transcript
correctness, compared candidates, or issued this decision. Automation captured
raw events and checked only source, time-base, transport, telemetry,
persistence, and performance contracts. The reviewer read every Live, Final,
and comprehensive-view contribution in chronological order and again in
reverse conversational order against the complete human context.

## Exact Evidence

- artifact:
  `artifacts/spec014/candidates/asr-official-accumulated-1s/focused-legal-context/`;
- source: the continuous `1536-1638 s` span of `test.mp3`, 102.000 seconds,
  1,632,000 mono 16 kHz samples;
- WAV SHA-256:
  `5c806e0e3dd6839cf9657804b639381f07abe8a4f3c5a5befee9d565d88f0cdc`;
- streamed PCM SHA-256:
  `49b86debff7b300281886d0680e112d300a3cf8b287f5fe6039fe11620b23d10`;
- run SHA-256:
  `881bfa08a98c407cb25aae44800ad41c1017cd09a1c499da4946e49b1fe11725`;
- isolated TOML SHA-256:
  `08108eed61503d93fb4e0a326788cd4640a3a7506517b400b41f91cafcc74ffd`;
- server binary SHA-256:
  `4737eb7dfa87b88554a6a86d3ee9ecbf533bb96dcaa3bff993b821b286693bba`;
- source commit, worktree, TOML, and binary remained unchanged from client
  start through terminal capture;
- all seven pipeline extents equal 1,632,000 samples with no declared gap;
- `wall_clock_ok`, `timebase_ok`, and `timebase_reconciled` are true;
- total wall time is 103.282 seconds, reported stream factor is `0.988x`, and
  direct terminal return is 1.280 seconds;
- diarization and ASR compute totals are 2.537 and 52.337 seconds;
- terminal output contains 22 diarization entries, eight Final ASR records,
  eight alignment groups, and 28 comprehensive contributions;
- all early and late observers match the producer terminal state; and
- 101 runtime telemetry samples plus 102 tegrastats samples cover GPU use,
  memory, power, and temperature without a missing required field.

The telemetry records show runtime GPU use up to 98%, memory use up to
35,688.6 MB, system power up to 65.672 W across the two evidence sources, and
temperature up to 57 C. These values and the preceding facts establish only a
clean, complete, real-time production-path capture. They do not evaluate ASR or
speaker correctness.

## Complete Contextual Reading

1. The first one-second Live contains only `扯鸡巴蛋`. The next Live correctly
   says `雷总也不说话了`, but Final changes this to `雷总也是出汗了`. The
   comprehensive view therefore publishes a false action in place of the human
   negation.
2. `他过完年之后再没找过我` is preserved in Final. Its comprehensive text is
   split across two speaker labels even though the human context has one
   speaker; this is known frozen FR50 fragmentation rather than a promotion.
3. The human question `他是知道我们签了独家吗` first becomes `这个项目` and
   then `总项目`; no Live state contains `独家`. The reply `没跟他说` becomes
   `没他说`. The comprehensive view also fragments this exchange.
4. The long legal contribution reaches its later option-pool discussion, but
   changes both instances of `一致行动的人` to `一直听的人`, `灵启疆界` to
   `零起量界`, and `有限合伙人直接持` to `刘先河直接吃`. Those substitutions
   change legal, company, and ownership meaning.
5. The following contribution invents `十五天`, changes employee `期权` to
   `资源`, and changes `3万4` to `三到四`. A provisional Live also says
   `我觉得挺幸福的时候了` before revision. Repeated discussion of `十` is
   retained, but the critical unit and equity meanings are not.
6. The next contribution preserves the founders' rationale and the commitment
   `我不可能给股份`, including its emphasis through `太致命了`.
7. `故事就是这么个故事` becomes `故事就是这么多故事`, a smaller lexical
   change that does not repair the surrounding critical errors.
8. The last Final preserves the holding-ratio goal and `好嘞`, but its Live
   trajectory temporarily asserts `让老刘投钱`. Later revision removes that
   unsupported investment instruction, so the terminal text is better than the
   exposed Live history but the Live state remains unusable.

The complete reverse reading reaches the same interpretation. The
comprehensive view preserves all Final substitutions and known speaker
fragmentation; no other pipeline repairs the missing negation, agreement term,
legal names, ownership phrase, option term, or numeric unit.

## Decision and Root-Cause Boundary

The one-second cadence reaches the rollback state earlier, but does not repair
any critical Final that failed at two seconds. Its critical Finals are
effectively unchanged, it exposes additional unstable Live text, and its ASR
compute total rises to 52.337 seconds for the same 102-second input. Therefore
the Phase 3F cadence hypothesis is rejected and no 360-second, 600-second, or
full-length candidate gate is authorized.

The evidence also bounds the next investigation. The correct short negation is
present before Final and disappears after rollback, while `独家` never appears
in any accumulated Live state. A global rollback reduction could retain an
early correct tail, but it could also lock early wrong text such as `这个项目`
and the visible hallucinations. The current evidence cannot justify another
rollback or cadence value. The implementation remains inactive as a diagnostic
capability; checked-in behavior returns to `kv_append`.

The next phase is evidence-only: capture the exact accumulated decode-step
state, tokenized retained prefix, rollback boundary, audio extent, and generated
continuation for this same fixed context, then compare the native transition
contract with the pinned official source. These traces may expose a concrete
implementation mismatch, but they may not score text, select parameters, or
authorize a product candidate without a new SDD decision and complete
contextual semantic review.

After restoration, focused `test_config` passes and complete CTest passes
`75/75` in `52.75` seconds. The build emits no warning or error, and no capture
process remains.
