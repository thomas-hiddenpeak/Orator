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

  // Timeline data for download
  let timelineData = null;

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
      console.log("[WS] Connected to", defaultWsUrl());
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
      let msg;
      try { msg = JSON.parse(ev.data); } catch (_) { return; }
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
      if (!id) continue;
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

  /* ── Init ── */
  setStatus(false);
  connect();

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
    }, 1000);
  }
})();
