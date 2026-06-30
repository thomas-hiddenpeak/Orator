// render/sessions.js — saved-session list + load (RPC: sessions/load_session).
import { fmtTime, fmtSec } from "../format.js";

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
      const id = s.id || "?";
      item.innerHTML =
        '<span class="session-item-id" title="' + id + '">' + id + '</span>' +
        '<span class="session-item-time">' + (s.time ? fmtTime(s.time) : "") + '</span>' +
        '<span class="session-item-dur">' + (s.audio_sec ? fmtSec(s.audio_sec) : "") + '</span>' +
        '<button class="session-load-btn" aria-label="Load session ' + id + '">Load</button>';
      const load = () => this.onLoad(id);
      item.querySelector(".session-load-btn").addEventListener("click", (e) => { e.stopPropagation(); load(); });
      item.addEventListener("click", load);
      this.list.appendChild(item);
    }
  }
}
