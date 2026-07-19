# FR44 Three-Run Middle-Slot Phrase Abstention Diagnosis (2026-07-19)

## Scope and decision authority

This diagnosis follows retained FR43 and examines the remaining critical
T111/T123 regression at `ref-0071`. It uses only the human-listened
`test/data/reference/test.txt`, the frozen T111 and T123 typed producer
packages, and the current production projector. It does not rerun audio,
change a producer track, tune TOML, evaluate ASR wording, or claim a new
real-WebSocket result.

No compiled code, script, query, formula, score operation, interval operation,
or automated metric assigns correctness, ranks a candidate, or issues a
product verdict. Shell and `jq` commands only display immutable source,
speaker, alignment, voiceprint, VAD, and projection evidence. The speaker
finding below comes from reading the complete conversation chronologically and
in reverse against `test.txt`.

## Complete contextual finding

The complete `07:52-08:21` exchange is a calculation between Tang Yunfeng and
Shi Yi. Tang asks how much Shi and Zhu hold together. Shi answers that 28 plus
15 is not much. Tang inserts `有没有`. Shi answers `才44 45`. Tang then says
that Shi can veto, Shi confirms that he can veto, and Tang asks whether another
pair can also veto.

The same section was read in reverse from the later `6+28` calculation through
Tang's veto question, Shi's `44/45` answer, Tang's `有没有`, and the original
question. Both reading directions preserve the Tang-to-Shi-to-Tang sequence at
`ref-0070` through `ref-0072`. The substantive `四十四十五` response belongs to
Shi Yi. This is a contextual speaker judgment; the score and timing records
below only diagnose why the current projector removes that attribution.

## Immutable common-clock evidence

The identity map is `spk_0=朱杰`, `spk_1=唐云峰`, `spk_2=徐子景`, and
`spk_3=石一`. T111 and T123 have byte-identical activity-diarization and
primary-speaker tracks:

| Track | SHA-256 |
|---|---|
| activity diarization | `7f2a78abc0a1e26e9ebfb5782894c642f88398e81a3e52ec86318799f71d338f` |
| primary speaker | `bff99424c79440cde425a2177b00278338559d0bd78f810b9d462a1e08c8d4d9` |

Around the changed source, the primary track is Shi
`489.919989-492.159989`, Tang `492.159989-492.639989`, and Shi
`492.639989-493.599989`. Activity contains Shi across
`489.839989-496.719989` and Tang across `490.879989-495.039989`.

T111 `text_id=38` represents the local phrase as source `[29,36)`,
`有四十四十五，`, with two base identity runs: Tang then Shi. The existing
two-run sustained native-handoff guard preserves the Shi run. Its final view
therefore retains `492.660-493.380 四十四十五， spk_3`.

T123 `text_id=43` changes the source representation. Its punctuation phrase is
source `[23,36)`, `我们两个一共才四十四十五，`, at
`491.228-493.308`. Before phrase fusion, the exact source identity runs are:

| Source range | Base identity | Base evidence |
|---|---|---|
| `[23,29)` | Shi / `spk_3` | diarization plus terminal primary refinement |
| `[29,30)` | Tang / `spk_1` | one positively aligned primary-refined character |
| `[30,36)` | Shi / `spk_3` | positively aligned primary refinement |

The immediately adjacent aligned characters map to the same Shi-Tang-Shi
primary sequence. Both Shi outer runs expose at least the existing configured
primary-consensus duration through positive aligned time and local
activity/primary evidence. The middle Tang run contains exactly one source
character.

The phrase's session scores select Tang and pass the existing regular score and
margin gates. The robust scores have the same raw top identity and pass the
existing margin gate, but the raw top `0.543589056` remains below the checked-in
regular score gate `0.55`; the robust view therefore abstains. The phrase also
crosses exactly two ordered, non-containing VAD records:

| VAD | Span | Session/robust raw top |
|---|---|---|
| `vad:134` | `490.020-491.932` | Shi / Shi |
| `vad:135` | `492.260-493.532` | Tang / Tang |

Because the base phrase has three runs rather than two, the retained FR16ABM
guard does not apply. The generic session-only phrase write then assigns all of
`[23,36)` to Tang and emits
`491.228-493.308 我们两个一共才四十四十五， spk_1`. The regression is
therefore a final fusion-precedence issue, not changed Sortformer activity,
primary identity, common-clock coordinates, or speaker registration.

## Rejected broader changes

The first native-handoff prototype already showed that protecting every mixed
phrase preserves short primary fluctuations that should be replaced by
eligible phrase evidence. FR31 separately showed that a generic primary
A-B-A rule introduces contextual regressions in both frozen packages. FR44
must not restore either broad behavior.

The current case differs from those rejected forms in four independent ways:
the phrase-selected identity is the one-character middle base run rather than
an outer identity; both outer runs have the same identity; the session and raw
robust rankings agree but only the session view is eligible; and the phrase
crosses exactly two ordered VAD records instead of lying in one containing VAD.
These conditions provide a representation-level boundary without source text,
known names, timestamps, fitted constants, or a new threshold.

## FR44 candidate contract

FR44 may make a generic punctuation phrase abstain only when every condition
below holds:

1. The phrase duration uses the existing regular gate and does not exceed the
   existing punctuation-phrase maximum.
2. The session view is score- and margin-eligible. The robust gallery is
   complete, has the same unique raw top identity, passes the existing regular
   margin, and abstains only because it fails the existing regular score gate.
3. Exactly two positive VAD records overlap the phrase. They are ordered and
   non-overlapping, the phrase begins inside the first and ends inside the
   second, and neither VAD contains the complete phrase. Both VAD galleries
   have unique raw rankings: the first selects the outer base identity and the
   second selects the phrase-selected identity under the existing margin gate.
4. The phrase source is reconstructed exactly and has exactly three contiguous
   known base-identity runs `A-B-A`. The outer identities are equal, the middle
   identity is the phrase-selected identity, and the middle run contains
   exactly one visible source character. The configured punctuation set must
   be present so that the character class can be established.
5. The source character immediately before, inside, and after the middle run
   has positive alignment and typed primary tie-break/refinement provenance.
   Unique primary segments at their alignment midpoints form the same ordered
   `A-B-A` sequence. The middle primary segment meets the existing configured
   primary-consensus duration.
6. Each outer source run has at least the existing configured primary-consensus
   duration in positive aligned time. The outer identity has at least that
   much activity and primary coverage on each outer run's aligned intervals.
7. Missing or duplicate rankings, score ties, gallery disagreement, a third
   identity, unknown source identity, a multi-character middle run, missing or
   zero-duration adjacent alignment, incomplete primary/activity coverage,
   overlapping or reversed VAD records, a containing VAD, missing punctuation
   configuration, or any gate failure preserves the existing projector
   behavior.

The rule only prevents the generic phrase write; it does not assign a new
identity, change a source boundary, synthesize text, or override any later
specialized rule. It adds no TOML value and changes no existing value.

## Validation protocol

Focused tests must cover the retained two-run guard and the existing
subminimum outer-selected A-B-A behavior, one strict FR44 positive case, and
independent abstentions for every new ranking, run, alignment, primary,
activity, and VAD condition. A warning-clean build and complete CTest pass
verify engineering contracts only.

The clean projector must replay both frozen T123 and T111 packages at least
twice. Automation may verify hashes, determinism, source reconstruction, time
order, and display the complete mechanical change scope. Every changed
conversation must then be read chronologically and in reverse against
`test.txt`. That complete contextual semantic review alone decides whether
FR44 is retained or removed and whether the manual ledger changes.
