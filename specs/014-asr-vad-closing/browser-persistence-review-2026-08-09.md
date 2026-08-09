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

## Exact clean-commit 120-second repeat

Commit `b0eadbe09e2d100a03b2721980ce91cfffa050c8` was clean and synchronized
with `origin/master` before the longer run. The run used a fresh storage tree,
empty registry, and a complete TOML copy differing only in port and isolated
paths.

| Artifact | SHA-256 |
|---|---|
| 120-second PCM input | `102edda3ffead0057f000872b56c54f40b51d2cfd193c3bd7edcfe19517b3c48` |
| TOML copy | `553dd5530059d0c59b6c28262dcbeb44863586160e40931b76fc00d0070790d2` |
| Browser log | `f16617d9cd319ca5fb669a457687b1d6e7957763c45b8545917d00a42782924c` |
| Persisted terminal JSON | `d56850ab103c3520730fb42d2397815d0cc3c6bb8d4a5948ebe37abc38a091fd` |
| Desktop screenshot | `547df89aa4e9aae14089dc0bddfe196ad7fab540bcfdb08de20c1f57d0ce14c7` |
| Mobile screenshot | `bf312675b3aa4efd8257e914f494e13ceda3fd594c0348038223719b5e5b5c58` |

The browser reaches 120 seconds, 11 final ASR records, matching 11 alignment
groups, 34 final business entries, exact export, exact persisted reload, clean
server-restart reconnection, and fake-device microphone start/stop. Every track
closes at 1,920,000 samples with no extent gap. Desktop and mobile screenshots
remain readable without page-level overflow or incoherent overlap.

The reviewer reads all 18 in-scope contributions chronologically and then from
`ref-0018` back to `ref-0001`. The main opening, `15`, `40%`, `5%`, `3.14`,
decision, interruption, and audible final continuation remain readable. The
existing `RM1 -> M一`, malformed Hangzhou/Chengdu relation, false negation before
`当你最前面说的话为准`, and inherited rapid-handoff speaker edges remain.
No Live row introduces a new omission, duplicate final, stale draft, semantic
cut, or speaker-policy change. This is the same bounded product interpretation
as the earlier signed 120-second gate; no executable result produced it.

The run nevertheless stops T040 because `wall_clock_ok=false`. Its first-sample
wall time is 22:30:09.433 and the stored terminal file time is 22:32:12.554,
bounding the path to approximately 123.121 seconds. Source inspection shows
that browser file streaming uses a new relative 60 ms timeout after every
frame, so event-loop delay accumulates across the session. Spec 014 therefore
authorizes one browser-only absolute-deadline pacing correction. No model,
TOML, endpoint, speaker, or final-text change is authorized.

## File-pacing engineering candidate

The browser sender now calculates the next delay from stream start time and the
exact number of PCM bytes already sent. It retains the 60 ms frame size and
exact byte coverage. A late callback therefore waits zero milliseconds to
return to the source clock instead of adding another full relative interval to
all later frames.

The dependency-free Web model suite adds a pure deadline test and passes all
nine cases. The complete warning-enabled build has no `warning:` or `error:`
diagnostic, and all `74/74` CTest entries pass in `52.71 s`. This engineering
result authorizes an exact clean-commit 120-second browser repeat; it does not
complete T040 or establish any product-accuracy result.

## Accepted clean browser repeat

Exact clean commit `d09b13b6fa7da74baeaef957703e4f6f5e02d8bc` repeats the
complete 120-second Chromium flow from empty storage and registry state.

| Artifact | SHA-256 |
|---|---|
| TOML copy | `f9aaba018495eed53014e15fbd16a4f080a482073a6116741b4f5ff4fbe4d1e0` |
| 120-second PCM input | `102edda3ffead0057f000872b56c54f40b51d2cfd193c3bd7edcfe19517b3c48` |
| Browser log | `78b28b7ac134467ebab7d8a2071f8521572736c8fd064fe096d9a7cb77f243ce` |
| Persisted terminal JSON | `db70ba66bdc693a7f37748941b26723c3210cbf7d5f091102bf2e00ab7923530` |
| Desktop screenshot | `654df6cd02737ca289f14b7f8a8686b5c2a416012a80c1bda9fb956b1dca3667` |
| Mobile screenshot | `cd6e79dfd72ad803f3a4ae40b73db67ede69620b8ced66009b5df6ba40f90db3` |
| Independent pacing log | `51c95d8aaea4413cf336b3c3b110c473fea89691e01c4bd1efca864b88acfc09` |

The exact clean run again completes browser decode, Live population, 120-second
source extent, 11 ASR finals, 11 alignments, 34 business entries, terminal
reconciliation, exact download, one collision-safe persisted session, exact
reload after Clear, server-restart reconnection, and fake-device microphone
start/stop. Both screenshots have coherent desktop and 390 px mobile layout;
the mobile Live and JSON panels scroll internally without covering later
sections.

The terminal still records `wall_clock_ok=false`. A separate clean browser run
resolves why: selection through decode and source completion takes 120.322
seconds, automatic nonterminal Flush takes 0.477 seconds, and Flush through
terminal End takes 2.165 seconds, with no browser error. `wall_clock_ok` measures
from the first sample through final End and is specified for the direct-end
production path. The browser intentionally inserts a Flush before the user End,
so using the combined field to reject browser source pacing would compare a
different control sequence. Spec 014 records all three mechanical stages and
does not tune audio faster to force that unrelated field true.

The reviewer again reads `ref-0001` through `ref-0018` chronologically and then
in reverse. Text and final speaker-business evidence are unchanged from the
preceding clean run. The same `RM1`, relation, false-negation, and rapid-handoff
residuals remain, while no browser-only omission, duplication, stale partial,
endpoint cut, or speaker-policy change appears. This contextual conclusion is
manual and not produced by the pacing measurements.

T040 and T041 are complete. Physical microphone and non-Chromium evidence remain
open; no ASR, endpoint, or speaker accuracy promotion follows.
