# Speaker Policy Maintainability Evidence - 2026-07-17

## Scope

This checkpoint changes ownership only. It separates the accepted speaker
fusion policy from `BusinessSpeakerPipeline` orchestration without changing the
public pipeline API, typed timeline tracks, TOML surface, policy order,
thresholds, decision reasons/sources, projected ranges, or serialized output.
No model, runtime dependency, reference-specific branch, or product-accuracy
rule was added.

`BusinessSpeakerPipeline` remains responsible for subscriptions, typed evidence
collection, revisions, and publishing the `business_speaker` track. The new
internal `SpeakerFusionPolicy` owns the accepted fusion-rule execution. Shared
interval and UTF-8 helpers live in one private implementation header. The main
pipeline implementation decreased from 4,189 to 1,093 lines; the extracted
policy is 3,078 lines and remains deliberately unchanged pending a separate
exact-equivalence consolidation.

## Frozen Equivalence Gate

Before extraction, `business_speaker_replay_probe` replayed the retained full
3615.120-second typed diarization, primary-diarization, ASR, alignment, and
voiceprint tracks with the checked-in `orator.toml`. It read 755 diarization,
1,348 primary, 275 ASR, 275 alignment, and 16,093 voiceprint records and emitted
1,775 business entries.

The pre- and post-extraction JSON files are byte-identical. Both have SHA-256:

```text
04ba82a844a14edb08b3cce60a543e831dfd6bb1e1368d58440303f6f2251db9
```

`cmp` returned zero. This is a mechanical behavior-equivalence gate only. It
does not assign correctness, calculate accuracy, rank a candidate, or issue an
acceptance verdict. The previously accepted full-context semantic review is
unchanged because the complete serialized projector output is unchanged.

## Engineering Verification

- A complete build finished without compiler warnings or errors.
- Focused `test_speaker_evidence_stage`,
  `test_business_speaker_pipeline`, and `test_typed_evidence_flow` passed 3/3.
- The complete configured suite passed 101/101.
- A new-binary real-WebSocket run streamed the first 120 seconds of
  `test.mp3` at 1.0x through an isolated temporary TOML and speaker registry.
  It completed in 120.808 seconds with no mechanical contract issues.
- The early observer, late observer, and producer received an identical
  terminal timeline SHA-256. The competing producer was rejected as required.
- Runtime GPU utilization, GPU memory use, system power, CPU, RAM, and
  temperature each had 100 percent required-field coverage; runtime and
  `tegrastats` cadence satisfied the 95 percent gate.
- The config, source workspace, and server binary remained unchanged during
  the run. The production speaker registry was not read or modified.

## Remaining Maintainability Work

Rule consolidation and removal of rejected one-off candidate tools/configs are
not part of this checkpoint. They require a separate frozen-output equivalence
change so that ownership cleanup cannot be confused with a speaker-business
accuracy modification.
