# FR30 VAD Sensitivity Diagnosis (2026-07-19)

## Scope and authority

This report records the frozen root-cause diagnosis that follows the rejected
T123 full-session FR29 promotion. It does not promote FR30, replace T111, or
assign product accuracy. `test/data/reference/test.txt` remains the human-
listened authority. Code and shell tools were used only to replay immutable
typed inputs, verify equality and time-base contracts, display raw intervals,
and hash evidence.

No code, script, query, formula, metric, interval count, or model score assigned
correctness, ranked a threshold, calculated candidate accuracy, or issued an
acceptance verdict. The two source contexts below were selected from the
already completed T123 full contextual review, then traced manually through the
frozen producer tracks.

## Frozen projection boundary

T111 and T123 Run A have byte-identical Sortformer diarization and primary-
speaker TSV exports. Their ASR, forced-alignment, and derived voiceprint tracks
differ. The current `business_speaker_replay_probe` was then run against both
frozen typed-input sets with the checked-in projector and TOML policy:

| Replay | Output SHA-256 | Mechanical observation |
|---|---|---|
| T123 tracks, current projector | `cce31176e46ede2ea68c248810fa3d94838205b65ac2608fc6ee1240d97fabb4` | Reproduces the 1,707-entry T123 view; differences from the captured view are confined to sub-microsecond split serialization |
| T111 tracks, current projector | `646ea91b357cafaf8af82c4f45e5cc771c622a501e71b12f2d1aa1555fb055f2` | Zero reference-interval speaker-sequence changes from the original T111 view |

This rules out a new session-wide business-projector policy regression. Given
the accepted T111 evidence, the current projector preserves its speaker
sequence. The causal difference must enter through changed producer evidence.

## First causal loss

FR28 waits for stable typed VAD evidence and feeds only decided audio to ASR.
T111 instead fed undecided audio whenever ASR happened to lead VAD. That old
behavior was scheduling-dependent, but it also happened to retain low-energy
speech that Silero did not finalize.

The production VAD probe at checked-in threshold `0.5` reproduces the relevant
T123 intervals:

| Context from completed manual review | Stable VAD evidence at threshold 0.5 | Propagation observed in frozen tracks |
|---|---|---|
| `ref-0409`, source `2752-2754 s` | Prior segment ends `2751.996`; next starts `2754.724` | T123 ASR closes at `2752.996` and omits the remainder of the contribution; alignment and business text are consequently absent |
| `ref-0503`, source inside `3278-3284 s` | Prior segment ends `3277.980`; next starts `3284.132` | T123 ASR has no audio for the intervening utterance; T111 ASR had retained it while ahead of VAD |

Sortformer independently exposes activity at `3281.040-3281.920 s`. This is a
raw cross-pipeline time-base observation, not a correctness score. It confirms
that the second VAD gap contains non-silence evidence and that another fusion-
policy exception would act after the first data loss.

The loss propagates in one direction:

`stable VAD gap -> omitted ASR audio -> changed final text boundary -> changed
forced-alignment units -> changed short-window TitaNet evidence -> changed
business revision`.

## Single-variable TOML probe

Only `vad.threshold` was changed in temporary copies of `orator.toml`. Model
weights, `min_speech_ms`, `min_silence_ms`, `speech_pad_ms`, ASR lead/trail,
ASR feed quantum, and all speaker-fusion settings remained unchanged.

| TOML threshold | Raw intervals displayed at the reviewed gaps | Probe SHA-256 |
|---:|---|---|
| `0.5` | No interval inside `2752.028-2754.724` or `3277.980-3284.132` | `e017b1b0fef9f6f80584a9f6816f31ba7ab50caf59dab89bdaeb3b7d47e5c508` |
| `0.4` | `2753.668-2754.076`, `3278.276-3278.844` | `f083971a20dd84a3e7e6a8e48d0334fab1226cfd65725874582a36aecb825828` |
| `0.3` | `2753.540-2754.140`, `3278.276-3278.908`, `3281.124-3281.628` | `4d853eb926b885fbf29c59fea41eeb40e53265f94cfa8b0515786e5cdd06fa06` |

The `0.3` output at `3281.124-3281.628 s` is contained within the independent
Sortformer activity interval. The frozen 30-second all-zero fixture produces
zero VAD segments at threshold `0.3`. These observations justify one bounded
candidate; they do not establish product correctness or threshold optimality.

## Checked-in candidate and engineering gates

The checked-in candidate changes only `orator.toml`:

`vad.threshold = 0.5 -> 0.3`

The resolved TOML SHA-256 is
`047071760aff89aafc6fc8ebf8bfd60af108af0fd4c86cc74706fc127c119f07`.
Its production full-audio VAD export is byte-identical to the temporary `0.3`
probe at SHA-256
`4d853eb926b885fbf29c59fea41eeb40e53265f94cfa8b0515786e5cdd06fa06`.
The checked-in threshold still emits zero segments for the frozen silence
fixture; that empty export has SHA-256
`cd53721021faa8c0deb0bd36679189e8c12257ac67b5a25f23c493c8a8a3e900`.

The VAD numerical test passes. A clean build has an empty warning/error scan,
and all 69 configured CTest entries pass. Build-log SHA-256 is
`89565712cc02fdfb8def2747baac32661adffbad86af3a7a183192811677177f`.
These are engineering and mechanical gates only.

## Decision boundary

FR30 is a transitional experiment, not an accepted configuration. The next
step is T131 from one clean commit: three independent real-WebSocket silence
runs and two independent 120-second real-WebSocket captures, followed by
complete chronological and reverse contextual review of every in-scope
`test.txt` contribution. A 600-second run is forbidden until T131 is manually
retained, and no full A/B run is authorized until the complete 600-second
contextual gate passes.
