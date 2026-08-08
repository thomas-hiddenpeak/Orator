# Spec 014 Digital-Silence Review (2026-08-09)

## Scope and authority

This report completes T020-T021 on clean commit
`f1d0e054524b70df1445ade5025b8a1b4ffea146`. It covers three independent
30-second digital-silence sessions through the production WebSocket at `1.0x`.
Each session used a separate process, port pair, empty speaker registry,
storage tree, observer set, terminal document, and provenance manifest.

The reviewer directly read every application event and every terminal track in
Run A, then independently repeated that reading for Run B and Run C. No code,
script, query, formula, count, metric, or algorithm decided whether output was
a hallucination or issued the conclusion below. Automation generated the
all-zero PCM fixture, captured raw evidence, checked mechanical contracts, and
recorded hashes only.

## Frozen fixture and behavior

| Item | Value |
|---|---|
| Commit | `f1d0e054524b70df1445ade5025b8a1b4ffea146`, clean |
| Runtime anchor | unchanged from `1417334` |
| Fixture | signed 16-bit mono PCM, 16 kHz, exactly 30 seconds, every sample zero |
| Fixture extent | `480,000` samples / `960,000` bytes |
| Fixture SHA-256 | `b9163d03c43083a18e6101539b555cb5e363eed61fa4b3a3b54f50ae60eb5b52` |
| Checked-in behavior | unchanged `orator.toml` |
| Server binary SHA-256 | `222e5b55e6e5ea62a1ea7600d676616e044af93c0b22bdba0fd7b9d0a3cbdc84` |
| Transport | production WebSocket, 100 ms frames, `1.0x`, direct `end` |
| Observers | early, transient disconnect, and late join per run |
| Telemetry | required runtime fields plus continuous `tegrastats` |

Each private TOML differs from the checked-in file only in the WebSocket/UI
ports and isolated registry/storage/session paths. All VAD, ASR, endpoint,
speaker, fusion, model, scheduling, and telemetry values remain unchanged.

One preliminary server-only launch exposed a malformed generated port before
any client connected or audio was sent. It was stopped, all three TOMLs and
manifests were regenerated, and it is not one of the sessions below.

## Mechanical capture record

| Fact | Run A | Run B | Run C |
|---|---:|---:|---:|
| Raw artifact SHA-256 | `1ce8dd3d4bc2a7565aa88c7bfdc83f34ac2b235296748c1d56cf21a68a62c652` | `939441bc464f2d2c60ec20fb0ce8a4bcb024c3335f8ec60d6cfe40fc4e73fbc2` | `580e1f05561ba275fcdb2080534c78e4b3f6f7699575ce8b96f8c13cc4fc1da0` |
| Pre-run manifest SHA-256 | `7c9ddbb90bb206426358e1cf1d7822eae9dbbe0116215ba2755da154cc6c1198` | `6a98c5982c7206a677937f53242f00ad7cd4b02e861213757ba39b436f2f6d02` | `8b13bd3e3101c7863bf5d1dede0a1b1532929bdd5dd9f60b43cfabbbb8245773` |
| Stream factor | `0.991x` | `0.991x` | `0.991x` |
| Direct-end wait | `0.275 s` | `0.265 s` | `0.269 s` |
| Runtime / tegrastats samples | `30 / 30` | `29 / 30` | `29 / 30` |
| Runtime / tegrastats cadence | `100% / 100%` | `96.667% / 100%` | `96.667% / 100%` |
| Required telemetry-field coverage | `100%` | `100%` | `100%` |
| Mechanical issue list | empty | empty | empty |
| Observer terminal convergence | exact | exact | exact |

In every run, input, diarization, speaker identity, ASR, VAD, alignment, and
business speaker close at exactly 480,000 samples with zero extent gap. The
time-base, wall-clock, reconciliation, provenance, terminal-latency, observer,
and telemetry contracts pass. These facts did not make the hallucination
judgment.

The displayed ASR real-time factor is extremely large because ASR compute time
is effectively zero when no audio is admitted by VAD. It is a division artifact
and is not used as product or performance evidence.

## Direct event and terminal review

### Run A

The complete application-event sequence is one `vad_state` event with
`speech=false`, followed by four diarization publications whose `segments`
arrays are empty. None contains transcript text. There is no `asr_partial`,
`asr_retract`, finalized `asr`, alignment, revision, or speech-positive VAD
event.

The terminal document contains empty diarization, ASR, VAD, alignment,
speaker-voiceprint, and business-speaker entry arrays, plus an empty
comprehensive view. Reading the event stream and terminal document together,
the output makes no assertion that speech occurred and contains no substantive
transcript.

### Run B

Run B was read independently. Its complete application-event sequence is again
one `vad_state` event with `speech=false` and four empty diarization
publications. No event contains words or an asserted speech interval. Its
terminal document independently contains empty entry arrays for every product
track and an empty comprehensive view. The output makes no assertion that
speech occurred and contains no substantive transcript.

### Run C

Run C was read independently after Run B. Its complete event sequence and
terminal document have the same contextual interpretation: `speech=false`,
four empty diarization publications, no transcript-bearing event, no asserted
speech span, empty product tracks, and an empty comprehensive view. The output
makes no assertion that speech occurred and contains no substantive transcript.

## Manual conclusion and boundary

All three independently reviewed digital-silence sessions satisfy the Spec 014
and Spec 013 digital-silence requirement: each contains zero substantive final
transcripts and no live hallucination content. This conclusion comes from
direct reading of every event and terminal document, not from the empty-entry
counts or an integration assertion.

This closes only the current-config generated digital-silence gate. It does not
establish behavior for microphone self-noise, room tone, ventilation, distant
speech, music, impulsive noise, or ordinary background sound. Those inputs
remain Phase 4 physical-microphone requirements and must receive their own
contextual review.

T020-T021 are complete. T022-T026 remain open; the next authorized operation is
one clean full-length current-config `test.mp3` baseline followed by complete
chronological and reverse contextual review before any behavior change.
