// format.js — shared formatting + speaker-identity helpers (Spec 006 Phase 2).
// No third-party dependencies (plain ES module).

// Stable per-identity palette. Index chosen by a stable hash of the identity
// key so a speaker keeps one color across transcript, comprehensive and the
// timeline (FR11).
export const SPEAKER_COLORS = [
  "#5b8def", "#34d399", "#f59e0b", "#ef4444", "#a78bfa", "#22d3ee",
  "#f472b6", "#a3e635",
];

const UNKNOWN_COLOR = "#8b90a0";

function hashStr(s) {
  let h = 0;
  for (let i = 0; i < s.length; i++) h = (h * 31 + s.charCodeAt(i)) | 0;
  return Math.abs(h);
}

// A diar/comprehensive entry's stable identity key: the global voiceprint id
// (spk_N, Spec 010) when present, else the diarizer-local label.
export function identityKey(entry) {
  if (entry && entry.speaker_id) return String(entry.speaker_id);
  if (entry && entry.speaker != null && entry.speaker >= 0) return "S" + entry.speaker;
  return "S?";
}

// Human-readable label: display name > global id > local index.
export function identityLabel(entry) {
  if (entry && entry.speaker_name) return String(entry.speaker_name);
  if (entry && entry.speaker_id) return String(entry.speaker_id);
  if (entry && entry.speaker != null && entry.speaker >= 0) return "S" + entry.speaker;
  return "S?";
}

export function colorForKey(key) {
  if (!key || key === "S?" || key === "S-1") return UNKNOWN_COLOR;
  return SPEAKER_COLORS[hashStr(key) % SPEAKER_COLORS.length];
}

export function fmtTime(sec) {
  if (typeof sec !== "number" || isNaN(sec)) return "--:--";
  const m = Math.floor(sec / 60);
  const s = Math.floor(sec % 60);
  const ms = Math.floor((sec % 1) * 10);
  return String(m).padStart(2, "0") + ":" + String(s).padStart(2, "0") + "." + ms;
}

export function fmtWallTime(sec) {
  if (typeof sec !== "number" || !Number.isFinite(sec) || sec <= 0) return "-";
  const date = new Date(sec * 1000);
  const pad = (value) => String(value).padStart(2, "0");
  return `${date.getFullYear()}-${pad(date.getMonth() + 1)}-${pad(date.getDate())} ` +
    `${pad(date.getHours())}:${pad(date.getMinutes())}:${pad(date.getSeconds())}`;
}

export function fmtMMSS(sec) {
  if (typeof sec !== "number" || isNaN(sec)) return "--:--";
  const m = Math.floor(sec / 60);
  const s = Math.floor(sec % 60);
  return String(m).padStart(2, "0") + ":" + String(s).padStart(2, "0");
}

export function fmtSec(sec) {
  if (typeof sec !== "number") return "-";
  return sec.toFixed(1) + "s";
}

export function fmtRtf(rtf) {
  if (typeof rtf !== "number" || rtf <= 0) return "-";
  return rtf.toFixed(rtf >= 10 ? 0 : 2) + "×";
}
