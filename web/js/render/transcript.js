// render/transcript.js — live transcript with global speaker identity (FR11),
// partial draft, and per-segment forced-alignment unit count (FR12).
import { fmtTime, identityKey, identityLabel, colorForKey } from "../format.js";

const MAX_ROWS = 600;

export class TranscriptView {
  constructor(listEl, draftWrapEl, draftTextEl) {
    this.list = listEl;
    this.draftWrap = draftWrapEl;
    this.draftText = draftTextEl;
    this.rows = new Map(); // text_id -> element
  }

  clear() {
    this.list.innerHTML = "";
    this.rows.clear();
    if (this.draftWrap) this.draftWrap.classList.add("hidden");
  }

  render(model) {
    // Draft (partial)
    if (model.draft && model.draft.text) {
      this.draftText.textContent = model.draft.text;
      this.draftWrap.classList.remove("hidden");
    } else {
      this.draftWrap.classList.add("hidden");
    }

    for (const id of model.asrOrder) {
      const row = model.asr.get(id);
      if (!row) continue;
      let el = this.rows.get(id);
      if (!el) {
        el = this._make(id);
        this.rows.set(id, el);
        this.list.appendChild(el);
        this._prune();
      }
      this._update(el, row, model.alignUnits.get(id), model);
    }
    this.list.scrollTop = this.list.scrollHeight;
  }

  _make(id) {
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

  _update(el, row, units, model) {
    el.className = "t-item " + (row.status || "");
    el.querySelector(".t-time").textContent = fmtTime(row.start) + "\u2013" + fmtTime(row.end);
    const key = identityKey(row);
    const spk = el.querySelector(".t-speaker");
    if (row.speaker != null || row.speaker_id) {
      spk.textContent = model ? model.labelFor(row) : identityLabel(row);
      spk.style.background = colorForKey(key);
      spk.style.color = "#0b0d12";
    } else {
      spk.textContent = "";
      spk.style.background = "transparent";
    }
    el.querySelector(".t-text").textContent = row.text || "";
    const al = el.querySelector(".t-align");
    al.textContent = units && units.length ? "⏱ " + units.length : "";
  }

  _prune() {
    if (this.rows.size <= MAX_ROWS) return;
    const firstId = this.rows.keys().next().value;
    const el = this.rows.get(firstId);
    if (el && el.parentNode) el.parentNode.removeChild(el);
    this.rows.delete(firstId);
  }
}
