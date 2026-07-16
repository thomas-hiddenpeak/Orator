# Primary-Aligned Island Review (2026-07-16)

## Governance

All 36 displayed changed contexts were read manually against the surrounding
conversation and the complete reference transcript. Tools only ran frozen
models, checked source/time/hash contracts, and arranged unjudged evidence. No
code, script, formula, query, notebook, metric, or algorithm assigned
correctness, aggregated accuracy, ranked or selected a candidate or parameter,
or issued the verdict.

## Candidate Result

Both full-session generations produce the same business track. The candidate
requires one sustained primary run, matching production activity, robust
TitaNet support, complete forced-alignment units, and a uniform conflicting
baseline identity. It changes 36 displayed contexts across 34 primary runs.

Complete changed-context reading rejects the candidate. Valid repairs exist,
including the isolated replies near 467, 504, 1007, and 1083 seconds, but the
same policy also rewrites genuine contributions from another speaker. Reviewed
regressions include:

- near 3 seconds, Zhu Jie's contribution is reassigned to Tang Yunfeng;
- near 760 seconds, Tang Yunfeng's phrase is reassigned to Shi Yi;
- near 1048 seconds, Shi Yi's terminal reply is reassigned to Tang Yunfeng;
- near 2301 seconds, Xu Zijing's contribution is reassigned to Tang Yunfeng;
- near 2421 seconds, Shi Yi's sustained phrase is reassigned to Tang Yunfeng;
- near 2895 seconds, Xu Zijing's phrase is reassigned to Shi Yi;
- near 3038 seconds, Tang Yunfeng's phrase is reassigned to Shi Yi;
- near 3204 seconds, Shi Yi's phrase is reassigned to Tang Yunfeng;
- near 3401 seconds, Zhu Jie's phrase is reassigned to Xu Zijing; and
- near 3535 seconds, Shi Yi's phrase is reassigned to Tang Yunfeng.

The primary track and production activity track share the same Sortformer
ancestry. Their agreement therefore does not constitute independent identity
evidence, and forced-alignment lag can make both support the wrong side of a
handoff. Exact-island dual TitaNet probes veto some regressions but also reject
valid repairs and misidentify at least one sustained cross-speaker phrase.
Consequently, no deployment-only rule observed in this evidence separates the
valid subset. FR16AAC is not promoted and does not proceed to full two-pass
review.

## Frozen Artifacts

- Policy: `speaker-v21-primary-arbitration.toml`
- Candidate A: `/tmp/orator-spec013/runtime-v21/native-postprocess/candidate-primary-aligned-island-a.json`
- Candidate B: `/tmp/orator-spec013/runtime-v21/native-postprocess/candidate-primary-aligned-island-b.json`
- Island spans: `/tmp/orator-spec013/runtime-v21/native-postprocess/primary-aligned-island-query-spans.tsv`
- Session evidence: `/tmp/orator-spec013/runtime-v21/native-postprocess/primary-aligned-island-session-titanet.tsv`
- Robust evidence: `/tmp/orator-spec013/runtime-v21/native-postprocess/primary-aligned-island-query-robust-titanet.tsv`

The candidate metadata records the immutable source paths and hashes.
