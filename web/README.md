# Orator Web UI

The browser UI is served directly by `orator_ws`. It uses plain ES modules,
browser APIs, and no runtime frontend dependency or build step.

## Current Surface

- microphone capture and browser-side audio-file decoding to mono int16LE 16 kHz;
- live ASR partial/final rows, speaker revisions, diarization, VAD, and alignment;
- terminal and loaded-session reconciliation by stable `text_id`;
- per-pipeline RTF, scheduling, backlog, GPU usage, VRAM, and power telemetry;
- developer status and command-copy controls;
- speaker naming plus saved-session list/load;
- exact terminal timeline JSON inspection and download.

The main timeline panel currently shows the authoritative JSON document. A
graphical multi-track timeline, zoom, pan, and seek controls are not present in
the current implementation and remain open Spec 006 work.

## Architecture

```text
web/
|-- index.html
|-- style.css
`-- js/
    |-- app.js                 # lifecycle, controls, render scheduling
    |-- audio.js               # microphone/file input and exact PCM framing
    |-- format.js              # time, RTF, and speaker display helpers
    |-- model.js               # authoritative browser session state
    |-- ws.js                  # reconnect, envelope decode, typed routing
    `-- render/
        |-- dev_status.js
        |-- observability.js
        |-- sessions.js
        |-- speakers.js
        |-- timeline.js        # formatted authoritative JSON view
        `-- transcript.js
```

Data flow:

```text
orator_ws -> ws.js -> model.js -> requestAnimationFrame -> render/*
```

The server permits one audio-producing connection per session and broadcasts
the resulting events to browser and diagnostic observers. Opening or closing
the UI does not reset an external producer. A connection that attempts to send
audio while another producer is active receives an error and remains an
observer.

Live state is provisional. A terminal or loaded `timeline` clears stale live
state and rebuilds ASR, alignment, business-speaker turns, and raw tracks from
the server document. A new `ready` event starts a clean browser session because
server IDs restart at zero after reconnect.

Known routed messages are `ready`, `asr_partial`, `asr_retract`, `asr`,
`revision`, `align`, `diar`, `vad`, `vad_progress`, `vad_state`, `timeline`,
`gpu_telemetry`, `cursor_progress`, `sessions`, `speakers`, `reset_ok`,
`describe`, and `error`. Protocol envelopes without an inner `type` are typed
from their topic. Unknown messages are reported instead of silently discarded.

## Validation

```bash
# Registered dependency-free browser-model contract test
node --test test/web/model_contract.test.mjs

# Complete configured test suite, including the real-WebSocket speech/silence gate
ctest --test-dir build --output-on-failure
```

The registered real-WebSocket gate uses the sole unified client with early,
transient, and late observer connections. Playwright remains a tools-only
manual acceptance dependency; real-browser checks cover live rows, terminal
reconciliation, exact parsed JSON content, session persistence/load,
desktop/mobile layout, and the browser microphone path with a fake media
device. A physical microphone and non-Chromium browsers still require manual
acceptance evidence.

## Editing Guide

- Change protocol parsing only in `js/ws.js`.
- Change browser state convergence only in `js/model.js`.
- Keep renderers as consumers of `Model`; do not infer new speaker decisions in
  the browser.
- Preserve exact byte coverage in `audio.js::copyPcmFrame`; a typed-array view's
  backing buffer may contain more bytes than the view.
- Add deterministic state transitions to `test/web/model_contract.test.mjs` and
  repeat the real-browser flow for user-visible changes.
