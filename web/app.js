(function () {
  "use strict";

  /* ── DOM refs ── */
  const $ = (id) => document.getElementById(id);
  const connBadge     = $("connBadge");
  const micBtn        = $("micBtn");
  const uploadBtn     = $("uploadBtn");
  const fileInput     = $("fileInput");
  const flushBtn      = $("flushBtn");
  const endBtn        = $("endBtn");
  const clearBtn      = $("clearBtn");
  const downloadBtn   = $("downloadBtn");
  const progressRow   = $("progressRow");
  const progressFill  = $("progressFill");
  const durationLabel = $("durationLabel");
  const statusLabel   = $("statusLabel");
  const transcriptList= $("transcriptList");
  const liveDraft     = $("liveDraft");
  const draftText     = $("draftText");

  const mAudio     = $("mAudio");
  const mSampleRate= $("mSampleRate");
  const mAsrRtf    = $("mAsrRtf");
  const mDiarRtf   = $("mDiarRtf");
  const mAsrSeg    = $("mAsrSeg");
  const mDiarSeg   = $("mDiarSeg");
  const mVadRtf    = $("mVadRtf");
  const mGpuStatus = $("mGpuStatus");

  // Session history panel
  const sessionList = $("sessionList");
  const refreshSessionsBtn = $("refreshSessionsBtn");

  /* ── State ── */
  let ws = null;
  let micCtx = null, micStream = null, micSrc = null, micProc = null;
  let micRunning = false;
  let fileSending = false;
  let targetSampleRate = 16000;
  let reconnectTimer = null;
  let reconnectAttempt = 0;
  let lastError = "";

  // Transcript rows keyed by text_id
  const asrRows = new Map();
  const MAX_TRANSCRIPT_ROWS = 500;

  // Timeline data for download
  let timelineData = null;

  const MIN_ZOOM = 0.5;
  const MAX_ZOOM = 20;

  // Speaker colors for canvas
  const SPEAKER_COLORS = ["#5b8def","#34d399","#f59e0b","#ef4444","#a78bfa","#22d3ee"];

  /* ── Timeline canvas state ── */
  const timelineCanvas  = document.getElementById("timelineCanvas");
  const timelineCtx     = timelineCanvas ? timelineCanvas.getContext("2d") : null;
  let tracksData        = null;
  let audioDuration     = 0;
  let timeScale         = 100;   // px per second (initial, will auto-fit)
  let panOffset         = 0;
  let isDragging        = false;
  let dragStartX        = 0;
  let dragStartPan      = 0;

  // Layout constants
  var TRACK_HEIGHT      = 40;
  var TIME_AXIS_HEIGHT  = 28;
  var LABEL_WIDTH       = 80;
  var PADDING_TOP       = 4;
  var PADDING_BOTTOM    = 4;

  /* ── Protocol envelope unwrapper ── */
  // Spec 004 protocol layer wraps legacy JSON in a topic-based envelope:
  //   { "topic": "...", "pipeline": "...", "msg_id": N, "ts": T,
  //     "qos": Q, "schema_version": V, "data": <original JSON string> }
  // This function detects and unwraps such envelopes, returning the inner
  // payload. If the message is already in legacy format (no "topic" key),
  // it is returned as-is.
  function unwrapEnvelope(raw) {
    if (typeof raw !== "string") return null;
    let obj;
    try { obj = JSON.parse(raw); } catch (_) { return null; }
    if (!obj || typeof obj !== "object") return null;

    // Envelope detection: presence of "topic" + "data" keys
    if (obj.topic && obj.data !== undefined) {
      // obj.data is the escaped JSON string of the original payload.
      // It may already be a parsed object or a raw JSON string.
      if (typeof obj.data === "string") {
        try { return JSON.parse(obj.data); } catch (_) { return null; }
      }
      return obj.data;
    }

    // Legacy format — return as-is
    return obj;
  }

  /* ── Helpers ── */
  function fmtTime(sec) {
    if (typeof sec !== "number" || isNaN(sec)) return "--:--";
    const m = Math.floor(sec / 60);
    const s = Math.floor(sec % 60);
    const ms = Math.floor((sec % 1) * 10);
    return String(m).padStart(2, "0") + ":" + String(s).padStart(2, "0") + "." + ms;
  }

  function fmtSec(sec) {
    if (typeof sec !== "number") return "-";
    return sec.toFixed(1) + "s";
  }

  function setStatus(online) {
    connBadge.textContent = online ? "● Connected" : "● Disconnected";
    connBadge.className = "badge " + (online ? "online" : "offline");
    const enabled = online && !fileSending;
    micBtn.disabled = !enabled;
    uploadBtn.disabled = !online;
    flushBtn.disabled = !online;
    endBtn.disabled = !online;
    clearBtn.disabled = !online;
  }

  function enableControls(v) {
    micBtn.disabled = !v;
    uploadBtn.disabled = !v;
    flushBtn.disabled = !v;
    endBtn.disabled = !v;
    clearBtn.disabled = !v;
  }

  function showError(msg) {
    lastError = msg;
    connBadge.textContent = "● Error";
    connBadge.className = "badge offline";
    connBadge.title = msg;
  }

  /* ── WebSocket ── */
  function defaultWsUrl() {
    const host = window.location.hostname || "127.0.0.1";
    // Infer WS port from HTTP port: WS = HTTP - 1 (per server convention)
    const httpPort = window.location.port || "8766";
    const wsPort = (parseInt(httpPort, 10) - 1) || 8765;
    return "ws://" + host + ":" + wsPort;
  }

  function connect() {
    if (ws && (ws.readyState === WebSocket.OPEN || ws.readyState === WebSocket.CONNECTING)) return;

    ws = new WebSocket(defaultWsUrl());

    ws.onopen = function () {
      reconnectAttempt = 0;
      lastError = "";
      connBadge.title = "";
      setStatus(true);
      console.log("[WS] Connected to", defaultWsUrl());
      // Request protocol description for debugging (Spec 004 Phase 12)
      describeProtocol();
      // Request session history (Spec 004 Phase 13)
      setTimeout(refreshSessions, 500);
    };

    ws.onclose = function (evt) {
      setStatus(false);
      console.log("[WS] Closed:", evt.code, evt.reason, evt.wasClean);
      if (!lastError) scheduleReconnect();
    };

    ws.onerror = function (evt) {
      showError("WebSocket connection error");
      console.error("[WS] Error:", evt);
    };

    ws.onmessage = function (ev) {
      if (typeof ev.data !== "string") return;
      let msg = unwrapEnvelope(ev.data);
      if (!msg || !msg.type) return;
      console.log("[WS] Received:", msg.type, msg);

      switch (msg.type) {
        case "ready":
          targetSampleRate = msg.sample_rate || 16000;
          mSampleRate.textContent = targetSampleRate;
          break;
        case "asr_partial":
          handleAsrPartial(msg);
          break;
        case "asr":
          handleAsrFinal(msg);
          break;
        case "revision":
          handleRevision(msg);
          break;
        case "diar":
          handleDiar(msg);
          break;
        case "vad":
          handleVad(msg);
          break;
        case "timeline":
          handleTimeline(msg);
          break;
        case "gpu_telemetry":
          handleGpuTelemetry(msg);
          break;
        case "reset_ok":
          handleResetOk();
          break;
        case "vad_state":
          handleVadState(msg);
          break;
        case "sessions":
          handleSessions(msg);
          break;
        default:
          console.warn("[Orator] Unknown WS message type:", msg.type, msg);
          break;
      }
    };
  }

  function scheduleReconnect() {
    if (reconnectTimer) return;
    reconnectAttempt++;
    const delay = Math.min(10000, 500 * Math.pow(2, Math.min(5, reconnectAttempt - 1)));
    reconnectTimer = setTimeout(function () {
      reconnectTimer = null;
      connect();
    }, delay);
  }

  function sendCmd(cmd) {
    if (!ws || ws.readyState !== WebSocket.OPEN) return;
    ws.send(JSON.stringify(cmd));
  }

  // Request protocol description from server (Spec 004 Phase 12).
  // Server responds with topic/schema registry info.
  function describeProtocol() {
    sendCmd({ cmd: "describe" });
  }

  /* ── ASR handlers ── */
  function handleAsrPartial(msg) {
    const id = msg.text_id != null ? msg.text_id : "__draft__";
    const text = msg.text || "";

    // Show live draft
    draftText.textContent = text;
    liveDraft.classList.remove("hidden");

    // Update existing row or create new one
    let row = asrRows.get(id);
    if (!row) {
      row = createTranscriptRow(id, msg.start, msg.end, null, text, "partial");
      asrRows.set(id, row);
    } else {
      updateTranscriptRow(row, msg.start, msg.end, null, text, "partial");
    }
    scrollToBottom();
  }

  function handleAsrFinal(msg) {
    const id = msg.text_id != null ? msg.text_id : ("__f_" + Date.now());
    const text = msg.text || "";

    // Hide live draft
    liveDraft.classList.add("hidden");

    let row = asrRows.get(id);
    if (!row) {
      row = createTranscriptRow(id, msg.start, msg.end, null, text, "confirmed");
      asrRows.set(id, row);
    } else {
      updateTranscriptRow(row, msg.start, msg.end, null, text, "confirmed");
    }
    scrollToBottom();
  }

  function handleRevision(msg) {
    if (!Array.isArray(msg.entries)) return;
    for (const e of msg.entries) {
      const id = e.text_id;
      if (id == null) continue;
      const row = asrRows.get(id);
      if (!row) continue;
      const spk = e.speaker != null ? e.speaker : null;
      updateTranscriptRow(row, e.start, e.end, spk, e.text || "", "revised");
    }
  }

  function handleDiar(msg) {
    // Diarization hints — mainly used by timeline
    // Could show speaker labels in transcript if available
  }

  function handleVad(msg) {
    // VAD segment events
  }

  function handleVadState(msg) {
    const led = $("vadLed");
    if (msg.speech) {
      led.classList.add("active");
    } else {
      led.classList.remove("active");
    }
  }

  /* ── Timeline handler (updates transcript speaker labels + metrics) ── */
  function handleTimeline(msg) {
    timelineData = msg;
    // Update metrics
    mAudio.textContent = fmtSec(msg.audio_sec);
    mSampleRate.textContent = msg.sample_rate || "-";

    const tracks = Array.isArray(msg.tracks) ? msg.tracks : [];
    const asrTrack = tracks.find(function (t) { return t.kind === "asr"; });
    const diarTrack = tracks.find(function (t) { return t.kind === "diarization"; });
    const vadTrack = tracks.find(function (t) { return t.kind === "vad"; });

    if (asrTrack) {
      mAsrRtf.textContent = asrTrack.real_time_factor != null ? asrTrack.real_time_factor.toFixed(1) + "x" : "-";
      mAsrSeg.textContent = (asrTrack.entries || []).length;
    }
    if (diarTrack) {
      mDiarRtf.textContent = diarTrack.real_time_factor != null ? diarTrack.real_time_factor.toFixed(1) + "x" : "-";
      mDiarSeg.textContent = (diarTrack.entries || []).length;
    }
    if (vadTrack) {
      mVadRtf.textContent = vadTrack.real_time_factor != null ? vadTrack.real_time_factor.toFixed(1) + "x" : "-";
    }

    // Update transcript with speaker labels from comprehensive timeline
    const comprehensive = msg.comprehensive || [];
    for (const entry of comprehensive) {
      const id = entry.text_id;
      if (id == null) continue;
      const row = asrRows.get(id);
      if (!row) continue;
      const spk = entry.speaker;
      if (spk != null) {
        const spkEl = row.querySelector(".t-speaker");
        if (spkEl) {
          spkEl.textContent = "S" + spk;
          spkEl.style.background = SPEAKER_COLORS[spk % SPEAKER_COLORS.length];
          spkEl.style.color = "#000";
        }
      }
    }

    // Show download button
    downloadBtn.classList.remove("hidden");

    // Wire into canvas timeline
    tracksData = msg.tracks;
    audioDuration = msg.audio_sec || 0;
    fitTimeline();
  }

  /* ── GPU Telemetry handler ── */
  function handleGpuTelemetry(msg) {
    if (!Array.isArray(msg.pipelines)) return;

    // Reset to defaults
    mVadRtf.textContent = "-";
    mGpuStatus.textContent = "-";

    let asrRtf = null, diarRtf = null, vadRtf = null;
    let activePipelines = [];

    for (const p of msg.pipelines) {
      const name = p.name || "?";
      const rtf = p.real_time_factor;
      const active = p.stream_active;

      if (rtf != null) {
        if (name === "asr") asrRtf = rtf;
        else if (name === "diarization") diarRtf = rtf;
        else if (name === "vad") vadRtf = rtf;
      }
      if (active) activePipelines.push(name);
    }

    // Update RTF metrics from live telemetry (more granular than timeline)
    if (vadRtf != null) mVadRtf.textContent = vadRtf.toFixed(1) + "x";
    if (asrRtf != null) mAsrRtf.textContent = asrRtf.toFixed(1) + "x";
    if (diarRtf != null) mDiarRtf.textContent = diarRtf.toFixed(1) + "x";

    // GPU status shows active pipelines
    mGpuStatus.textContent = activePipelines.length > 0
      ? activePipelines.join(", ")
      : "idle";
  }

  /* ── Reset OK handler ── */
  function handleResetOk() {
    // Visual feedback: flash the badge briefly
    const orig = connBadge.textContent;
    const origClass = connBadge.className;
    connBadge.textContent = "✓ Reset";
    connBadge.className = "badge online";
    setTimeout(function () {
      connBadge.textContent = orig;
      connBadge.className = origClass;
    }, 800);
    // Refresh session list after reset (new session may have been saved).
    setTimeout(refreshSessions, 1000);
  }

  /* ── Session history handlers ── */
  function handleSessions(msg) {
    var sessions = Array.isArray(msg.sessions) ? msg.sessions : (Array.isArray(msg.list) ? msg.list : []);
    renderSessionList(sessions);
  }

  function renderSessionList(sessions) {
    sessionList.innerHTML = "";
    if (!sessions || sessions.length === 0) {
      var empty = document.createElement("div");
      empty.className = "session-empty";
      empty.textContent = "No saved sessions";
      sessionList.appendChild(empty);
      return;
    }
    for (var i = 0; i < sessions.length; i++) {
      var s = sessions[i];
      var item = document.createElement("div");
      item.className = "session-item";
      item.setAttribute("role", "listitem");

      var idEl = document.createElement("span");
      idEl.className = "session-item-id";
      idEl.textContent = s.id || "?";
      idEl.title = s.id || "";

      var timeEl = document.createElement("span");
      timeEl.className = "session-item-time";
      timeEl.textContent = s.time ? fmtTime(s.time) : "";

      var durEl = document.createElement("span");
      durEl.className = "session-item-dur";
      durEl.textContent = s.audio_sec ? fmtSec(s.audio_sec) : "";

      var loadBtn = document.createElement("button");
      loadBtn.className = "session-load-btn";
      loadBtn.textContent = "Load";
      loadBtn.setAttribute("aria-label", "Load session " + (s.id || ""));
      loadBtn.addEventListener("click", function (sid) {
        return function (e) {
          e.stopPropagation();
          loadSession(sid);
        };
      }(s.id || ""));

      item.appendChild(idEl);
      item.appendChild(timeEl);
      item.appendChild(durEl);
      item.appendChild(loadBtn);
      sessionList.appendChild(item);

      // Click on item row also loads
      item.addEventListener("click", function (sid) {
        return function () { loadSession(sid); };
      }(s.id || ""));
    }
  }

  function refreshSessions() {
    sendCmd({ cmd: "sessions" });
  }

  function loadSession(sessionId) {
    sendCmd({ cmd: "load_session", session_id: sessionId });
  }

  function createTranscriptRow(id, start, end, speaker, text, cls) {
    const div = document.createElement("div");
    div.className = "t-item " + cls;
    div.dataset.id = id;

    const timeEl = document.createElement("span");
    timeEl.className = "t-time";
    timeEl.textContent = fmtTime(start) + "–" + fmtTime(end);

    const spkEl = document.createElement("span");
    spkEl.className = "t-speaker";
    if (speaker != null) {
      spkEl.textContent = "S" + speaker;
      spkEl.style.background = SPEAKER_COLORS[speaker % SPEAKER_COLORS.length];
      spkEl.style.color = "#000";
    }

    const textEl = document.createElement("span");
    textEl.className = "t-text";
    textEl.textContent = text;

    div.appendChild(timeEl);
    div.appendChild(spkEl);
    div.appendChild(textEl);
    transcriptList.appendChild(div);

    // Prune oldest rows when over limit to keep DOM lean
    if (asrRows.size > MAX_TRANSCRIPT_ROWS) {
      var oldestId = null;
      var oldestTime = Infinity;
      asrRows.forEach(function (r, id) {
        var ts = parseFloat(r.dataset.start) || 0;
        if (ts < oldestTime) { oldestTime = ts; oldestId = id; }
      });
      if (oldestId != null) {
        var oldRow = asrRows.get(oldestId);
        if (oldRow && oldRow.parentNode) oldRow.parentNode.removeChild(oldRow);
        asrRows.delete(oldestId);
      }
    }

    return div;
  }

  function updateTranscriptRow(row, start, end, speaker, text, cls) {
    row.className = "t-item " + cls;
    row.querySelector(".t-time").textContent = fmtTime(start) + "–" + fmtTime(end);
    const spkEl = row.querySelector(".t-speaker");
    if (speaker != null) {
      spkEl.textContent = "S" + speaker;
      spkEl.style.background = SPEAKER_COLORS[speaker % SPEAKER_COLORS.length];
      spkEl.style.color = "#000";
    }
    row.querySelector(".t-text").textContent = text;
  }

  function scrollToBottom() {
    transcriptList.scrollTop = transcriptList.scrollHeight;
  }

  /* ── Timeline Canvas Rendering ── */
  function fmtMMSS(sec) {
    if (typeof sec !== "number" || isNaN(sec)) return "--:--";
    var m = Math.floor(sec / 60);
    var s = Math.floor(sec % 60);
    return String(m).padStart(2, "0") + ":" + String(s).padStart(2, "0");
  }

  function getTickInterval(scale) {
    var pxPerSec = scale;
    if (pxPerSec < 10) return 60;
    if (pxPerSec < 30) return 30;
    if (pxPerSec < 60) return 10;
    if (pxPerSec < 120) return 5;
    if (pxPerSec < 250) return 2;
    return 1;
  }

  function fitTimeline() {
    if (!timelineCanvas || !tracksData) return;
    var wrap = timelineCanvas.parentElement;
    var availW = wrap ? wrap.clientWidth - 2 : 800;
    if (audioDuration <= 0) audioDuration = 30;
    timeScale = Math.max(MIN_ZOOM, Math.min(MAX_ZOOM, availW / audioDuration));
    panOffset = 0;
    _scheduleRender();
  }

  /* ── rAF render scheduler ── */
  var _renderPending = false;
  function _scheduleRender() {
    if (_renderPending) return;
    _renderPending = true;
    requestAnimationFrame(function () {
      _renderPending = false;
      renderTimeline();
    });
  }

  function zoomTimeline(factor) {
    if (!timelineCanvas) return;
    var oldScale = timeScale;
    timeScale = Math.max(MIN_ZOOM, Math.min(MAX_ZOOM, timeScale * factor));
    // keep center point fixed
    var center = timelineCanvas.width / 2;
    panOffset = panOffset * (timeScale / oldScale) + (center - center * (timeScale / oldScale));
    _scheduleRender();
  }

  function renderTimeline() {
    if (!timelineCanvas || !timelineCtx) return;

    var wrap = timelineCanvas.parentElement;
    var availW = wrap ? wrap.clientWidth - 2 : 800;
    var canvasH = PADDING_TOP + 3 * TRACK_HEIGHT + TIME_AXIS_HEIGHT + PADDING_BOTTOM;
    var dpr = window.devicePixelRatio || 1;

    timelineCanvas.style.width = availW + "px";
    timelineCanvas.style.height = canvasH + "px";
    timelineCanvas.width = availW * dpr;
    timelineCanvas.height = canvasH * dpr;
    timelineCtx.setTransform(dpr, 0, 0, dpr, 0, 0);

    var w = availW;
    var h = canvasH;

    // clear
    timelineCtx.clearRect(0, 0, w, h);

    // background
    timelineCtx.fillStyle = "#232733";
    timelineCtx.fillRect(0, 0, w, h);

    if (!tracksData || !Array.isArray(tracksData)) {
      timelineCtx.fillStyle = "#8b90a0";
      timelineCtx.font = "13px Inter, system-ui, sans-serif";
      timelineCtx.textAlign = "center";
      timelineCtx.textBaseline = "middle";
      timelineCtx.fillText("No timeline data", w / 2, h / 2);
      return;
    }

    var diarEntries = [];
    var asrEntries  = [];
    var vadEntries  = [];

    for (var t = 0; t < tracksData.length; t++) {
      var tr = tracksData[t];
      var kind = tr.kind || "";
      var entries = Array.isArray(tr.entries) ? tr.entries : [];
      if (kind === "diarization") diarEntries = entries;
      else if (kind === "asr") asrEntries = entries;
      else if (kind === "vad") vadEntries = entries;
    }

    // Viewport pre-filter: only iterate entries overlapping the visible time range
    var visibleStart = panOffset < 0 ? 0 : panOffset / timeScale;
    var visibleEnd = audioDuration > 0 ? Math.min(audioDuration, (panOffset + w) / timeScale) : (panOffset + w) / timeScale;
    function filterVisible(arr) {
      var out = [];
      for (var i = 0; i < arr.length; i++) {
        var e = arr[i];
        if (e.end <= visibleStart || e.start >= visibleEnd) continue;
        out.push(e);
      }
      return out;
    }
    if (diarEntries.length > 200) diarEntries = filterVisible(diarEntries);
    if (asrEntries.length > 200)  asrEntries  = filterVisible(asrEntries);
    if (vadEntries.length > 200)  vadEntries  = filterVisible(vadEntries);

    var trackY0 = PADDING_TOP;

    // ── Track 1: Diarization ──
    (function () {
      var y = trackY0;
      timelineCtx.fillStyle = "#8b90a0";
      timelineCtx.font = "bold 11px Inter, system-ui, sans-serif";
      timelineCtx.textAlign = "left";
      timelineCtx.textBaseline = "middle";
      timelineCtx.fillText("Diarization", 6, y + TRACK_HEIGHT / 2);

      for (var i = 0; i < diarEntries.length; i++) {
        var e = diarEntries[i];
        var x = e.start * timeScale - panOffset;
        var bw = (e.end - e.start) * timeScale;
        if (bw < 1 || x + bw < 0 || x > w) continue;
        var color = SPEAKER_COLORS[(e.speaker || 0) % SPEAKER_COLORS.length];
        timelineCtx.fillStyle = color + "cc";
        timelineCtx.fillRect(x, y + 4, bw, TRACK_HEIGHT - 8);
        timelineCtx.strokeStyle = color;
        timelineCtx.lineWidth = 0.5;
        timelineCtx.strokeRect(x, y + 4, bw, TRACK_HEIGHT - 8);
        if (bw > 40) {
          timelineCtx.fillStyle = "#000";
          timelineCtx.font = "bold 10px Inter, system-ui, sans-serif";
          timelineCtx.textAlign = "left";
          timelineCtx.textBaseline = "middle";
          timelineCtx.fillText("S" + (e.speaker || 0), x + 4, y + TRACK_HEIGHT / 2);
        }
      }
    })();

    // ── Track 2: ASR ──
    (function () {
      var y = trackY0 + TRACK_HEIGHT;
      timelineCtx.fillStyle = "#8b90a0";
      timelineCtx.font = "bold 11px Inter, system-ui, sans-serif";
      timelineCtx.textAlign = "left";
      timelineCtx.textBaseline = "middle";
      timelineCtx.fillText("ASR", 6, y + TRACK_HEIGHT / 2);

      for (var i = 0; i < asrEntries.length; i++) {
        var e = asrEntries[i];
        var x = e.start * timeScale - panOffset;
        var bw = (e.end - e.start) * timeScale;
        if (bw < 1 || x + bw < 0 || x > w) continue;
        timelineCtx.fillStyle = "rgba(91,141,239,0.12)";
        timelineCtx.fillRect(x, y + 4, bw, TRACK_HEIGHT - 8);
        timelineCtx.strokeStyle = "rgba(91,141,239,0.4)";
        timelineCtx.lineWidth = 0.5;
        timelineCtx.strokeRect(x, y + 4, bw, TRACK_HEIGHT - 8);
        if (bw > 30 && e.text) {
          timelineCtx.fillStyle = "#c9cdd8";
          timelineCtx.font = "10px Inter, system-ui, sans-serif";
          timelineCtx.textAlign = "left";
          timelineCtx.textBaseline = "middle";
          var txt = e.text;
          var maxChars = Math.floor(bw / 6);
          if (txt.length > maxChars) txt = txt.substring(0, maxChars) + "\u2026";
          timelineCtx.fillText(txt, x + 3, y + TRACK_HEIGHT / 2);
        }
      }
    })();

    // ── Track 3: VAD ──
    (function () {
      var y = trackY0 + 2 * TRACK_HEIGHT;
      timelineCtx.fillStyle = "#8b90a0";
      timelineCtx.font = "bold 11px Inter, system-ui, sans-serif";
      timelineCtx.textAlign = "left";
      timelineCtx.textBaseline = "middle";
      timelineCtx.fillText("VAD", 6, y + TRACK_HEIGHT / 2);

      for (var i = 0; i < vadEntries.length; i++) {
        var e = vadEntries[i];
        var x = e.start * timeScale - panOffset;
        var bw = (e.end - e.start) * timeScale;
        if (bw < 1 || x + bw < 0 || x > w) continue;
        if (e.speech) {
          timelineCtx.fillStyle = "rgba(52,211,153,0.5)";
        } else {
          timelineCtx.fillStyle = "rgba(139,144,160,0.25)";
        }
        timelineCtx.fillRect(x, y + 10, bw, TRACK_HEIGHT - 20);
      }
    })();

    // ── Time axis ──
    (function () {
      var y = trackY0 + 3 * TRACK_HEIGHT;
      var tickInterval = getTickInterval(timeScale);
      var maxTime = audioDuration > 0 ? audioDuration : 30;

      // axis line
      timelineCtx.strokeStyle = "#2e3345";
      timelineCtx.lineWidth = 1;
      timelineCtx.beginPath();
      timelineCtx.moveTo(0, y);
      timelineCtx.lineTo(w, y);
      timelineCtx.stroke();

      // ticks + labels
      timelineCtx.fillStyle = "#8b90a0";
      timelineCtx.font = "10px Inter, system-ui, sans-serif";
      timelineCtx.textAlign = "center";
      timelineCtx.textBaseline = "top";

      var firstTick = Math.ceil((-panOffset / timeScale) / tickInterval) * tickInterval;
      for (var t = firstTick; t <= maxTime + (w / timeScale); t += tickInterval) {
        var tx = t * timeScale - panOffset;
        if (tx < 0 || tx > w) continue;
        timelineCtx.strokeStyle = "#2e3345";
        timelineCtx.lineWidth = 0.5;
        timelineCtx.beginPath();
        timelineCtx.moveTo(tx, y);
        timelineCtx.lineTo(tx, y + 5);
        timelineCtx.stroke();
        timelineCtx.fillText(fmtMMSS(t), tx, y + 8);
      }
    })();

    // ── Track separator lines ──
    timelineCtx.strokeStyle = "#2e3345";
    timelineCtx.lineWidth = 0.5;
    for (var i = 0; i < 3; i++) {
      var ly = trackY0 + (i + 1) * TRACK_HEIGHT;
      timelineCtx.beginPath();
      timelineCtx.moveTo(0, ly);
      timelineCtx.lineTo(w, ly);
      timelineCtx.stroke();
    }
  }

  /* ── Timeline Zoom / Pan Event Wiring ── */
  function wireTimelineEvents() {
    if (!timelineCanvas) return;

    var zoomInBtn  = document.getElementById("zoomInBtn");
    var zoomOutBtn = document.getElementById("zoomOutBtn");
    var zoomResetBtn = document.getElementById("zoomResetBtn");

    if (zoomInBtn)  zoomInBtn.addEventListener("click", function () { zoomTimeline(1.4); });
    if (zoomOutBtn) zoomOutBtn.addEventListener("click", function () { zoomTimeline(0.7); });
    if (zoomResetBtn) zoomResetBtn.addEventListener("click", function () { fitTimeline(); });

    // mouse wheel zoom
    timelineCanvas.addEventListener("wheel", function (e) {
      e.preventDefault();
      var factor = e.deltaY < 0 ? 1.15 : 0.87;
      var oldScale = timeScale;
      timeScale = Math.max(MIN_ZOOM, Math.min(MAX_ZOOM, timeScale * factor));
      // zoom toward mouse position
      var mx = e.offsetX;
      panOffset = panOffset * (timeScale / oldScale) + mx * (1 - timeScale / oldScale);
      _scheduleRender();
    }, { passive: false });

    // drag pan
    timelineCanvas.addEventListener("mousedown", function (e) {
      if (e.button !== 0) return;
      isDragging = true;
      dragStartX = e.clientX;
      dragStartPan = panOffset;
      timelineCanvas.style.cursor = "grabbing";
    });

    window.addEventListener("mousemove", function (e) {
      if (!isDragging) return;
      panOffset = dragStartPan - (e.clientX - dragStartX);
      _scheduleRender();
    });

    window.addEventListener("mouseup", function () {
      if (isDragging) {
        isDragging = false;
        timelineCanvas.style.cursor = "";
      }
    });

    timelineCanvas.addEventListener("mouseleave", function () {
      if (isDragging) {
        isDragging = false;
        timelineCanvas.style.cursor = "";
      }
    });

    // keyboard navigation: arrow keys pan, +/- zoom
    timelineCanvas.addEventListener("keydown", function (e) {
      var step = panOffset * 0.1 + 20;  // adaptive step
      switch (e.key) {
        case "ArrowLeft":  panOffset = Math.max(0, panOffset - step); _scheduleRender(); e.preventDefault(); break;
        case "ArrowRight": panOffset += step; _scheduleRender(); e.preventDefault(); break;
        case "+": case "=": zoomTimeline(1.4); e.preventDefault(); break;
        case "-": case "_": zoomTimeline(0.7); e.preventDefault(); break;
        case "Home": fitTimeline(); e.preventDefault(); break;
      }
    });
  }

  /* ── Microphone ── */
  function f32ToInt16(buf) {
    const out = new Int16Array(buf.length);
    for (let i = 0; i < buf.length; i++) {
      const s = Math.max(-1, Math.min(1, buf[i]));
      out[i] = s < 0 ? Math.round(s * 32768) : Math.round(s * 32767);
    }
    return out.buffer;
  }

  function downsample(input, srcRate, dstRate) {
    if (srcRate === dstRate) return input;
    const ratio = srcRate / dstRate;
    const len = Math.max(1, Math.floor(input.length / ratio));
    const out = new Float32Array(len);
    let pos = 0;
    for (let i = 0; i < len; i++) {
      const end = Math.min(input.length, Math.floor((i + 1) * ratio));
      const start = Math.floor(pos);
      let sum = 0;
      for (let j = start; j < end; j++) sum += input[j];
      out[i] = sum / (end - start);
      pos = end;
    }
    return out;
  }

  async function startMic() {
    if (!ws || ws.readyState !== WebSocket.OPEN) return;
    try {
      micStream = await navigator.mediaDevices.getUserMedia({ audio: true });
      micCtx = new (window.AudioContext || window.webkitAudioContext)();
      micSrc = micCtx.createMediaStreamSource(micStream);
      micProc = micCtx.createScriptProcessor(4096, 1, 1);
      micProc.onaudioprocess = function (ev) {
        if (!ws || ws.readyState !== WebSocket.OPEN) return;
        const ch = ev.inputBuffer.getChannelData(0);
        const down = downsample(ch, micCtx.sampleRate, targetSampleRate);
        if (down.length) ws.send(f32ToInt16(down));
      };
      micSrc.connect(micProc);
      micProc.connect(micCtx.destination);
      micRunning = true;
      micBtn.innerHTML = '<span class="btn-icon">&#x1F3A4;</span> Stop Mic';
      progressRow.hidden = false;
      startProgressTimer();
    } catch (err) {
      console.error(err);
      alert("Microphone permission denied");
    }
  }

  function stopMic() {
    if (micProc) { micProc.disconnect(); micProc.onaudioprocess = null; micProc = null; }
    if (micSrc) { micSrc.disconnect(); micSrc = null; }
    if (micStream) { micStream.getTracks().forEach(function(t){t.stop()}); micStream = null; }
    if (micCtx) { micCtx.close(); micCtx = null; }
    micRunning = false;
    micBtn.innerHTML = '<span class="btn-icon">&#x1F3A4;</span> Start Mic';
    stopProgressTimer();
  }

  /* ── File Upload ── */
  async function sendAudioFile(file) {
    if (!ws || ws.readyState !== WebSocket.OPEN) return;
    if (fileSending) return;
    fileSending = true;
    enableControls(false);
    progressRow.hidden = false;
    statusLabel.textContent = "Decoding...";

    try {
      const audioCtx = new (window.AudioContext || window.webkitAudioContext)();
      const arrayBuf = await file.arrayBuffer();
      const audioBuf = await audioCtx.decodeAudioData(arrayBuf);

      // Get channel data and downmix to mono
      let channelData;
      if (audioBuf.numberOfChannels === 1) {
        channelData = audioBuf.getChannelData(0);
      } else {
        // Mix down to mono
        const len = audioBuf.length;
        channelData = new Float32Array(len);
        for (let ch = 0; ch < audioBuf.numberOfChannels; ch++) {
          const chData = audioBuf.getChannelData(ch);
          for (let i = 0; i < len; i++) channelData[i] += chData[i];
        }
        for (let i = 0; i < len; i++) channelData[i] /= audioBuf.numberOfChannels;
      }

      // Downsample to target rate
      const resampled = downsample(channelData, audioBuf.sampleRate, targetSampleRate);
      const int16 = new Int16Array(resampled.length);
      for (let i = 0; i < resampled.length; i++) {
        const s = Math.max(-1, Math.min(1, resampled[i]));
        int16[i] = s < 0 ? Math.round(s * 32768) : Math.round(s * 32767);
      }
      
      const duration = int16.length / targetSampleRate;
      durationLabel.textContent = fmtTime(duration);
      statusLabel.textContent = "Streaming...";

      // Send in 60ms chunks
      const bytesPerChunk = Math.floor(targetSampleRate * 0.06 * 2); // bytes
      let offset = 0;
      const chunkTime = 60; // ms

      function sendChunk() {
        if (ws.readyState !== WebSocket.OPEN || !fileSending) return;
        const end = Math.min(offset + bytesPerChunk, int16.byteLength);
        
        // Create a proper ArrayBuffer slice
        const slice = new Int16Array(int16.buffer, offset, (end - offset) / 2);
        ws.send(slice.buffer);
        offset = end;

        const progress = (offset / int16.byteLength) * 100;
        progressFill.style.width = progress + "%";

        if (offset < int16.byteLength) {
          setTimeout(sendChunk, chunkTime);
        } else {
          // Done sending — auto-flush
          fileSending = false;
          enableControls(true);
          statusLabel.textContent = "Complete";
          progressFill.style.width = "100%";
          sendCmd({ flush: 1 });
        }
      }

      audioCtx.close();
      sendChunk();

    } catch (err) {
      console.error(err);
      alert("Failed to decode audio file: " + err.message);
      fileSending = false;
      enableControls(true);
      progressRow.hidden = true;
    }
  }

  /* ── Progress timer for mic ── */
  let progressStart = 0;
  let progressInterval = null;

  function startProgressTimer() {
    progressStart = Date.now();
    progressInterval = setInterval(function () {
      const elapsed = (Date.now() - progressStart) / 1000;
      durationLabel.textContent = fmtTime(elapsed);
    }, 500);
  }

  function stopProgressTimer() {
    if (progressInterval) { clearInterval(progressInterval); progressInterval = null; }
  }

  /* ── Clear / Reset ── */
  function clearAll() {
    transcriptList.innerHTML = "";
    asrRows.clear();
    liveDraft.classList.add("hidden");
    timelineData = null;
    downloadBtn.classList.add("hidden");
    mAudio.textContent = "-";
    mSampleRate.textContent = "-";
    mAsrRtf.textContent = "-";
    mDiarRtf.textContent = "-";
    mAsrSeg.textContent = "0";
    mDiarSeg.textContent = "0";
    mVadRtf.textContent = "-";
    mGpuStatus.textContent = "-";
    progressRow.hidden = true;
    progressFill.style.width = "0%";
    durationLabel.textContent = "00:00";
    statusLabel.textContent = "Streaming...";
    tracksData = null;
    audioDuration = 0;
    timeScale = 100;
    panOffset = 0;
    _scheduleRender();
    sendCmd({ reset: 1 });
  }

  /* ── Download JSON ── */
  function downloadTimeline() {
    if (!timelineData) return;
    const blob = new Blob([JSON.stringify(timelineData, null, 2)], { type: "application/json" });
    const url = URL.createObjectURL(blob);
    const a = document.createElement("a");
    a.href = url;
    a.download = "orator_timeline_" + Date.now() + ".json";
    a.click();
    URL.revokeObjectURL(url);
  }

  /* ── Event Listeners ── */
  micBtn.addEventListener("click", function () {
    if (micRunning) stopMic(); else startMic();
  });

  fileInput.addEventListener("change", function () {
    if (fileInput.files.length) sendAudioFile(fileInput.files[0]);
    fileInput.value = "";
  });

  // Drag and drop support
  document.addEventListener("dragover", function (e) {
    e.preventDefault();
    e.stopPropagation();
  });
  document.addEventListener("drop", function (e) {
    e.preventDefault();
    e.stopPropagation();
    const files = e.dataTransfer.files;
    if (files.length > 0) {
      const file = files[0];
      if (file.type.startsWith("audio/")) {
        sendAudioFile(file);
      }
    }
  });

  flushBtn.addEventListener("click", function () { sendCmd({ flush: 1 }); });
  endBtn.addEventListener("click", function () {
    if (micRunning) stopMic();
    sendCmd({ end: 1 });
  });
  clearBtn.addEventListener("click", clearAll);
  downloadBtn.addEventListener("click", downloadTimeline);
  if (refreshSessionsBtn) {
    refreshSessionsBtn.addEventListener("click", refreshSessions);
  }

  /* ── Init ── */
  setStatus(false);
  connect();
  wireTimelineEvents();
  _scheduleRender();

  /* ── Demo data (for visualization testing) ── */
  // Load demo transcript if ?demo=1 in URL
  const urlParams = new URLSearchParams(window.location.search);
  if (urlParams.get("demo") === "1" || window.location.href.includes("demo=1") || window.location.href.includes("demo%3D1")) {
    setTimeout(function () {
      const demoEntries = [
        { text_id: "1", start: 0.0, end: 3.2, speaker: 0, text: "你好，欢迎来到实时转录演示系统，这是一个非常完整的测试案例" },
        { text_id: "2", start: 3.2, end: 6.8, speaker: 1, text: "今天我们要测试多说话人分离功能，看看系统能否准确区分不同的声音" },
        { text_id: "3", start: 6.8, end: 10.5, speaker: 0, text: "这个系统可以同时识别多个说话人的内容，并且实时显示在屏幕上" },
        { text_id: "4", start: 10.5, end: 14.2, speaker: 2, text: "说话人二开始发言，测试准确性，看看系统能否正确识别我的声音" },
        { text_id: "5", start: 14.2, end: 18.0, speaker: 1, text: "说话人一继续对话，系统实时处理，这个功能非常强大和实用" },
        { text_id: "6", start: 18.0, end: 22.5, speaker: 0, text: "这是一个完整的端到端演示流程，从音频输入到文字输出都非常流畅" },
        { text_id: "7", start: 22.5, end: 26.0, speaker: 2, text: "说话人三加入讨论，增加复杂度，看看系统能否处理三个人的对话" },
        { text_id: "8", start: 26.0, end: 30.0, speaker: 1, text: "演示结束，感谢使用 Orator 系统，希望这个演示能帮助你了解我们的技术" }
      ];

      // Populate transcript
      transcriptList.innerHTML = "";
      demoEntries.forEach(function (entry) {
        const div = document.createElement("div");
        div.className = "t-item confirmed";
        div.dataset.id = entry.text_id;

        const timeEl = document.createElement("span");
        timeEl.className = "t-time";
        timeEl.textContent = fmtTime(entry.start) + "–" + fmtTime(entry.end);

        const spkEl = document.createElement("span");
        spkEl.className = "t-speaker";
        spkEl.textContent = "S" + entry.speaker;
        spkEl.style.background = SPEAKER_COLORS[entry.speaker % SPEAKER_COLORS.length];
        spkEl.style.color = "#000";

        const textEl = document.createElement("span");
        textEl.className = "t-text";
        textEl.textContent = entry.text;

        div.appendChild(timeEl);
        div.appendChild(spkEl);
        div.appendChild(textEl);
        transcriptList.appendChild(div);
      });

      console.log("[Demo] Transcript loaded");

      // Populate timeline canvas with demo data
      tracksData = [
        { kind: "diarization", entries: demoEntries.map(function (e) { return { start: e.start, end: e.end, speaker: e.speaker, confidence: 0.95 }; }) },
        { kind: "asr", entries: demoEntries.map(function (e) { return { start: e.start, end: e.end, text: e.text }; }) },
        { kind: "vad", entries: demoEntries.map(function (e) { return { start: e.start, end: e.end, speech: true }; }) }
      ];
      audioDuration = 30;
      fitTimeline();
    }, 1000);
  }
})();
