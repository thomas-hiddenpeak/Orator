# Sortformer v2.1 Orthogonal Evidence Capture (2026-07-19)

## Scope and governance

This T192 record arranges the capture-faithful T191 posterior beside frozen
T123 typed evidence and the current FR45 business view. It contains no derived
speaker label, correctness field, channel score, product total, candidate
ranking, threshold search, or acceptance result. Automation copied rows by
common-clock intersection, preserved their order, validated file presence, and
computed hashes only.

The product authority remains the complete human-listened
`test/data/reference/test.txt`. No conclusion may be drawn from this capture
until every context has been read chronologically and in reverse under T193.

## Immutable inputs

| Input | SHA-256 |
|---|---|
| T191 four-channel Sortformer v2.1 frames | `79fd2c416ac76a0af477f98bf8d848f6e604b2d94c5c4445e653978afd6c7e41` |
| Frozen T123 terminal session | `22e1e0a899dbfa17f783128f253630d3db09ad646f1285eb3c027385f97e1727` |
| Current FR45 T123 business view | `5a595ca1aa5816612b2603062d8467ee60bc3a342219cf5eda066cfddc3bb61a` |
| Reference-oriented display of `test.txt` | `ebb49f090f5cb7271099efc5ed5a0f40dda464cb313002fa9b5143ee98d1927f` |
| Context query table | `c484f3ea500e4a572258c9f43ea539996ce64606c14d0c814498480358401b59` |
| Local identity epoch table | `f3fdc829ea5b8b1e48404732df8e9eb119d10d85759f895a45e9265da5ff6c04` |

## Worksheet layout

The capture root is
`/tmp/orator-spec013/release-fr47-orthogonal-evidence/worksheets`. It contains
23 context directories, one for each FR46 critical residual. Every directory
contains:

- `context.tsv`: focus and accepted-control reference IDs plus the copied
  common-clock bounds;
- `reference-sections.md`: the complete selected human reference entries;
- `sortformer-frames.csv`: every intersecting `0.08 s` frame and all four raw
  sigmoid values;
- `local-identity-epochs.tsv`: every intersecting local-slot-to-global-ID
  epoch;
- `typed-tracks.json`: intersecting frozen activity, primary, VAD, ASR,
  alignment, and TitaNet voiceprint entries;
- `current-business.json`: intersecting FR45 business entries.

All 23 directories contain all six files. The posterior subsets range from 38
to 1,214 rows because context duration is preserved rather than normalized.
Each typed display contains all six upstream track kinds. The capture has no
empty file. Its 141-file content manifest is
`/tmp/orator-spec013/release-fr47-orthogonal-evidence/worksheets/artifact-manifest.sha256`
at SHA-256
`f839d1efd360b812ef771585e62081ec9a4a1b7efe70e66f79e31c1e02810359`.

## Frozen identity epochs

The capture-faithful TitaNet replay records the following local-slot epochs:

| Local slot | Common-clock interval | Global identity |
|---|---:|---|
| 0 | `0.000-3243.280` | `spk_1` |
| 0 | `3243.280-3615.120` | `spk_3` |
| 1 | `0.000-2136.640` | `spk_0` |
| 1 | `2136.640-3108.480` | `spk_1` |
| 1 | `3108.480-3330.640` | `spk_0` |
| 1 | `3330.640-3615.120` | `spk_1` |
| 2 | `0.000-3615.120` | `spk_2` |
| 3 | `0.000-3615.120` | `spk_3` |

This mechanically means slots 0 and 1 both carry `spk_1` from
`2136.640-3108.480`, while slots 0 and 3 both carry `spk_3` after
`3243.280`. It does not itself establish whether an individual business turn
is correct or whether a production identity rule should change. T193 must
relate these epochs to complete conversational evidence manually.

T192 is complete. The evidence is now frozen for the T193 forward and reverse
contextual semantic review; no model, TOML, production code, audio result, or
speaker ledger changed.
