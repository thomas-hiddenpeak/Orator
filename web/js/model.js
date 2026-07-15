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
    this.connectionGeneration = 0;
    this.reset();
  }

  reset(options = {}) {
    const retainedNames = options.preserveSpeakerNames && this.speakerNames
      ? new Map(this.speakerNames)
      : new Map();
    this.sampleRate = 16000;
    this.audioSec = 0;
    this.asrEnabled = true;

    // Per-track entries (filled from timeline + live events).
    this.tracks = {
      diarization: [],
      asr: [],
      vad: [],
      align: [],
      business_speaker: [],
    };

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
    this.speakerNames = retainedNames;

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
    this.vadHorizon = 0;

    this.timeline = null; // last full timeline document (for download)
  }

  beginSession(msg = {}) {
    const generation = this.connectionGeneration + 1;
    this.reset({ preserveSpeakerNames: true });
    this.connectionGeneration = generation;
    this.sampleRate = msg.sample_rate || 16000;
    this.asrEnabled = msg.asr !== false;
  }

  // ── live ASR ──
  applyAsr(msg, isFinal) {
    const id = msg.text_id;
    if (id == null) return null;
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
    if (isFinal) {
      if (this.draft?.text_id === id) this.draft = null;
      const index = this.tracks.asr.findIndex((entry) => entry.text_id === id);
      const entry = {
        text_id: id,
        start: row.start,
        end: row.end,
        text: row.text,
      };
      if (index >= 0) this.tracks.asr[index] = entry;
      else this.tracks.asr.push(entry);
    } else {
      this.draft = { text_id: id, text: row.text };
    }
    this._syncRowSpeaker(id);
    return row;
  }

  retractAsr(msg) {
    const id = msg.text_id;
    if (id == null) return;
    const row = this.asr.get(id);
    if (row?.status === "partial") {
      this.asr.delete(id);
      this.asrOrder = this.asrOrder.filter((candidate) => candidate !== id);
    }
    if (this.draft?.text_id === id) this.draft = null;
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
      const turn = this._copyTurn(e);
      this.turns.set(this._turnKey(turn), turn);
    }
    this.tracks.business_speaker = Array.from(this.turns.values());
    for (const id of textIds) this._syncRowSpeaker(id);
  }

  // ── live forced alignment (FR12) ──
  applyAlign(msg) {
    const id = msg.text_id != null ? msg.text_id : msg.id;
    if (id == null || !Array.isArray(msg.units)) return;
    this.alignUnits.set(id, msg.units);
    const entry = {
      text_id: id,
      start: msg.start,
      end: msg.end,
      units: msg.units,
    };
    const index = this.tracks.align.findIndex(
      (candidate) => candidate.text_id === id);
    if (index >= 0) this.tracks.align[index] = entry;
    else this.tracks.align.push(entry);
  }

  applyDiar(msg) {
    if (!Array.isArray(msg.segments)) return;
    this.tracks.diarization = msg.segments.map((segment) => {
      let speaker = segment.speaker;
      if (typeof speaker === "string" && speaker.startsWith("speaker_")) {
        const parsed = Number.parseInt(speaker.slice(8), 10);
        speaker = Number.isNaN(parsed) ? -1 : parsed;
      }
      return {
        start: segment.start,
        end: segment.end,
        speaker,
        speaker_id: segment.speaker_id,
        speaker_name: segment.speaker_name,
        confidence: segment.confidence,
      };
    });
  }

  applyVad(msg) {
    const incoming = Array.isArray(msg.segments) ? msg.segments : [msg];
    for (const segment of incoming) {
      if (!Number.isFinite(segment.start) || !Number.isFinite(segment.end)) {
        continue;
      }
      const exists = this.tracks.vad.some((entry) =>
        entry.start === segment.start && entry.end === segment.end);
      if (!exists) {
        this.tracks.vad.push({ start: segment.start, end: segment.end });
      }
    }
    this.tracks.vad.sort((a, b) => a.start - b.start || a.end - b.end);
  }

  applyVadState(msg) { this.vadSpeech = !!msg.speech; }

  applyVadProgress(msg) {
    if (Number.isFinite(msg.horizon)) this.vadHorizon = msg.horizon;
  }

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
    this.audioSec = msg.audio_sec ?? this.audioSec;
    this.sampleRate = msg.sample_rate ?? this.sampleRate;
    this.tracks = {
      diarization: [],
      asr: [],
      vad: [],
      align: [],
      business_speaker: [],
    };
    this.asr.clear();
    this.asrOrder = [];
    this.draft = null;
    this.alignUnits.clear();
    this.turns.clear();

    const tracks = Array.isArray(msg.tracks) ? msg.tracks : [];
    for (const tr of tracks) {
      if (tr.kind && this.tracks[tr.kind] !== undefined) {
        this.tracks[tr.kind] = Array.isArray(tr.entries) ? tr.entries : [];
      }
      if (tr.kind === "asr") {
        for (const entry of (tr.entries || [])) {
          if (entry.text_id == null) continue;
          const row = {
            text_id: entry.text_id,
            start: entry.start,
            end: entry.end,
            text: entry.text || "",
            status: "confirmed",
            key: null,
            label: null,
            speaker: null,
          };
          this.asr.set(entry.text_id, row);
          this.asrOrder.push(entry.text_id);
        }
      }
      if (tr.kind === "align") {
        for (const e of (tr.entries || [])) {
          if (e.text_id != null && Array.isArray(e.units)) {
            this.alignUnits.set(e.text_id, e.units);
          }
        }
      }
    }

    const terminalTurns = this.tracks.business_speaker.length > 0
      ? this.tracks.business_speaker
      : (Array.isArray(msg.comprehensive) ? msg.comprehensive : []);
    this.tracks.business_speaker = terminalTurns;
    const textIds = new Set();
    for (const e of terminalTurns) {
      if (e.text_id == null) continue;
      if (e.speaker_id && e.speaker_name) {
        this.speakerNames.set(e.speaker_id, e.speaker_name);
      }
      const turn = this._copyTurn(e);
      this.turns.set(this._turnKey(turn), turn);
      textIds.add(e.text_id);
    }
    for (const id of textIds) this._syncRowSpeaker(id);
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
    if (entry.speakerMixed) return "Mixed";
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
    const speaker = e.speaker_id ?? (e.speaker != null ? e.speaker : "s");
    return `${e.text_id}:${s}:${end}:${speaker}`;
  }

  _removeTurnsForTextIds(textIds) {
    if (!textIds.size) return;
    for (const [key, turn] of this.turns) {
      if (turn && textIds.has(turn.text_id)) this.turns.delete(key);
    }
  }

  _copyTurn(entry) {
    return {
      text_id: entry.text_id,
      start: entry.start,
      end: entry.end,
      speaker: entry.speaker,
      speaker_id: entry.speaker_id,
      speaker_name: entry.speaker_name,
      speaker_support: entry.speaker_support,
      speaker_uncertain: entry.speaker_uncertain,
      diar_overlap_sec: entry.diar_overlap_sec,
      diar_total_overlap_sec: entry.diar_total_overlap_sec,
      diar_coverage_ratio: entry.diar_coverage_ratio,
      diar_total_coverage_ratio: entry.diar_total_coverage_ratio,
      diar_max_gap_sec: entry.diar_max_gap_sec,
      diar_island_count: entry.diar_island_count,
      speaker_decision: entry.speaker_decision ? {
        ...entry.speaker_decision,
        candidates: Array.isArray(entry.speaker_decision.candidates)
          ? entry.speaker_decision.candidates.map((candidate) => ({ ...candidate }))
          : [],
      } : undefined,
      text: entry.text || "",
    };
  }

  _syncRowSpeaker(textId) {
    const row = this.asr.get(textId);
    if (!row) return;
    const turns = Array.from(this.turns.values())
      .filter((turn) => turn.text_id === textId)
      .sort((a, b) => a.start - b.start || a.end - b.end);
    row.speakerSegments = turns;
    row.speaker = null;
    delete row.speaker_id;
    delete row.speaker_name;
    delete row.speaker_support;
    delete row.speaker_uncertain;
    delete row.speaker_decision;
    delete row.speakerMixed;
    if (turns.length === 0) return;

    const identities = new Set(turns.map((turn) =>
      turn.speaker_id || `speaker:${turn.speaker}`));
    if (identities.size !== 1) {
      row.speakerMixed = true;
      row.speaker_uncertain = true;
      return;
    }

    const representative = turns[0];
    row.speaker = representative.speaker;
    row.speaker_id = representative.speaker_id;
    row.speaker_name = representative.speaker_name;
    row.speaker_support = representative.speaker_support;
    row.speaker_uncertain = turns.some((turn) => turn.speaker_uncertain);
    row.diar_overlap_sec = representative.diar_overlap_sec;
    row.diar_total_overlap_sec = representative.diar_total_overlap_sec;
    row.diar_coverage_ratio = representative.diar_coverage_ratio;
    row.diar_total_coverage_ratio = representative.diar_total_coverage_ratio;
    row.diar_max_gap_sec = representative.diar_max_gap_sec;
    row.diar_island_count = representative.diar_island_count;
    row.speaker_decision = representative.speaker_decision;
  }
}
