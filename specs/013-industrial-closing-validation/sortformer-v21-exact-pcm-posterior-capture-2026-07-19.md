# Sortformer v2.1 Exact-PCM Posterior Capture (2026-07-19)

## Scope and governance

This T191 record reconstructs the complete four-channel Sortformer v2.1 frame
posterior underlying frozen T123. It validates evidence provenance only. The
capture does not read `test.txt`, name an expected speaker, assign correctness,
aggregate product accuracy, rank a channel or candidate, select a parameter, or
issue a product verdict.

Automation only runs the frozen model, serializes raw values, checks hashes and
shape, and compares the model's mechanical top-1 compression with an immutable
producer track. No output in this record authorizes a speaker write.

## Immutable inputs

| Input | SHA-256 |
|---|---|
| T123 exact streamed PCM WAV | `a60af82d57958ddfaf5d0358820adc7536544c97d59aab9f55a3b2f53694a16e` |
| Frozen T123 TOML | `6a92c582a2cba7e26542f38a60516bb53929dc5d109ba331bbf4b5b614eb9b22` |
| Sortformer v2.1 weights | `d036020b6b93977098929d417b1b106a952ec02cc38cafc9d3315ae0ec4d90b8` |
| Diagnostic probe binary | `276ba9ad12a6d08fcea7f75562f4464a8bddccce976f7d5a48dd6e9b70f463a8` |
| Frozen T123 primary producer | retained by T123 fixture manifest |

Both independent executions load all behavioral values from the frozen TOML:
`spkcache_len=188`, `chunk_len=340`, `spkcache_update_period=188`,
`chunk_left_context=1`, `chunk_right_context=1`, `fifo_len=188`, and
`reset_period_sec=0`. The input contains 57,841,920 samples at 16 kHz and has a
common-clock duration of `3615.12 s`.

## Repeated raw capture

Each execution emits 45,189 rows with four sigmoid probabilities per row. The
first frame starts at `0.000000 s`, the period is `0.08 s`, and the final frame
starts at `3615.039919 s`; its end reaches the frozen audio extent within the
serialized floating-point clock precision. Both executions use one continuous
Sortformer session and emit the same 755 onset/offset diagnostic segments.

| Repeated artifact | Run A SHA-256 | Run B SHA-256 |
|---|---|---|
| Four-channel frame CSV | `79fd2c416ac76a0af477f98bf8d848f6e604b2d94c5c4445e653978afd6c7e41` | `79fd2c416ac76a0af477f98bf8d848f6e604b2d94c5c4445e653978afd6c7e41` |
| Onset/offset segment CSV | `94a2a758ef9771e1646d27eb56a6257421ae620870e3bf467eb9fed9976264c0` | `94a2a758ef9771e1646d27eb56a6257421ae620870e3bf467eb9fed9976264c0` |

The repeated files are byte-identical. Compute time was `37.2632 s` and
`36.7804 s`; these component timings are not streaming performance results.

## Primary-producer reproduction

For the mechanical provenance check, each raw row retains the highest of its
four channel values only when that value meets the existing frozen
`speaker_fusion.frame_activity_threshold=0.5`. Consecutive eligible rows are
coalesced only when their local channel and frame sequence remain identical,
matching `SpeakerEvidenceStage::BuildPrimarySpeaker`. The comparison removes
global identity strings and rounds both displays to the raw probe's six-decimal
serialization precision; it does not compare with a reference speaker.

The raw capture and frozen T123 producer each contain 1,348 positive-duration
runs. All local-channel indices and run ordering are identical. Start, end, and
mean-probability differences are each at most `0.000001`, with no value beyond
that serialization tolerance. The raw-derived display has SHA-256
`f15cdafcbcdecd3555b80abe75f77cf6aa79254aa76edbfbc14259493052ea34`;
the separately normalized frozen display has SHA-256
`5295883970e67ef4e4a736f29307527b3788345fbac54690ebda00f3be06ff71`.
Their hashes differ because last-digit decimal rounding differs on some mean
probabilities and clock values; the field-level comparison records zero local
slot differences and zero values beyond the stated precision.

This establishes that the reconstructed four-channel posterior is the
capture-faithful source of T123's frozen top-1 primary track. It authorizes T192
raw evidence display only; no speaker-policy or accuracy conclusion follows.
