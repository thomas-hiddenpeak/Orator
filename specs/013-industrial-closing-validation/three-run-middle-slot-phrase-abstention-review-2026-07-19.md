# FR44 Three-Run Middle-Slot Phrase Abstention Review (2026-07-19)

## Scope and decision authority

This record completes the frozen gate specified by
`three-run-middle-slot-phrase-abstention-diagnosis-2026-07-19.md`. It evaluates
only FR44's abstention from one generic session-only punctuation-phrase write.
It does not rerun audio, change TOML, alter a producer track, evaluate ASR
accuracy, or claim a new real-WebSocket result.

No compiled code, test, script, query, formula, score operation, interval
operation, or metric assigns correctness, aggregates an accuracy result, ranks
FR44, or issues the retention decision. Automation is limited to engineering
contracts, immutable hashes, deterministic replay, source reconstruction, time
order, and display of the complete raw change scope. The decision below comes
only from complete chronological and reverse contextual semantic reading
against the human-listened `test.txt`.

## Engineering and replay evidence

The clean source builds without a `warning:` or `error:` diagnostic, and all
69 registered CTest entries pass. Focused coverage preserves the existing
two-run native-handoff guard and the existing outer-selected subminimum A-B-A
behavior. The FR44 positive requires a regular session-eligible phrase, a
same-top robust score-only abstention, exactly three `A-B-A` base runs with a
one-character selected middle run, positive adjacent primary alignment,
configured-duration outer aligned/activity/primary coverage, and exactly two
ordered non-containing VAD records with the required raw rankings.

Independent abstention cases cover the short gate, robust score eligibility,
robust top and margin disagreement, missing and duplicate phrase rankings,
multi-character, punctuation, and whitespace middle runs, missing punctuation
configuration, zero-duration adjacent alignment, different outer identities,
subminimum middle primary,
partial middle activity, non-primary adjacent provenance, insufficient outer
alignment, one/three/containing/overlapping VAD records, different VAD raw
tops, incomplete VAD galleries, duplicate VAD rankings, and a VAD margin
failure.

The final binary replays each frozen producer package twice:

| Frozen package | Replay A SHA-256 | Replay B SHA-256 | Mechanical result |
|---|---|---|---|
| T123 | `174319361040f648b4f930e312986e626f6b5cba9e3d8eaad9aeaa4a0bc7e7f1` | `174319361040f648b4f930e312986e626f6b5cba9e3d8eaad9aeaa4a0bc7e7f1` | byte-identical |
| T111 | `ad2abee782ab30ff67be1a86fa46f4ec0c16b5422ae18954107c398131157aa4` | `ad2abee782ab30ff67be1a86fa46f4ec0c16b5422ae18954107c398131157aa4` | byte-identical and unchanged from FR43 |

Supporting immutable evidence:

| Evidence | SHA-256 |
|---|---|
| warning-clean build log | `40838341b7d988224f64159d0968979691286c338c3ca32ed091ffcb8972a67e` |
| complete CTest log | `13c685273fa5a2cb877861cf7718b56df730addbab4d4239dbb98367f0604113` |
| frozen replay probe | `9a09a0f32fae3540f9ba8fbca84d216bdc5c59fa20ec59f1c9ea6b81ccb23b4a` |
| checked-in `orator.toml` | `c3c1acbc723fb3f0d600625025023edb51750fb1b51731342bc893dd2d3273b1` |
| human-listened `test.txt` | `35e8695057be82f3028877f7dc159f10ecfa0ab7f06c444cefa9a079b0e24a86` |
| T123 source timeline | `61119ab2eb4f66ed08be85652df44619001227b4986fa6b770239a840b26a9f0` |
| T111 source timeline | `d5c97db9ff91b41da4ccd5414d5f2bca4966592e60fb2717058fee2e600132e9` |
| shared activity track | `7f2a78abc0a1e26e9ebfb5782894c642f88398e81a3e52ec86318799f71d338f` |
| shared primary track | `bff99424c79440cde425a2177b00278338559d0bd78f810b9d462a1e08c8d4d9` |

These hashes establish engineering provenance and determinism only. They do
not support the semantic decision.

## Raw mechanical change scope

After omitting replay-only `turn_id`, T123 has one changed source interval. The
FR43 output was:

```text
491.228-493.308  我们两个一共才四十四十五，  spk_1
                 voiceprint_phrase_session
```

The FR44 output is:

```text
491.228-491.548  我们两个一  spk_3  primary_speaker_tie_break
491.548-492.508  共          spk_3  primary_speaker_tie_break
492.588-492.668  才          spk_1  primary_speaker_tie_break
492.668-493.308  四十四十五， spk_3  primary_speaker_tie_break
```

The T123 record count changes from 1711 to 1714 because the source regains its
three identity runs and two same-identity source pieces remain separated by
their alignment owners. There is no other text, identity, reason, timestamp,
or source-coordinate change. T111 remains byte-identical at 1752 records.
These facts expose the candidate boundary only.

## Complete contextual semantic review

The complete `07:42-08:33` conversation was read chronologically against
`test.txt`. Tang Yunfeng says that Shi Yi and Zhu Jie can pass a proposal
together, then asks how much they hold. Shi answers with the 28 plus 15
calculation. Tang inserts `有没有`. Shi supplies the `44/45` result. Tang says
Shi can veto; Shi confirms; Tang asks whether another pair can also veto; Shi
continues with the 6 plus 28 calculation.

FR44 keeps the T123 source prefix `我们两个一共` under Shi, preserves the
single aligned `才` under Tang, and restores `四十四十五` to Shi. The ASR
character `才` does not literally match the human reference's `有没有`, but it
occupies the short Tang interjection between Shi's calculation premise and
numeric answer. ASR wording is outside this speaker-only decision. Tang's
preceding question, Shi's preceding calculation, Tang's following veto
statement, and Shi's following confirmation are unchanged.

The same interval was then read in reverse from the later 6 plus 28 calculation
through Tang's second veto question, Shi's confirmation, Tang's first veto
statement, Shi's 44/45 answer, Tang's short interjection, and the original 28
plus 15 premise. This reverse read preserves the same Shi-to-Tang-to-Shi-to-
Tang sequence and the same contribution boundaries.

## Manual decision and ledger

The complete forward and reverse contextual semantic review **retains FR44**.
Only current T123 `ref-0071` changes from a critical confident-wrong Tang
attribution to accepted Shi attribution. The additional source partition
inside already accepted neighboring context is not counted as another repaired
natural contribution. T111 is unchanged and is not double-counted.

The manually reconciled frozen ledger is now `518/556`: 38 contributions
remain incorrect, comprising 32 confident-wrong, five missing, and one
uncertain. Twenty-three confident-wrong and one missing contribution remain
critical. The seven blocks are `88/93`, `79/84`, `75/80`, `75/80`, `118/129`,
`80/87`, and `3/3`; every complete 600-second natural-turn block remains above
its floor. The manually reviewed per-speaker natural-turn ledgers are Zhu Jie
`76/83`, Tang Yunfeng `174/189`, Xu Zijing `69/73`, and Shi Yi `199/211`. All
four per-speaker natural-turn floors remain passed.

Speaker-business closure remains open. Critical attribution and confident-
wrong attribution still fail, and speaker-time, per-speaker time, source-time-
offset, independent full real-path repeatability, locked holdout, report
review, and release signing remain unsigned. FR44 is a retained transitional
frozen experiment, not an industrial-release verdict.
