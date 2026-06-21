# Frontend Architecture — Orator Web UI

**Domain:** Web UI, Canvas timeline visualization, WebSocket client, real-time dashboard
**Triggers:** UI changes, new visualization, layout redesign, WebSocket client logic, style changes

---

## When to Use

- Modifying `web/index.html`, `web/app.js`, `web/style.css`
- Adding new visualization components (timeline, charts, gauges)
- Changing WebSocket message handling or protocol envelope unwrapping
- Any UI/UX change to the real-time dashboard

---

## Architecture Overview

```
web/
├── index.html    # SPA shell — CANVAS + controls + metadata panels
├── style.css     # Dark theme, responsive layout, no framework
└── app.js        # State machine: connect → describe → timeline streaming
```

### Current stack
- **No framework** — vanilla JS SPA (zero dependencies)
- **Canvas rendering** — timeline visualization with zoom/pan
- **WebSocket** — native `WebSocket` with auto-reconnect
- **Protocol** — Spec 004 topic-based envelope (`unwrapEnvelope()`)
- **Backend** — HTTP static server (`http_static_server.h/.cc`), port = WS port + 1

---

## Key Patterns

### WebSocket message flow
```
connect → send {"describe": {}} → receive envelope {"topic":"describe","data":{...}}
        → receive stream {"topic":"diar"|"asr"|"asr_partial"|"vad"|"timeline"|"revision"}
        → unmarshal via unwrapEnvelope() → dispatch by topic
```

### Timeline rendering
- Canvas-based with `requestAnimationFrame` render loop
- Zoom: mouse wheel → scale time axis
- Pan: drag → shift viewport
- Layers: diarization tracks (colored bars) + ASR text overlay
- Performance: only redraw visible viewport, batch DOM updates

### Auto-reconnect
- Exponential backoff: 1s → 2s → 4s → ... → 30s max
- Full state reset on reconnect (request fresh `describe`)

---

## Change Guidelines

| Change Type | Files | Notes |
|-------------|-------|-------|
| Visual/layout | `style.css`, `index.html` | Maintain dark theme, responsive design |
| Canvas rendering | `app.js` render functions | Use `requestAnimationFrame`, respect zoom/pan state |
| WebSocket protocol | `app.js` WS handlers | Keep envelope unwrapping (`unwrapEnvelope()`), dispatch by topic |
| New data display | `index.html` panel + `app.js` | Follow existing panel pattern (GPU telemetry, metadata) |

### Constraints
- **Zero npm/runtime dependencies** — the web UI serves from `ORATOR_UI_ROOT` via the built-in HTTP server, no build step
- **Keep Spec 004 envelope compatibility** — all WS messages go through `unwrapEnvelope()`
- **Test via** `tools/ws_ui_integration_test.py` — envelope-aware message parsing
- **Don't break server-served mode** — UI is served from C++ HTTP server, not a dev server

---

## Anti-Patterns

- **Adding npm/build tooling** — no bundler, no transpiler, no framework. The UI is served as raw `.html`/`.js`/`.css`.
- **Breaking envelope compatibility** — every WS message must remain wrappable in `{"topic":...,"data":...}`
- **Heavy DOM during streaming** — Canvas for timeline; avoid creating thousands of DOM nodes per second
- **CSS framework imports** — no Tailwind, Bootstrap, etc. Pure CSS only.

---

## Relationship with SDD

- Web UI work follows Spec 006 (`specs/006-web-ui/`): spec.md → plan.md → tasks.md
- Protocol changes (envelope format, new topics) must be vetted through Spec 004
- Visual changes should be validated manually or via screenshot comparison
