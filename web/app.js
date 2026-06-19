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
  const canvas        = $("timelineCanvas");
  const ctx           = canvas.getContext("2d");

  const mAudio     = $("mAudio");
  const mSampleRate= $("mSampleRate");
  const mAsrRtf    = $("mAsrRtf");
  const mDiarRtf   = $("mDiarRtf");
  const mAsrSeg    = $("mAsrSeg");
  const mDiarSeg   = $("mDiarSeg");
  const mVadRtf    = $("mVadRtf");
  const mGpuStatus = $("mGpuStatus");

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

  // Timeline data (from server)
  let timelineData = null;

  // Timeline zoom
  let zoomLevel = 1; // px per second
  const MIN_ZOOM = 0.5;
  const MAX_ZOOM = 20;

  // Speaker colors for canvas
  const SPEAKER_COLORS = ["#5b8def","#34d399","#f59e0b","#ef4444","#a78bfa","#22d3ee"];

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
    };

    ws.onclose = function () {
      setStatus(false);
      if (!lastError) scheduleReconnect();
    };

    ws.onerror = function () {
      showError("WebSocket connection error");
    };

    ws.onmessage = function (ev) {
      if (typeof ev.data !== "string") return;
      let msg;
      try { msg = JSON.parse(ev.data); } catch (_) { return; }
      if (!msg || !msg.type) return;

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

  /* ── ASR handlers ── */
  function handleAsrPartial(msg) {
    const id = msg.text_id || "__draft__";
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
    const id = msg.text_id || ("__f_" + Date.now());
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
      if (!id) continue;
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
    // VAD segment events — used by timeline
  }

  function handleVadState(msg) {
    const led = $("vadLed");
    if (msg.speech) {
      led.classList.add("active");
    } else {
      led.classList.remove("active");
    }
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

  /* ── Timeline ── */
  function handleTimeline(msg) {
    timelineData = msg;

    // Update metrics
    mAudio.textContent = fmtSec(msg.audio_sec);
    mSampleRate.textContent = msg.sample_rate || "-";

    const tracks = Array.isArray(msg.tracks) ? msg.tracks : [];
    const asrTrack = tracks.find(function (t) { return t.kind === "asr"; });
    const diarTrack = tracks.find(function (t) { return t.kind === "diarization"; });

    if (asrTrack) {
      mAsrRtf.textContent = asrTrack.real_time_factor != null ? asrTrack.real_time_factor.toFixed(1) + "x" : "-";
      mAsrSeg.textContent = (asrTrack.entries || []).length;
    }
    if (diarTrack) {
      mDiarRtf.textContent = diarTrack.real_time_factor != null ? diarTrack.real_time_factor.toFixed(1) + "x" : "-";
      mDiarSeg.textContent = (diarTrack.entries || []).length;
    }

    // Update transcript with speaker labels from comprehensive timeline
    updateTranscriptFromTimeline(msg);

    // Show download button
    downloadBtn.classList.remove("hidden");

    // Auto-zoom to fit
    if (msg.audio_sec > 0) {
      const cw = canvas.parentElement.clientWidth;
      zoomLevel = Math.max(MIN_ZOOM, Math.min(MAX_ZOOM, cw / msg.audio_sec));
    }

    renderTimeline();
  }

  function updateTranscriptFromTimeline(msg) {
    // Apply speaker labels from comprehensive timeline entries
    const comprehensive = msg.comprehensive || [];
    for (const entry of comprehensive) {
      const id = entry.text_id;
      if (!id) continue;
      const row = asrRows.get(id);
      if (!row) continue;
      const spk = entry.speaker;
      if (spk != null) {
        const spkEl = row.querySelector(".t-speaker");
        spkEl.textContent = "S" + spk;
        spkEl.style.background = SPEAKER_COLORS[spk % SPEAKER_COLORS.length];
        spkEl.style.color = "#000";
      }
    }
  }

  function renderTimeline() {
    if (!timelineData) {
      ctx.clearRect(0, 0, canvas.width, canvas.height);
      return;
    }

    const audioSec = timelineData.audio_sec || 10;
    const labelW = 80; // Left label column width
    const headerH = 35;
    const trackH = 40; // Fixed height for DIAR and VAD tracks
    const gap = 8;
    const asrEntryH = 15; // Height per ASR entry (lineHeight 13 + padding 2)

    // Determine number of tracks and ASR entry count
    const tracks = Array.isArray(timelineData.tracks) ? timelineData.tracks : [];
    const hasDiar = tracks.some(function (t) { return t.kind === "diarization"; });
    const hasAsr = tracks.some(function (t) { return t.kind === "asr"; });
    const hasVad = tracks.some(function (t) { return t.kind === "vad"; });
    
    // Calculate ASR track height based on entry count
    const asrTrack = hasAsr ? tracks.find(function (t) { return t.kind === "asr"; }) : null;
    const asrEntryCount = asrTrack && asrTrack.entries ? asrTrack.entries.length : 0;
    const asrTrackH = Math.max(trackH, asrEntryCount * asrEntryH + 4);
    
    const numTracks = (hasDiar ? 1 : 0) + (hasAsr ? 1 : 0) + (hasVad ? 1 : 0);

    // Canvas width = container width
    const containerW = canvas.parentElement.clientWidth || 1200;
    const totalW = containerW;
    // Calculate total height: header + DIAR + gap + ASR + gap + VAD + padding
    let totalH = headerH + 20;
    if (hasDiar) totalH += trackH + gap;
    if (hasAsr) totalH += asrTrackH + gap;
    if (hasVad) totalH += trackH + gap;
    const timeW = totalW - labelW; // Width for time-aligned content

    const dpr = window.devicePixelRatio || 1;
    canvas.width = totalW * dpr;
    canvas.height = totalH * dpr;
    canvas.style.width = totalW + "px";
    canvas.style.height = totalH + "px";
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);

    // Background
    ctx.fillStyle = "#1a1d27";
    ctx.fillRect(0, 0, totalW, totalH);

    // Time axis at top
    const axisY = headerH - 8;
    const timeX0 = labelW;
    ctx.strokeStyle = "#2e3345";
    ctx.lineWidth = 1;
    ctx.beginPath();
    ctx.moveTo(timeX0, axisY);
    ctx.lineTo(totalW, axisY);
    ctx.stroke();

    // Time ticks
    ctx.fillStyle = "#8b90a0";
    ctx.font = "11px system-ui, sans-serif";
    ctx.textAlign = "center";
    const tickInterval = computeTickInterval(audioSec, timeW);
    for (let t = 0; t <= audioSec; t += tickInterval) {
      const x = timeX0 + (t / audioSec) * timeW;
      ctx.beginPath();
      ctx.moveTo(x, axisY - 4);
      ctx.lineTo(x, axisY + 4);
      ctx.strokeStyle = "#2e3345";
      ctx.stroke();
      ctx.fillText(fmtTime(t), x, axisY + 18);
    }

    // Draw tracks (each track is one horizontal band, entries time-aligned)
    let y = headerH + 10;

    if (hasDiar) {
      const diarTrack = tracks.find(function (t) { return t.kind === "diarization"; });
      drawTimeTrack(diarTrack, "DIAR", labelW, y, timeW, trackH, audioSec);
      y += trackH + gap;
    }

    if (hasAsr) {
      drawTimeTrack(asrTrack, "ASR", labelW, y, timeW, asrTrackH, audioSec);
      y += asrTrackH + gap;
    }

    if (hasVad) {
      const vadTrack = tracks.find(function (t) { return t.kind === "vad"; });
      drawTimeTrack(vadTrack, "VAD", labelW, y, timeW, trackH, audioSec);
      y += trackH + gap;
    }
  }

  function groupByOverlap(entries) {
    // Greedy row assignment: each entry goes to first row where it doesn't overlap
    if (entries.length === 0) return [[]];
    const rows = [];
    for (const e of entries) {
      let placed = false;
      for (let r = 0; r < rows.length; r++) {
        const lastInRow = rows[r][rows[r].length - 1];
        if (e.start >= lastInRow.end) {
          rows[r].push(e);
          placed = true;
          break;
        }
      }
      if (!placed) {
        rows.push([e]);
      }
    }
    return rows;
  }

  function drawTimeTrack(track, label, labelW, y0, timeW, trackH, audioSec) {
    const entries = track && track.entries ? track.entries : [];
    const x0 = labelW;

    // Label on left
    ctx.fillStyle = "#8b90a0";
    ctx.font = "bold 11px system-ui, sans-serif";
    ctx.textAlign = "right";
    ctx.textBaseline = "middle";
    ctx.fillText(label, labelW - 8, y0 + trackH / 2);

    // Track background
    ctx.fillStyle = "#232733";
    ctx.fillRect(x0, y0, timeW, trackH);

    // Grid lines at time ticks
    const tickInterval = computeTickInterval(audioSec, timeW);
    ctx.strokeStyle = "#1a1d27";
    ctx.lineWidth = 1;
    for (let t = 0; t <= audioSec; t += tickInterval) {
      const x = x0 + (t / audioSec) * timeW;
      ctx.beginPath();
      ctx.moveTo(x, y0);
      ctx.lineTo(x, y0 + trackH);
      ctx.stroke();
    }

    // Draw entries time-aligned within the track band
    const kind = track.kind;
    const pad = 2;
    const barH = trackH - pad * 2;
    const barY = y0 + pad;
    const r = 3;

    if (kind === "diarization" || kind === "vad") {
      // Draw bars time-aligned
      for (const e of entries) {
        const x1 = x0 + (e.start / audioSec) * timeW;
        const x2 = x0 + (e.end / audioSec) * timeW;
        const bw = Math.max(3, x2 - x1);

        if (kind === "diarization") {
          const spk = e.speaker != null ? e.speaker : 0;
          const color = SPEAKER_COLORS[spk % SPEAKER_COLORS.length];
          ctx.fillStyle = color + "cc";
        } else {
          ctx.fillStyle = "#34d399cc";
        }

        ctx.beginPath();
        ctx.moveTo(x1 + r, barY);
        ctx.lineTo(x1 + bw - r, barY);
        ctx.quadraticCurveTo(x1 + bw, barY, x1 + bw, barY + r);
        ctx.lineTo(x1 + bw, barY + barH - r);
        ctx.quadraticCurveTo(x1 + bw, barY + barH, x1 + bw - r, barY + barH);
        ctx.lineTo(x1 + r, barY + barH);
        ctx.quadraticCurveTo(x1, barY + barH, x1, barY + barH - r);
        ctx.lineTo(x1, barY + r);
        ctx.quadraticCurveTo(x1, barY, x1 + r, barY);
        ctx.fill();

        // Label for diarization
        if (kind === "diarization" && bw > 30) {
          const spk = e.speaker != null ? e.speaker : 0;
          ctx.fillStyle = "#ffffffcc";
          ctx.font = "10px system-ui";
          ctx.textAlign = "center";
          ctx.textBaseline = "middle";
          ctx.fillText("S" + spk, x1 + bw / 2, barY + barH / 2);
        }
      }
    } else if (kind === "asr") {
      // For ASR: all entries in one horizontal band, positioned by time
      // Text wraps within segment, but doesn't affect other entries
      const sorted = entries.slice().sort(function(a, b) { return a.start - b.start; });
      const padding = 2;
      const midY = y0 + trackH / 2;

      sorted.forEach(function(e) {
        const x1 = x0 + (e.start / audioSec) * timeW;
        const x2 = x0 + (e.end / audioSec) * timeW;
        const segW = Math.max(40, x2 - x1);
        const text = e.text || "";

        // Background with subtle border
        ctx.fillStyle = "#2a3050";
        ctx.fillRect(x1, y0 + padding, segW, trackH - padding * 2);
        ctx.strokeStyle = "#3a4060";
        ctx.lineWidth = 1;
        ctx.strokeRect(x1, y0 + padding, segW, trackH - padding * 2);

        // Text centered in segment
        ctx.fillStyle = "#e4e6ed";
        ctx.font = "11px system-ui, sans-serif";
        ctx.textAlign = "left";
        ctx.textBaseline = "top";
        
        // Wrap text to fit in segment width and height
        const charsPerLine = Math.max(4, Math.floor((segW - 8) / 12));
        const maxLines = Math.floor((trackH - padding * 2) / 13);
        const lines = [];
        for (let i = 0; i < text.length && lines.length < maxLines; i += charsPerLine) {
          lines.push(text.substring(i, i + charsPerLine));
        }
        
        // Center text vertically in segment
        const textBlockH = lines.length * 13;
        const startY = midY - textBlockH / 2;
        
        lines.forEach(function(line, lineIdx) {
          const ly = startY + lineIdx * 13;
          ctx.fillText(line, x1 + 4, ly);
        });
      });
    }
    return y0 + trackH;
  }

  function computeTickInterval(audioSec, availW) {
    const targetTicks = availW / 80;
    const raw = audioSec / targetTicks;
    const mag = Math.pow(10, Math.floor(Math.log10(raw)));
    const norm = raw / mag;
    if (norm <= 1.5) return mag;
    if (norm <= 3) return 2 * mag;
    if (norm <= 7) return 5 * mag;
    return 10 * mag;
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
    ctx.clearRect(0, 0, canvas.width, canvas.height);
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

  $("zoomInBtn").addEventListener("click", function () {
    zoomLevel = Math.min(MAX_ZOOM, zoomLevel * 1.5);
    renderTimeline();
  });
  $("zoomOutBtn").addEventListener("click", function () {
    zoomLevel = Math.max(MIN_ZOOM, zoomLevel / 1.5);
    renderTimeline();
  });
  $("resetZoomBtn").addEventListener("click", function () {
    if (timelineData && timelineData.audio_sec > 0) {
      const cw = canvas.parentElement.clientWidth;
      zoomLevel = Math.max(MIN_ZOOM, Math.min(MAX_ZOOM, cw / timelineData.audio_sec));
    } else {
      zoomLevel = 1;
    }
    renderTimeline();
  });

  // Resize handler
  window.addEventListener("resize", function () { renderTimeline(); });

  /* ── Init ── */
  setStatus(false);
  connect();

  /* ── Demo data (for visualization testing) ── */
  // Load demo timeline if ?demo=1 in URL
  const urlParams = new URLSearchParams(window.location.search);
  if (urlParams.get("demo") === "1" || window.location.href.includes("demo=1") || window.location.href.includes("demo%3D1")) {
    setTimeout(function () {
      const demoTimeline = {
        type: "timeline",
        audio_sec: 30.0,
        sample_rate: 16000,
        session_start_wall_sec: Date.now() / 1000,
        tracks: [
          {
            kind: "diarization",
            real_time_factor: 9.6,
            entries: [
              { start: 0.0, end: 3.2, speaker: 0, confidence: 0.94 },
              { start: 3.2, end: 6.8, speaker: 1, confidence: 0.89 },
              { start: 6.8, end: 10.5, speaker: 0, confidence: 0.92 },
              { start: 10.5, end: 14.2, speaker: 2, confidence: 0.87 },
              { start: 14.2, end: 18.0, speaker: 1, confidence: 0.91 },
              { start: 18.0, end: 22.5, speaker: 0, confidence: 0.93 },
              { start: 22.5, end: 26.0, speaker: 2, confidence: 0.88 },
              { start: 26.0, end: 30.0, speaker: 1, confidence: 0.90 }
            ]
          },
          {
            kind: "asr",
            real_time_factor: 2.6,
            entries: [
              { text_id: "1", start: 0.0, end: 3.2, text: "你好，欢迎来到实时转录演示系统" },
              { text_id: "2", start: 3.2, end: 6.8, text: "今天我们要测试多说话人分离功能" },
              { text_id: "3", start: 6.8, end: 10.5, text: "这个系统可以同时识别多个说话人的内容" },
              { text_id: "4", start: 10.5, end: 14.2, text: "说话人二开始发言，测试准确性" },
              { text_id: "5", start: 14.2, end: 18.0, text: "说话人一继续对话，系统实时处理" },
              { text_id: "6", start: 18.0, end: 22.5, text: "这是一个完整的端到端演示流程" },
              { text_id: "7", start: 22.5, end: 26.0, text: "说话人三加入讨论，增加复杂度" },
              { text_id: "8", start: 26.0, end: 30.0, text: "演示结束，感谢使用 Orator 系统" }
            ]
          },
          {
            kind: "vad",
            real_time_factor: 15.2,
            entries: [
              { start: 0.0, end: 3.2 },
              { start: 3.2, end: 6.8 },
              { start: 6.8, end: 10.5 },
              { start: 10.5, end: 14.2 },
              { start: 14.2, end: 18.0 },
              { start: 18.0, end: 22.5 },
              { start: 22.5, end: 26.0 },
              { start: 26.0, end: 30.0 }
            ]
          }
        ],
        comprehensive: [
          { text_id: "1", start: 0.0, end: 3.2, speaker: 0, text: "你好，欢迎来到实时转录演示系统" },
          { text_id: "2", start: 3.2, end: 6.8, speaker: 1, text: "今天我们要测试多说话人分离功能" },
          { text_id: "3", start: 6.8, end: 10.5, speaker: 0, text: "这个系统可以同时识别多个说话人的内容" },
          { text_id: "4", start: 10.5, end: 14.2, speaker: 2, text: "说话人二开始发言，测试准确性" },
          { text_id: "5", start: 14.2, end: 18.0, speaker: 1, text: "说话人一继续对话，系统实时处理" },
          { text_id: "6", start: 18.0, end: 22.5, speaker: 0, text: "这是一个完整的端到端演示流程" },
          { text_id: "7", start: 22.5, end: 26.0, speaker: 2, text: "说话人三加入讨论，增加复杂度" },
          { text_id: "8", start: 26.0, end: 30.0, speaker: 1, text: "演示结束，感谢使用 Orator 系统" }
        ]
      };

      // Populate transcript
      transcriptList.innerHTML = "";
      demoTimeline.comprehensive.forEach(function (entry) {
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

      // Trigger timeline handler
      handleTimeline(demoTimeline);
      console.log("[Demo] Timeline loaded");
    }, 1000);
  }
})();
