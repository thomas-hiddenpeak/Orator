# Official Accumulated 2-Second Review (2026-08-09)

- **Scope**: Spec 014 T072-T074
- **Candidate commit**: `a6ba893e7feb5da9c2b1af58dcebfc8357f6f7f2`
- **Candidate**: `accumulated_redecode`, 2000 ms, 2 unfixed chunks, 5 rollback tokens
- **Human reference**: `test/data/reference/test.txt`, lines 445-463
- **Decision**: rejected; `kv_append` restored

No code, script, query, formula, or metric evaluated transcript correctness,
ranked output, or issued this decision. Automation captured raw events and
checked only source, time-base, transport, telemetry, and persistence contracts.
The reviewer read every Live, Final, and comprehensive-view contribution in
chronological order and again in reverse conversational order.

## Exact Evidence

- artifact:
  `artifacts/spec014/candidates/asr-official-accumulated/focused-legal-context/`;
- source: the continuous `1536-1638 s` span of `test.mp3`, 102.000 seconds,
  1,632,000 mono 16 kHz samples;
- WAV SHA-256:
  `5c806e0e3dd6839cf9657804b639381f07abe8a4f3c5a5befee9d565d88f0cdc`;
- streamed PCM SHA-256:
  `49b86debff7b300281886d0680e112d300a3cf8b287f5fe6039fe11620b23d10`;
- run SHA-256:
  `914bb0f612f6efa7daf9d88e722d1e8b476845cb2112cbca3b357a21e0ca520d`;
- isolated TOML SHA-256:
  `b128bf861b77f961185e2d55505254664df4e3b68298008484384bf573e3c51a`;
- server binary SHA-256:
  `4737eb7dfa87b88554a6a86d3ee9ecbf533bb96dcaa3bff993b821b286693bba`;
- source commit, worktree, TOML, and binary are unchanged from client start
  through terminal capture;
- all seven pipeline extents equal 1,632,000 samples with no declared gap;
- `wall_clock_ok`, `timebase_ok`, and `timebase_reconciled` are true;
- total wall time is 103.026 seconds, reported stream factor is `0.99x`, and
  direct terminal return is 1.024 seconds;
- terminal output contains 22 diarization entries, eight Final ASR records,
  eight alignment groups, and 28 comprehensive contributions; and
- 101 runtime telemetry samples and 102 tegrastats samples cover the required
  fields, while early and late observers match the producer terminal hash.

These values establish only that the production path is clean, complete, and
real-time for this focus. They do not establish transcript or speaker accuracy.

## Complete Contextual Reading

1. The first Live state says `雷总也不说话了`, matching the human negation.
   Residual-tail Final replaces it with `雷总也是出汗了`, inventing a different
   action. The comprehensive view publishes the false Final action.
2. `他过完年之后再没找过我` is contextually preserved and improves the
   earlier control's missing `过`.
3. The human question `他是知道我们签了独家吗` becomes `签了总项目`.
   This regresses a critical agreement term that the restored control retained.
   The following `没跟他说` remains understandable only as the degraded
   `没他说`.
4. The long legal contribution no longer stops after `然后`. It reaches the
   ten-percent option pool and the ten/fifteen discussion. It also replaces
   both instances of `一致行动的人` with `一直听的人`, `灵启疆界` with
   `零起量界`, and `有限合伙人直接持` with `刘先河直接吃`; those critical
   legal and ownership terms remain unusable.
5. The next contribution preserves repeated `十够了`, but introduces
   `十五天`, changes employee `期权` to `资源`, and changes `3万4` to
   `三到四`. Long-segment continuation is improved while critical unit and
   equity meaning remain wrong.
6. The following contribution preserves the rationale about work the four
   founders do not understand and the commitment `我不可能给股份` through
   `太致命了`. This removes the truncation seen in the 800-frame candidate.
7. `故事就是这么个故事` regresses slightly to `这么多故事` without changing
   the surrounding decision.
8. The last Final removes the earlier unsupported labor-service investment and
   preserves the holding-ratio goal plus `好嘞`. However, an exposed Live state
   temporarily asserts `让老刘投钱`, another unsupported investment meaning,
   before later revision.

The reverse reading reaches the same interpretation. Improvements in long-form
continuation and several option-pool phrases do not repair the critical
exclusive-signing regression, false Final action, legal-term substitutions, or
visible investment hallucination. The comprehensive view also retains the
frozen focused speaker fragmentation and supplies no speaker-business promotion.

## Causal Conclusion and Next Bound

The accumulated implementation is technically viable and removes the prior
eight-second continuation starvation. Its two-second cadence leaves a short
segment with only one completed decode before Final. Because the first two
chunks are intentionally unfixed, Final then discards a correct Live prefix and
runs as a fresh full-context decode, reproducing a previously rejected result.

The official API exposes chunk duration. Phase 3F therefore authorizes exactly
one 1000 ms accumulated candidate while retaining the two-chunk/five-token
rollback policy. This allows short segments to cross the rollback boundary
before Final without adding a model special case. It is a bounded hypothesis,
not a product conclusion, and it must repeat this complete review before any
longer gate is authorized.

After restoration, checked-in `test_config` passes and complete CTest passes
`75/75` in `52.89` seconds. The build emits no warning or error, and all capture
processes are stopped. Commit and push remain the last T075 actions.
