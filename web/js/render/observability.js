// render/observability.js — live per-pipeline health panel (FR13).
// Fed by gpu_telemetry (RTF, scheduling class/priority/active) and
// cursor_progress (backlog pending_sec). Surfaces a starvation warning when
// backlog is rising while RTF < 1 (Spec 011 cross-dimension methodology).
import { fmtRtf } from "../format.js";

const ORDER = ["diarization", "asr", "vad"];

export class ObservabilityView {
  constructor(containerEl) {
    this.el = containerEl;
    this.cards = new Map(); // pipe -> {root, rtfVal, rtfSpark, blVal, blSpark, badges, warn}
  }

  clear() {
    if (this.el) this.el.innerHTML = "";
    this.cards.clear();
  }

  render(model) {
    if (!this.el) return;
    const names = ORDER.filter((n) => model.telemetry.has(n));
    if (names.length === 0) {
      if (!this.el.querySelector(".obs-empty")) {
        this.el.innerHTML = '<div class="obs-empty">Telemetry off — enable [telemetry] in orator.toml</div>';
      }
      return;
    }
    const empty = this.el.querySelector(".obs-empty");
    if (empty) empty.remove();

    for (const name of names) {
      const t = model.telemetry.get(name);
      let card = this.cards.get(name);
      if (!card) { card = this._make(name); this.cards.set(name, card); this.el.appendChild(card.root); }
      this._update(card, t);
    }
  }

  _make(name) {
    const root = document.createElement("div");
    root.className = "obs-card";
    root.innerHTML =
      '<div class="obs-head"><span class="obs-name">' + name + '</span>' +
      '<span class="obs-badges"></span></div>' +
      '<div class="obs-metric"><span class="obs-label">RTF</span>' +
      '<span class="obs-val rtf">-</span><canvas class="obs-spark rtf" width="120" height="22"></canvas></div>' +
      '<div class="obs-metric"><span class="obs-label">Backlog</span>' +
      '<span class="obs-val bl">-</span><canvas class="obs-spark bl" width="120" height="22"></canvas></div>' +
      '<div class="obs-warn hidden">⚠ starvation: backlog rising, RTF &lt; 1×</div>';
    return {
      root,
      badges: root.querySelector(".obs-badges"),
      rtfVal: root.querySelector(".obs-val.rtf"),
      rtfSpark: root.querySelector(".obs-spark.rtf"),
      blVal: root.querySelector(".obs-val.bl"),
      blSpark: root.querySelector(".obs-spark.bl"),
      warn: root.querySelector(".obs-warn"),
    };
  }

  _update(card, t) {
    const rtf = t.rtf.length ? t.rtf[t.rtf.length - 1] : null;
    const bl = t.backlogSec.length ? t.backlogSec[t.backlogSec.length - 1] : 0;
    card.rtfVal.textContent = fmtRtf(rtf);
    card.rtfVal.classList.toggle("warn", rtf != null && rtf < 1);
    card.blVal.textContent = bl.toFixed(2) + "s";
    card.blVal.classList.toggle("warn", bl > 2);

    // Scheduling badges
    card.badges.innerHTML = "";
    if (t.class) this._badge(card.badges, t.class, t.class === "foreground" ? "fg" : "bg");
    if (t.active) this._badge(card.badges, "active", "active");
    if (t.cudaPriority != null) this._badge(card.badges, "prio " + t.cudaPriority, "prio");

    this._spark(card.rtfSpark, t.rtf, 1.0, "#5b8def");
    this._spark(card.blSpark, t.backlogSec, null, "#f59e0b");

    // Starvation: backlog trending up AND rtf < 1.
    const rising = t.backlogSec.length >= 4 &&
      t.backlogSec[t.backlogSec.length - 1] > t.backlogSec[t.backlogSec.length - 4] + 0.2;
    const starving = rising && rtf != null && rtf < 1;
    card.warn.classList.toggle("hidden", !starving);
  }

  _badge(parent, text, cls) {
    const b = document.createElement("span");
    b.className = "obs-badge " + cls;
    b.textContent = text;
    parent.appendChild(b);
  }

  _spark(canvas, data, refLine, color) {
    const c = canvas.getContext("2d");
    const w = canvas.width, h = canvas.height;
    c.clearRect(0, 0, w, h);
    if (!data || data.length < 2) return;
    let max = 0;
    for (const v of data) if (v > max) max = v;
    if (refLine != null && refLine > max) max = refLine;
    if (max <= 0) max = 1;
    // reference line (e.g. RTF=1)
    if (refLine != null) {
      const ry = h - (refLine / max) * (h - 2) - 1;
      c.strokeStyle = "rgba(239,68,68,0.5)"; c.lineWidth = 1;
      c.setLineDash([3, 2]); c.beginPath(); c.moveTo(0, ry); c.lineTo(w, ry); c.stroke();
      c.setLineDash([]);
    }
    c.strokeStyle = color; c.lineWidth = 1.5; c.beginPath();
    const n = data.length;
    for (let i = 0; i < n; i++) {
      const x = (i / (n - 1)) * (w - 2) + 1;
      const y = h - (data[i] / max) * (h - 2) - 1;
      if (i === 0) c.moveTo(x, y); else c.lineTo(x, y);
    }
    c.stroke();
  }
}
