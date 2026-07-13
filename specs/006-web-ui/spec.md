# Spec 006 — Web UI Client for Real-Time Dual-Pipeline Transcription

- **Status**: In progress (updated 2026-07-13). The modular ES client, complete
  typed-event routing, live observability, stable-ID convergence, session
  persistence/load, exact export, reconnect reset, and file/microphone input
  paths are implemented. A real Chromium + real WebSocket 12 s run passed file
  upload, live rows, terminal reconciliation, export, reload, restart/reconnect,
  responsive screenshots, and fake-device microphone capture. The requested
  graphical multi-track timeline/zoom controls, physical microphone evidence,
  and non-Chromium compatibility evidence remain open. See §4b and §4c.
- **Created**: 2026-06-18
- **Owner**: project owner

## 1. Summary

A browser-based web UI for **live visualization and interaction** with the Orator
WebSocket service. The client connects to a running `orator_ws` server, streams
audio (microphone/file), and renders the output of **all five pipelines**
(diarization, ASR, VAD, speaker identity, forced alignment) plus the derived
comprehensive speaker-turn timeline, on one shared absolute time base, in real
time. It also surfaces **live observability** — per-pipeline real-time factor,
GPU scheduling class/priority/activity, and per-pipeline audio backlog — so the
same run can be watched for both content and health. The browser renders CJK
text natively, so the Web UI is the **home for readable transcript/speaker
content** (the offline rerun dashboard, Spec 011, covers deep metric history).
The UI is served by an HTTP endpoint built into `orator_ws`, with no external
framework or build step.

---

## 2. Goals

- **G1** Provide an accessible, modern interface for end users to transcribe audio with speaker separation without writing code.
- **G2** Display ASR and diarization results **live** (incremental updates) and final (comprehensive timeline), aligned on one shared time base.
- **G3** Support **multiple input methods**: file upload, real-time microphone capture, and URL-based test data.
- **G4** Visualize **speaker activity** (segments with confidence) and **utterance text** (with timing and speaker attribution).
- **G5** Expose **pipeline performance metrics** (real-time factors, compute time) for debugging and optimization.
- **G6** Serve the UI from a simple HTTP endpoint (e.g., `/`), launched by the main `orator_ws` binary without external dependencies.

---

## 3. User Scenarios

### Scenario A — Live Transcription with Visualization
A user opens `http://localhost:<ui_port>` in a browser, starts recording their microphone or uploads an audio file. The UI connects to `ws://localhost:<ws_port>` for streaming data. As audio streams to the server, they see:
- ASR utterances appear and update in real time.
- Speaker segments appear below, color-coded by speaker ID.
- A horizontal timeline shows progress and speaker activity.
After transcription completes, the final comprehensive timeline is displayed with full diarization/ASR alignment.

### Scenario B — Batch File Processing
A user uploads a pre-recorded MP3 or WAV file. The system decodes it client-side (or via server) to PCM, streams it to Orator over WebSocket, and presents the full timeline with performance metrics (RTF, compute time) once complete.

### Scenario C — Performance Monitoring
A developer or operator views the real-time performance dashboard: wall-clock time, pipeline compute time, real-time factors per pipeline, and GPU telemetry. This aids in identifying bottlenecks and validating concurrency settings.

---

## 4. Functional Requirements

### FR1 — Web Server Integration
- The UI is served as static HTML/CSS/JavaScript from a built-in HTTP server within `orator_ws`.
- Serve static assets at root path `/` on a dedicated UI port (default `ws_port + 1`).
- No external web framework required (use minimal C++ HTTP library or built-in socket).

### FR2 — WebSocket Client Connection
- Client pre-fills the server's WebSocket address automatically and allows manual override for testing.
- Auto-reconnect on network loss; display connection status to the user.

### FR3 — Audio Input
- **Microphone**: Real-time capture using Web Audio API; stream to WebSocket as int16LE PCM at 16 kHz.
- **File upload**: Accept MP3, WAV, FLAC; decode client-side using a web audio library or server-side and re-stream.
- **Test URL**: Load a test recording from a configurable endpoint for reproducible demos.

### FR4 — Incremental Result Display
- **ASR events**: Display each completed utterance as it arrives (`{"type":"asr",...}`); append to a transcript panel with timestamp and speaker attribution.
- **Diarization hints**: Show speaker activity predictions before the final timeline (from periodic diarization snapshots if available).

### FR5 — Timeline Visualization
- Render the final `{"type":"timeline",...}` as a horizontal scrollable timeline with:
  - **Diarization track**: colored horizontal bars per speaker with confidence overlay.
  - **ASR track**: text utterances positioned at their (start, end) on the time axis.
  - **Comprehensive track** (if present): speaker turns with combined text.
- Synchronize all tracks on one time axis; allow scrubbing and seeking.

### FR6 — Performance Metrics Panel
- Display alongside the timeline:
  - Audio duration (seconds).
  - Wall-clock time (seconds).
  - Real-time factor (audio/wall).
  - Per-pipeline compute time and RTF (diarization, ASR).
  - GPU telemetry if available (occupancy, stream priority).

### FR7 — Controls
- **Start/Stop Recording**: Toggle microphone capture.
- **Flush**: Send `{"flush"}` to emit the timeline so far; streaming continues.
- **End**: Send `{"end"}` to finalize and show the complete result.
- **Download**: Export the final timeline as JSON.
- **Clear**: Reset state and start a fresh session.

### FR8 — Error Handling
- Display connection errors, audio input permission failures, and server errors in a user-facing alert.
- Retry logic for transient failures.
- Graceful fallback if WebSocket is unavailable.

### FR9 — Accessibility & Responsive Design
- Responsive layout (desktop, tablet, mobile).
- Keyboard navigation (play/pause, export).
- ARIA labels for screen reader support.
- High contrast for readability on various displays.

---

## 4b. Functional requirements — Phase 2 rebuild (current project)

The MVP (FR1–FR9) targeted the dual-pipeline (diarization + ASR) era and a final
`timeline` render. The project has since added VAD, speaker identity (Spec 010),
forced alignment (Spec 009), the protocol envelope (Spec 004) and observability
telemetry (Spec 011). The rebuild adds:

### FR10 — Complete WS message coverage (no dropped data)
- A single envelope-aware router consumes **every** server message: `ready`,
  `asr_partial`, `asr`, `revision`, `align`, `vad_state`, `timeline`,
  `gpu_telemetry`, `cursor_progress`, `sessions`, `reset_ok`, `describe`,
  `error`. (The MVP silently dropped `align` and `cursor_progress`.)
- Spec 004 envelope (`{topic,pipeline,msg_id,data}`) is unwrapped for enveloped
  events; raw RPC responses are handled directly. Unknown topics are logged, not
  crashed.

### FR11 — Global speaker identity (Spec 010)
- Diarization and comprehensive entries render by **global** `speaker_id`
  (`spk_N`) with `speaker_name` when present, falling back to the diarizer-local
  `speaker` index. Stable per-identity color across transcript, comprehensive
  view, and timeline.

### FR12 — Forced alignment (Spec 009)
- The `align` track and live `align` events are rendered: per-segment
  character/word units with their timestamps, shown as an alignment lane on the
  timeline and (optionally) karaoke-style highlighting in the transcript.

### FR13 — Live observability panel
- A dashboard fed by `gpu_telemetry` + `cursor_progress`, showing per pipeline
  (diar/asr/vad): **real-time factor** (live value + recent sparkline),
  scheduling **class** (foreground/background) + **cuda_priority** +
  **stream_active**, and **backlog** (`pending_sec`). Backlog rising while
  RTF < 1 is surfaced as a starvation warning (cross-dimension diagnosis,
  consistent with Spec 011 methodology).

### FR14 — Comprehensive live timeline
- The comprehensive speaker-turn view updates **incrementally** from `revision`
  events (not only the final `timeline`), each turn labelled by global identity
  and carrying its text; the final `timeline` reconciles the full view.

## 4c. Functional requirements — Phase 3 contract hardening

### FR15 — Stable session and evidence convergence
- Diarization and VAD evidence committed to the typed server timeline must also
  be emitted as self-describing live WebSocket events and update their browser
  tracks without changing the raw evidence.
- Partial ASR, final ASR, alignment, business revisions, terminal tracks,
  loaded-session state, and downloaded JSON must preserve the same `text_id`.
- A terminal or loaded `timeline` is authoritative: the browser rebuilds ASR,
  alignment, raw tracks, and business turns from that document and removes
  stale live-only state.
- Because the server starts a fresh session on every WebSocket open, reconnect
  must clear prior session state before accepting IDs that restart at zero.
- Revision-before-final-ASR ordering must retain speaker evidence and apply it
  when the matching ASR row arrives.

### FR16 — Contract validation
- Dependency-free JavaScript tests must cover envelope inference, raw errors,
  revision-before-final ordering, partial/final replacement, terminal rebuild,
  live diar/VAD updates, and reconnect reset.
- Final acceptance still requires the real browser, real WebSocket, microphone,
  reconnect, and JSON export path; module tests alone are not browser evidence.

---

## 5. UI Layout & Components

### 5.1 Main Layout (Desktop)

```
┌─────────────────────────────────────────────────────────────┐
│  Orator Transcription UI                        [Connection] │
├─────────────────────────────────────────────────────────────┤
│                                                               │
│  Input Control Panel                                         │
│  ┌─────────────────────────────────────────────────────┐    │
│  │ [🎤 Start Mic] [📁 Upload File] [⏹ End Session]    │    │
│  │ Duration: 00:00  |  Progress: [════════░░░░░░░░]   │    │
│  └─────────────────────────────────────────────────────┘    │
│                                                               │
│  Transcript Panel (Left, scrollable)        Metrics (Right)  │
│  ┌──────────────────────────┐  ┌───────────────────────┐    │
│  │ Real-time ASR:           │  │ Performance Metrics:  │    │
│  │                          │  │ ─────────────────────│    │
│  │ [12s] Speaker_0:         │  │ Audio: 120.0s         │    │
│  │ "Hello, how are you"     │  │ Wall: 25.3s           │    │
│  │                          │  │ RTF: 4.75x            │    │
│  │ [15s] Speaker_1:         │  │ ─────────────────────│    │
│  │ "I'm fine, thanks"       │  │ Diar RTF: 9.2x        │    │
│  │                          │  │ ASR RTF: 4.1x         │    │
│  └──────────────────────────┘  │ GPU Occ: 45%          │    │
│                                │ ─────────────────────│    │
│  Timeline Visualization        │ [📥 Download JSON]    │    │
│  ┌──────────────────────────────────────────────────┐      │
│  │ Diarization ┌─Speaker_0──┐┌─Speaker_1─┐         │      │
│  │             │conf:0.94   ││conf:0.87  │         │      │
│  │             └───────────┘└──────────┘         │      │
│  │                                               │      │
│  │ ASR         │Hello, how are you│I'm fine...│ │      │
│  │             └──────────────────┘────────────┘ │      │
│  │ ┴─────────┬──────────┬──────────┬──────────┘ │      │
│  │ 0s        30s       60s        90s   120s    │      │
│  │ [◀  ▶]  [⏸ ⏯]  [🔍+]              [🔍-]     │      │
│  └──────────────────────────────────────────────────┘      │
│                                                               │
└─────────────────────────────────────────────────────────────┘
```

### 5.2 Core Components

#### Input Control Panel
- **Microphone toggle**: Real-time PCM stream with visual level meter.
- **File upload**: Drag-and-drop or file picker for audio files.
- **Progress bar**: Shows audio duration, current playback/encoding position.
- **Session controls**: Flush, End, Clear buttons.

#### Transcript Panel
- **Incremental display**: Each ASR event appended with timestamp and speaker label.
- **Scrollable history**: Full conversation transcript.
- **Copy/export**: Ability to select and copy text, or export as plain text or JSON.

#### Timeline Visualization
- **Current implementation**: authoritative live/terminal JSON inspection. The
  graphical controls below are acceptance requirements, not current features.
- **Horizontal scrollbar**: Pan across long recordings.
- **Zoom controls**: Zoom in/out on time axis.
- **Playback sync** (optional, future): Seek and replay audio at selected position.
- **Color legend**: Speaker IDs mapped to distinct colors (auto-assigned, customizable).

#### Metrics Panel
- **Real-time updates**: Display latest compute/RTF values as they arrive.
- **Collapsible sections**: Summary view (primary metrics) vs. detailed view (GPU, per-pipeline).
- **History graph** (optional, future): Plot RTF over time to detect variability.

---

## 6. Technical Architecture

### 6.1 Frontend (Browser)

**Technology Stack**:
- **HTML5 + CSS3** (no CSS framework required; custom CSS for layout).
- **Vanilla JavaScript** (no jQuery or framework; Web APIs only).
- **Web Audio API**: Microphone access, client-side audio processing.
- **Canvas**: telemetry sparklines only; the main timeline is currently a
  formatted JSON view.
- **WebSocket API**: Real-time communication with server.

**Key Modules**:
- `js/ws.js`: connection, reconnect, protocol-envelope decode, typed routing.
- `js/model.js`: authoritative browser session state and terminal convergence.
- `js/app.js`: controls, lifecycle, and render scheduling.
- `js/audio.js`: microphone/file input and exact PCM framing.
- `js/render/*.js`: transcript, telemetry, speakers, sessions, developer status,
  and formatted timeline JSON.

### 6.2 Backend (C++)

**Changes to `orator_ws`**:
1. **Embedded HTTP server** (using `net/websocket_server.h` base or minimal extension):
   - Serve static files (HTML, CSS, JS) at root `/`.
   - Keep WebSocket at existing port.
2. **UI directory**: `web/` folder bundled into the binary (as embedded strings or files, or served from disk if present).
3. **No new network dependencies**: Reuse existing WebSocket infrastructure.

**File Structure**:
```
web/
  |-- index.html
  |-- style.css
  `-- js/
      |-- app.js
      |-- audio.js
      |-- format.js
      |-- model.js
      |-- ws.js
      `-- render/
```

---

## 7. Data Flow

### 7.1 Streaming Lifecycle

```
[User Opens Browser]
        ↓
[Page loads; ws_client.js connects to ws://host:port]
        ↓
[{"type":"ready",...} received → UI enables input controls]
        ↓
[User starts microphone OR uploads file]
        ↓
[Audio frames sent to server over WebSocket (binary)]
        ↓
[Server processes; sends {"type":"asr",...} incremental updates]
        ↓
[UI appends utterances to transcript panel in real time]
        ↓
[User clicks "End" or sends {"end"} message]
        ↓
[Server sends {"type":"timeline",...} final result]
        ↓
[UI renders full timeline; displays metrics]
        ↓
[User can download JSON, clear, and start a new session]
```

### 7.2 Event Messages (from WebSocket)

**Inbound (Server → Client)**:
```json
{"type":"ready","sample_rate":16000,"asr":true,"time_base":"absolute_samples","origin_sample":0}

{"type":"asr","start":12.34,"end":15.67,"text":"Hello, how are you"}

{"type":"timeline","schema_version":1,"audio_sec":120.0,"sample_rate":16000,"tracks":[
  {"kind":"diarization","source":"sortformer","compute_sec":12.5,"real_time_factor":9.6,"entries":[{"start":0.0,"end":4.32,"speaker":0,"confidence":0.94}]},
  {"kind":"asr","source":"qwen3_asr","compute_sec":28.5,"real_time_factor":4.2,"entries":[{"start":12.34,"end":15.67,"text":"Hello, how are you"}]}
]}

{"type":"gpu_telemetry","gpu_mode":"ASR-concurrent","streams":[...]}  [optional Spec 002 metric]
```

**Outbound (Client → Server)**:
```json
{"format":"f32"}  [optional; switch to float32 PCM]

{"flush"}

{"end"}

{"reset"}
```

---

## 8. Implementation Plan

### Phase 1: MVP (Minimum Viable Product)
1. Create `web/` directory with skeleton HTML/JS.
2. Implement `ws_client.js`: connect to WebSocket, parse JSON events.
3. Implement `ui_controller.js`: update DOM with ASR utterances and final timeline summary.
4. Extend `orator_ws` to serve HTTP (wrap existing socket with a simple file-serving layer).
5. Render timeline as a simple **text-based summary** (no canvas initially).

### Phase 2: Enhanced Visualization
1. Implement `timeline_renderer.js` using Canvas or SVG for graphical timeline.
2. Add **color-coded speakers**, **confidence bars**, and **time axis**.
3. Add zoom/pan controls.

### Phase 3: Full-Featured
1. Microphone capture (Web Audio API) and real-time streaming.
2. File upload with client-side decoding (using a codec library if needed, or server-side re-encoding).
3. Performance metrics dashboard with live updates and (optional) history graphs.

---

## 9. Acceptance Criteria

### AC1 — UI Served from `orator_ws`
- Start `./build/orator_ws 8765 <models>...` and navigate to `http://localhost:8766` (or `ORATOR_UI_PORT`).
- Page loads with no external network requests (all assets are local).
- Connection status displayed; WebSocket connects to configured WS URL.

### AC2 — Real-Time ASR Display
- Send audio over WebSocket (binary PCM).
- Each completed utterance appears in the transcript panel within 200 ms of server receipt (user-perceivable latency acceptable for live transcription).
- Utterances are labeled with speaker and timestamp.

### AC3 — Timeline Visualization
- Upon flush/end, the final `{"type":"timeline",...}` is parsed and rendered.
- Diarization and ASR tracks are visible and synchronized on a shared time axis.
- Speaker segments are distinct (color or pattern).

**Current status**: open. The final document is parsed and displayed exactly,
but no synchronized graphical time-axis view is present.

### AC4 — File Upload
- User can drag-and-drop or select an audio file.
- File is decoded to PCM (client or server) and streamed at the requested frame rate.
- Final timeline is correctly generated and displayed.

### AC5 — Metrics Display
- Real-time factors, compute times, and audio duration are displayed accurately.
- Metrics update when new events arrive and are correct in the final timeline JSON.

### AC6 — Controls Functional
- **Start/Stop Mic**, **Upload**, **Flush**, **End**, **Download**, **Clear** buttons all work as intended.
- Session state is properly reset between runs.

### AC7 — Error Handling
- Closed WebSocket or network loss does not crash the UI; appropriate message displayed.
- Microphone permission denial is handled gracefully.
- Malformed JSON or server errors are logged and user-informed.

### AC8 — Quality & Compatibility
- Build produces no new compiler warnings (`-Wall -Wextra`).
- UI is responsive on desktop and tablet (mobile optional for MVP).
- Tested on Chrome, Firefox, Safari (modern versions).
- No new runtime dependencies in C++ or JavaScript.

### AC9 — SDD Consistency
- All code changes documented with evidence in spec, plan, and tasks.
- Final timeline JSON is identical to that produced by the command-line client.
- Performance metrics are consistent with other measurement tools (Spec 002 telemetry).

---

## 10. Out of Scope (for Spec 006)

- **HTTPS/TLS**: UI assumes localhost development setup (can be added later).
- **User authentication**: No login or multi-user support.
- **Audio playback**: No built-in audio player; timeline is visualization only.
- **Mobile app**: Web is mobile-responsive but not a native app.
- **Persistent storage**: No database; session data exists only in-memory during connection.
- **Advanced features**: Speaker re-identification, keyword highlighting, export to subtitles (future specs).

---

## 11. Constraints & Dependencies

- **Runtime**: Pure C++20/CUDA runtime; embedded web server must use only standard C++ or Orator's existing dependencies.
- **Frontend deps**: Vanilla JavaScript, Web APIs only (no npm/build step).
- **Network**: Assumes HTTP and WebSocket on the same host and port (or easily configurable).
- **Accuracy**: UI is a visualization layer; no changes to pipeline accuracy expectations (Spec 001–005 unchanged).

---

## 12. References

- **Spec 001** — Real-Time Streaming Dual Pipeline: Defines WebSocket protocol, JSON schemas, and timeline structure.
- **Spec 002** — GPU Scheduling: Defines GPU telemetry messages (optional for UI, but displayed if present).
- **Spec 004** — ASR Live Revision: Defines revision messages (optional display in UI).
- **net/websocket_server.h** — Existing WebSocket server; can be extended with HTTP serving.
- **Web Audio API** — MDN spec for microphone access and PCM streaming.
- **RFC 6455** — WebSocket protocol (already implemented server-side).
