// render/transcript.js — Live view: ASR utterances first.
//
// The comprehensive timeline may split a text segment at diarization
// boundaries. That is correct for the final alignment view, but the live
// microphone transcript must keep finalized ASR utterances readable.
import { fmtTime, identityKey, identityLabel, colorForKey } from "../format.js";

const MAX_ROWS = 600;

function supportTitle(entry) {
  if (!entry || !entry.speaker_support) return "";
  const pct = typeof entry.diar_coverage_ratio === "number"
    ? `${Math.round(entry.diar_coverage_ratio * 100)}%`
    : "-";
  const gap = typeof entry.diar_max_gap_sec === "number"
    ? `${entry.diar_max_gap_sec.toFixed(2)}s`
    : "-";
  const islands = entry.diar_island_count ?? "-";
  const decision = entry.speaker_decision;
  const rejected = Array.isArray(decision?.candidates)
    ? decision.candidates.filter((candidate) => !candidate.selected).length
    : 0;
  const reason = decision?.reason ? `; decision ${decision.reason}` : "";
  const alternatives = rejected > 0 ? `; rejected alternatives ${rejected}` : "";
  return `speaker support: ${entry.speaker_support}; coverage ${pct}; max gap ${gap}; islands ${islands}${reason}${alternatives}`;
}

function applySpeakerSupport(el, speakerEl, entry) {
  const level = entry?.speaker_support || "";
  el.classList.toggle("speaker-weak", level === "weak");
  el.classList.toggle("speaker-none", level === "none");
  if (speakerEl) {
    speakerEl.dataset.support = level;
    speakerEl.title = supportTitle(entry);
  }
}

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

    // Primary: finalized ASR utterances, enriched with speaker attribution when
    // revisions arrive. Comprehensive rows remain a fallback for loaded sessions
    // that do not have live ASR rows in memory.
    const diarSegs = model.tracks?.diarization || [];
    const hasTurns = model.turns && model.turns.size > 0;

    if (model.asr && model.asr.size > 0) {
      this._renderAsrRows(model);
    } else if (hasTurns) {
      this._renderComprehensiveFallback(model);
    } else if (diarSegs.length > 0) {
      this._renderDiarDriven(model, diarSegs);
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
      .sort((a, b) => (a.start ?? a.start_sec ?? 0) -
        (b.start ?? b.start_sec ?? 0));

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
      const segInfo = this._textForDiarSegment(seg, turns, model);
      this._updateEntry(el, seg, segInfo.text, model, segInfo.supportEntry);
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
    const ds = seg.start ?? seg.start_sec ?? 0;
    const de = seg.end ?? seg.end_sec ?? 0;
    let text = "";
    let supportEntry = null;
    let weakestRank = -1;
    for (const t of turns) {
      // Overlap check: turn overlaps diar segment.
      if (t.start >= de || t.end <= ds) continue;
      if (text) text += " ";
      text += t.text;
      const rank = t.speaker_support === "weak" ? 2 :
        (t.speaker_support === "none" ? 1 : 0);
      if (rank > weakestRank) {
        weakestRank = rank;
        supportEntry = t;
      }
    }
    return { text, supportEntry };
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

  _updateEntry(el, seg, text, model, supportEntry = null) {
    const localSpeaker = seg.speaker ?? seg.local_speaker ?? -1;
    const start = seg.start ?? seg.start_sec ?? 0;
    const end = seg.end ?? seg.end_sec ?? 0;
    const key = seg.speaker_id || `speaker_${localSpeaker}`;
    const color = colorForKey(key);

    el.style.borderLeftColor = color;
    el.querySelector(".live-row-time").textContent =
      `${fmtTime(start)} – ${fmtTime(end)}`;

    const spk = el.querySelector(".live-row-speaker");
    const entry = {
      speaker: localSpeaker,
      speaker_id: seg.speaker_id || undefined,
    };
    spk.textContent = model.labelFor(entry);
    spk.style.background = color;
    spk.style.color = "#0b0d12";
    applySpeakerSupport(el, spk, supportEntry);

    el.querySelector(".live-row-text").textContent = text || "";
  }

  // ── fallback: comprehensive turns sorted by time (no diar yet) ──
  _renderComprehensiveFallback(model) {
    const turns = [];
    const activeIds = new Set();
    for (const [key, t] of model.turns) {
      if (!t || t.text_id == null || !t.text?.trim()) continue;
      const id = `turn:${key}`;
      turns.push({ ...t, _row_id: id });
      activeIds.add(id);
    }
    turns.sort((a, b) => (a.start || 0) - (b.start || 0));

    for (const t of turns) {
      const id = t._row_id;
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
      applySpeakerSupport(el, spk, t);
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

  // ── primary live view: ASR rows, optionally speaker-enriched ──
  _renderAsrRows(model) {
    const activeIds = new Set();
    for (const id of model.asrOrder) {
      const row = model.asr.get(id);
      if (!row) continue;
      if (row.status === "partial") continue;
      const rowId = `asr:${id}`;
      activeIds.add(rowId);
      let el = this.rows.get(rowId);
      if (!el) {
        el = this._makeFallbackEntry(rowId);
        this.rows.set(rowId, el);
        this.list.appendChild(el);
        this._prune();
      }
      this._updateFallbackEntry(el, row, model.alignUnits.get(id), model);
    }

    for (const [id, el] of this.rows) {
      if (!activeIds.has(id)) {
        if (el && el.parentNode) el.parentNode.removeChild(el);
        this.rows.delete(id);
      }
    }
  }

  // ── fallback: live ASR rows (streaming before comprehensive arrives) ──
  _renderFallbackASR(model) {
    this._renderAsrRows(model);
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
    const speakerEntry = this._dominantSpeakerForAsr(row, model) || row;
    const key = identityKey(speakerEntry);
    const color = colorForKey(key);
    el.style.borderLeftColor = color;
    const spk = el.querySelector(".t-speaker");
    if (speakerEntry.speaker != null || speakerEntry.speaker_id) {
      spk.textContent = model
        ? model.labelFor(speakerEntry)
        : identityLabel(speakerEntry);
      spk.style.background = color;
      spk.style.color = "#0b0d12";
    } else {
      spk.textContent = "";
      spk.style.background = "transparent";
    }
    applySpeakerSupport(el, spk, speakerEntry);
    el.querySelector(".t-text").textContent = row.text || "";
    const al = el.querySelector(".t-align");
    al.textContent = units && units.length ? `⏱ ${units.length}` : "";
  }

  _dominantSpeakerForAsr(row, model) {
    if (!model?.turns || row?.start == null || row?.end == null) return null;
    let best = null;
    let bestOverlap = 0;
    for (const t of model.turns.values()) {
      if (!t || String(t.text_id) !== String(row.text_id)) continue;
      const s = Math.max(row.start || 0, t.start || 0);
      const e = Math.min(row.end || 0, t.end || 0);
      const overlap = Math.max(0, e - s);
      if (overlap > bestOverlap) {
        bestOverlap = overlap;
        best = t;
      }
    }
    return best;
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
