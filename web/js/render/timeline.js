// render/timeline.js — wrapping, row-based timeline (Spec 006).
//
// Replaces the horizontally-scrolling canvas. The session is split into fixed
// time windows; each window is one full-width ROW that wraps to the next line,
// so there is never a horizontal scrollbar. Every row carries its own time axis
// (a ruler with mm:ss ticks) and the pipelines are aligned ON that axis:
//   - a diarization strip (speaker-coloured blocks positioned by time),
//   - a VAD strip (speech blocks positioned by time),
//   - the transcript, shown as the ALIGNMENT-refined, speaker-attributed
//     comprehensive turns (NOT the raw ASR blocks), each on its true timecode
//     and fully readable (text flows/wraps normally).
import { fmtMMSS, colorForKey, identityKey } from "../format.js";

// Seconds per row (zoom levels: fewer sec = more horizontal detail per row).
const WINDOWS = [10, 15, 20, 30, 45, 60, 90];

export class TimelineView {
  constructor(containerEl) {
    this.el = containerEl;
    this.model = null;
    this.windowIdx = 3; // 30 s/row default
    this._pending = false;
  }

  setModel(m) { this.model = m; }

  get windowSec() { return WINDOWS[this.windowIdx]; }

  // factor > 1 → zoom in (fewer seconds per row); < 1 → zoom out.
  zoom(factor) {
    if (factor > 1) this.windowIdx = Math.max(0, this.windowIdx - 1);
    else if (factor < 1) this.windowIdx = Math.min(WINDOWS.length - 1, this.windowIdx + 1);
    this.schedule();
  }

  fit() { this.windowIdx = 3; this.schedule(); }

  schedule() {
    if (this._pending) return;
    this._pending = true;
    requestAnimationFrame(() => { this._pending = false; this.render(this.model); });
  }

  _diarSegs(m) {
    const t = m.tracks && m.tracks.diarization;
    return Array.isArray(t) ? t : [];
  }

  _vadSegs(m) {
    const t = m.tracks && m.tracks.vad;
    return Array.isArray(t) ? t : [];
  }

  // Speaker-attributed, alignment-refined content for the transcript lane:
  // the comprehensive turns (who-said-what on the common time base). Falls back
  // to live ASR rows before the first comprehensive revision arrives.
  _turns(m) {
    const out = [];
    if (m.turns && m.turns.size) {
      for (const t of m.turns.values()) {
        if (t.text && t.text.trim()) out.push(t);
      }
    } else {
      for (const id of m.asrOrder) {
        const r = m.asr.get(id);
        if (r && r.text) out.push(r);
      }
    }
    out.sort((a, b) => (a.start || 0) - (b.start || 0));
    return out;
  }

  render(model) {
    if (model) this.model = model;
    const m = this.model;
    if (!this.el) return;
    const dur = m && m.audioSec > 0 ? m.audioSec : 0;
    if (!dur) {
      this.el.innerHTML = '<div class="tl-empty">No timeline yet — stream audio, then Flush/End</div>';
      return;
    }

    const win = this.windowSec;
    const diar = this._diarSegs(m);
    const vad = this._vadSegs(m);
    const turns = this._turns(m);
    const nRows = Math.max(1, Math.ceil(dur / win));

    // Bucket turns by row (by their start time) for O(n) placement.
    const rowTurns = Array.from({ length: nRows }, () => []);
    for (const t of turns) {
      const r = Math.min(nRows - 1, Math.max(0, Math.floor((t.start || 0) / win)));
      rowTurns[r].push(t);
    }

    const frag = document.createDocumentFragment();
    for (let r = 0; r < nRows; r++) {
      const rowStart = r * win;
      const rowEnd = Math.min(dur, rowStart + win);
      frag.appendChild(this._row(m, rowStart, rowEnd, win, diar, vad, rowTurns[r]));
    }
    this.el.replaceChildren(frag);
  }

  _row(m, rowStart, rowEnd, win, diar, vad, turns) {
    const row = document.createElement("div");
    row.className = "tl-row";

    const gutter = document.createElement("div");
    gutter.className = "tl-gutter";
    gutter.textContent = fmtMMSS(rowStart);
    row.appendChild(gutter);

    const body = document.createElement("div");
    body.className = "tl-body";

    // Per-row time axis (ruler with mm:ss ticks).
    body.appendChild(this._ruler(rowStart, win));

    // Diarization strip (who spoke when), aligned on the same axis.
    body.appendChild(this._strip("diar", diar, rowStart, win, (s) => {
      const key = identityKey(s);
      return { color: colorForKey(key), label: m.labelFor(s),
               title: `${m.labelFor(s)} ${fmtMMSS(s.start)}–${fmtMMSS(s.end)}` };
    }));

    // VAD strip (speech activity), aligned on the same axis.
    body.appendChild(this._strip("vad", vad, rowStart, win,
      () => ({ color: "#34d399", label: "", title: "speech" })));

    // Transcript lane: alignment-refined, speaker-attributed turns, readable.
    const lane = document.createElement("div");
    lane.className = "tl-turns";
    for (const t of turns) lane.appendChild(this._turn(m, t, rowStart, win));
    body.appendChild(lane);

    row.appendChild(body);
    return row;
  }

  _ruler(rowStart, win) {
    const ruler = document.createElement("div");
    ruler.className = "tl-ruler";
    const step = win <= 15 ? 5 : win <= 30 ? 10 : win <= 60 ? 15 : 30;
    for (let t = 0; t <= win + 1e-6; t += step) {
      const tick = document.createElement("span");
      tick.className = "tl-tick";
      tick.style.left = (t / win) * 100 + "%";
      const lbl = document.createElement("span");
      lbl.className = "tl-tick-lbl";
      lbl.textContent = fmtMMSS(rowStart + t);
      tick.appendChild(lbl);
      ruler.appendChild(tick);
    }
    return ruler;
  }

  _strip(cls, segs, rowStart, win, styleFor) {
    const strip = document.createElement("div");
    strip.className = "tl-strip tl-strip-" + cls;
    const rowEnd = rowStart + win;
    for (const s of segs) {
      const st = s.start || 0, en = s.end || 0;
      if (en <= rowStart || st >= rowEnd) continue; // not in this window
      const a = Math.max(st, rowStart), b = Math.min(en, rowEnd);
      const left = ((a - rowStart) / win) * 100;
      const width = Math.max(0.6, ((b - a) / win) * 100);
      const sty = styleFor(s);
      const blk = document.createElement("span");
      blk.className = "tl-blk";
      blk.style.left = left + "%";
      blk.style.width = width + "%";
      blk.style.background = sty.color;
      if (sty.title) blk.title = sty.title;
      if (sty.label && width > 6) {
        const t = document.createElement("span");
        t.className = "tl-blk-lbl";
        t.textContent = sty.label;
        blk.appendChild(t);
      }
      strip.appendChild(blk);
    }
    return strip;
  }

  _turn(m, t, rowStart, win) {
    const el = document.createElement("div");
    el.className = "tl-turn";
    const key = identityKey(t);
    const color = colorForKey(key);
    el.style.setProperty("--spk", color);

    const time = document.createElement("span");
    time.className = "tl-turn-time";
    time.textContent = fmtMMSS(t.start);
    el.appendChild(time);

    if (t.speaker != null || t.speaker_id) {
      const spk = document.createElement("span");
      spk.className = "tl-turn-spk";
      spk.textContent = m.labelFor(t);
      spk.style.background = color;
      el.appendChild(spk);
    }

    const txt = document.createElement("span");
    txt.className = "tl-turn-text";
    txt.textContent = t.text || "";
    el.appendChild(txt);
    return el;
  }
}
