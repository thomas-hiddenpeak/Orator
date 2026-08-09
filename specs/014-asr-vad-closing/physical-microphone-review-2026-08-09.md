# Spec 014 Physical Microphone Review - 2026-08-09

## Scope and claim boundary

This report records the physical-input evidence available on the current Jetson
Thor host at clean documentation commit `d2ac5d1`. It covers hardware discovery,
direct capture probes, one real-Chromium room-tone session through the production
WebSocket, complete contextual review of that session, and non-Chromium browser
availability.

The host exposes an ALSA/PipeWire capture endpoint, but the available evidence
does not establish that a working microphone transducer delivers speech to it.
Accordingly, this report does not claim coverage of short speech, continuous
speech, pauses, interruption, overlap, or ordinary voiced background noise.
Those Spec 014 requirements remain open. A fake browser device is not accepted
as a substitute for physical-microphone evidence.

Automation captured audio, events, terminal state, mechanical contracts, and
hashes. The reviewer directly read the complete event stream in chronological
and reverse order and inspected the waveform and browser state. No code assigned
the hallucination, endpoint, ASR, speaker, or product conclusion.

## Environment and hardware provenance

| Item | Evidence |
|---|---|
| Host | NVIDIA Jetson AGX Thor developer kit, aarch64 |
| OS | Ubuntu 24.04.4 LTS, kernel `6.8.12-1021-tegra` |
| Audio service | PipeWire 1.0.5 |
| Browser | Chromium 148.0.7778.0 from the Playwright cache |
| Browser launch | Microphone permission granted; fake-device flags absent |
| Capture source | PipeWire node 46, `Built-in Audio Analog Stereo` |
| ALSA identity | Card 1, `NVIDIA Jetson Thor AGX APE`, `front:1`, capture |
| Source format | 48 kHz, stereo, signed 16-bit PCM |
| Other inputs | No USB or other physical audio source enumerated |
| Runtime model | Streaming Sortformer v2.1, frozen FR50 profile |

Chromium enumerated `default` and `Built-in Audio Analog Stereo` as its two
audio-input choices. The named choice resolves to the same board-level APE
analog source shown by PipeWire. Device enumeration proves that a capture node
is readable; it does not prove that a microphone is physically connected.

## Hardware availability probes

The reviewer first inspected a 9.941-second direct `pw-record` capture from node
46. Its two-channel waveform contains a startup transient followed by no
sustained speech-shaped activity. The capture was made in a room-tone context
without deliberate speech.

A second 15.936-second direct capture ran while the known 12-second canonical
opening WAV was played through PipeWire sink 45 on the same APE card. The
recorded waveform again contains only a startup transient and one small isolated
impulse; it does not contain the sustained activity visible in the played
source. This probe cannot distinguish a missing capture transducer from a
missing acoustic playback path, but it establishes that this host cannot
currently deliver controlled speech into the enumerated capture source.

| Probe artifact | SHA-256 |
|---|---|
| 9.941 s direct capture | `c0f1e90d2cb7b961c703da1055d819cf535d0a171c4e93fda91d15443ada3cef` |
| Direct-capture waveform | `420604bdc1ab208d18bdcb3974ea8ad4553cce215fc766c9265b0e7a2dce03b6` |
| Direct-capture audio statistics | `a9369525e06596540081f39b3bc6beb5b664e27276515b83d632c53a1cba80b1` |
| 15.936 s playback/capture probe | `0d153c5c364f1bb75ea046bd91fc7dcb51afa4bf14f88026d2e0b5b8a13881e7` |
| Playback/capture waveform | `dfdc64deb45a91ac8d8dc1bc2939f5a7ff919770e1aee67b045734932f92cee1` |
| Playback/capture audio statistics | `5f3124caef8e4a5adfe283a9def5f772cdf7790166deb0a84ea856bba5fda46e` |
| Known 12-second playback source | `a0b3322a2a90f956bf7d381edc4ad407b911f0f1c42a4ae2b7a0c5bc25b5eebf` |

These measurements describe the input path only. They do not evaluate Orator
accuracy or establish a product verdict.

## Real-browser production session

The real Chromium page requested the physical browser input, streamed it as
16 kHz mono PCM to `orator_ws`, and sent direct End after 30.211 seconds of
browser capture. The runtime accepted 29.3475 seconds, or 469,560 common-clock
samples. The complete TOML copy differs from the checked-in configuration only
in isolated server ports, storage, registry, and WebSocket log paths.

### Test summary

| Item | Content |
|---|---|
| Test type | Real Chromium physical-input room-tone streaming |
| Input audio | Board analog capture endpoint; no deliberate speech |
| Reference text | Test procedure: room tone only; `test.txt` is not applicable |
| Run result | Success for transport and terminal mechanics |
| Wall time | 30.211 s browser capture |
| Source extent | 29.3475 s / 469,560 samples |
| Stream RTF | Not separately emitted by the browser; terminal `wall_clock_ok=true` |
| ASR RTF | Not meaningful; no decoder invocation recorded |
| Diar RTF | 45.240x from 0.649 s compute |
| Subjective conclusion | No speech assertion appears in this reviewed room-tone context; active-speech microphone coverage remains unavailable |

Every active track reconciles to 469,560 samples with zero declared gap;
`wall_clock_ok`, `timebase_ok`, and `timebase_reconciled` are true. Browser
capture and terminal rendering report no console or page error. The desktop
screenshot shows a coherent 29.3-second terminal state, empty Live and speaker
regions, device telemetry, and no overlapping or clipped control.

### Complete contextual review

| Time span | Reference context | System output context | ASR semantic | Speaker evaluation | Issues |
|---|---|---|---|---|---|
| 00:00-00:29.3475 | Physical endpoint room tone; no deliberate speech | Initial `vad_state` is false; cursor/telemetry and empty diar updates continue until End; terminal ASR, VAD, diar, align, voiceprint, business-speaker, and comprehensive tracks are empty | Not applicable; no speech | Not applicable; no speaker | The source has no proven active microphone signal, so this row cannot validate speech capture or endpointing |

The reviewer read all 89 WebSocket log lines from ready through terminal and
speaker refresh, then read the same lines in reverse from the terminal context
back to ready. The event stream contains no ASR partial, final, or retract; no
VAD speech transition or speech segment; no speaker identity; and no
comprehensive contribution. Three diar publications near the end explicitly
contain empty segment arrays. The terminal document and visible browser state
remain consistent with that event history.

In the known no-deliberate-speech context, the reviewer finds no substantive
speech assertion or hallucinated meaning. This is a direct contextual judgment,
not a conclusion produced from empty-array counts or audio statistics. There is
no spoken reference in this session, so ASR semantic and diarization accuracy
are not scored.

### Artifacts

| Artifact | SHA-256 |
|---|---|
| Candidate TOML | `67dc10da1224f33f28cbb2ec3e9059e972028426d08b9db5666dcfec3d02412b` |
| Server binary | `7aa4a37898c895e81fd0d653350c593c4f17517c0c93d5b5377020038060b6d7` |
| Physical raw capture | `46089f563c10eec9a6692d68c87041475bb701e75e663a673e450e3a93a431e3` |
| Physical raw waveform | `b62d47d0e4902d84acf33edb40eae60cd4b005a3b48ebe6070f04cdafd9ce342` |
| Physical raw audio statistics | `57bfdec62b84408e29a384d055721320b396b2578283443f36247fae91b902d5` |
| WebSocket event log | `2ceae04b4bdc98bc3f2b6046a7b13d40540cbae493d82a7573cb6fd441e1f1a2` |
| Terminal JSON | `a54b161fbc0a123074a3033a46ab58d9fbdba151e177e94bad124d8da66ecdb2` |
| Desktop screenshot | `a9a7888ad8a0b8089e0c3ee2da13b4c7cd49f9f3a9d66c294934fa0aefb11498` |
| Browser log | `3126949c9f4a7146656209e17e98e42102e7f126510dbaefe8f28a921667ee9d` |
| Server log | `f70234cd7858347a53029d69567ebfda68ead807d5d9773ca7085f7a80a4327d` |

Raw evidence remains under gitignored
`artifacts/spec014/microphone/physical-room-tone-d2ac5d1/`.

## Non-Chromium availability

The Ubuntu `firefox` package is only a launcher that requests an uninstalled
Firefox snap. No snap is installed, and the Playwright cache contains no
Firefox binary. The host has the WebKitGTK shared library but no
`WebKitWebDriver`, `MiniBrowser`, or Playwright WebKit executable. Safari is not
available on Linux. Therefore no Firefox or Safari/WebKit behavioral result is
claimed; this is the explicit target-environment limitation required by T044.

## Engineering validation

After the evidence and SDD update, the existing build completes without an
error. The complete registered CTest suite passes all `74/74` entries in
`52.80 s`, including the JavaScript model and real-WebSocket contract tests.
This engineering result does not change the contextual product conclusions or
complete the unavailable voiced microphone scenarios.

## Conclusions and execution decision

- **Room-tone hallucination**: pass for this one physical-endpoint session by
  complete contextual review; no substantive speech assertion is present.
- **ASR semantic accuracy**: not applicable because the session contains no
  reviewed speech.
- **Diarization accuracy**: not applicable because the session contains no
  reviewed speaker turn.
- **Physical-microphone requirement**: incomplete. Short speech, continuous
  speech, pauses, interruption, overlap, and voiced background noise remain
  untested because no effective microphone signal is available.
- **Browser compatibility record**: complete for the available environment;
  Chromium is exercised, while Firefox and Safari/WebKit are unavailable as
  documented above.
- **Phase decision**: do not start full-candidate acceptance. Preserve this
  evidence, leave T042/T043 open, resume the next ASR semantic defect class, and
  repeat the remaining physical-microphone cases when functional capture
  hardware is attached.
