// model.js — normalized client state (Spec 006 Phase 2, FR10/FR11/FR14).
// The single source of truth the renderers read on each animation frame.

const TELEMETRY_HISTORY = 120; // samples kept per pipeline for sparklines

function ring(push, arr, v, cap) {
  arr.push(v);
  if (arr.length > cap) arr.shift();
  return arr;
}

export class Model {
  constructor() {
    this.reset();
  }

  reset() {
    this.sampleRate = 16000;
    this.audioSec = 0;
    this.asrEnabled = true;

    // Per-track entries (filled from timeline + live events).
    this.tracks = { diarization: [], asr: [], vad: [], align: [] };

    // ASR transcript rows keyed by text_id: {start,end,text,status,key,label,speaker}
    this.asr = new Map();
    this.asrOrder = []; // insertion order of text_ids
    this.draft = null;  // current partial: {text_id,text}

    // Forced-alignment units keyed by text_id: [{start,end,text}]
    this.alignUnits = new Map();

    // Comprehensive speaker turns keyed by stable segment key. A single ASR
    // text_id can be revised into multiple speaker/time spans.
    this.turns = new Map();

    // Speaker registry: key -> {label, color}; filled lazily by renderers.
    this.speakers = new Map();
    // Global identity display names: speaker_id -> name (from the `speakers`
    // command and from speaker_name fields on revisions/timeline).
    this.speakerNames = new Map();

    // Telemetry: per-pipeline ring buffers + latest scheduling state.
    // pipe -> { rtf:[], backlogSec:[], class, cudaPriority, active, computeSec }
    this.telemetry = new Map();
    this.deviceTelemetry = {
      gpuUtil: [],
      gpuMemPct: [],
      powerW: [],
      latest: null,
    };
    this.lastTelemetryTime = 0;
    this.vadSpeech = false;

    this.timeline = null; // last full timeline document (for download)
  }

  // ── live ASR ──
  applyAsr(msg, isFinal) {
    const id = msg.text_id != null ? msg.text_id : (isFinal ? "f" + Date.now() : "__draft__");
    let row = this.asr.get(id);
    if (!row) {
      row = { text_id: id, key: null, label: null, speaker: null };
      this.asr.set(id, row);
      this.asrOrder.push(id);
    }
    row.start = msg.start;
    row.end = msg.end;
    row.text = msg.text || "";
    row.status = isFinal ? "confirmed" : "partial";
    this.draft = isFinal ? null : { text_id: id, text: row.text };
    return row;
  }

  // ── live comprehensive revision (FR14) ──
  applyRevision(msg) {
    if (!Array.isArray(msg.entries)) return;
    const textIds = new Set();
    for (const e of msg.entries) {
      if (e.text_id != null) textIds.add(e.text_id);
    }
    this._removeTurnsForTextIds(textIds);
    for (const e of msg.entries) {
      if (e.text_id == null) continue;
      this.turns.set(this._turnKey(e), {
        text_id: e.text_id, start: e.start, end: e.end,
        speaker: e.speaker, speaker_id: e.speaker_id, speaker_name: e.speaker_name,
        text: e.text || "",
      });
      // Reflect identity onto the matching transcript row if present.
      const row = this.asr.get(e.text_id);
      if (row) {
        row.speaker = e.speaker;
        row.speaker_id = e.speaker_id;
        row.speaker_name = e.speaker_name;
      }
    }
  }

  // ── live forced alignment (FR12) ──
  applyAlign(msg) {
    const id = msg.id != null ? msg.id : msg.text_id;
    if (id == null || !Array.isArray(msg.units)) return;
    this.alignUnits.set(id, msg.units);
  }

  applyVadState(msg) { this.vadSpeech = !!msg.speech; }

  // ── periodic GPU telemetry (FR13) ──
  applyGpuTelemetry(msg) {
    if (!Array.isArray(msg.pipelines)) return;
    this.lastTelemetryTime = msg.time_sec || this.lastTelemetryTime;
    if (msg.device && typeof msg.device === "object") {
      this.applyDeviceTelemetry(msg.device);
    }
    for (const p of msg.pipelines) {
      const t = this._pipe(p.name);
      if (p.real_time_factor != null) ring(true, t.rtf, p.real_time_factor, TELEMETRY_HISTORY);
      t.class = p.class;
      t.cudaPriority = p.cuda_priority;
      t.active = !!p.stream_active;
      t.computeSec = p.compute_sec;
      t.priorityIndex = p.priority_index;
    }
  }

  applyDeviceTelemetry(device) {
    this.deviceTelemetry.latest = device;
    if (device.gpu_utilization_pct != null) {
      ring(true, this.deviceTelemetry.gpuUtil,
           Number(device.gpu_utilization_pct), TELEMETRY_HISTORY);
    }
    if (device.gpu_mem_used_pct != null) {
      ring(true, this.deviceTelemetry.gpuMemPct,
           Number(device.gpu_mem_used_pct), TELEMETRY_HISTORY);
    }
    if (device.system_power_w != null) {
      ring(true, this.deviceTelemetry.powerW,
           Number(device.system_power_w), TELEMETRY_HISTORY);
    }
  }

  // ── periodic cursor/backlog telemetry (FR13) ──
  applyCursorProgress(msg) {
    if (!Array.isArray(msg.cursors)) return;
    const sr = this.sampleRate || 16000;
    for (const c of msg.cursors) {
      const name = this._cursorToPipe(c.id);
      const t = this._pipe(name);
      ring(true, t.backlogSec, (c.pending || 0) / sr, TELEMETRY_HISTORY);
      t.positionSec = (c.position || 0) / sr;
    }
  }

  // ── full timeline (final / flush) ──
  applyTimeline(msg) {
    this.timeline = msg;
    this.audioSec = msg.audio_sec || this.audioSec;
    this.sampleRate = msg.sample_rate || this.sampleRate;
    const tracks = Array.isArray(msg.tracks) ? msg.tracks : [];
    for (const tr of tracks) {
      if (tr.kind && this.tracks[tr.kind] !== undefined) {
        this.tracks[tr.kind] = Array.isArray(tr.entries) ? tr.entries : [];
      }
      if (tr.kind === "align") {
        for (const e of (tr.entries || [])) {
          if (e.text_id != null && Array.isArray(e.units)) this.alignUnits.set(e.text_id, e.units);
        }
      }
    }
    // Reconcile comprehensive turns + transcript identity from the final doc.
    this.turns.clear();
    for (const e of (msg.comprehensive || [])) {
      if (e.text_id == null) continue;
      if (e.speaker_id && e.speaker_name) this.speakerNames.set(e.speaker_id, e.speaker_name);
      this.turns.set(this._turnKey(e), {
        text_id: e.text_id, start: e.start, end: e.end,
        speaker: e.speaker, speaker_id: e.speaker_id, speaker_name: e.speaker_name,
        text: e.text || "",
      });
      const row = this.asr.get(e.text_id);
      if (row) {
        row.speaker = e.speaker;
        row.speaker_id = e.speaker_id;
        row.speaker_name = e.speaker_name;
      }
    }
  }

  trackRtf(kind) {
    const t = this.timeline && (this.timeline.tracks || []).find((x) => x.kind === kind);
    return t ? t.real_time_factor : null;
  }

  // Speaker registry list from the `speakers` command: [{id,name}].
  applySpeakers(msg) {
    if (!Array.isArray(msg.speakers)) return;
    for (const s of msg.speakers) {
      if (s.id) this.speakerNames.set(s.id, s.name || "");
    }
  }

  // Best display label for a diar/comprehensive entry: an explicit name field,
  // else a renamed global id from the registry, else the global id, else local.
  labelFor(entry) {
    if (!entry) return "S?";
    if (entry.speaker_name) return String(entry.speaker_name);
    if (entry.speaker_id) {
      const nm = this.speakerNames.get(entry.speaker_id);
      if (nm) return nm;
      return String(entry.speaker_id);
    }
    if (entry.speaker != null && entry.speaker >= 0) return "S" + entry.speaker;
    return "S?";
  }

  _pipe(name) {
    let t = this.telemetry.get(name);
    if (!t) {
      t = { name, rtf: [], backlogSec: [], class: null, cudaPriority: null,
            active: false, computeSec: 0, positionSec: 0, priorityIndex: null };
      this.telemetry.set(name, t);
    }
    return t;
  }

  _cursorToPipe(id) {
    if (id === "diar") return "diarization";
    return id; // asr, vad
  }

  _turnKey(e) {
    const s = Number.isFinite(e.start) ? e.start.toFixed(3) : "na";
    const end = Number.isFinite(e.end) ? e.end.toFixed(3) : "na";
    const speaker = e.speaker_id || e.speaker || "s";
    return `${e.text_id}:${s}:${end}:${speaker}`;
  }

  _removeTurnsForTextIds(textIds) {
    if (!textIds.size) return;
    for (const [key, turn] of this.turns) {
      if (turn && textIds.has(turn.text_id)) this.turns.delete(key);
    }
  }
}
