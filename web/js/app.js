// app.js — bootstrap (Spec 006 Phase 2). Wires the data layer (ws + model) to
// the renderers (transcript, timeline, observability, sessions) and controls.
import { OratorWs } from "./ws.js";
import { Model } from "./model.js";
import { MicCapture, streamFile } from "./audio.js";
import { fmtTime, fmtSec } from "./format.js";
import { TranscriptView } from "./render/transcript.js";
import { TimelineView } from "./render/timeline.js";
import { ObservabilityView } from "./render/observability.js";
import { SessionsView } from "./render/sessions.js";
import { SpeakersView } from "./render/speakers.js";

const $ = (id) => document.getElementById(id);

const connBadge = $("connBadge");
const micBtn = $("micBtn"), uploadBtn = $("uploadBtn"), fileInput = $("fileInput");
const flushBtn = $("flushBtn"), endBtn = $("endBtn"), clearBtn = $("clearBtn");
const downloadBtn = $("downloadBtn");
const progressRow = $("progressRow"), progressFill = $("progressFill");
const durationLabel = $("durationLabel"), statusLabel = $("statusLabel");
const vadLed = $("vadLed");
const mAudio = $("mAudio"), mSampleRate = $("mSampleRate");
const mAsrSeg = $("mAsrSeg"), mDiarSeg = $("mDiarSeg");

const model = new Model();
const transcript = new TranscriptView($("transcriptList"), $("liveDraft"), $("draftText"));
const timeline = new TimelineView($("timelineView"));
const observability = new ObservabilityView($("obsPanel"));
const sessions = new SessionsView($("sessionList"), (id) => ws.loadSession(id));
const speakers = new SpeakersView($("speakerList"), (id, name) => ws.renameSpeaker(id, name));
timeline.setModel(model);

let micRunning = false, fileSending = false, lastError = "";
let mic = null, fileHandle = null;

/* ── render scheduler (coalesced) ── */
let renderPending = false;
function scheduleRender() {
  if (renderPending) return;
  renderPending = true;
  requestAnimationFrame(() => {
    renderPending = false;
    transcript.render(model);
    timeline.render(model);
    observability.render(model);
    speakers.render(model);
    updateMetrics();
  });
}

function updateMetrics() {
  mAudio.textContent = model.audioSec ? fmtSec(model.audioSec) : "-";
  mSampleRate.textContent = model.sampleRate || "-";
  mAsrSeg.textContent = model.tracks.asr.length || model.asr.size || 0;
  mDiarSeg.textContent = model.tracks.diarization.length || 0;
  vadLed.classList.toggle("active", model.vadSpeech);
}

/* ── WS wiring ── */
const ws = new OratorWs({
  onOpen() {
    lastError = "";
    setConn(true);
    ws.describe();
    setTimeout(() => ws.sessions(), 500);
    setTimeout(() => ws.speakers(), 600);
  },
  onClose() { setConn(false); },
  onError() { showError("WebSocket connection error"); },
  onMessage(type, msg) {
    switch (type) {
      case "ready":
        model.sampleRate = msg.sample_rate || 16000;
        model.asrEnabled = msg.asr !== false;
        break;
      case "asr_partial": model.applyAsr(msg, false); break;
      case "asr": model.applyAsr(msg, true); break;
      case "revision": model.applyRevision(msg); break;
      case "align": model.applyAlign(msg); timeline.schedule(); break;
      case "vad_state": model.applyVadState(msg); break;
      case "gpu_telemetry": model.applyGpuTelemetry(msg); break;
      case "cursor_progress": model.applyCursorProgress(msg); break;
      case "timeline":
        model.applyTimeline(msg);
        downloadBtn.classList.remove("hidden");
        timeline.fit();
        ws.speakers();
        break;
      case "sessions": sessions.render(msg); break;
      case "speakers": model.applySpeakers(msg); break;
      case "reset_ok": flashBadge("✓ Reset"); setTimeout(() => ws.sessions(), 800); break;
      case "describe": console.log("[describe]", msg); break;
      case "error": showError(msg.error || "server error"); break;
      default: break;
    }
    scheduleRender();
  },
  onUnknown(type) { console.warn("[Orator] unknown WS type:", type); },
});

/* ── connection UI ── */
function setConn(online) {
  connBadge.textContent = online ? "● Connected" : "● Disconnected";
  connBadge.className = "badge " + (online ? "online" : "offline");
  if (online) connBadge.title = "";
  const en = online && !fileSending;
  micBtn.disabled = !en;
  uploadBtn.disabled = !online;
  [flushBtn, endBtn, clearBtn].forEach((b) => (b.disabled = !online));
}
function showError(msg) {
  lastError = msg;
  connBadge.textContent = "● Error";
  connBadge.className = "badge offline";
  connBadge.title = msg;
}
function flashBadge(text) {
  const t = connBadge.textContent, c = connBadge.className;
  connBadge.textContent = text; connBadge.className = "badge online";
  setTimeout(() => { connBadge.textContent = t; connBadge.className = c; }, 800);
}

/* ── controls ── */
micBtn.addEventListener("click", async () => {
  if (micRunning) { stopMic(); return; }
  try {
    mic = new MicCapture((buf) => ws.sendBinary(buf), model.sampleRate);
    await mic.start();
    micRunning = true;
    micBtn.innerHTML = '<span class="btn-icon">&#x1F3A4;</span> Stop Mic';
    progressRow.hidden = false; startTimer();
  } catch (err) { console.error(err); alert("Microphone permission denied"); }
});
function stopMic() {
  if (mic) mic.stop();
  micRunning = false;
  micBtn.innerHTML = '<span class="btn-icon">&#x1F3A4;</span> Start Mic';
  stopTimer();
}

fileInput.addEventListener("change", () => {
  if (fileInput.files.length) sendFile(fileInput.files[0]);
  fileInput.value = "";
});
document.addEventListener("dragover", (e) => { e.preventDefault(); e.stopPropagation(); });
document.addEventListener("drop", (e) => {
  e.preventDefault(); e.stopPropagation();
  const f = e.dataTransfer.files[0];
  if (f && f.type.startsWith("audio/")) sendFile(f);
});

async function sendFile(file) {
  if (!ws.isOpen() || fileSending) return;
  fileSending = true; setConn(true);
  progressRow.hidden = false; statusLabel.textContent = "Decoding...";
  try {
    fileHandle = await streamFile(file, (buf) => ws.sendBinary(buf), model.sampleRate,
      (frac, dur) => {
        durationLabel.textContent = fmtTime(dur);
        statusLabel.textContent = "Streaming...";
        progressFill.style.width = (frac * 100) + "%";
      },
      () => {
        fileSending = false; setConn(true);
        statusLabel.textContent = "Complete"; progressFill.style.width = "100%";
        ws.flush();
      });
  } catch (err) {
    console.error(err); alert("Failed to decode audio: " + err.message);
    fileSending = false; setConn(true); progressRow.hidden = true;
  }
}

flushBtn.addEventListener("click", () => ws.flush());
endBtn.addEventListener("click", () => { if (micRunning) stopMic(); ws.end(); });
clearBtn.addEventListener("click", () => {
  model.reset();
  transcript.clear(); observability.clear(); speakers.clear();
  timeline.setModel(model); timeline.schedule();
  downloadBtn.classList.add("hidden");
  progressRow.hidden = true; progressFill.style.width = "0%";
  durationLabel.textContent = "00:00"; statusLabel.textContent = "Streaming...";
  updateMetrics();
  ws.reset();
});
downloadBtn.addEventListener("click", () => {
  if (!model.timeline) return;
  const blob = new Blob([JSON.stringify(model.timeline, null, 2)], { type: "application/json" });
  const url = URL.createObjectURL(blob);
  const a = document.createElement("a");
  a.href = url; a.download = "orator_timeline_" + Date.now() + ".json"; a.click();
  URL.revokeObjectURL(url);
});
const refreshBtn = $("refreshSessionsBtn");
if (refreshBtn) refreshBtn.addEventListener("click", () => ws.sessions());
const refreshSpk = $("refreshSpeakersBtn");
if (refreshSpk) refreshSpk.addEventListener("click", () => ws.speakers());

/* ── mic progress timer ── */
let timerStart = 0, timerHandle = null;
function startTimer() {
  timerStart = Date.now();
  timerHandle = setInterval(() => {
    durationLabel.textContent = fmtTime((Date.now() - timerStart) / 1000);
  }, 500);
}
function stopTimer() { if (timerHandle) { clearInterval(timerHandle); timerHandle = null; } }

/* ── init ── */
setConn(false);
ws.connect();
timeline.schedule();
window.addEventListener("resize", () => timeline.fit());
