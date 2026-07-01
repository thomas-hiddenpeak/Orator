// render/speakers.js — speaker identity + naming panel (Spec 006 / Spec 010).
// Lists the global voiceprint identities seen this session with a stable color
// and an editable display name; committing a name issues a rename command.
import { colorForKey } from "../format.js";

export class SpeakersView {
  constructor(listEl, onRename) {
    this.list = listEl;
    this.onRename = onRename; // (id, name) => void
    this.rows = new Map();    // id -> {root, input}
    this._editing = null;     // id currently being edited (don't clobber input)
  }

  clear() {
    this.list.innerHTML = "";
    this.rows.clear();
    this._editing = null;
  }

  render(model) {
    // Union of ids from the registry (names) and ids seen live in turns.
    const ids = new Set(model.speakerNames.keys());
    for (const t of model.turns.values()) if (t.speaker_id) ids.add(t.speaker_id);
    for (const e of (model.tracks.diarization || [])) if (e.speaker_id) ids.add(e.speaker_id);

    if (ids.size === 0) {
      if (!this.list.querySelector(".spk-empty")) {
        this.list.innerHTML = '<div class="spk-empty">No speakers identified yet</div>';
      }
      return;
    }
    const empty = this.list.querySelector(".spk-empty");
    if (empty) empty.remove();

    for (const id of [...ids].sort()) {
      let row = this.rows.get(id);
      if (!row) { row = this._make(id); this.rows.set(id, row); this.list.appendChild(row.root); }
      // Don't overwrite the field the user is editing.
      if (this._editing !== id) {
        const nm = model.speakerNames.get(id) || "";
        if (row.input.value !== nm) row.input.value = nm;
      }
    }
  }

  _make(id) {
    const root = document.createElement("div");
    root.className = "spk-row";
    const sw = document.createElement("span");
    sw.className = "spk-swatch";
    sw.style.background = colorForKey(id);
    const idEl = document.createElement("span");
    idEl.className = "spk-id";
    idEl.textContent = id;
    const input = document.createElement("input");
    input.className = "spk-name";
    input.type = "text";
    input.placeholder = "name…";
    input.setAttribute("aria-label", "Display name for " + id);
    input.addEventListener("focus", () => { this._editing = id; });
    input.addEventListener("blur", () => {
      if (this._editing === id) this._editing = null;
    });
    const commit = () => {
      this.onRename(id, input.value.trim());
      input.blur();
    };
    input.addEventListener("keydown", (e) => { if (e.key === "Enter") commit(); });
    const save = document.createElement("button");
    save.className = "spk-save";
    save.textContent = "Save";
    save.setAttribute("aria-label", "Save name for " + id);
    save.addEventListener("click", commit);
    root.appendChild(sw);
    root.appendChild(idEl);
    root.appendChild(input);
    root.appendChild(save);
    return { root, input };
  }
}
