# Primary-Island Alignment-Gap Evidence Diagnosis (2026-07-19)

## Scope and decision authority

This diagnosis follows retained FR44 and examines two unresolved T123
speaker-business contexts in which the independent activity and primary tracks
contain a short speaker return but forced alignment does not place a complete
source character on that return: `ref-0066` and `ref-0192`. It uses the
human-listened `test/data/reference/test.txt`, frozen T111/T123 producer tracks,
the checked-in `orator.toml`, and read-only TitaNet probes. It does not change a
model, producer track, common-clock coordinate, TOML value, or product result.

No executable tool assigns correctness, aggregates accuracy, ranks a product
candidate, selects a repair, or issues a verdict. Shell and C++ probes only
decode, arrange, hash, reproduce, and display immutable producer and model
evidence. Complete chronological and reverse contextual semantic reading
against `test.txt` remains the only authority for speaker-product judgments.

## Complete contextual findings

### `ref-0066`: no source content to attribute

The complete `07:39-08:08` exchange has Tang Yunfeng state that Shi Yi and
another participant can pass a proposal, Shi repeat that they can, and Tang
insert the short echo `你们俩可以` before Shi resumes the `15+51` calculation.
Reading backward from the calculation preserves the same Shi-Tang-Shi order.

T111 ASR source `text_id=37` contains both repetitions and places positive
aligned source characters on the Tang primary return at
`478.640-479.120 s`. T123 ASR source `text_id=42` contains only Shi's first
`我我们俩可以` and proceeds directly to `十五加五十一`; the second Tang
contribution has no source text or source coordinate. The speaker fusion
projector cannot truthfully recover this contribution without synthesizing
text or inventing a source span. `ref-0066` is therefore an upstream content
capture problem, not a safe frozen-projector repair.

### `ref-0192`: source remains, identity evidence is not queried at its clock

The complete `20:33-22:07` exchange has Tang ask whether anyone objects, Shi
answer `没问题。嗯，我没有我没有意见`, Zhu Jie add the distinct short
`没有意见`, and Shi immediately urge anyone with an objection to speak. Reading
backward from Shi's request through Zhu's insertion, Shi's answer, and Tang's
question preserves the same Tang-Shi-Zhu-Shi sequence.

T123 ASR `text_id=111` retains the repeated source content:
`嗯，我没有，我没有意见。没有意见，赶紧说...`. The independent activity
track identifies Zhu (`spk_0`) at `1303.359970868-1304.079970852`; the primary
track independently identifies Zhu at
`1303.359970868-1303.999970853`, bracketed by Shi (`spk_3`). Both tracks use
the same session time base as ASR and alignment.

Forced alignment ends source index 56 at `1302.828` and begins the contiguous
source index 57 at `1304.348`. The complete Zhu primary island lies inside that
`1.520 s` temporal gap. The following repeated `没有意见` begins at source
index 59, but its first positive aligned character does not begin until
`1304.508`, after the Zhu primary island. Consequently the current evidence
producer creates no exact aligned or business query on Zhu's acoustic span.

The containing `vad:332` spans `1303.396-1305.724` and includes both Zhu's
short insertion and Shi's following request. Its frozen session and robust
galleries both rank Shi first. That mixed VAD is not evidence for rewriting
the Zhu island. In contrast, a read-only frozen-registry centroid probe over
the exact primary island displays `spk_0` first at `0.328956`, with the next
displayed score `0.226480`; the VAD-intersected island also displays `spk_0`
first. These values are diagnostics only. The probe does not reconstruct the
runtime's independent robust gallery and therefore cannot authorize a repair.

Immutable diagnostic artifacts:

| Artifact | SHA-256 |
|---|---|
| Exact-span query list | `c7bdfd7e172b161d00f36b6d08e4832610fac76a852c0e125ca5609f36574757` |
| Fusion-window probe TOML | `e3a12f04a91b94684fa4acf3e8cc8e3fe6817f9d640ab4b0e262c70371e3fe69` |
| Centroid-score display | `18c0ca6f25c66d7f5786dbf7bb513554f3167e7b6544d2d5c71f8a7eb48b4ad2` |
| Frozen T123 registry | `66461a77755984a08231d06306da7ce9e1eeac07be1927e91f8a772fc54c7b3f` |

## Replay implementation and input fidelity

`SpeakerIdentityStage::EvaluateSpan` exposes both the session centroid gallery
and the independent robust reference gallery. The replay probe now accepts a
strict grouped snapshot TSV while preserving its prior final-segment CSV mode.
Snapshot rows are ordered by original event index and then segment time. The
probe strips every optional captured identity before each production
`SpeakerIdentityStage::Process` call, starts from a genuinely empty registry
when passed `-`, and rejects a full-audio preload unless `speaker_retain_sec`
covers the complete input. A focused parser/replay test checks grouping,
ordering, empty snapshots, identity stripping, exactly-once processing, and
malformed-input rejection. A warning-clean complete build and all `70/70`
CTest entries pass. These are engineering results only.

The T123 WebSocket artifact contains 3,575 chronological `diar` snapshots and
1,254,049 segment rows. Direct mechanical export by original absolute event
index reads no reference or product label. Replaying those snapshots against
floats decoded directly from `test.mp3` first displayed 1,267 identity-string
differences beginning near `3243 s`. That result is an invalid capture-fidelity
comparison: the production WebSocket client used FFmpeg PCM s16le, while the
probe's normal MP3 loader used a different decoder and floating-point sample
path.

Recreating the client's exact FFmpeg command produces PCM with SHA-256
`17f0edda49989f3ceada60170885091023eeb9d67faae0d6dd67bb585b8857fe`,
identical to the streamed-audio hash in the original manifest. A lossless PCM16
WAV wrapper lets the unchanged C++ audio loader consume those exact samples.
Two empty-registry snapshot replays then each process all 3,575 snapshots and
mechanically display 1,254,049 captured identity values equal to the replayed
values, with zero differing values. Their final identity TSV files are
byte-identical at
`b1f42d0085adcafaf6564479bbc88895518adfa80d84891cd8be3dde843467fa`.
This establishes producer-input reproduction only; it is not speaker accuracy.

Immutable reproduction artifacts:

| Artifact | SHA-256 |
|---|---|
| Original T123 WS capture | `61119ab2eb4f66ed08be85652df44619001227b4986fa6b770239a840b26a9f0` |
| Chronological diar snapshot TSV | `1c39108da56011e921056fa5876532181c738aa3d7a4ad0dc8960a859f60c08a` |
| Exact streamed PCM s16le | `17f0edda49989f3ceada60170885091023eeb9d67faae0d6dd67bb585b8857fe` |
| Lossless PCM16 WAV wrapper | `a60af82d57958ddfaf5d0358820adc7536544c97d59aab9f55a3b2f53694a16e` |
| Repeated final identity view | `b1f42d0085adcafaf6564479bbc88895518adfa80d84891cd8be3dde843467fa` |
| Repeated dual-gallery evidence | `7c732e89ec250e7157a4a85c60dda55e5a3b8c17290ee03e852e505769197a37` |
| T123 TOML | `6a92c582a2cba7e26542f38a60516bb53929dc5d109ba331bbf4b5b614eb9b22` |
| TitaNet weights | `59e1e3069a33bef8da8b9467a5a64a30a5de8c7ae2550b15d5323ef537acaad1` |

## Capture-faithful dual-gallery review

The exact full primary island `1303.359970868-1303.999970853` has an
available embedding and explicitly complete session and robust galleries. The
raw session scores are `spk_0 0.324660450`, `spk_1 0.224973798`, `spk_2
0.222769573`, and `spk_3 0.202382147`. The raw robust scores are `spk_0
0.339976102`, `spk_1 0.285040081`, `spk_2 0.233051181`, and `spk_3
0.203694642`. Manual inspection of each complete list finds the same unique
top identity and finds that both top-to-next differences exceed the existing
checked-in short-span margin `0.04`; the checked-in short score floor remains
`0.0`.

The VAD-intersected subset does not pass the same independent gate: its robust
top-to-next difference is below `0.04`. The preceding VAD, mixed containing
VAD, and following primary controls each display Shi (`spk_3`) first in both
complete galleries. The full exact island therefore passes the previously
specified evidence gate; neither its VAD intersection nor the mixed VAD may be
substituted. This is a manual component-evidence decision, not a speaker-product
accuracy judgment.

## Authorized FR45 boundary

The evidence authorizes a separately specified, generic FR45 candidate under
the following limits:

1. Add the immutable primary-speaker track to the evidence-stage snapshot and
   preserve explicit session-gallery completeness in typed evidence.
2. Emit one `primary_alignment_gap_echo` query only when a short middle primary
   run is bracketed within the existing configured alignment-boundary tolerance
   by the same distinct outer identity, lies wholly
   inside one temporal gap between consecutive positive alignment characters
   of a punctuation phrase, and the immediately following punctuation phrase's
   visible source is a strict suffix repetition of that preceding phrase.
   Multiple islands, gaps, or source mappings must emit no query.
3. The query's acoustic bounds are the exact middle primary run; its source
   bounds are the following repeated phrase. Repetition only locates retained
   source content. It never chooses the speaker.
4. Fusion may write only when both explicitly complete galleries contain the
   same unique identity set, independently pass the existing short score and
   margin gates, and select the exact middle primary identity. Activity must
   cover that identity across the full island. The outer primary runs must be
   long enough under the existing primary minimum, be covered by matching
   activity, and share one identity distinct from the middle.
5. The target source must still be uniformly assigned to the outer identity by
   baseline, primary arbitration, or ordinary direct-short evidence. Any
   missing, duplicate, tied, incomplete, differently ranked, differently
   gated, non-repeated, non-bracketed, source-inconsistent, or competing typed
   evidence preserves existing behavior.
6. FR45 changes only the target source speaker label, identity, support audit,
   and decision reason. It changes no transcript, forced-alignment unit,
   producer time, business time, TOML value, threshold, model, or registry.

FR45 still requires synthetic positive and independent abstention coverage,
deterministic frozen T111/T123 replay, complete display of every changed
conversation, and complete forward and reverse contextual semantic review
against `test.txt`. No automated product judgment is permitted.
