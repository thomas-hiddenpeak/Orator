// render/timeline.js — canvas timeline: diarization (by global identity),
// ASR, VAD, and forced-alignment unit lanes on one shared time axis (FR12).
// Reuses the MVP's zoom/pan/viewport-clipping math.
import { fmtMMSS, identityKey, identityLabel, colorForKey } from "../format.js";

const TRACK_H = 38;
const AXIS_H = 26;
const PAD_TOP = 4;
const PAD_BOTTOM = 4;
const LANES = ["diarization", "asr", "vad", "align"];
const MIN_ZOOM = 0.5;
const MAX_ZOOM = 40;

export class TimelineView {
  constructor(canvas) {
    this.canvas = canvas;
    this.ctx = canvas ? canvas.getContext("2d") : null;
    this.model = null;
    this.scale = 100;       // px/sec
    this.pan = 0;
    this.dragging = false;
    this.dragX = 0;
    this.dragPan = 0;
    this._pending = false;
    if (canvas) this._wire();
  }

  setModel(m) { this.model = m; }

  fit() {
    if (!this.canvas) return;
    const wrap = this.canvas.parentElement;
    const availW = wrap ? wrap.clientWidth - 2 : 800;
    const dur = (this.model && this.model.audioSec) || 30;
    this.scale = Math.max(MIN_ZOOM, Math.min(MAX_ZOOM, availW / dur));
    this.pan = 0;
    this.schedule();
  }

  schedule() {
    if (this._pending) return;
    this._pending = true;
    requestAnimationFrame(() => { this._pending = false; this._draw(); });
  }

  zoom(factor) {
    const old = this.scale;
    this.scale = Math.max(MIN_ZOOM, Math.min(MAX_ZOOM, this.scale * factor));
    const center = this.canvas.clientWidth / 2;
    this.pan = this.pan * (this.scale / old) + center * (1 - this.scale / old);
    this.schedule();
  }

  _wire() {
    this.canvas.addEventListener("wheel", (e) => {
      e.preventDefault();
      const old = this.scale;
      const factor = e.deltaY < 0 ? 1.15 : 0.87;
      this.scale = Math.max(MIN_ZOOM, Math.min(MAX_ZOOM, this.scale * factor));
      this.pan = this.pan * (this.scale / old) + e.offsetX * (1 - this.scale / old);
      this.schedule();
    }, { passive: false });
    this.canvas.addEventListener("mousedown", (e) => {
      if (e.button !== 0) return;
      this.dragging = true; this.dragX = e.clientX; this.dragPan = this.pan;
      this.canvas.style.cursor = "grabbing";
    });
    window.addEventListener("mousemove", (e) => {
      if (!this.dragging) return;
      this.pan = this.dragPan - (e.clientX - this.dragX);
      this.schedule();
    });
    window.addEventListener("mouseup", () => {
      if (this.dragging) { this.dragging = false; this.canvas.style.cursor = ""; }
    });
    this.canvas.addEventListener("keydown", (e) => {
      const step = this.pan * 0.1 + 20;
      switch (e.key) {
        case "ArrowLeft":  this.pan = Math.max(0, this.pan - step); this.schedule(); e.preventDefault(); break;
        case "ArrowRight": this.pan += step; this.schedule(); e.preventDefault(); break;
        case "+": case "=": this.zoom(1.4); e.preventDefault(); break;
        case "-": case "_": this.zoom(0.7); e.preventDefault(); break;
        case "Home": this.fit(); e.preventDefault(); break;
      }
    });
  }

  _tick(scale) {
    if (scale < 10) return 60; if (scale < 30) return 30; if (scale < 60) return 10;
    if (scale < 120) return 5; if (scale < 250) return 2; return 1;
  }

  _draw() {
    const c = this.ctx; if (!c || !this.canvas) return;
    const wrap = this.canvas.parentElement;
    const w = wrap ? wrap.clientWidth - 2 : 800;
    const h = PAD_TOP + LANES.length * TRACK_H + AXIS_H + PAD_BOTTOM;
    const dpr = window.devicePixelRatio || 1;
    this.canvas.style.width = w + "px";
    this.canvas.style.height = h + "px";
    this.canvas.width = w * dpr;
    this.canvas.height = h * dpr;
    c.setTransform(dpr, 0, 0, dpr, 0, 0);
    c.clearRect(0, 0, w, h);
    c.fillStyle = "#1c2029";
    c.fillRect(0, 0, w, h);

    const m = this.model;
    if (!m || !m.timeline && m.tracks.diarization.length === 0) {
      c.fillStyle = "#8b90a0";
      c.font = "13px Inter, system-ui, sans-serif";
      c.textAlign = "center"; c.textBaseline = "middle";
      c.fillText("No timeline yet — stream audio, then Flush/End", w / 2, h / 2);
      return;
    }

    const dur = m.audioSec > 0 ? m.audioSec : 30;
    const vStart = this.pan < 0 ? 0 : this.pan / this.scale;
    const vEnd = Math.min(dur, (this.pan + w) / this.scale);
    const vis = (arr) => arr.length > 200
      ? arr.filter((e) => !(e.end <= vStart || e.start >= vEnd)) : arr;
    const X = (t) => t * this.scale - this.pan;

    // Lane labels + separators
    LANES.forEach((name, i) => {
      const y = PAD_TOP + i * TRACK_H;
      c.fillStyle = "#8b90a0";
      c.font = "bold 11px Inter, system-ui, sans-serif";
      c.textAlign = "left"; c.textBaseline = "middle";
      c.fillText(name === "diarization" ? "Diar" : name.toUpperCase(), 6, y + TRACK_H / 2);
    });

    // Diarization (by global identity)
    (() => {
      const y = PAD_TOP;
      for (const e of vis(m.tracks.diarization)) {
        const x = X(e.start), bw = (e.end - e.start) * this.scale;
        if (bw < 1 || x + bw < 0 || x > w) continue;
        const col = colorForKey(identityKey(e));
        c.fillStyle = col + "cc"; c.fillRect(x, y + 4, bw, TRACK_H - 8);
        c.strokeStyle = col; c.lineWidth = 0.5; c.strokeRect(x, y + 4, bw, TRACK_H - 8);
        if (bw > 44) {
          c.fillStyle = "#0b0d12"; c.font = "bold 10px Inter, system-ui, sans-serif";
          c.textAlign = "left"; c.textBaseline = "middle";
          c.fillText(identityLabel(e), x + 4, y + TRACK_H / 2);
        }
      }
    })();

    // ASR
    (() => {
      const y = PAD_TOP + TRACK_H;
      for (const e of vis(m.tracks.asr)) {
        const x = X(e.start), bw = (e.end - e.start) * this.scale;
        if (bw < 1 || x + bw < 0 || x > w) continue;
        c.fillStyle = "rgba(91,141,239,0.14)"; c.fillRect(x, y + 4, bw, TRACK_H - 8);
        c.strokeStyle = "rgba(91,141,239,0.4)"; c.lineWidth = 0.5; c.strokeRect(x, y + 4, bw, TRACK_H - 8);
        if (bw > 30 && e.text) {
          c.fillStyle = "#c9cdd8"; c.font = "10px Inter, system-ui, sans-serif";
          c.textAlign = "left"; c.textBaseline = "middle";
          let txt = e.text; const maxC = Math.floor(bw / 8);
          if (txt.length > maxC) txt = txt.substring(0, maxC) + "…";
          c.fillText(txt, x + 3, y + TRACK_H / 2);
        }
      }
    })();

    // VAD
    (() => {
      const y = PAD_TOP + 2 * TRACK_H;
      for (const e of vis(m.tracks.vad)) {
        const x = X(e.start), bw = (e.end - e.start) * this.scale;
        if (bw < 1 || x + bw < 0 || x > w) continue;
        c.fillStyle = "rgba(52,211,153,0.5)"; c.fillRect(x, y + 10, bw, TRACK_H - 20);
      }
    })();

    // Align units (each unit a thin tick)
    (() => {
      const y = PAD_TOP + 3 * TRACK_H;
      c.fillStyle = "rgba(245,158,11,0.85)";
      for (const units of m.alignUnits.values()) {
        for (const u of units) {
          if (u.end <= vStart || u.start >= vEnd) continue;
          const x = X(u.start), bw = Math.max(1, (u.end - u.start) * this.scale);
          if (x + bw < 0 || x > w) continue;
          c.fillRect(x, y + 8, bw, TRACK_H - 16);
        }
      }
    })();

    // Time axis
    (() => {
      const y = PAD_TOP + LANES.length * TRACK_H;
      const tick = this._tick(this.scale);
      const maxT = dur + w / this.scale;
      c.strokeStyle = "#2e3345"; c.lineWidth = 1;
      c.beginPath(); c.moveTo(0, y); c.lineTo(w, y); c.stroke();
      c.fillStyle = "#8b90a0"; c.font = "10px Inter, system-ui, sans-serif";
      c.textAlign = "center"; c.textBaseline = "top";
      const first = Math.ceil((-this.pan / this.scale) / tick) * tick;
      for (let t = first; t <= maxT; t += tick) {
        const tx = X(t); if (tx < 0 || tx > w) continue;
        c.strokeStyle = "#2e3345"; c.lineWidth = 0.5;
        c.beginPath(); c.moveTo(tx, y); c.lineTo(tx, y + 5); c.stroke();
        c.fillText(fmtMMSS(t), tx, y + 8);
      }
    })();

    // Lane separators
    c.strokeStyle = "#2e3345"; c.lineWidth = 0.5;
    for (let i = 1; i <= LANES.length; i++) {
      const ly = PAD_TOP + i * TRACK_H;
      c.beginPath(); c.moveTo(0, ly); c.lineTo(w, ly); c.stroke();
    }
  }
}
