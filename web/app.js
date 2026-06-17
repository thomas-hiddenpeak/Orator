(function () {
  const wsUrlInput = document.getElementById("wsUrl");
  const connectBtn = document.getElementById("connectBtn");
  const micBtn = document.getElementById("micBtn");
  const flushBtn = document.getElementById("flushBtn");
  const endBtn = document.getElementById("endBtn");
  const resetBtn = document.getElementById("resetBtn");
  const micState = document.getElementById("micState");
  const wsState = document.getElementById("wsState");
  const connStatus = document.getElementById("connStatus");
  const asrList = document.getElementById("asrList");
  const timelineRaw = document.getElementById("timelineRaw");
  const metrics = document.getElementById("metrics");

  let ws = null;
  let targetSampleRate = 16000;
  let micRunning = false;
  let shouldReconnect = true;
  let reconnectTimer = null;
  let reconnectAttempt = 0;
  const eventCounters = { ready: 0, asr: 0, timeline: 0 };
  let micContext = null;
  let micStream = null;
  let micSource = null;
  let micProcessor = null;

  function setStatus(text, online) {
    connStatus.textContent = text;
    connStatus.classList.toggle("online", online);
    connStatus.classList.toggle("offline", !online);
    flushBtn.disabled = !online;
    endBtn.disabled = !online;
    resetBtn.disabled = !online;
    micBtn.disabled = !online;
    if (!online && micRunning) stopMic();
  }

  function setMicState(text) {
    micState.textContent = "Mic: " + text;
  }

  function setWsState(text) {
    wsState.textContent = "WS: " + text;
  }

  function resetEventCounters() {
    eventCounters.ready = 0;
    eventCounters.asr = 0;
    eventCounters.timeline = 0;
    updateMetric("ws_ready", "0");
    updateMetric("asr_events", "0");
    updateMetric("timeline_events", "0");
    updateMetric("last_event", "-");
  }

  function markEvent(type) {
    if (type === "ready") {
      eventCounters.ready += 1;
      updateMetric("ws_ready", String(eventCounters.ready));
    } else if (type === "asr") {
      eventCounters.asr += 1;
      updateMetric("asr_events", String(eventCounters.asr));
    } else if (type === "timeline") {
      eventCounters.timeline += 1;
      updateMetric("timeline_events", String(eventCounters.timeline));
    }
    updateMetric("last_event", type + " @ " + new Date().toLocaleTimeString());
  }

  function f32ToInt16Buffer(float32Array) {
    const out = new Int16Array(float32Array.length);
    for (let i = 0; i < float32Array.length; ++i) {
      const s = Math.max(-1, Math.min(1, float32Array[i]));
      out[i] = s < 0 ? Math.round(s * 32768) : Math.round(s * 32767);
    }
    return out.buffer;
  }

  function downsampleToRate(input, srcRate, dstRate) {
    if (!input || input.length === 0) return new Float32Array(0);
    if (srcRate === dstRate) return input;
    const ratio = srcRate / dstRate;
    const outLen = Math.max(1, Math.floor(input.length / ratio));
    const out = new Float32Array(outLen);
    let pos = 0;
    for (let i = 0; i < outLen; ++i) {
      const next = Math.min(input.length, Math.floor((i + 1) * ratio));
      const start = Math.floor(pos);
      const end = Math.max(start + 1, next);
      let sum = 0;
      for (let j = start; j < end; ++j) sum += input[j];
      out[i] = sum / (end - start);
      pos = next;
    }
    return out;
  }

  function stopMic() {
    if (micProcessor) {
      micProcessor.disconnect();
      micProcessor.onaudioprocess = null;
      micProcessor = null;
    }
    if (micSource) {
      micSource.disconnect();
      micSource = null;
    }
    if (micStream) {
      const tracks = micStream.getTracks();
      for (let i = 0; i < tracks.length; ++i) tracks[i].stop();
      micStream = null;
    }
    if (micContext) {
      micContext.close();
      micContext = null;
    }
    micRunning = false;
    micBtn.textContent = "Start Mic";
    setMicState("idle");
  }

  async function startMic() {
    if (!ws || ws.readyState !== WebSocket.OPEN) return;
    const MediaCtx = window.AudioContext || window.webkitAudioContext;
    if (!MediaCtx || !navigator.mediaDevices || !navigator.mediaDevices.getUserMedia) {
      setMicState("unsupported");
      return;
    }
    try {
      micStream = await navigator.mediaDevices.getUserMedia({ audio: true });
      micContext = new MediaCtx();
      micSource = micContext.createMediaStreamSource(micStream);
      micProcessor = micContext.createScriptProcessor(4096, 1, 1);
      micProcessor.onaudioprocess = function (ev) {
        if (!ws || ws.readyState !== WebSocket.OPEN) return;
        const ch0 = ev.inputBuffer.getChannelData(0);
        const down = downsampleToRate(ch0, micContext.sampleRate, targetSampleRate);
        if (down.length === 0) return;
        ws.send(f32ToInt16Buffer(down));
      };
      micSource.connect(micProcessor);
      micProcessor.connect(micContext.destination);
      micRunning = true;
      micBtn.textContent = "Stop Mic";
      setMicState("streaming");
    } catch (err) {
      setMicState("permission denied");
      console.error(err);
      stopMic();
    }
  }

  function toggleMic() {
    if (micRunning) {
      stopMic();
      return;
    }
    startMic();
  }

  function defaultWsUrl() {
    const host = window.location.hostname || "127.0.0.1";
    const params = new URLSearchParams(window.location.search);
    const qp = params.get("ws");
    if (qp) return qp;
    return "ws://" + host + ":8765";
  }

  function updateMetric(name, value) {
    const dtNodes = metrics.querySelectorAll("dt");
    for (let i = 0; i < dtNodes.length; ++i) {
      if (dtNodes[i].textContent === name) {
        const dd = dtNodes[i].nextElementSibling;
        if (dd) dd.textContent = value;
        return;
      }
    }
  }

  function appendAsr(msg) {
    const div = document.createElement("div");
    div.className = "log-item";
    const start = typeof msg.start === "number" ? msg.start.toFixed(2) : "?";
    const end = typeof msg.end === "number" ? msg.end.toFixed(2) : "?";
    div.textContent = "[" + start + "-" + end + "] " + (msg.text || "");
    asrList.appendChild(div);
    asrList.scrollTop = asrList.scrollHeight;
  }

  function handleTimeline(msg) {
    timelineRaw.textContent = JSON.stringify(msg, null, 2);
    updateMetric("audio_sec", String(msg.audio_sec ?? "-"));
    updateMetric("sample_rate", String(msg.sample_rate ?? "-"));
    const tracks = Array.isArray(msg.tracks) ? msg.tracks : [];
    const diar = tracks.find((t) => t.kind === "diarization");
    const asr = tracks.find((t) => t.kind === "asr");
    updateMetric("diar_rtf", diar && diar.real_time_factor != null ? String(diar.real_time_factor) : "-");
    updateMetric("asr_rtf", asr && asr.real_time_factor != null ? String(asr.real_time_factor) : "-");
  }

  function scheduleReconnect() {
    if (!shouldReconnect) return;
    if (reconnectTimer) return;
    reconnectAttempt += 1;
    const delayMs = Math.min(10000, 500 * Math.pow(2, Math.min(5, reconnectAttempt - 1)));
    setWsState("reconnect in " + Math.round(delayMs / 1000) + "s");
    reconnectTimer = setTimeout(function () {
      reconnectTimer = null;
      connect(false);
    }, delayMs);
  }

  function disconnect(manual) {
    shouldReconnect = !manual;
    if (reconnectTimer) {
      clearTimeout(reconnectTimer);
      reconnectTimer = null;
    }
    if (ws) {
      try {
        ws.close();
      } catch (_err) {
      }
      ws = null;
    }
    if (manual) {
      setStatus("Disconnected", false);
      setWsState("manual");
      connectBtn.textContent = "Connect";
    }
  }

  function connect(manual) {
    if (ws && (ws.readyState === WebSocket.OPEN || ws.readyState === WebSocket.CONNECTING)) {
      return;
    }
    if (reconnectTimer) {
      clearTimeout(reconnectTimer);
      reconnectTimer = null;
    }
    const url = wsUrlInput.value.trim();
    if (!url) return;

    if (manual) {
      shouldReconnect = true;
      reconnectAttempt = 0;
      resetEventCounters();
    }

    setStatus("Connecting", false);
    setWsState("connecting");
    ws = new WebSocket(url);

    ws.onopen = function () {
      reconnectAttempt = 0;
      setStatus("Connected", true);
      setWsState("open");
      connectBtn.textContent = "Disconnect";
    };

    ws.onclose = function () {
      setStatus("Disconnected", false);
      setWsState("closed");
      connectBtn.textContent = "Connect";
      const needReconnect = shouldReconnect;
      ws = null;
      if (needReconnect) scheduleReconnect();
    };

    ws.onerror = function () {
      setStatus("Error", false);
      setWsState("error");
    };

    ws.onmessage = function (ev) {
      if (typeof ev.data !== "string") return;
      let msg = null;
      try {
        msg = JSON.parse(ev.data);
      } catch (_err) {
        updateMetric("last_event", "non-json @ " + new Date().toLocaleTimeString());
        return;
      }
      if (!msg || typeof msg.type !== "string") return;
      markEvent(msg.type);
      if (msg.type === "asr") appendAsr(msg);
      if (msg.type === "timeline") handleTimeline(msg);
      if (msg.type === "ready") {
        targetSampleRate = typeof msg.sample_rate === "number" ? msg.sample_rate : 16000;
        updateMetric("sample_rate", String(msg.sample_rate ?? "-"));
      }
    };
  }

  function sendText(text) {
    if (!ws || ws.readyState !== WebSocket.OPEN) return;
    ws.send(text);
  }

  wsUrlInput.value = defaultWsUrl();
  setStatus("Disconnected", false);
  setMicState("idle");
  setWsState("idle");
  resetEventCounters();

  connectBtn.addEventListener("click", function () {
    if (ws && ws.readyState === WebSocket.OPEN) {
      disconnect(true);
      return;
    }
    connect(true);
  });
  micBtn.addEventListener("click", toggleMic);
  flushBtn.addEventListener("click", function () {
    sendText('{"flush":1}');
  });
  endBtn.addEventListener("click", function () {
    sendText('{"end":1}');
  });
  resetBtn.addEventListener("click", function () {
    asrList.innerHTML = "";
    timelineRaw.textContent = "-";
    resetEventCounters();
    sendText('{"reset":1}');
  });

  // Auto-connect on page load to reduce manual steps during test cycles.
  connect(true);
})();
