# Partition-Invariant Cross-Scale Abstention Diagnosis (2026-07-19)

## Scope and authority

This diagnosis follows the completed T142 full promotion review and uses only
the already frozen T111 and T123 typed tracks. It does not rerun audio or alter
any producer evidence. `test/data/reference/test.txt` remains the authoritative
human-listened reference. No code, script, query, formula, score, or interval
operation assigns correctness, aggregates accuracy, selects FR33, or issues a
product verdict. Shell tools only display immutable typed evidence on the
common time base.

The complete T142 forward and reverse contextual reviews identify
`ref-0517` as a 唐云峰 contribution whose first phrase is assigned to 朱杰.
That human judgment precedes this evidence diagnosis; model scores below only
explain how the current projector reaches its output.

## Frozen evidence topology

The runtime identity map is `spk_0=朱杰`, `spk_1=唐云峰`,
`spk_2=徐子景`, and `spk_3=石一`. T123 `text_id=289` covers
`3378.364-3383.492` on the common source clock. The first punctuation phrase
uses source characters `0-7` and aligned time `3378.604-3379.324`.

Both native speaker views are uniform over that phrase:

- activity: local slot 1 / `spk_1`, `3378.559924483-3381.279924423`;
- primary: local slot 1 / `spk_1`, `3378.559924483-3381.199924424`;
- forced alignment: six positive units reconstruct the phrase range.

The ASR-derived TitaNet views disagree by scale:

| View | Session top pair | Robust top pair | Existing-gate state |
|---|---|---|---|
| Phrase `3378.604-3379.324` | `spk_0 0.293987`, `spk_1 0.198693` | `spk_0 0.292711`, `spk_1 0.286944` | Session passes short score and margin; robust fails only short margin |
| Unique containing VAD `3378.564-3379.516` | `spk_0 0.416362`, `spk_1 0.324539` | `spk_1 0.395473`, `spk_0 0.392083` | Session passes short score and margin; robust reverses to current and fails only short margin |
| Containing business interval `3378.604-3381.004` | `spk_1 0.501351`, `spk_0 0.431384` | `spk_1 0.528978`, `spk_0 0.414933` | Both margins pass; both fail only regular absolute score |
| Complete source `3378.364-3383.492` | `spk_1 0.489046`, `spk_0 0.347262` | `spk_1 0.496800`, `spk_0 0.341280` | Both margins pass; both fail only regular absolute score |

The ordinary projector processes business intervals before phrases. The broad
business interval correctly abstains because its score is below the unchanged
regular gate. The phrase's session view then passes the short gate and paints
the first phrase as `spk_0`; its robust view cannot participate because its
margin fails. The later complete-source view also abstains at the unchanged
regular score gate. The error is therefore an evidence-precedence dependence
on ASR partition scale, not missing native speaker evidence.

## FR33 boundary

FR33 preserves the current phrase identity only for this generic topology:

1. The item is a short punctuation phrase with an available embedding,
   complete robust gallery, and the existing minimum aligned-unit coverage.
2. Every current and base character label is the same non-voiceprint identity.
   One activity slot and one primary slot with that identity cover the phrase
   completely, with no competing overlap.
3. The phrase session view selects one challenger under the existing short
   score and margin gates. The phrase robust view ranks the same challenger
   first and the current identity second, passing score but failing margin.
4. Exactly one typed VAD interval contains the phrase. Its session view selects
   the same challenger with the current identity second; its robust view
   reverses the pair, ranks the current identity first, and fails only margin.
5. Exactly one containing same-text `business_interval` and one containing
   same-text `complete_source` have available embeddings and complete robust
   galleries. All four broad views rank the current identity first and the
   challenger second, pass the unchanged regular margin, and fail only the
   unchanged regular absolute score.
6. Existing specialized challenge rules execute first. If any required view,
   rank, coverage, uniqueness, provenance, or gate state differs, FR33 does
   nothing and ordinary policy remains authoritative.

This rule adds no TOML value, threshold, duration, identity, transcript,
speaker name, timestamp, reference label, or fitted constant. It changes no
common-clock coordinate and no diarization, primary, ASR, VAD, alignment, or
voiceprint record. Its claim is only that a narrow session-only write must not
erase a uniform native identity when the complete cross-scale typed evidence
has the exact abstention topology above.

## Validation sequence

Focused tests first cover the positive topology and abstention for changed
phrase ranks, VAD count or gallery state, reversed VAD pattern, missing or
duplicate broad evidence, broad rank/margin/score changes, native competition,
non-native current labels, and insufficient alignment. A warning-clean build
and all CTest entries are engineering evidence only.

The production C++ projector then replays frozen T123 and T111 inputs at least
twice. Automation may verify hashes, source reconstruction, time order,
configuration, immutable inputs, and arrange every changed context. Every
changed conversation must be read completely forward and reverse against
`test.txt`; only that contextual review may retain or reject FR33. No new audio
run occurs before the frozen review passes.
