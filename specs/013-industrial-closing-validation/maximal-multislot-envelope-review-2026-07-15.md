# Maximal Multi-Slot Envelope Review (2026-07-15)

## Governance

Every displayed changed context was read manually with the surrounding
conversation. Tools only executed models, verified numerical/source/time-base
contracts, and arranged unjudged evidence. No code, script, formula, query,
notebook, metric, or algorithm assigned correctness, aggregated accuracy,
ranked or selected a candidate or parameter, or issued the verdict.

## Unrestricted candidate

The first frozen FR16ZW candidate emits 64 acoustic queries, accepts 12 query
regions under the frozen model gates, and changes 20 displayed reference
contexts. Complete contextual reading rejects it. In particular, the regions
around 459--467, 505--512, 1302--1307, 2752--2759, and 3183--3187 seconds bridge
independent speech endpoints and absorb real contributions from another
speaker. A maximal same-channel query and a duration-dominant foreign-channel
veto are not sufficient when multiple contributions occur inside the query.

## Comprehensive-timeline VAD guard

The final contract requires every projected complete clause to be wholly
contained by one identical frozen VAD segment. This uses an independent
pipeline on the common time base and adds no score or duration threshold. The
five reviewed regression classes cross a VAD endpoint or silence gap and no
longer enter the acoustic query set.

The guarded evidence emits 12 queries and changes three conversational
contexts after the existing dual-gallery and phrase-veto contracts. Complete
manual contextual reading confirms:

- `ref-0209`: the repaired clauses continue Xu Zijing's pricing narrative;
- `ref-0445`: the transfer-price question remains one Shi Yi contribution; and
- `ref-0533`: three fragmented clauses are restored to Shi Yi's continuous
  explanation of the independent trading window.

No reviewed context regresses. FR16ZW is retained as the next composition
baseline. This changed-context review does not claim a new full-session
accuracy result; that requires the complete two-pass contextual review of the
retained full candidate.

## Frozen artifacts

- Policy: `speaker-v21-maximal-multislot-envelope.toml`
- VAD evidence: `/tmp/orator-spec013/runtime-v21/native-postprocess/vad-padded-segments.tsv`
- Metadata: `/tmp/orator-spec013/runtime-v21/native-postprocess/maximal-multislot-envelope-metadata.json`
- Query embeddings: `/tmp/orator-spec013/runtime-v21/native-postprocess/maximal-multislot-envelope-query-embeddings.tsv`
- Session evidence: `/tmp/orator-spec013/runtime-v21/native-postprocess/maximal-multislot-envelope-session-evidence.tsv`
- Robust evidence: `/tmp/orator-spec013/runtime-v21/native-postprocess/maximal-multislot-envelope-robust-evidence.tsv`
- Candidate: `/tmp/orator-spec013/runtime-v21/native-postprocess/candidate-maximal-multislot-envelope-vad-guarded.json`
- Review: `/tmp/orator-spec013/runtime-v21/native-postprocess/review-maximal-multislot-envelope-vad-guarded.md`

The metadata and candidate record source paths and hashes.
