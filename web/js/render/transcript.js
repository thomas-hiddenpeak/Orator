// render/transcript.js — Live view: diarization-driven chronological timeline.
//
// The segment boundaries come from the diarization track (who spoke when),
// not from ASR. Each diar segment is a row carrying its speaker identity
// (coloured left bar + badge) and the text attributed to it from the
// comprehensive timeline. This is the "test.txt" style view — linear,
// time-ordered, with speaker attribution per row.
//
// Revisions to model.turns or model.tracks.diarization automatically sync
// because we re-read on each render frame.
import { fmtTime, identityKey, identityLabel, colorForKey } from "../format.js";

const MAX_ROWS = 600;

export class TranscriptView {
  constructor(listEl, draftWrapEl, draftTextEl) {
    this.list = listEl;
    this.draftWrap = draftWrapEl;
    this.draftText = draftTextEl;
    this.rows = new Map(); // diar_segment_index -> element
  }

  clear() {
    this.list.innerHTML = "";
    this.rows.clear();
    if (this.draftWrap) this.draftWrap.classList.add("hidden");
  }

  render(model) {
    // Draft (partial) — real-time streaming feedback at bottom.
    if (model.draft && model.draft.text) {
      this.draftText.textContent = model.draft.text;
      this.draftWrap.classList.remove("hidden");
    } else {
      this.draftWrap.classList.add("hidden");
    }

    // Primary: diarization-driven view (segments from diar track, text from
    // comprehensive turns). Fallback: live ASR rows (streaming before any
    // comprehensive data arrives).
    const diarSegs = model.tracks?.diarization || [];
    const hasTurns = model.turns && model.turns.size > 0;

    if (diarSegs.length > 0 && hasTurns) {
      this._renderDiarDriven(model, diarSegs);
    } else if (hasTurns) {
      // Turns exist but no diar yet — fall back to turns sorted by time.
      this._renderComprehensiveFallback(model);
    } else {
      this._renderFallbackASR(model);
    }

    this.list.scrollTop = this.list.scrollHeight;
  }

  // ── diarization-driven view (primary) ──
  _renderDiarDriven(model, diarSegs) {
    // Build a time-indexed list of comprehensive turns for fast lookup.
    const turns = [];
    for (const t of model.turns.values()) {
      if (!t || t.text_id == null || !t.text?.trim()) continue;
      turns.push(t);
    }
    turns.sort((a, b) => (a.start || 0) - (b.start || 0));

    // Sort diar segments by start time.
    const sorted = diarSegs
      .map((s, idx) => ({ ...s, _idx: idx }))
      .sort((a, b) => (a.start_sec || 0) - (b.start_sec || 0));

    const activeIndices = new Set(sorted.map((s) => s._idx));

    // For each diar segment, find the overlapping comprehensive turns and
    // concatenate their text.
    for (const seg of sorted) {
      const sid = seg._idx;
      let el = this.rows.get(sid);
      if (!el) {
        el = this._makeEntry(sid);
        this.rows.set(sid, el);
        this.list.appendChild(el);
        this._prune();
      }

      // Find turns that overlap this diar segment.
      const segText = this._textForDiarSegment(seg, turns, model);
      this._updateEntry(el, seg, segText, model);
    }

    // Remove stale entries.
    for (const [sid, el] of this.rows) {
      if (!activeIndices.has(sid)) {
        if (el && el.parentNode) el.parentNode.removeChild(el);
        this.rows.delete(sid);
      }
    }
  }

  // Find comprehensive turns overlapping a diar segment and concatenate text.
  _textForDiarSegment(seg, turns, model) {
    const ds = seg.start_sec || 0;
    const de = seg.end_sec || 0;
    let text = "";
    for (const t of turns) {
      // Overlap check: turn overlaps diar segment.
      if (t.start >= de || t.end <= ds) continue;
      if (text) text += " ";
      text += t.text;
    }
    return text;
  }

  _makeEntry(idx) {
    const div = document.createElement("div");
    div.className = "live-row";
    div.dataset.id = idx;
    div.innerHTML =
      '<span class="live-row-time"></span>' +
      '<span class="live-row-speaker"></span>' +
      '<span class="live-row-text"></span>';
    return div;
  }

  _updateEntry(el, seg, text, model) {
    // Speaker identity: use speaker_id (global) or local_speaker.
    const key = seg.speaker_id || `speaker_${seg.local_speaker}`;
    const color = colorForKey(key);

    el.style.borderLeftColor = color;
    el.querySelector(".live-row-time").textContent =
      `${fmtTime(seg.start_sec)} – ${fmtTime(seg.end_sec)}`;

    const spk = el.querySelector(".live-row-speaker");
    const entry = {
      speaker: seg.local_speaker,
      speaker_id: seg.speaker_id || undefined,
    };
    spk.textContent = model.labelFor(entry);
    spk.style.background = color;
    spk.style.color = "#0b0d12";

    el.querySelector(".live-row-text").textContent = text || "";
  }

  // ── fallback: comprehensive turns sorted by time (no diar yet) ──
  _renderComprehensiveFallback(model) {
    // Clear diar rows when falling back.
    for (const [, el] of this.rows) {
      if (el && el.parentNode) el.parentNode.removeChild(el);
    }
    this.rows.clear();

    const turns = [];
    const activeIds = new Set();
    for (const t of model.turns.values()) {
      if (!t || t.text_id == null || !t.text?.trim()) continue;
      turns.push(t);
      activeIds.add(String(t.text_id));
    }
    turns.sort((a, b) => (a.start || 0) - (b.start || 0));

    for (const t of turns) {
      const id = String(t.text_id);
      let el = this.rows.get(id);
      if (!el) {
        el = this._makeEntry(id);
        this.rows.set(id, el);
        this.list.appendChild(el);
        this._prune();
      }
      const key = identityKey(t);
      const color = colorForKey(key);
      el.style.borderLeftColor = color;
      el.querySelector(".live-row-time").textContent =
        `${fmtTime(t.start)} – ${fmtTime(t.end)}`;
      const spk = el.querySelector(".live-row-speaker");
      spk.textContent = model.labelFor(t);
      spk.style.background = color;
      spk.style.color = "#0b0d12";
      el.querySelector(".live-row-text").textContent = t.text || "";
    }

    // Remove stale.
    for (const [id, el] of this.rows) {
      if (!activeIds.has(id)) {
        if (el && el.parentNode) el.parentNode.removeChild(el);
        this.rows.delete(id);
      }
    }
  }

  // ── fallback: live ASR rows (streaming before comprehensive arrives) ──
  _renderFallbackASR(model) {
    for (const [, el] of this.rows) {
      if (el && el.parentNode) el.parentNode.removeChild(el);
    }
    this.rows.clear();

    for (const id of model.asrOrder) {
      const row = model.asr.get(id);
      if (!row) continue;
      let el = this.rows.get(id);
      if (!el) {
        el = this._makeFallbackEntry(id);
        this.rows.set(id, el);
        this.list.appendChild(el);
        this._prune();
      }
      this._updateFallbackEntry(el, row, model.alignUnits.get(id), model);
    }
  }

  _makeFallbackEntry(id) {
    const div = document.createElement("div");
    div.className = "t-item";
    div.dataset.id = id;
    div.innerHTML =
      '<span class="t-time"></span>' +
      '<span class="t-speaker"></span>' +
      '<span class="t-text"></span>' +
      '<span class="t-align" title="forced-alignment units"></span>';
    return div;
  }

  _updateFallbackEntry(el, row, units, model) {
    el.className = "t-item " + (row.status || "");
    el.querySelector(".t-time").textContent =
      `${fmtTime(row.start)} – ${fmtTime(row.end)}`;
    const key = identityKey(row);
    const color = colorForKey(key);
    el.style.borderLeftColor = color;
    const spk = el.querySelector(".t-speaker");
    if (row.speaker != null || row.speaker_id) {
      spk.textContent = model ? model.labelFor(row) : identityLabel(row);
      spk.style.background = color;
      spk.style.color = "#0b0d12";
    } else {
      spk.textContent = "";
      spk.style.background = "transparent";
    }
    el.querySelector(".t-text").textContent = row.text || "";
    const al = el.querySelector(".t-align");
    al.textContent = units && units.length ? `⏱ ${units.length}` : "";
  }

  _prune() {
    while (this.rows.size > MAX_ROWS) {
      const firstId = this.rows.keys().next().value;
      const el = this.rows.get(firstId);
      if (el && el.parentNode) el.parentNode.removeChild(el);
      this.rows.delete(firstId);
    }
  }
}
