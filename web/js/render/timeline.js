// render/timeline.js — comprehensive timeline raw data view.
//
// Shows the full timeline JSON document as formatted, read-only text.
// This is the raw data stream output, not a processed visualization.
export class TimelineView {
  constructor(containerEl) {
    this.el = containerEl;
    this.model = null;
    this._pending = false;
  }

  setModel(m) { this.model = m; }

  fit() {
    // JSON timeline view has no viewport-dependent layout to recompute.
  }

  schedule() {
    if (this._pending) return;
    this._pending = true;
    requestAnimationFrame(() => {
      this._pending = false;
      this.render(this.model);
    });
  }

  render(model) {
    if (model) this.model = model;
    const m = this.model;
    if (!this.el) return;

    // Source: the full timeline document from flush/end, or build from live state.
    let data = null;

    if (m.timeline) {
      data = m.timeline;
    } else if (m.turns && m.turns.size > 0) {
      // Live state: build a minimal timeline-like structure.
      const turns = [];
      for (const t of m.turns.values()) {
        if (t && t.text_id != null && t.text?.trim()) {
          turns.push({
            text_id: t.text_id,
            start: t.start,
            end: t.end,
            speaker: t.speaker,
            speaker_id: t.speaker_id,
            speaker_name: t.speaker_name,
            speaker_support: t.speaker_support,
            speaker_uncertain: t.speaker_uncertain,
            diar_overlap_sec: t.diar_overlap_sec,
            diar_total_overlap_sec: t.diar_total_overlap_sec,
            diar_coverage_ratio: t.diar_coverage_ratio,
            diar_total_coverage_ratio: t.diar_total_coverage_ratio,
            diar_max_gap_sec: t.diar_max_gap_sec,
            diar_island_count: t.diar_island_count,
            text: t.text,
          });
        }
      }
      turns.sort((a, b) => (a.start || 0) - (b.start || 0));

      data = {
        audio_sec: m.audioSec || 0,
        sample_rate: m.sampleRate || 16000,
        tracks: [
          { kind: "diarization", entries: m.tracks?.diarization || [] },
          { kind: "asr", entries: (m.tracks?.asr || []).map((e) => ({
            ...e,
            text: m.asr.get(e.id || e.text_id)?.text || e.text || "",
          })) },
          { kind: "vad", entries: m.tracks?.vad || [] },
        ],
        comprehensive: turns,
      };
    }

    if (!data || (!data.comprehensive?.length && !data.tracks?.length)) {
      this.el.innerHTML =
        '<div class="tl-empty">No timeline yet — stream audio, then Flush/End</div>';
      return;
    }

    // Render as formatted JSON.
    let pre = this.el.querySelector("pre");
    if (!pre) {
      pre = document.createElement("pre");
      pre.className = "tl-json";
      this.el.replaceChildren(pre);
    }
    pre.textContent = JSON.stringify(data, null, 2);
  }
}
