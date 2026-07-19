# FR35 Aligned-Unit Isolation Tolerance Diagnosis (2026-07-19)

## Scope and authority

This frozen-evidence diagnosis addresses the `ref-0420` speaker-attribution
regression between T111 and T123. It does not score ASR text and does not run a
new audio path. `test/data/reference/test.txt` remains the authoritative
human-listened reference, and the completed full contextual review identifies
the isolated `嗯` as Zhu Jie's contribution between Tang Yunfeng's preceding
instruction and Shi Yi's following question.

No program, script, query, formula, score, or interval operation assigns
correctness, aggregates accuracy, selects FR35, or issues a product verdict.
Shell tools only display frozen typed evidence on the common source clock. A
complete forward and reverse contextual reading decides whether the resulting
candidate is retained.

## Frozen evidence

The Sortformer activity, independent primary track, VAD interval, identity
epochs, and checked-in TOML are identical in the T111 and T123 frozen producer
packages around the contribution:

- activity local slot `1` carries the current `spk_1` identity across the
  region, while its initial identity is `spk_0`;
- primary local slot `1` covers the isolated response with current identity
  `spk_1`;
- the unique containing VAD is `2809.380-2810.684`; both session and robust
  galleries select initial identity `spk_0` under the existing short-evidence
  score and margin gates;
- the aligned response has no independent embedding and is shorter than the
  existing `speaker_fusion.min_embed_sec = 0.4` floor.

Only the ASR/forced-alignment partition changes the isolation geometry:

| Frozen package | Exact aligned unit | Previous gap | Following gap | Result before FR35 |
|---|---:|---:|---:|---|
| T111 | `2809.460-2809.620` `嗯` | greater than `0.25 s` | `0.320 s` | initial-slot VAD rule applies |
| T123 | `2809.436-2809.676` `嗯，` | `0.720 s` | `0.240 s` | rule abstains by `0.010 s` |

The checked-in values are `timeline.align_snap_pause_sec = 0.25` and
`timeline.align_boundary_split_tolerance_sec = 0.08`. T123 extends the aligned
unit through punctuation and advances the following unit within the already
declared boundary tolerance. Treating that bounded alignment jitter as a real
loss of acoustic isolation makes the final business view depend on ASR
partition punctuation rather than the common-clock evidence.

## FR35 contract

FR35 changes only the two aligned-neighbour isolation checks inside the
existing `isolated_subminimum_unit_vad_challenge` rule. A preceding or following
gap satisfies the pause contract when it reaches
`timeline.align_snap_pause_sec` after adding at most the existing
`timeline.align_boundary_split_tolerance_sec`. All other existing conditions
remain conjunctive and unchanged:

1. the target is one positive-duration aligned unit without an embedding and
   below the existing minimum embedding duration;
2. all current source labels are native, uniform, and not already written by
   voiceprint evidence;
3. one uncontested activity local slot covers the unit, and that slot has a
   different non-empty initial identity;
4. exactly one current-identity primary segment covers the unit;
5. exactly one complete-gallery VAD contains the unit; and
6. both VAD galleries independently pass the existing gates and select the
   same initial identity.

FR35 adds no threshold, model, identity, transcript, timestamp, reference
lookup, or command-line override. It changes no TOML value. The existing TOML
values remain the sole runtime parameters for pause and boundary tolerance.

## Verification and decision boundary

Focused tests must cover the bounded-tolerance positive case, a gap outside the
tolerance, zero tolerance, missing identity epoch, competing or mismatched
native evidence, weak/disagreeing VAD, and an independently embedded unit.
After a warning-clean build and complete CTest pass, the production C++
projector replays the frozen T123 and T111 packages at least twice.

Automation may verify immutable input hashes, deterministic output hashes,
source reconstruction, monotonic time, and raw change scope only. Every changed
complete conversation is then read chronologically and in reverse against
`test.txt`. That contextual semantic review alone retains or removes FR35 and
updates any manually derived ledger. A retained frozen replay is not a new
real-WebSocket result and cannot close the speaker business by itself.
