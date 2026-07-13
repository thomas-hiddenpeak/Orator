# Spec 006 — Plan

## Architecture Overview

Sections 1-13 record the original design proposal. Section 14 describes the
implemented modular client, and Section 15 records current Spec 013 hardening.
Where the original proposal mentions a graphical Canvas timeline, that remains
an open requirement: the current main timeline renderer displays authoritative
formatted JSON.

### Frontend–Backend Boundary

The UI is a **single-page application (SPA)** served as static files from a dedicated HTTP port. WebSocket streaming remains on the existing WS port. No client-side state is persisted; all application state lives in:
- **Browser memory** (during the session): DOM state, current transcript, latest metrics.
- **Server-side** (during streaming): the `AuditoryStream` and its pipelines (unchanged from Spec 001).

### Data Flow Diagram

```
User Browser                          orator_ws (C++)
┌─────────────┐                      ┌────────────────┐
│   index.html    │──────HTTP────────→ Serve static files
│  (load UI)      │                   └────────────────┘
└─────────────┘
        │                                ┌───────────────┐
        │◄──────────────────────WebSocket─→ WebSocket port
        │  ws://localhost:8765           └───────────────┘
        │                                 (AuditoryStream)
        │
┌─────────────────────────────────┐
│    ui_controller.js             │
│   (state, DOM updates)          │
│                                 │
│  ws_client.js ─────────────────→ Server events (asr, timeline, meta)
│  (connection, event parsing)    │
│                                 │
│  audio_input.js ─────────────→  Binary PCM frames → server
│  (mic/file, encoding)           │
│                                 │
│  timeline_renderer.js           │
│  (Canvas/SVG rendering)         │
└─────────────────────────────────┘
        │
        ├─ Transcript panel (appended utterances)
        ├─ Timeline visualization (diar + ASR tracks)
        ├─ Metrics display (RTF, compute_sec)
        └─ Controls (start/stop, flush, end, download)
```

---

## 2. Component Design

### 2.1 Backend: HTTP + WebSocket Integration

**Current State**:
- `orator_ws` runs a WebSocket server on port 8765 (default).
- `orator_ws` now also runs a static HTTP UI server on a separate UI port (default 8766, configurable by `ORATOR_UI_PORT`).
- Clients connect over WebSocket for streaming and over HTTP for UI assets.

**Changes Required**:
1. **Add `net/http_static_server.h/.cc`**:
  - Implement minimal GET-only static file serving.
  - Serve static files from `web/` directory for `GET /`, `GET /style.css`, `GET /app.js`, etc.
  - Keep WebSocket server independent on its own port.

2. **File serving strategy**:
   - **Option A (Embedded)**: Compile HTML/CSS/JS into binary as string literals (using `xxd` or CMake magic).
     - Pro: Single binary, no external files.
     - Con: Larger binary, rebuild needed for UI changes.
   - **Option B (Disk)**: Look for `web/` directory relative to `orator_ws` binary; serve from disk if found.
     - Pro: Live editing during development; smaller binary.
     - Con: Requires file distribution.
   - **Recommendation**: Start with **Option B** (disk), add Option A (embedding) as a future optimization.

3. **Minimal HTTP implementation**:
   - Parse `GET /path HTTP/1.1` header.
   - Determine MIME type (`.html` → text/html, `.js` → text/javascript, `.css` → text/css).
   - Serve file content with appropriate headers.
   - Non-existent files → 404 or fallback to `index.html` (SPA pattern).

4. **Port separation**: HTTP UI and WebSocket use different ports by default (`ws=8765`, `ui=8766`) to simplify test tooling.

**Code location**:
- `src/net/http_static_server.cc`.
- `include/net/http_static_server.h`.

---

### 2.2 Frontend: JavaScript Architecture

**Entry Point**: `web/index.html`
```html
<!DOCTYPE html>
<html>
<head>
  <link rel="stylesheet" href="style.css">
</head>
<body>
  <!-- UI layout here -->
  <script src="ws_client.js"></script>
  <script src="audio_input.js"></script>
  <script src="timeline_renderer.js"></script>
  <script src="ui_controller.js"></script>
</body>
</html>
```

**Module Responsibilities**:

#### `ws_client.js`
- **Init**: Detect server URL (from `window.location`), establish WebSocket connection.
- **Event dispatch**: Parse incoming JSON, dispatch to handlers via event emitter.
  - `onReady(msg)`: UI enables controls.
  - `onAsr(msg)`: Append utterance to transcript.
  - `onTimeline(msg)`: Render full timeline, update metrics.
  - `onError()`: Show connection error.
- **Send**: Methods to send binary PCM, control messages (`flush`, `end`, `reset`).
- **Reconnect**: Exponential backoff if connection drops.

#### `audio_input.js`
- **Microphone capture**:
  - Request permission via `navigator.mediaDevices.getUserMedia()`.
  - Create `AudioContext`, `MediaStreamAudioSourceNode`, `ScriptProcessorNode` (or `AudioWorklet`).
  - Receive `AudioBuffer` frames (typically 4096 samples at 48 kHz by default).
  - Resample to 16 kHz (if needed).
  - Convert float32 to int16LE PCM.
  - Send to WebSocket as binary frames.
  - Visual level meter (optional, using `AnalyserNode`).

- **File upload**:
  - Accept MP3, WAV, FLAC, OGG via `<input type="file">`.
  - Load file as `ArrayBuffer`, decode using `AudioContext.decodeAudioData()`.
  - Resample to 16 kHz, convert to int16LE.
  - Send frames at controlled rate (real-time or faster) to WebSocket.

- **Rate control**:
  - Default: Send frames at real-time rate (frame_size / sample_rate).
  - Option: Send as fast as possible (for testing), or at user-specified rate.

#### `timeline_renderer.js`
- **Input**: Parsed `{"type":"timeline",...}` JSON from server.
- **Output**: Canvas or SVG element displaying:
  - **Diarization track**: Horizontal bars per speaker, colored and labeled.
  - **ASR track**: Text boxes positioned by (start, end) on time axis.
  - **Time axis**: Labeled ticks (0s, 30s, 60s, ...).
  - **Legend**: Speaker ID → color mapping.
- **Interactivity** (future):
  - Scrub/seek (click on timeline to jump to position).
  - Zoom in/out.
  - Hover tooltips (exact timing, confidence).

#### `ui_controller.js`
- **State management**:
  - `currentSession`: metadata (start time, audio_sec, current speaker count).
  - `transcript`: array of {start, end, speaker, text}.
  - `diarization`: array of {start, end, speaker, confidence}.
  - `metrics`: {audio_sec, wall_sec, stream_rtf, diar_rtf, asr_rtf, ...}.
- **DOM updates**:
  - Append utterances to transcript panel (batch updates, avoid re-rendering entire DOM).
  - Update progress bar.
  - Update metrics panel.
  - Render timeline on completion.
- **Event handlers**:
  - Button clicks: start mic, upload file, flush, end, download, clear.
  - Form inputs: file picker, frame rate slider.
- **Export**:
  - Download final timeline as JSON (`<a href="data:...">Download</a>`).
  - Copy transcript to clipboard.

---

## 3. Threading & Concurrency

**Frontend (Browser)**:
- Single-threaded (main thread runs JavaScript).
- Web Worker (optional, future): Decode audio in background without blocking UI.
- No data races by design.

**Backend (C++)** (unchanged from Spec 001–005):
- Main thread: HTTP socket accept, WebSocket frames.
- Diarization worker thread: `AuditoryStream`.
- ASR worker thread: `AuditoryStream`.
- GPU scheduling: Spec 002 (ASR-concurrent default, diar/VAD optional lock-free).

**HTTP → WebSocket Transition**:
- When a client requests a file, it's served synchronously on the accept thread.
- When a client upgrades to WebSocket, the connection is handed off to `AuditoryWsHandler` (existing logic).
- No new threads created for UI.

---

## 4. Resource Usage

### Bandwidth
- **Audio upload**: 16 kHz × 1 ch × 2 bytes/sample = 32 kB/s.
- **JSON responses**: Incremental ASR ~0.1 kB each; timeline ~10–100 kB total (depends on duration).
- **Total for a 2-hour session**: ~230 MB audio + ~0.5 MB JSON ≈ manageable over LAN or WAN.

### Browser Memory
- **Transcript array**: ~100 utterances × ~100 bytes = 10 kB.
- **Timeline JSON**: Up to 1 MB (if very long recording).
- **DOM nodes**: ~1000 nodes × 100 bytes = 100 kB.
- **Total**: ~1–2 MB typical (acceptable for modern browsers).

### Server Memory
- No new memory usage (UI serves static files, then delegates to existing `AuditoryStream`).

---

## 5. Error Handling & Robustness

### Client-Side

1. **WebSocket closed unexpectedly**: Reconnect with exponential backoff (1s, 2s, 4s, ..., up to 30s). Display "Connecting..." status.
2. **Malformed JSON**: Log to browser console; ignore bad message and continue.
3. **Microphone denied**: Show permission error; disable mic button.
4. **File too large**: Reject files >1 GB; show error.
5. **Audio decode failure**: Show error; allow retry with different file format.
6. **Browser storage**: Use SessionStorage (not LocalStorage) to avoid persisting sensitive audio metadata.

### Server-Side

- **HTTP request parsing**: Validate `Content-Length`, reject malformed requests.
- **File not found**: Return 404 with body `File not found`.
- **WebSocket upgrade failure**: Return 400 with error message.
- **Existing error handling** (from Spec 001–005): All server-side errors in streaming remain unchanged.

---

## 6. Testing & Validation

### Unit Tests
- **Frontend (optional)**: Mock WebSocket; test ui_controller state transitions.
- **Backend**: HTTP file serving (test 200, 404 responses); WebSocket upgrade logic unchanged.

### Integration Tests
1. Start `orator_ws` on WS port 8765 (UI defaults to 8766).
2. Open browser to `http://localhost:8766` → page loads.
3. Send audio over WebSocket (existing `ws_client_test.py` or new HTTP client).
4. Verify transcript panel updates in real time.
5. Verify timeline renders correctly on completion.
6. Verify metrics match server-side measurements.
7. Test microphone capture (manual, requires user action in browser).
8. Test file upload with various formats (WAV, MP3, FLAC).

### Performance Baseline
- **Page load**: < 500 ms (static files only).
- **First ASR message arrival**: < 3 s (depends on audio, not UI).
- **Timeline render**: < 1 s (for typical 2-hour recording).
- **WebSocket latency**: < 100 ms (existing from Spec 001).

---

## 7. Risks & Mitigations

### Risk 1: Cross-origin security (CORS)
- **Problem**: If UI is served from one origin and WebSocket is expected from another, browsers block it.
- **Mitigation**: Keep UI and WS on localhost; allow explicit WS URL override in UI. No browser CORS issue for WebSocket in this model.

### Risk 2: Audio codec compatibility
- **Problem**: Different browsers support different audio formats natively.
- **Mitigation**: Standardize on WAV for upload (uncompressed, always supported). For MP3/FLAC, use `AudioContext.decodeAudioData()` (Web Audio API). Worst case: user uploads WAV.

### Risk 3: Microphone latency in browser
- **Problem**: Web Audio API introduces variable latency (buffers, OS).
- **Mitigation**: Acceptable for UI demo; real production systems use native clients (out of scope).

### Risk 4: Large timeline rendering
- **Problem**: 10+ hour recording → 10k+ diarization segments → slow Canvas/SVG render.
- **Mitigation**: Render timeline in chunks; use viewport clipping (only render visible portion). For MVP, limit to < 2 hour sessions.

### Risk 5: Session state loss on refresh
- **Problem**: User refreshes browser → transcript and metrics lost.
- **Mitigation**: Accept as expected behavior for MVP. Future: add SessionStorage or server-side session persistence.

---

## 8. Future Extensions (Out of Scope)

- **Playback & seek**: Embed audio player; seek to position on timeline click.
- **Speaker re-identification**: UI to rename or merge speaker IDs.
- **Keyword highlighting**: Mark key terms in transcript.
- **Export to VTT/SRT**: Generate subtitle files from timeline.
- **Multi-user sessions**: Share results via URL or database.
- **HTTPS/TLS**: Secure deployment.
- **Accessibility**: Full WCAG 2.1 compliance (MVP is basic ARIA only).
- **Mobile app**: Native iOS/Android clients (out of web scope).

---

## 9. Build Integration

### CMakeLists.txt Changes
1. Identify or create a directory for web assets (`web/`).
2. Ensure `orator_ws` executable has access to `web/` at runtime (via relative path or install target).
3. No new external CMake dependencies; static file serving is minimal C++.

### Deployment
- **Development**: `./build/orator_ws 8765 <models>...` with UI at `http://localhost:8766` by default (override via `ORATOR_UI_PORT`).
- **Production**: Bundle `web/` into binary (future, Spec 006 Phase 3) or deploy alongside binary.

---

## 10. Communication Contract (No Changes to Existing Schemas)

All WebSocket message formats remain unchanged from Spec 001:
- `{"type":"ready",...}` — unchanged.
- `{"type":"asr",...}` — unchanged.
- `{"type":"timeline",...}` — unchanged.
- `{"type":"revision",...}` — unchanged (Spec 004).
- `{"type":"gpu_telemetry",...}` — unchanged (Spec 002, optional).

The UI is a **visualization layer** and does not modify protocol semantics. New controls (`flush`, `end`, `reset`) are already supported by the server.

---

## 11. Summary of New Code

### Backend (C++)
- `src/net/http_static_server.cc` (~200 lines): HTTP request parsing, static file serving.
- `include/net/http_static_server.h` (~50 lines): Header.
- Changes to `src/ws_main.cc` (~20 lines): instantiate UI server, separate UI port, configure web root.

### Frontend (JavaScript)
- `web/index.html` (~100 lines): Page structure.
- `web/style.css` (~300 lines): Layout, responsive design, theming.
- `web/ws_client.js` (~200 lines): WebSocket connection, event dispatch.
- `web/ui_controller.js` (~300 lines): State, DOM updates, button handlers.
- `web/audio_input.js` (~250 lines): Microphone, file upload, encoding.
- `web/timeline_renderer.js` (~400 lines): Canvas/SVG rendering.

**Total new code**: ~1700–2000 lines (including comments and spacing).

---

## 12. Phase Breakdown

### Phase 1: MVP (1–2 weeks)
- Create `web/` directory and static files.
- Implement HTTP file serving in `orator_ws`.
- Implement `ws_client.js` and `ui_controller.js` (basic DOM updates).
- Render timeline as HTML list (no Canvas yet).
- **Deliverable**: Static UI, real-time transcript appending, basic metrics.

### Phase 2: Visualization (1 week)
- Implement `timeline_renderer.js` (Canvas-based).
- Add speaker color legend, confidence bars.
- Add zoom/pan controls.
- **Deliverable**: Graphical timeline, interactivity.

### Phase 3: Full Features (1–2 weeks)
- Implement `audio_input.js` (microphone + file upload).
- Add performance metrics dashboard.
- Polish responsive design for mobile.
- Optimize for large recordings (viewport clipping, lazy loading).
- **Deliverable**: Production-ready UI with all inputs and outputs.

---

## 13. Validation Checklist

- [ ] Static HTML page loads from `http://localhost:8766` without external requests.
- [ ] WebSocket connection status displayed accurately.
- [ ] Incremental ASR utterances appear in transcript panel within 200 ms of arrival.
- [ ] Final timeline renders with diarization and ASR tracks aligned on time axis.
- [ ] Metrics (audio_sec, wall_sec, RTF) displayed correctly and match server logs.
- [ ] File upload works for WAV, MP3 (decoded), FLAC.
- [ ] Microphone capture produces correct int16LE PCM at 16 kHz.
- [ ] Flush and End buttons work as expected.
- [ ] Download JSON exports the complete timeline correctly.
- [ ] Clear button resets session state.
- [ ] Error messages displayed for network loss, permission denied, malformed JSON.
- [ ] Build produces no new warnings.
- [ ] Tested on Chrome 120+, Firefox 121+, Safari 17+.
- [ ] UI is responsive on desktop (1920×1080, 1280×720) and tablet (768×1024).

---

## 14. Phase 2 rebuild — architecture (current project)

The MVP was a single 1159-line `app.js` prototype that consumed only a subset of
the protocol (no `align`, no `cursor_progress`, GPU telemetry reduced to one
status string) and modelled only two pipelines. The rebuild restructures the
client into small ES modules served as-is by the static server (no build step,
no framework — consistent with the project's zero-dependency ethos):

```
web/
  index.html              # layout shell, <script type="module" src="/js/app.js">
  style.css               # styles incl. observability panel + speaker palette
  js/
    app.js                # bootstrap: wire modules, controls, lifecycle
    ws.js                 # connection, reconnect, Spec 004 envelope decode,
                          #   message router → typed callbacks
    model.js              # normalized client state: segments per track,
                          #   comprehensive turns (by text_id), speaker registry
                          #   (id→color/name), telemetry ring buffers
    audio.js              # mic capture + file decode → int16LE 16k framing
    render/transcript.js  # live transcript + partial draft + per-unit (align)
    render/timeline.js    # authoritative live/terminal formatted JSON
    render/observability.js # per-pipeline RTF gauge+sparkline, class/priority,
                          #   backlog (pending_sec), starvation warning
    render/sessions.js    # list/load/download
```

### Data flow
```
WS frame ─ ws.js(envelope decode) ─ router ─ model.js(update) ─ render/*(rAF)
```
- `ws.js` is the single ingress; it unwraps the Spec 004 envelope, dispatches by
  inner `type`, and never drops a known message. Unknown topics are logged.
- `model.js` holds the authoritative client state; renderers are consumers
  scheduled on `requestAnimationFrame`. Observability uses small Canvas
  sparklines; the main timeline is currently formatted JSON.
- Speaker identity: a registry maps `speaker_id`→stable color + display name,
  shared by transcript, comprehensive and timeline (FR11).
- Observability: `gpu_telemetry`/`cursor_progress` feed per-pipeline ring buffers
  (last N samples) → live value + sparkline; backlog↑ while RTF<1 → warning
  (FR13, Spec 011 methodology).

### Constraints
- No runtime third-party JS dependency (no React/Vue/CDN); plain ES modules.
- Same HTTP static server / ports; only `web/` content changes.
- CJK renders natively in the browser — the Web UI is the readable-text home.

### Validation (Phase 2)
- Real `rate=1` WebSocket run with telemetry on: every message type is consumed
  (logged inventory), align lane + speaker identity + observability panel
  populate, comprehensive view updates live from `revision`, final `timeline`
  reconciles. Streaming stays ~1.0× real-time. Context review, not a script.

---

## 15. Phase 3 contract hardening — Spec 013

The server keeps typed tracks authoritative. Diarization and VAD sinks mirror
their committed evidence to both protocol storage and live WebSocket events.
Protocol payloads remain backward compatible while adding explicit `type` and
`text_id` fields where applicable.

The browser model treats live events as provisional and a terminal or loaded
timeline as an authoritative snapshot. Applying that snapshot clears stale ASR,
alignment, raw-track, draft, and business-turn state before rebuilding all maps
from stable IDs. The model separately retains business revisions so a revision
that arrives before its final ASR event is not lost. A new WebSocket `ready`
message starts a clean browser session because the backend resets the stream on
every connection open.

Validation has two levels:
1. A dependency-free Node/Bun contract test covers deterministic message and
   model transitions and is registered in CTest when Node is available.
2. A real server/browser run verifies the served module graph, live diar/VAD,
   partial/final/revision/align convergence, reconnect reset, loaded sessions,
   and exact JSON export. The browser result is required before FR15 closes.

On 2026-07-13, a 12 s real Chromium run passed file upload, live transcript,
flush/end, terminal contract checks, exact JSON download, saved-session lookup
and exact reload, deliberate server restart with clean reconnect, fake-device
microphone start/stop, and desktop/mobile screenshots. The file-stream test also
found and fixed whole-backing-buffer retransmission, and the session test found
and fixed `audio_sec` metadata parsing. Physical microphone and Firefox/Safari
evidence remain open.
