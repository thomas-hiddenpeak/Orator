// render/dev_status.js — compact project/runtime status for developers.
// Uses only client-side state already present in the SPA.
import { fmtSec } from "../format.js";

function latest(arr) {
  return arr && arr.length ? arr[arr.length - 1] : null;
}

function tileClass(value) {
  if (value === "bad") return "dev-tile bad";
  if (value === "warn") return "dev-tile warn";
  if (value === "good") return "dev-tile good";
  return "dev-tile";
}

export class DevStatusView {
  constructor(gridEl, updatedEl) {
    this.gridEl = gridEl;
    this.updatedEl = updatedEl;
  }

  render(model, runtime) {
    if (!this.gridEl) return;
    const wsOnline = !!runtime?.online;
    const telemetryCount = model.telemetry ? model.telemetry.size : 0;
    const rtfValues = [];
    if (model.telemetry) {
      for (const t of model.telemetry.values()) {
        const rtf = latest(t.rtf);
        if (rtf != null) rtfValues.push(rtf);
      }
    }
    const minRtf = rtfValues.length ? Math.min(...rtfValues) : null;
    const backlogMax = this._maxBacklog(model);
    const device = model.deviceTelemetry?.latest || null;
    const trackCount = Object.values(model.tracks || {})
      .reduce((sum, arr) => sum + (Array.isArray(arr) ? arr.length : 0), 0);
    const speakerCount = model.speakerNames ? model.speakerNames.size : 0;
    const timelineReady = !!model.timeline;
    const wsUrl = runtime?.wsUrl || "ws://-";

    const tiles = [
      {
        label: "Runtime",
        value: wsOnline ? "Online" : "Offline",
        meta: wsUrl,
        tone: wsOnline ? "good" : "bad",
        small: false,
      },
      {
        label: "Protocol",
        value: model.sampleRate ? `${model.sampleRate} Hz` : "-",
        meta: `ASR ${model.asrEnabled ? "enabled" : "disabled"} · envelope v2`,
        tone: model.asrEnabled ? "good" : "warn",
        small: false,
      },
      {
        label: "Tracks",
        value: String(trackCount),
        meta: `${model.tracks?.diarization?.length || 0} diar · ${model.tracks?.asr?.length || 0} asr · ${model.tracks?.vad?.length || 0} vad · ${model.tracks?.align?.length || 0} align`,
        tone: trackCount ? "good" : "",
        small: false,
      },
      {
        label: "Telemetry",
        value: telemetryCount ? `${telemetryCount} pipes` : "Off",
        meta: this._telemetryMeta(minRtf, backlogMax, device),
        tone: telemetryCount ? (minRtf != null && minRtf < 1 ? "warn" : "good") : "warn",
        small: false,
      },
      {
        label: "Session",
        value: model.audioSec ? fmtSec(model.audioSec) : "Live",
        meta: timelineReady ? "timeline ready" : `${model.asr.size || 0} ASR rows · ${model.turns.size || 0} turns`,
        tone: timelineReady || model.asr.size || model.turns.size ? "good" : "",
        small: false,
      },
      {
        label: "Speakers",
        value: speakerCount ? String(speakerCount) : "-",
        meta: speakerCount ? "registry names loaded" : "waiting for identities",
        tone: speakerCount ? "good" : "",
        small: false,
      },
    ];

    this.gridEl.replaceChildren(...tiles.map((t) => this._tile(t)));
    if (this.updatedEl) {
      const at = new Date();
      this.updatedEl.textContent = `updated ${at.toLocaleTimeString([], { hour12: false })}`;
    }
  }

  _telemetryMeta(minRtf, backlogMax, device) {
    const parts = [];
    if (minRtf == null) {
      parts.push("no RTF sample");
    } else {
      parts.push(`min RTF ${minRtf.toFixed(2)}x`);
      parts.push(`backlog ${backlogMax.toFixed(2)}s`);
    }
    if (device?.gpu_mem_used_pct != null) {
      parts.push(`VRAM ${Number(device.gpu_mem_used_pct).toFixed(0)}%`);
    }
    if (device?.system_power_w != null) {
      parts.push(`${Number(device.system_power_w).toFixed(1)} W`);
    }
    return parts.join(" · ");
  }

  _maxBacklog(model) {
    let max = 0;
    if (!model.telemetry) return max;
    for (const t of model.telemetry.values()) {
      const v = latest(t.backlogSec);
      if (v != null && v > max) max = v;
    }
    return max;
  }

  _tile(t) {
    const root = document.createElement("div");
    root.className = tileClass(t.tone);
    const label = document.createElement("div");
    label.className = "dev-label";
    label.textContent = t.label;
    const value = document.createElement("div");
    value.className = "dev-value" + (t.small ? " small" : "");
    value.textContent = t.value;
    const meta = document.createElement("div");
    meta.className = "dev-meta";
    meta.textContent = t.meta || "";
    root.append(label, value, meta);
    return root;
  }
}
