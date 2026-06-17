# Spec 006 — Tasks

## Overview
Spec 006 implementation broken into ordered, independently verifiable tasks organized by phase.

---

## Phase 1: MVP — Basic UI, Real-Time Transcript, Metrics (DRAFT)

### T001 — Create web/ directory and static assets structure
**Status**: ✅ COMPLETED
**Goal**: Set up directory layout and placeholder files.
**Steps**:
1. Create `web/` directory in repository root.
2. Create MVP files: `index.html`, `style.css`, `app.js`.
3. Add `.web/` reference to `.gitignore` if needed (or commit all files).
**Verification**:
- `ls web/` shows the MVP files.
- `git status` includes new web files.

---

### T002 — Implement HTTP file server in C++
**Status**: ✅ COMPLETED
**Goal**: Add dedicated HTTP static server for UI while keeping WebSocket on its own port.
**Steps**:
1. Create `src/net/http_static_server.cc` and `include/net/http_static_server.h`.
   - Parse HTTP GET requests.
   - Map path to `web/` file, read from disk, return with correct MIME type.
   - Handle 404 (file not found), 400 (bad request).
2. Modify `src/ws_main.cc` to launch both servers.
3. Configure web root and UI port via environment variables.
**Verification**:
- `./build/orator_ws 8765 <models>...` starts without error.
- `curl http://localhost:8766/index.html` returns HTML.
- `curl http://localhost:8766/nonexistent` returns 404.
- `curl http://localhost:8766/` serves `index.html`.
- Build produces no new warnings (`-Wall -Wextra`).

---

### T003 — Implement web/index.html with basic layout
**Status**: ✅ COMPLETED
**Goal**: Create the main page structure with placeholders for UI regions.
**Content**:
- DOCTYPE, head (title, CSS link), body.
- Input control panel (buttons: Start Mic, Upload File, End Session).
- Transcript panel (div for appending utterances).
- Metrics panel (div for displaying compute time, RTF, audio duration).
- Timeline placeholder (empty div, filled later).
- Script tags to load JS modules.
**Verification**:
- Page renders in browser without JavaScript errors (check console).
- All buttons are clickable (no-op handlers ok for now).
- Layout is readable (buttons, text areas visible).

---

### T004 — Implement web/style.css with responsive layout
**Status**: ✅ COMPLETED
**Goal**: Style the UI for desktop and tablet.
**Content**:
- Base layout: 3-column (input, transcript, metrics) or 2-section (transcript left, metrics right).
- Input panel: Flex row with buttons, progress bar.
- Transcript panel: Scrollable div, monospace font.
- Metrics panel: Fixed width, background highlight.
- Timeline area: Placeholder, will be Canvas later.
- Responsive: On tablet (max-width 768px), stack into 2 rows.
- Color scheme: Light theme (white/gray) or dark theme (configurable via CSS var).
**Verification**:
- Page renders on desktop (1920×1080) without overflow.
- Page renders on tablet (768×1024) with readable layout (no horizontal scroll).
- Buttons are 40px height (touch-friendly).
- Transcript panel scrolls independently.

---

### T005 — Implement web/ws_client.js with connection and event parsing
**Status**: DRAFT
**Goal**: Establish WebSocket connection and dispatch events to listeners.
**Functions**:
- `WsClient.connect(host, port)`: Create WebSocket, add event listeners (open, message, close, error).
- `WsClient.addEventListener(type, callback)`: Register handlers for "ready", "asr", "timeline", "error", "close".
- `WsClient.send(data)`: Send binary or text to server.
- `WsClient.isConnected()`: Return true if open.
- `WsClient.reconnect(backoffMs)`: Exponential backoff (1s, 2s, 4s, ..., cap at 30s).
**Verification**:
- Create WebSocket, verify connection open.
- Parse `{"type":"ready",...}` and dispatch "ready" event.
- Simulate `{"type":"asr",...}` message (send via browser DevTools); verify "asr" event fired.
- Close connection; verify reconnect attempts logged to console.
- No JavaScript errors in browser console.

---

### T006 — Implement web/ui_controller.js with basic state and DOM updates
**Status**: DRAFT
**Goal**: Manage application state and update DOM in response to WebSocket events.
**State**:
- `uiState.transcript`: Array of {start, end, speaker, text}.
- `uiState.metrics`: {audio_sec, wall_sec, stream_rtf, diar_rtf, asr_rtf, gpu_occupancy}.
- `uiState.connectionStatus`: "connecting", "ready", "closed", "error".
**Functions**:
- `onReady(msg)`: Update connection status; enable input buttons.
- `onAsr(msg)`: Parse {start, end, speaker, text}; append to transcript array; update DOM with new utterance.
- `onTimeline(msg)`: Parse metrics; update metrics panel DOM.
- `onError(msg)`: Display error alert.
- `updateTranscriptPanel()`: Batch DOM updates; append new utterances without full re-render.
- `updateMetricsPanel()`: Update displayed values (audio_sec, RTF, etc.).
**Button Handlers** (stubs for now):
- `startMic()`: Log "mic started" (implement in T009).
- `uploadFile()`: Log "file upload dialog" (implement in T010).
- `endSession()`: Send `{"end"}` to server; disable buttons.
- `clearSession()`: Reset state, show confirmation dialog.
**Verification**:
- Simulated WebSocket events trigger DOM updates.
- Transcript panel appends utterances in reading order.
- Metrics panel displays values with correct formatting (e.g., "4.5x RTF").
- No JavaScript errors.
- Transcript and metrics update without full page refresh (inspect DOM before/after).

---

### T007 — Implement basic timeline rendering (HTML list, no Canvas yet)
**Status**: DRAFT
**Goal**: Display diarization and ASR entries as an HTML list (MVP).
**Content**:
- Parse final `{"type":"timeline",...}`.
- For diarization track: list entries as `[0.0–4.32] Speaker_0 (conf: 0.94)`.
- For ASR track: list entries as `[12.34–15.67] Speaker_0: "Hello, how are you"`.
- For comprehensive track (if present): list speaker turns.
- Sorted by start time.
**Verification**:
- Load sample timeline JSON (from test data or manual JSON).
- Render as HTML list in timeline div.
- All entries visible and formatted correctly.
- Timestamps in seconds (e.g., "12.34"), speakers labeled (e.g., "Speaker_0"), text visible.

---

### T008 — Integration test: Full MVP flow (basic)
**Status**: DRAFT
**Goal**: End-to-end test: start server, send audio via WebSocket, verify UI updates.
**Steps**:
1. Start `./build/orator_ws 8765 <models>...`.
2. Open `http://localhost:8766` in browser.
3. Verify page loads, connection status shows "Ready".
4. Use existing `ws_client_test.py` or a new minimal Python client to send PCM frames.
5. Observe transcript panel updates in real time (utterances appear as they arrive).
6. Send `{"end"}` from Python client.
7. Verify timeline renders in browser.
8. Verify metrics match server output (compare with server logs or `verify_streaming.cc` output).
**Verification**:
- Page never crashes; all messages logged.
- Transcript updates visible in < 500 ms per utterance.
- Timeline renders correctly (diarization + ASR).
- Metrics (RTF, audio_sec) match server-side calculations within 1% tolerance.

---

## Phase 2: Enhanced Visualization — Canvas Timeline, Zoom/Pan (NOT-STARTED)

### T009 — Implement Canvas-based timeline rendering
**Status**: NOT-STARTED
**Goal**: Replace HTML list with graphical Canvas timeline.
**Content**:
- `timeline_renderer.js` rewritten to use Canvas 2D API.
- Render horizontal tracks:
  - Diarization: colored rectangles (speaker ID → color), with confidence label.
  - ASR: text boxes positioned by start/end time.
  - Comprehensive (optional): speaker turns in combined track.
- Time axis: horizontal with labeled ticks (every 30s or adaptive).
- Legend: speaker ID ↔ color mapping.
- Dimensions: full canvas width (responsive), ~200px height per track.
**Verification**:
- Load sample timeline; render graphically.
- Speakers have distinct colors (e.g., Speaker_0 = blue, Speaker_1 = red).
- Confidence values displayed or indicated visually (opacity, bar height).
- Text legible at typical zoom (no overlap).
- Responsive: resizes with window width.

---

### T010 — Add zoom and pan controls to timeline
**Status**: NOT-STARTED
**Goal**: Allow users to zoom in/out and scroll horizontally.
**Controls**:
- Zoom buttons (+, −) or slider.
- Scroll bar (horizontal).
- Mouse wheel to zoom (optional).
- Double-click to reset zoom.
**Implementation**:
- Maintain a `timeScale` (pixels per second).
- On zoom: recalculate timeScale, re-render.
- On pan: adjust horizontal offset, re-render.
**Verification**:
- Zoom in: more detail visible (narrower time range).
- Zoom out: wider time range.
- Pan: scroll left/right to see different time regions.
- Zoom level persists during session.

---

## Phase 3: Full Features — Microphone, File Upload, Optimization (NOT-STARTED)

### T011 — Implement microphone capture in web/audio_input.js
**Status**: NOT-STARTED
**Goal**: Capture real-time audio from user's microphone.
**Steps**:
1. Request permission: `navigator.mediaDevices.getUserMedia({audio: true})`.
2. Create `AudioContext` (sample rate 48 kHz typical; resample to 16 kHz).
3. Create `MediaStreamAudioSourceNode` from microphone stream.
4. Create `ScriptProcessorNode` (or `AudioWorklet` for lower latency) with buffer size 4096.
5. On `onAudioProcess`: collect float32 samples, resample to 16 kHz, convert to int16LE PCM.
6. Send PCM frames to WebSocket at real-time rate or as-fast-as-possible.
7. Implement level meter (visual feedback of input level).
**Verification**:
- Click "Start Mic" button → permission prompt appears.
- Grant permission → level meter shows input level in real time.
- Audio frames sent to server; server receives PCM and processes (verify via transcript updates).
- RTF and compute times are correct (compare with file upload for same audio).
- No audio skips or glitches.

---

### T012 — Implement file upload in web/audio_input.js
**Status**: NOT-STARTED
**Goal**: Allow users to upload audio files (WAV, MP3, FLAC).
**Steps**:
1. Create file input (`<input type="file" accept="audio/*">`).
2. Implement drag-and-drop to file input area.
3. On file select: read as `ArrayBuffer`.
4. Decode using `AudioContext.decodeAudioData()` (browser-native).
5. Resample to 16 kHz if needed.
6. Convert float32 to int16LE PCM.
7. Send frames to WebSocket at controlled rate (real-time or faster).
8. Show progress bar (bytes sent / total bytes).
**Verification**:
- Drag-and-drop WAV file; decoded and sent to server.
- File upload with MP3: browser decodes, then sends PCM.
- Progress bar shows upload progress.
- Final timeline matches that from native `asr_stream_test` on the same audio.
- Error handling: reject files > 1 GB, show error for unsupported format.

---

### T013 — Performance optimization for large recordings
**Status**: NOT-STARTED
**Goal**: Handle long recordings (> 2 hours) without UI lag.
**Steps**:
1. Implement viewport clipping in Canvas rendering (only render visible portion).
2. Lazy-load transcript entries (only show last N utterances; scroll loads more from history).
3. Batch DOM updates (collect changes, apply every 500 ms instead of per-event).
4. Use `requestAnimationFrame` for smooth rendering.
**Verification**:
- Load 10-hour timeline JSON; Canvas renders without lag (< 60 ms per frame).
- Scroll transcript panel with 1000+ utterances; remains responsive.
- Zoom and pan operations complete in < 200 ms.

---

### T014 — Add metrics dashboard with live updates
**Status**: NOT-STARTED
**Goal**: Display detailed performance metrics alongside timeline.
**Content**:
- Summary section: audio_sec, wall_sec, stream_rtf.
- Per-pipeline section: diar_compute_sec, diar_rtf, asr_compute_sec, asr_rtf.
- GPU section (if available from Spec 002): occupancy%, stream priority, mode.
- Optional: history graph (RTF over time) using simple Canvas or SVG.
**Verification**:
- Metrics update in real time as events arrive.
- Values match server-side JSON (cross-check with logs).
- GPU telemetry displayed correctly if Spec 002 messages arrive.

---

### T015 — Polish responsive design and browser compatibility
**Status**: NOT-STARTED
**Goal**: Ensure UI works on Chrome, Firefox, Safari, and tablet devices.
**Steps**:
1. Test on Chrome 120+, Firefox 121+, Safari 17+ (desktop).
2. Test on iPad (768×1024), Android tablet (800×600).
3. Adjust layout breakpoints in CSS media queries.
4. Ensure touch targets are ≥ 44px.
5. Add ARIA labels for screen reader accessibility.
6. Test with keyboard navigation (Tab, Enter, Arrow keys).
**Verification**:
- Page renders identically (minor browser differences ok) on all 3 browsers.
- Touch-friendly on tablet (no pinch-zoom needed for normal use).
- Keyboard-navigable (all buttons accessible via Tab).
- Screen reader announces all controls and status updates.

---

### T016 — Final testing and documentation
**Status**: NOT-STARTED
**Goal**: Comprehensive testing, documentation, code review.
**Steps**:
1. Unit tests (optional): Mock WebSocket; test state transitions in ui_controller.
2. Integration test: Full flow with real server (T008 extended).
3. Performance test: Measure page load time (< 500 ms), timeline render time (< 1 s for 2-hour recording).
4. User testing: Have someone unfamiliar with Orator use the UI; collect feedback.
5. Documentation:
   - README in `web/` directory.
   - Developer guide for future UI modifications.
   - Screenshot or video demo.
6. Code review: Check for best practices, no console errors, clean commits.
**Verification**:
- All Acceptance Criteria (AC1–AC9 from spec.md) verified.
- Build clean: `cmake --build build -j` produces no warnings.
- Tests pass: `ctest --output-on-failure` includes new UI tests (if added).
- Documentation is present and clear.

---

## Summary Status

| Phase | Task | Status | Priority |
|-------|------|--------|----------|
| 1 | T001 | DRAFT | HIGH |
| 1 | T002 | DRAFT | HIGH |
| 1 | T003 | DRAFT | HIGH |
| 1 | T004 | DRAFT | HIGH |
| 1 | T005 | DRAFT | HIGH |
| 1 | T006 | DRAFT | HIGH |
| 1 | T007 | DRAFT | HIGH |
| 1 | T008 | DRAFT | HIGH |
| 2 | T009 | NOT-STARTED | MEDIUM |
| 2 | T010 | NOT-STARTED | MEDIUM |
| 3 | T011 | NOT-STARTED | MEDIUM |
| 3 | T012 | NOT-STARTED | MEDIUM |
| 3 | T013 | NOT-STARTED | LOW |
| 3 | T014 | NOT-STARTED | MEDIUM |
| 3 | T015 | NOT-STARTED | MEDIUM |
| 3 | T016 | NOT-STARTED | HIGH |

---

## Execution Order

1. **Phase 1 Foundation** (T001–T004): Set up directories and basic HTTP server.
2. **Phase 1 Frontend** (T005–T007): Implement JavaScript modules and basic rendering.
3. **Phase 1 Validation** (T008): End-to-end test.
4. **Phase 2 & 3** (T009–T016): Enhancements and full features (parallel or sequential, depends on priority).

---

## Rollback / Undo

If Spec 006 is not accepted or blocked, rollback steps:
1. `git revert` commits for Spec 006 (if any).
2. Remove `web/` directory.
3. Remove HTTP server code from `orator_ws` (revert `ws_main.cc`).
4. Revert CMakeLists.txt if modified.
5. All pipeline code (Specs 001–005) remains unaffected; WebSocket protocol unchanged.
