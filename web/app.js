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
    return "ws://" + host + ":8765";
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
    // VAD events — could show voice activity indicator
    // For now, just log
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
    const padding = 80; // Increased for labels
    const trackH = 50;  // Taller tracks
    const headerH = 35;
    const gap = 12;

    // Determine number of tracks
    const tracks = Array.isArray(timelineData.tracks) ? timelineData.tracks : [];
    const hasDiar = tracks.some(function (t) { return t.kind === "diarization"; });
    const hasAsr = tracks.some(function (t) { return t.kind === "asr"; });
    const numTracks = (hasDiar ? 1 : 0) + (hasAsr ? 1 : 0);
    const totalH = headerH + numTracks * (trackH + gap) + padding;

    const dpr = window.devicePixelRatio || 1;
    const totalW = Math.max(1000, audioSec * zoomLevel + padding * 2);

    canvas.width = totalW * dpr;
    canvas.height = totalH * dpr;
    canvas.style.width = totalW + "px";
    canvas.style.height = totalH + "px";
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);

    // Background
    ctx.fillStyle = "#1a1d27";
    ctx.fillRect(0, 0, totalW, totalH);

    // Time axis
    const axisY = headerH - 8;
    ctx.strokeStyle = "#2e3345";
    ctx.lineWidth = 1;
    ctx.beginPath();
    ctx.moveTo(padding, axisY);
    ctx.lineTo(totalW - padding, axisY);
    ctx.stroke();

    // Time ticks
    ctx.fillStyle = "#8b90a0";
    ctx.font = "11px system-ui, sans-serif";
    ctx.textAlign = "center";
    const tickInterval = computeTickInterval(audioSec, totalW - padding * 2);
    for (let t = 0; t <= audioSec; t += tickInterval) {
      const x = padding + t * zoomLevel;
      ctx.beginPath();
      ctx.moveTo(x, axisY - 4);
      ctx.lineTo(x, axisY + 4);
      ctx.strokeStyle = "#2e3345";
      ctx.stroke();
      ctx.fillText(fmtTime(t), x, axisY + 18);
    }

    // Draw tracks
    let y = headerH + 10;

    if (hasDiar) {
      const diarTrack = tracks.find(function (t) { return t.kind === "diarization"; });
      drawDiarTrack(diarTrack, padding, y, totalW - padding * 2, trackH, audioSec);
      y += trackH + gap;
    }

    if (hasAsr) {
      const asrTrack = tracks.find(function (t) { return t.kind === "asr"; });
      drawAsrTrack(asrTrack, padding, y, totalW - padding * 2, trackH, audioSec);
      y += trackH + gap;
    }
  }

  function drawDiarTrack(track, x0, y0, w, h, audioSec) {
    const entries = track && track.entries ? track.entries : [];

    // Label
    ctx.fillStyle = "#8b90a0";
    ctx.font = "bold 12px system-ui, sans-serif";
    ctx.textAlign = "left";
    ctx.fillText("DIARIZATION", x0 - 78, y0 + 16);

    // Background
    ctx.fillStyle = "#232733";
    ctx.fillRect(x0, y0, w, h);

    // Speaker bars
    for (const e of entries) {
      const x1 = x0 + e.start * zoomLevel;
      const x2 = x0 + e.end * zoomLevel;
      const spk = e.speaker != null ? e.speaker : 0;
      const colorIdx = spk % SPEAKER_COLORS.length;
      const color = SPEAKER_COLORS[colorIdx];

      ctx.fillStyle = color + "cc";
      const barH = h * 0.6;
      const barY = y0 + (h - barH) / 2;
      
      // Rounded rect
      const r = 4;
      const bw = Math.max(2, x2 - x1);
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

      // Confidence text
      if (e.confidence != null && bw > 40) {
        ctx.fillStyle = "#ffffffcc";
        ctx.font = "10px system-ui";
        ctx.textAlign = "center";
        ctx.fillText((e.confidence * 100).toFixed(0) + "%", (x1 + x2) / 2, barY + barH / 2 + 4);
      }
    }
  }

  function drawAsrTrack(track, x0, y0, w, h, audioSec) {
    const entries = track && track.entries ? track.entries : [];

    // Label
    ctx.fillStyle = "#8b90a0";
    ctx.font = "bold 12px system-ui, sans-serif";
    ctx.textAlign = "left";
    ctx.fillText("ASR", x0 - 78, y0 + 16);

    // Background
    ctx.fillStyle = "#232733";
    ctx.fillRect(x0, y0, w, h);

    // Text segments
    for (const e of entries) {
      const x1 = x0 + e.start * zoomLevel;
      const x2 = x0 + e.end * zoomLevel;
      const segW = x2 - x1;
      if (segW < 5) continue;

      // Segment background with rounded corners
      ctx.fillStyle = "#5b8def22";
      const r = 4;
      const sy = y0 + 6;
      const sh = h - 12;
      ctx.beginPath();
      ctx.moveTo(x1 + r, sy);
      ctx.lineTo(x1 + segW - r, sy);
      ctx.quadraticCurveTo(x1 + segW, sy, x1 + segW, sy + r);
      ctx.lineTo(x1 + segW, sy + sh - r);
      ctx.quadraticCurveTo(x1 + segW, sy + sh, x1 + segW - r, sy + sh);
      ctx.lineTo(x1 + r, sy + sh);
      ctx.quadraticCurveTo(x1, sy + sh, x1, sy + sh - r);
      ctx.lineTo(x1, sy + r);
      ctx.quadraticCurveTo(x1, sy, x1 + r, sy);
      ctx.fill();

      // Text
      const text = e.text || "";
      ctx.fillStyle = "#e4e6ed";
      ctx.font = "12px system-ui, sans-serif";
      ctx.textAlign = "left";

      // Truncate if needed
      const maxChars = Math.floor(segW / 7);
      const display = maxChars > 0 ? (text.length > maxChars ? text.substring(0, maxChars - 1) + "…" : text) : "";
      ctx.fillText(display, x1 + 6, y0 + h / 2 + 4);
    }
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
})();
