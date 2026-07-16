# Expanded Local Run Phrase Review (2026-07-15)

## Scope

FR16ZN was generated on top of the retained guarded FR16ZK candidate. The
acoustic query expands from each exact complete punctuation phrase to its
maximal contiguous native top-1 local run inside the same padded VAD. The write
range remains the exact phrase. Session-registry and robust clean-gallery
TitaNet both use the unchanged duration-aware TOML gates and must agree with
the frozen local-slot mapping.

The tool produced 13 query spans and one accepted phrase. These counts are
mechanical execution facts, not product evaluation. The review packet displayed
both affected reference contexts. No executable mechanism assigned
correctness, aggregated accuracy, ranked candidates, selected a parameter, or
issued the verdict below.

## Manual contextual semantic review

The only accepted phrase is `都已经聊到这个，` at approximately
`3400.86-3401.58`. The surrounding contribution is `可以聊聊嘛，都已经聊到这了`,
spoken by Zhu Jie. The candidate assigns the phrase to Xu Zijing. The following
Xu Zijing contribution begins with `因为实际上就是改架构这个事`, after this
phrase. Reading both displayed reference contexts together therefore shows
that the rewrite does not restore the real speaker and breaks the preceding
single-speaker contribution.

The complete changed-context review rejects FR16ZN. The retained baseline
remains guarded FR16ZK. Expanded same-slot audio is useful as independent
counter-evidence: it correctly prevents several locally confident but
voiceprint-conflicting rewrites around `2865-2869` seconds. It is not sufficient
positive evidence to authorize a phrase rewrite by itself.

## Reproducibility

- Policy: `speaker-v21-expanded-local-run-phrase.toml`
- Metadata: `/tmp/orator-spec013/runtime-v21/native-postprocess/expanded-local-run-phrase-metadata.json`
- Candidate: `/tmp/orator-spec013/runtime-v21/native-postprocess/candidate-expanded-local-run-phrase.json`
- Review packet: `/tmp/orator-spec013/runtime-v21/native-postprocess/review-expanded-local-run-phrase.md`

All source artifacts and hashes are recorded in the frozen metadata and
candidate. This report records manual contextual judgment only.
