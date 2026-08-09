# Spec 014 Browser Persistence Review - 2026-08-09

## Scope and claim boundary

This report records the first Phase 4 real-Chromium file-input gate and the
bounded session-persistence correction it exposed. Browser automation checks
transport, IDs, storage, DOM state, export equality, and screenshots only. The
reviewer reads the displayed transcript and human reference directly; no code
assigns semantic, endpoint, hallucination, speaker, or product correctness.

The candidate does not change TOML behavior, ASR, VAD, diarization, forced
alignment, speaker fusion, models, or final timeline construction.

## Initial failure

The first isolated 12-second Chromium run based on clean commit `ca3d9cb`
successfully completed all of the following before failing:

- browser-side decode and wall-time PCM streaming;
- visible Live transcript population;
- flush and direct `End` terminal convergence;
- exact parsed JSON download;
- desktop and 390 px mobile screenshots; and
- discovery of the newly persisted 12-second session row.

After `Clear`, loading that row timed out. The two retained files were both
empty timelines with `audio_sec=0`. Source tracing established that
`AuditoryStream::Reset()` persisted every reset, including the fresh empty
session created immediately after `End`. Its session ID used integer wall-clock
seconds and PID, so the following empty `Clear` could reuse the just-finalized
ID and atomically replace the non-empty JSON.

This is a storage-lifecycle defect, not a browser-state or accuracy result.

## Bounded correction

The correction freezes the pre-reset sample extent and persists only when it is
nonzero. A saved-session ID now contains microsecond wall time, PID, and a
monotonic per-process sequence. The existing atomic write, JSON schema, load
RPC, and browser model remain unchanged.

`test_auditory_stream` now proves directly that:

- an empty reset creates no stored session;
- one non-empty reset creates one loadable document with its audio extent;
- a following empty reset neither adds nor changes a document; and
- two rapid non-empty resets retain two distinct loadable IDs.

The complete warning-enabled build has no `warning:` or `error:` diagnostic.
All `74/74` registered CTest entries pass in `52.99 s`.

## Corrected real-browser evidence

The corrected 12-second run uses an empty isolated storage tree and a complete
TOML copy differing from the checked-in file only in server port, storage path,
and registry path.

| Artifact | SHA-256 |
|---|---|
| 12-second PCM input | `a0b3322a2a90f956bf7d381edc4ad407b911f0f1c42a4ae2b7a0c5bc25b5eebf` |
| Candidate TOML | `7d96ea8be26ab17b6d151f5a59da8355205b45fcb4368924841e7ff3f525b88d` |
| Candidate server binary | `7aa4a37898c895e81fd0d653350c593c4f17517c0c93d5b5377020038060b6d7` |
| Persisted terminal JSON | `e7bac86520e5ec6c3347493640ca0d9176a2359e936fb53df2bad403828a2b3c` |
| Desktop screenshot | `ef22bf4c332c599b189351e9e55c4ff9991d85b26e7784c1ac32812ded10b585` |
| Mobile screenshot | `25a074927a6fc91a82a77165f678332dad647fc2da0a3e724885619ab5c8faa6` |
| Browser log | `b5509c38eb708687f00f9ff8abe7129311c36a92041b1ed4c630c8ae8e31c99a` |

The run leaves exactly one stored non-empty document. Its opaque ID is
`0006589e00b336550023f44d00000000`; the document has `audio_sec=12`, reconciled
time bases, two final ASR records, matching alignment IDs, and three final
business entries. `End -> Clear -> Load` reconstructs the exact terminal JSON.
The same browser then observes a deliberate server stop, reconnects to a clean
server state, and starts/stops fake-device microphone capture without a browser
console or page error.

Direct visual inspection of both screenshots finds no page-level horizontal
overflow, incoherent overlap, clipped control, or text occlusion. The timeline
JSON panel intentionally scrolls its own long content.

## Contextual Live reading

The reviewer reads the human reference beginning at 00:00:03 and both rendered
final rows. The reference begins with Zhu Jie's statement that he is relatively
idealistic, followed by `其实，是这样的，就是说` and repeated `我是` as the
thought continues. The first displayed row preserves that opening meaning. The
second row, `我是。`, is the audible continuation at the artificial 12-second
file cutoff. The two-row Live presentation remains readable and has no stale
draft or duplicate final.

The final business view splits the first ASR text inside the known cold-start
speaker evidence. That conditional FR50 residual is visible and is neither
repaired nor worsened by this storage correction.

## Transitional status

- The persistence root cause is corrected and all local engineering gates pass.
- One real Chromium correction run passes the complete mechanical flow and its
  bounded contextual reading.
- The correction must be committed and repeated from the exact clean commit
  before T040 is signed complete.
- A longer browser Live reading, physical microphone evidence, and
  non-Chromium availability record remain open.
