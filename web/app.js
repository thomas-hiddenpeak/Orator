(function () {
  const wsUrlInput = document.getElementById("wsUrl");
  const connectBtn = document.getElementById("connectBtn");
  const flushBtn = document.getElementById("flushBtn");
  const endBtn = document.getElementById("endBtn");
  const resetBtn = document.getElementById("resetBtn");
  const connStatus = document.getElementById("connStatus");
  const asrList = document.getElementById("asrList");
  const timelineRaw = document.getElementById("timelineRaw");
  const metrics = document.getElementById("metrics");

  let ws = null;

  function setStatus(text, online) {
    connStatus.textContent = text;
    connStatus.classList.toggle("online", online);
    connStatus.classList.toggle("offline", !online);
    flushBtn.disabled = !online;
    endBtn.disabled = !online;
    resetBtn.disabled = !online;
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

  function connect() {
    if (ws && (ws.readyState === WebSocket.OPEN || ws.readyState === WebSocket.CONNECTING)) {
      return;
    }
    const url = wsUrlInput.value.trim();
    if (!url) return;

    setStatus("Connecting", false);
    ws = new WebSocket(url);

    ws.onopen = function () {
      setStatus("Connected", true);
    };

    ws.onclose = function () {
      setStatus("Disconnected", false);
    };

    ws.onerror = function () {
      setStatus("Error", false);
    };

    ws.onmessage = function (ev) {
      if (typeof ev.data !== "string") return;
      let msg = null;
      try {
        msg = JSON.parse(ev.data);
      } catch (_err) {
        return;
      }
      if (!msg || typeof msg.type !== "string") return;
      if (msg.type === "asr") appendAsr(msg);
      if (msg.type === "timeline") handleTimeline(msg);
      if (msg.type === "ready") {
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

  connectBtn.addEventListener("click", connect);
  flushBtn.addEventListener("click", function () {
    sendText('{"flush":1}');
  });
  endBtn.addEventListener("click", function () {
    sendText('{"end":1}');
  });
  resetBtn.addEventListener("click", function () {
    asrList.innerHTML = "";
    timelineRaw.textContent = "-";
    sendText('{"reset":1}');
  });
})();
