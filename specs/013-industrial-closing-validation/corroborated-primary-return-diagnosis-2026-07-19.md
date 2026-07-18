# Corroborated Primary-Return Diagnosis (2026-07-19)

## Scope and authority

This diagnosis uses the already completed T111 and T123 full-session contextual
reviews and their frozen typed tracks. `test/data/reference/test.txt` remains
the human-listened authority. No code, script, query, formula, metric, model
score, or interval calculation assigned correctness, aggregated accuracy,
selected a candidate, or issued a product verdict. Shell tools only displayed
immutable evidence and decision provenance.

## Uniform regression split

Complete manual comparison of the corrected T111 and T123 ledgers identifies
sixteen T123-only speaker errors. Three have no recognizable candidate entry in
the reference window before forced alignment: `ref-0063`, `ref-0409`, and
`ref-0432`. Thirteen retain recognizable text but change identity or a material
speaker boundary: `ref-0025`, `ref-0066`, `ref-0071`, `ref-0154`, `ref-0171`,
`ref-0192`, `ref-0194`, `ref-0268`, `ref-0350`, `ref-0406`, `ref-0420`,
`ref-0478`, and `ref-0517`.

The frozen T111 and T123 activity-diarization and primary-speaker tracks are
byte-identical. Their ASR, forced-alignment, ASR-derived voiceprint, and final
business tracks differ. Replaying T111 typed inputs through the current
projector preserves its reference-interval speaker sequences; replaying T123
inputs reproduces its final view apart from sub-microsecond serialization.
This bounds the regression to changed producer evidence interacting with final
projection, not a session-wide random projector change.

## Native return evidence

The speaker identity map is `spk_0=朱杰`, `spk_1=唐云峰`, `spk_2=徐子景`, and
`spk_3=石一`.

Several present-text contexts contain a short primary A-B-A return with matching
B activity on the common clock:

| Context | Immutable evidence | Final projection observation |
|---|---|---|
| `ref-0066` | Primary changes Shi-Tang-Shi at `478.64/479.12 s`; Tang activity covers the return | T123 alignment leaves no complete source character inside the return, so the candidate must abstain |
| `ref-0071` | Primary changes Tang-Shi-Tang at `492.64/493.60 s`; Shi activity covers the return | Base primary-refined text exists, then an ordinary phrase write assigns the complete phrase to Tang |
| `ref-0154` | Primary changes Shi-Tang-Shi across `1007.44-1008.16 s`; Tang activity covers that run | Base primary-refined characters exist, then an ordinary phrase write repaints part of the return as Shi |
| `ref-0171` | Alternating primary runs preserve the Tang/Shi reply boundary around `1141.76-1143.04 s`; both identities remain in activity | An ordinary phrase write extends Tang across aligned Shi characters |
| `ref-0192` | Zhu activity and primary agree across the short return near `1303.36-1304.00 s` | Forced alignment leaves the return inside a long source gap, so there is no aligned source character for FR31 to protect |
| `ref-0268` | Xu-Zhu-Xu primary evidence contains a `0.56 s` Zhu return beginning at `1932.24 s`; Zhu activity covers it | The base view assigns one aligned character to Zhu, then an ordinary phrase write repaints it as Xu |

This topology is not universal. `ref-0025`, `ref-0350`, `ref-0406`,
`ref-0420`, and `ref-0478` need voiceprint evidence because both native views do
not support the human speaker. `ref-0194` has only a terminal Xu native fragment
while the earlier T111 repair came from broader voiceprint evidence.
`ref-0517` has uniform Tang support in both native views rather than an A-B-A
return; its conflicting session-only phrase evidence is a separate abstention
hypothesis and is not part of FR31.

## Candidate boundary

FR31 protects only source characters already carrying typed primary-arbitration
provenance and positive forced-alignment time inside a short primary B run. The
nearest primary runs on both sides must carry the proposed voiceprint A
identity, and activity B must cover the complete primary return for the existing
TOML minimum duration. Ordinary phrase and complete-source writes may skip
those exact B characters; specialized challenge rules retain precedence.

The candidate does not synthesize text, infer a speaker from one native view,
move a boundary without alignment, lower a score, change TOML, or modify any
producer record. Consequently it intentionally abstains on the three missing
contributions, the alignment gaps at `ref-0066` and `ref-0192`, and all
voiceprint-dependent contexts. Frozen replay and complete manual review of
every changed conversational context determine whether this narrow evidence
rule is retained before any new audio run.
