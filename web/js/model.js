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

    // Comprehensive speaker turns keyed by text_id.
    this.turns = new Map();

    // Speaker registry: key -> {label, color}; filled lazily by renderers.
    this.speakers = new Map();

    // Telemetry: per-pipeline ring buffers + latest scheduling state.
    // pipe -> { rtf:[], backlogSec:[], class, cudaPriority, active, computeSec }
    this.telemetry = new Map();
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
    for (const e of msg.entries) {
      if (e.text_id == null) continue;
      this.turns.set(e.text_id, {
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
    for (const e of (msg.comprehensive || [])) {
      if (e.text_id == null) continue;
      this.turns.set(e.text_id, {
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
}
