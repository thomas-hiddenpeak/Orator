# Speaker Policy Active-Surface Evidence - 2026-07-17

## Scope

This T095 checkpoint consolidates duplicate guards inside the already accepted
speaker fusion policy and separates production validation from historical
candidate experiments. It changes no model, threshold, TOML production value,
challenge order, abstention behavior, decision reason/source, typed track, or
serialized business output.

## Policy Consolidation

Four identical local top-two voiceprint ranking implementations now use one
shared `ranked_pair` guard. Seven identical forced-alignment minimum-unit loops
now use one `has_minimum_aligned_units` guard. Challenge-specific VAD, activity,
primary-speaker, topology, and duration conditions remain local because their
differences are business-significant.

`speaker_fusion_policy.cc` decreased from 3,078 to 2,972 lines. No challenge
function or invocation was removed.

## Active Surface

- Forty-three standalone candidate generators and 43 matching tests moved to
  explicit, inactive historical archive directories.
- Fifty-one non-production TOML profiles moved under the Spec 013 configuration
  archive. The root `orator.toml` remains the sole production speaker profile.
- The Sortformer high/low profiles remain active only as same-checkpoint
  numerical-oracle fixtures.
- Thirty-three historical Python experiment tests were removed from CTest.
  The active suite retains the complete C++ production fusion/abstention test
  and seven Python speaker tests for evidence integrity, replay inputs,
  provenance, and unjudged review-packet construction.
- The robust-gallery evidence tool now owns its JSON and SHA-256 helpers instead
  of importing an archived posterior candidate.
- Two Python programs that calculated speaker-attribution/identification results
  and the compiled reference-projection harness were removed. Their historical
  implementations remain available through Git history only.

## Frozen Equivalence

Before T095, the retained 3615.120-second typed diarization, primary, ASR,
alignment, and voiceprint tracks emitted 1,775 business entries with SHA-256:

```text
04ba82a844a14edb08b3cce60a543e831dfd6bb1e1368d58440303f6f2251db9
```

The post-consolidation and post-archive replays are byte-identical to that
baseline; `cmp` returned zero. This check establishes mechanical equivalence
only and does not judge speaker correctness or produce an accuracy result.

## Engineering Verification

- The focused speaker evidence, business fusion, and typed-flow tests passed
  3/3 after guard consolidation.
- The final build completed without compiler warnings or errors. GCC emitted
  its existing ABI notes only.
- The complete active suite passed 68/68. An initial archive run exposed one
  robust-gallery import of an archived candidate; the two required evidence
  helpers were moved into the active evidence tool before the clean full run.
- A new-binary real-WebSocket run streamed the first 120 seconds of `test.mp3`
  at 1.0x using an isolated TOML, registry, and storage directory. It completed
  in 120.808 seconds (`0.993x`) with no mechanical contract issues.
- Producer, early observer, and late observer terminal SHA-256 values all equal
  `b332b4f622bae8d780d42d98d92a051e1747d68c7dd7f63eb180133f934e958a`.
- GPU utilization, GPU memory, system power, CPU, RAM, and temperature each had
  100 percent required-field coverage. Runtime cadence was 95.83 percent and
  `tegrastats` cadence was 100 percent.
- Source workspace, temporary TOML, and server binary remained stable during
  the run. The accepted production speaker registry was not read or modified.

The accepted contextual speaker-business review and its claim boundary remain
unchanged. No automated mechanism assigned correctness, aggregated accuracy,
ranked a candidate, selected a parameter, or issued a product verdict here.
