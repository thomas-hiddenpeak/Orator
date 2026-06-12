# Plan 003 — Bounded Sliding-Window Real-Time ASR

- **Feature**: `003-sliding-window-asr`
- **Spec**: [spec.md](spec.md)
- **Status**: Draft (in progress)
- **Constitution**: v1.1.0

> HOW to satisfy [spec.md](spec.md). Standard terminology; "window" is the
> bounded span of recent audio re-decoded each step.

---

## 1. Algorithm

State held by the ASR worker (all in the time domain, on the shared clock):

- `pcm_` — retained audio samples; `base_sample_` is the absolute index of
  `pcm_[0]`. Only the window's worth of recent audio is retained.
- `committed_text_` — the transcript that has been finalized (text for audio
  that has passed out of the window). Supplied to the model as a prefix prompt.
- `committed_until_sample_` — absolute sample index up to which audio has been
  committed (its text is in `committed_text_`).
- `last_decode_sample_` — absolute sample index of the last decode step.

Per arriving audio span (`ProcessSpan`):

1. Append samples to `pcm_`; advance the producer position.
2. If at least `step_sec` of new audio has arrived since `last_decode_sample_`,
   run a decode step:
   a. Define the window as `pcm_` from `committed_until_sample_` to the current
      end, capped at `window_sec`. If the span exceeds `window_sec`, the oldest
      part beyond the cap must be committed first (step 3 below) so the window
      stays bounded.
   b. Call `Qwen3Asr::TranscribeWindow(window_samples, committed_text_)`. It
      returns the continuation text for the window (the model continues from the
      committed prefix).
   c. The live transcript is `committed_text_ + continuation`. Emit it as the
      current incremental result.
3. Commit: when the window would exceed `window_sec`, the audio from
   `committed_until_sample_` up to `(end - window_sec)` leaves the window. Decode
   that leaving span together with a trailing `commit_margin_sec` of context,
   take the text for the leaving portion, append it to `committed_text_`, advance
   `committed_until_sample_`, and drop the corresponding samples from `pcm_`.
   The `commit_margin_sec` (the unfixed tail) prevents committing a word that is
   still being spoken at the boundary.

On `Finalize`: decode the remaining window, commit the whole continuation, drop
the buffer.

Timeline: each committed span is appended to `StreamTimeline` as an `AsrToken`
with absolute start/end derived from the committed sample range. The
comprehensive view is built by the controller as today.

### Why this is bounded
- The re-decoded window is at most `window_sec` of audio → encoder cost and
  decode token count per step are bounded by `window_sec`, independent of total
  stream length (spec FR6).
- `pcm_` holds at most `window_sec + commit_margin_sec` of audio → bounded
  memory. `committed_text_` grows as plain text (kilobytes per minute), which is
  negligible and bounded by the session.

## 2. Components

### 2.1 `AsrWorker` (rewrite the internal method)
- Replace the energy-VAD `DrainUtterances` / `EmitUtterance` with the
  sliding-window step above. The public surface (`ProcessSpan`, `Finalize`,
  `Reset`, `processed_samples`, `compute_sec`, `Emit`) is unchanged so the
  controller and WS handler need no change.
- New `Params`:
  - `window_sec` (re-decoded window length; swept, default from §4),
  - `step_sec` (decode cadence; how often a step runs),
  - `commit_sec` (how much audio leaves the window per commit; ≤ window_sec),
  - `commit_margin_sec` (unfixed tail kept for context at the commit boundary).
  The old energy-VAD params are removed.
- Uses `Qwen3Asr::TranscribeWindow(samples, n, prefix_text)` (already added).

### 2.2 `Qwen3Asr`
- `TranscribeWindow` exists. Add nothing unless the sweep shows a need (for
  example a max-new-tokens proportional to window length).

### 2.3 CER measurement (offline analysis tool, not a runtime path)
- A small Python tool (`tools/cer.py`, standard library only) computes character
  error rate between a hypothesis transcript and the gold transcript over the
  same time span, after normalization (strip punctuation/whitespace, keep CJK
  and alphanumerics). It reads the streamed JSON output (the `comprehensive`
  text or the asr-track text) and `asrTest2Final.txt`.
- This is offline analysis tooling under `tools/` (Constitution I.4 permits
  Python tooling), not part of the runtime.

## 3. Validation

- **Cost stability (AC1)**: instrument the worker (or the probe) to log per-step
  decode time; confirm the last-third average is within a small factor of the
  first-third average (bounded, not linearly growing).
- **RTF (AC2)**: measure streaming RTF on 120 s through the WS path, compare to
  the current 4.99x.
- **Accuracy (AC3)**: run `tools/cer.py` on the bounded-window output and on the
  current energy-VAD output against the gold; require CER(window) ≤ CER(VAD).
- **Determinism (AC4)**: two runs produce identical committed text.
- **Memory (AC5)**: VmRSS bounded over a long run (reuse the probe's VmRSS
  logging or `/proc/self/status`).
- **Tests/build (AC7)**: `ctest` green, `-Wall -Wextra` clean.

## 4. Parameter sweep (decide defaults from data, not assumption)

Sweep on 120 s of `test.mp3`, reporting RTF and CER for each:
- `window_sec` ∈ {8, 12, 16, 24}
- `step_sec` ∈ {1, 2}
- `commit_margin_sec` ∈ {1, 2}
- `commit_sec` derived (window minus margin, or a fixed fraction).

Pick the smallest window that does not increase CER versus the best, with RTF at
or above target. Record the chosen defaults and the sweep table.

## 5. Constitution Check

- **I**: existing CUDA + std lib; CER tool is offline Python under tools/.
- **II**: AC3 forbids CER regression; numerics unchanged.
- **III**: ASR stays independent; timeline contract unchanged.
- **IV**: measured on the real streaming path; honest RTF + CER.
- **V**: small single-purpose worker; documented state; threaded path race-free.
- **VI**: standard terminology.
- **VII**: parameter defaults chosen from the §4 sweep, not assumed.

## 6. Risks and Mitigations

- **Boundary word loss** → `commit_margin_sec` unfixed tail + tail re-decode
  (AC3 verifies).
- **Latency** → `step_sec` controls how often the live transcript updates;
  committed text lags by about `window_sec`. Reported, tuned in §4.
- **Repetition at prefix join** → the model continues from the committed prefix;
  the existing repetition cleanup applies. Verified by AC3.

## 7. Out of Scope (per spec Non-Goals)

Model numerics/quantization, per-word timestamps, GPU scheduling (Spec 002),
diarization changes.
