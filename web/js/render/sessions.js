// render/sessions.js — saved-session list + load (RPC: sessions/load_session).
import { fmtWallTime, fmtSec } from "../format.js";

export class SessionsView {
  constructor(listEl, onLoad) {
    this.list = listEl;
    this.onLoad = onLoad;
  }

  render(msg) {
    const sessions = Array.isArray(msg.sessions) ? msg.sessions
      : (Array.isArray(msg.list) ? msg.list : []);
    this.list.innerHTML = "";
    if (!sessions.length) {
      const empty = document.createElement("div");
      empty.className = "session-empty";
      empty.textContent = "No saved sessions";
      this.list.appendChild(empty);
      return;
    }
    for (const s of sessions) {
      const item = document.createElement("div");
      item.className = "session-item";
      item.setAttribute("role", "listitem");
      const id = String(s.id || "?");

      const idEl = document.createElement("span");
      idEl.className = "session-item-id";
      idEl.title = id;
      idEl.textContent = id;

      const timeEl = document.createElement("span");
      timeEl.className = "session-item-time";
      timeEl.textContent = s.time ? fmtWallTime(s.time) : "";

      const durationEl = document.createElement("span");
      durationEl.className = "session-item-dur";
      durationEl.textContent = s.audio_sec ? fmtSec(s.audio_sec) : "";

      const button = document.createElement("button");
      button.className = "session-load-btn";
      button.type = "button";
      button.setAttribute("aria-label", `Load session ${id}`);
      button.textContent = "Load";
      item.append(idEl, timeEl, durationEl, button);

      const load = () => this.onLoad(id);
      button.addEventListener("click", (e) => { e.stopPropagation(); load(); });
      item.addEventListener("click", load);
      this.list.appendChild(item);
    }
  }
}
