# Speaker-Decision Audit Contract - 2026-07-15

## Scope

This record closes T071 only. It adds reference-free evidence to every
`business_speaker` attribution without changing the selected speaker, support
classification, uncertainty, text projection, raw track, or checked-in TOML.
It is not an accuracy result and does not replace the unsigned 556-row audible
ledger.

## Frozen-Evidence Finding

The source-stable v2.1 full artifact from clean commit `3b40245` retains 755 raw
diarization segments. It contains 445 cross-speaker overlap pairs whose overlap
union covers 378.64 seconds. Of 935 terminal business entries, 347 overlap at
least one rejected diar candidate; 336 of those entries were still summarized
as strong support, 154 had a zero selected-versus-best-alternative overlap
margin, and 249 selected a candidate whose confidence was below an overlapping
alternative. These are auditability findings, not proof that another selection
policy is more accurate.

The previous scalar support fields described only the selected candidate and
the union of any diar evidence. They could not expose a rejected candidate or
the policy that resolved an overlap. The runtime now emits a structured
`speaker_decision` with source, text-projection source, reason, all candidates,
union overlap, coverage, overlap-weighted confidence, island count, selected
flag, and selected-versus-best-rejected margins. The existing attribution rule
is unchanged.

## Verification

The complete configured test suite passed 64/64. A real 120-second, 1x
WebSocket run used the checked-in v2.1 `orator.toml` without a behavior
override and completed in 120.801 seconds. It produced 23 diarization, 10 ASR,
39 VAD, 10 forced-alignment, and 28 business entries. All 28 business entries
contained `speaker_decision`; their reasons were 12 sole-support, nine
competing-support, six no-support, and one same-speaker gap-fill decisions.

After removing only `speaker_decision`, the five terminal tracks,
`comprehensive` alias, track extents, and audio duration were exactly equal to
the previously frozen
`closing-v21-3b40245/repeatability-120/current-binary-120s-01.json` artifact.
The new run retained zero mechanical issues, exact early/late-observer terminal
hashes, 114 runtime telemetry samples, 120 `tegrastats` samples, and at least 95
percent coverage for every required field and cadence. Its pre/post source,
config, and binary hashes were unchanged; the manifest records the complete
dirty-path set because this was the pre-commit validation run.

A separate real Chromium run streamed a 12-second prefix and verified five
structured decisions in the rendered terminal document. Rendered terminal,
downloaded JSON, and persisted-session reload were exactly equal. Reconnect,
clean browser-session reset, fake-device microphone start/stop, telemetry,
desktop rendering, and 390-pixel mobile rendering passed without unexpected
browser or console errors. Visual inspection found no overlap or horizontal
overflow.

## Evidence

| Artifact | SHA-256 |
|---|---|
| `/tmp/orator-spec013/speaker-decision-audit-886613c/120s-01/orator-speaker-decision-audit-120s-01.json` | `bb53179ecfdf5ba7d99da32ce7c7db6ee989669210439cc0cd7965fce0886549` |
| Per-run manifest | `b71492e02993b1812eb462e0df13db92f4b9e5d3b349d8f49ccac30711f9f0b8` |
| Desktop screenshot | `a1df9e2d676a45cede262280f01bc55bdfb7e9939b31186662a7e4dcc4c2aa62` |
| Mobile screenshot | `1d56f490806ccad8b04454e26a7046c2e766ab0d70d9b69f01152b4118ae5526` |

## Remaining Gates

T072 remains open for an attribution-changing fusion candidate, and T073 still
requires physical-microphone evidence. No 360-second, 600-second, full-length,
or signed contextual accuracy gate follows from this attribution-neutral run.
