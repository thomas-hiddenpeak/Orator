// ws.js — WebSocket ingress (Spec 006 Phase 2, FR10).
// Single connection point: reconnect, Spec 004 envelope decode, and a message
// router that dispatches EVERY known server message type to typed handlers.
// Reuses the proven envelope/command logic from the MVP client.

// Spec 004 protocol layer wraps legacy JSON in a topic-based envelope:
//   { "topic","pipeline","msg_id","ts","qos","schema_version",
//     "data": "<original JSON string>" }
// Raw RPC responses (error/sessions/reset_ok/describe) carry no "topic" key.
export function unwrapEnvelope(raw) {
  if (typeof raw !== "string") return null;
  let obj;
  try { obj = JSON.parse(raw); } catch (_) { return null; }
  if (!obj || typeof obj !== "object") return null;
  if (obj.topic && obj.data !== undefined) {
    if (typeof obj.data === "string") {
      try { return JSON.parse(obj.data); } catch (_) { return null; }
    }
    return obj.data;
  }
  return obj; // legacy / raw RPC
}

// Infer the WS url from the page: WS port = HTTP UI port - 1 (server convention).
export function defaultWsUrl() {
  const host = window.location.hostname || "127.0.0.1";
  const httpPort = window.location.port || "8766";
  const wsPort = (parseInt(httpPort, 10) - 1) || 8765;
  return "ws://" + host + ":" + wsPort;
}

export class OratorWs {
  // handlers: { onOpen, onClose, onError, onMessage(type, msg), onUnknown(type, msg) }
  constructor(handlers) {
    this.h = handlers || {};
    this.ws = null;
    this.reconnectTimer = null;
    this.reconnectAttempt = 0;
    this.lastError = "";
    this.url = defaultWsUrl();
  }

  isOpen() {
    return this.ws && this.ws.readyState === WebSocket.OPEN;
  }

  connect() {
    if (this.ws && (this.ws.readyState === WebSocket.OPEN ||
                    this.ws.readyState === WebSocket.CONNECTING)) return;
    this.ws = new WebSocket(this.url);
    this.ws.onopen = () => {
      this.reconnectAttempt = 0;
      this.lastError = "";
      if (this.h.onOpen) this.h.onOpen();
    };
    this.ws.onclose = (evt) => {
      if (this.h.onClose) this.h.onClose(evt);
      if (!this.lastError) this._scheduleReconnect();
    };
    this.ws.onerror = (evt) => {
      this.lastError = "WebSocket connection error";
      if (this.h.onError) this.h.onError(evt);
    };
    this.ws.onmessage = (ev) => {
      if (typeof ev.data !== "string") return;
      const msg = unwrapEnvelope(ev.data);
      if (!msg || !msg.type) return;
      this._route(msg);
    };
  }

  _route(msg) {
    // Every known type is routed; unknown topics are reported, never dropped.
    const known = new Set([
      "ready", "asr_partial", "asr", "revision", "align", "vad_state",
      "diar", "vad", "timeline", "gpu_telemetry", "cursor_progress",
      "sessions", "reset_ok", "describe", "error",
    ]);
    if (known.has(msg.type)) {
      if (this.h.onMessage) this.h.onMessage(msg.type, msg);
    } else if (this.h.onUnknown) {
      this.h.onUnknown(msg.type, msg);
    }
  }

  _scheduleReconnect() {
    if (this.reconnectTimer) return;
    this.reconnectAttempt++;
    const delay = Math.min(10000, 500 * Math.pow(2, Math.min(5, this.reconnectAttempt - 1)));
    this.reconnectTimer = setTimeout(() => {
      this.reconnectTimer = null;
      this.connect();
    }, delay);
  }

  // Command forms validated against the substring-matching server command parser.
  send(obj) {
    if (!this.isOpen()) return;
    this.ws.send(JSON.stringify(obj));
  }

  sendBinary(buf) {
    if (!this.isOpen()) return;
    this.ws.send(buf);
  }

  describe()        { this.send({ cmd: "describe" }); }
  sessions()        { this.send({ cmd: "sessions" }); }
  loadSession(id)   { this.send({ cmd: "load_session", session_id: id }); }
  flush()           { this.send({ flush: 1 }); }
  end()             { this.send({ end: 1 }); }
  reset()           { this.send({ reset: 1 }); }
}
