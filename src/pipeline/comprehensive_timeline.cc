#include "pipeline/comprehensive_timeline.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>

namespace orator {
namespace pipeline {

namespace {
double Overlap(double a0, double a1, double b0, double b1) {
  return std::max(0.0, std::min(a1, b1) - std::max(a0, b0));
}

// Byte offsets of each UTF-8 codepoint start in `s`, plus s.size() at the end.
// Used to split text on codepoint (character) boundaries, not bytes (Chinese).
std::vector<std::size_t> Utf8Offsets(const std::string& s) {
  std::vector<std::size_t> o;
  o.reserve(s.size() + 1);  // Pre-allocate to avoid repeated reallocations
  for (std::size_t i = 0; i < s.size();) {
    o.push_back(i);
    const unsigned char c = static_cast<unsigned char>(s[i]);
    int adv = 1;
    if (c >= 0xF0)
      adv = 4;
    else if (c >= 0xE0)
      adv = 3;
    else if (c >= 0xC0)
      adv = 2;
    i += adv;
  }
  o.push_back(s.size());
  return o;
}

bool EntriesEqual(const std::vector<ComprehensiveTimeline::Entry>& a,
                  const std::vector<ComprehensiveTimeline::Entry>& b) {
  if (a.size() != b.size()) return false;
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (a[i].speaker != b[i].speaker ||
        a[i].speaker_id != b[i].speaker_id || a[i].text != b[i].text ||
        std::abs(a[i].start - b[i].start) > 1e-9 ||
        std::abs(a[i].end - b[i].end) > 1e-9) {
      return false;
    }
  }
  return true;
}
}  // namespace

ComprehensiveTimeline::SpeakerAttr ComprehensiveTimeline::AttributeInterval(
    double start, double end) const {
  double best_overlap = 0.0;
  double best_span = 0.0;
  float best_conf = -1.0f;
  const SpeakerSeg* best = nullptr;
  for (const auto& s : speakers_) {
    const double ov = Overlap(start, end, s.start, s.end);
    if (ov <= 0.0) continue;
    const double span = s.end - s.start;
    bool better = ov > best_overlap + 1e-9;
    if (!better && ov > best_overlap - 1e-9 && best != nullptr) {
      // Near-equal overlap: prefer the more SPECIFIC (tighter) speaker turn,
      // then higher confidence -- a short turn embedded in a longer one wins.
      if (span < best_span - 1e-9 ||
          (span < best_span + 1e-9 && s.conf > best_conf)) {
        better = true;
      }
    }
    if (better) {
      best_overlap = ov;
      best_span = span;
      best_conf = s.conf;
      best = &s;
    }
  }
  // No diarization covers this interval: honestly "unknown". The comprehensive
  // layer never borrows a neighbouring speaker (that would be guessing on
  // diarization's behalf).
  if (best == nullptr) return {"unknown", ""};
  return {best->speaker, best->speaker_id};
}

void ComprehensiveTimeline::set_align_snap_pause_sec(double sec) {
  align_snap_pause_sec_ = std::max(0.0, sec);
}

void ComprehensiveTimeline::set_align_boundary_split_tolerance_sec(double sec) {
  align_boundary_split_tolerance_sec_ = std::max(0.0, sec);
}

std::vector<ComprehensiveTimeline::Entry>
ComprehensiveTimeline::SplitTextByDiar(const TextSeg& t) const {
  // The comprehensive VIEW's boundaries come from the DIARIZATION TRACK.
  // Collect the diarization boundary times that fall strictly inside this text
  // segment, partition [t.start,t.end) by them, attribute each sub-interval to
  // its max-overlap speaker, merge consecutive same-speaker sub-intervals within
  // this source text_id, then allocate the text's characters to each piece by
  // time (the ASR model emits no per-word timestamps, so a time-proportional
  // split is the faithful approximation).
  std::vector<double> bounds;
  bounds.reserve(speakers_.size() * 2 +
                 2);  // Pre-allocate to avoid repeated reallocations
  bounds.push_back(t.start);
  bounds.push_back(t.end);
  for (const auto& s : speakers_) {
    if (s.start > t.start + 1e-9 && s.start < t.end - 1e-9)
      bounds.push_back(s.start);
    if (s.end > t.start + 1e-9 && s.end < t.end - 1e-9) bounds.push_back(s.end);
  }
  std::sort(bounds.begin(), bounds.end());
  bounds.erase(
      std::unique(bounds.begin(), bounds.end(),
                  [](double a, double b) { return std::abs(a - b) < 1e-9; }),
      bounds.end());

  struct Turn {
    double start;
    double end;
    std::string speaker;
    std::string speaker_id;
  };
  std::vector<Turn> turns;
  turns.reserve(bounds.size());  // Pre-allocate to avoid repeated reallocations
  for (std::size_t i = 0; i + 1 < bounds.size(); ++i) {
    const double s = bounds[i];
    const double e = bounds[i + 1];
    if (e - s <= 1e-9) continue;
    const SpeakerAttr attr = AttributeInterval(s, e);
    if (!turns.empty() && turns.back().speaker == attr.speaker &&
        turns.back().speaker_id == attr.speaker_id) {
      turns.back().end = e;
    } else {
      turns.push_back({s, e, attr.speaker, attr.speaker_id});
    }
  }

  // Low-risk gap fill: a sub-interval with no diarization coverage ("unknown")
  // is attributed to the surrounding speaker ONLY when the diarization segments
  // bounding the gap on BOTH sides are the SAME speaker -- a brief pause inside
  // one speaker's turn. A gap at a speaker transition (different speakers on
  // each side), or with no segment on one side, stays "unknown": the layer
  // never guesses across a speaker change. ORATOR_TIMELINE_NO_GAPFILL=1
  // disables it (A/B + safety fallback to the honest-unknown behaviour).
  static const bool no_gapfill =
      std::getenv("ORATOR_TIMELINE_NO_GAPFILL") != nullptr;
  if (!no_gapfill) {
    for (auto& tn : turns) {
      if (tn.speaker != "unknown") continue;
      const SpeakerSeg* before = nullptr;  // largest end <= the gap start
      const SpeakerSeg* after = nullptr;   // smallest start >= the gap end
      for (const auto& sp : speakers_) {
        if (sp.end <= tn.start + 1e-9 && (!before || sp.end > before->end))
          before = &sp;
        if (sp.start >= tn.end - 1e-9 && (!after || sp.start < after->start))
          after = &sp;
      }
      if (before && after && before->speaker == after->speaker &&
          before->speaker_id == after->speaker_id) {
        tn.speaker = before->speaker;
        tn.speaker_id = before->speaker_id;
      }
    }
    // Re-coalesce consecutive same-speaker turns after the fill.
    std::vector<Turn> merged;
    merged.reserve(turns.size());
    for (const auto& tn : turns) {
      if (!merged.empty() && merged.back().speaker == tn.speaker &&
          merged.back().speaker_id == tn.speaker_id)
        merged.back().end = tn.end;
      else
        merged.push_back(tn);
    }
    turns.swap(merged);
  }

  if (turns.empty()) {
    return {Entry{t.start, t.end, "unknown", "", t.text, t.id}};
  }
  if (turns.size() == 1) {
    return {Entry{turns[0].start, turns[0].end, turns[0].speaker,
                  turns[0].speaker_id, t.text, t.id}};
  }

  // Alignment-aware allocation (preferred): when per-unit timestamps exist for
  // this text, attribute characters by their real timing. Adjacent units are
  // grouped only across short gaps; a larger gap is evidence for a possible turn
  // change inside one ASR segment. Each run is assigned to the diarization turn
  // containing its midpoint, snapping noisy diar boundaries to nearby alignment
  // pauses without letting long ASR segments swallow a real speaker change.
  if (auto ait = align_.find(t.id);
      ait != align_.end() && !ait->second.units.empty()) {
    const auto& units = ait->second.units;
    std::vector<std::string> slices(turns.size());
    auto boundary_near_gap = [&](double gap_start, double gap_end) {
      if (align_boundary_split_tolerance_sec_ <= 0.0) return false;
      for (std::size_t k = 1; k < turns.size(); ++k) {
        const Turn& left = turns[k - 1];
        const Turn& right = turns[k];
        if (left.speaker == right.speaker &&
            left.speaker_id == right.speaker_id) {
          continue;
        }
        const double boundary = right.start;
        if (boundary >= gap_start - align_boundary_split_tolerance_sec_ &&
            boundary <= gap_end + align_boundary_split_tolerance_sec_) {
          return true;
        }
      }
      return false;
    };
    std::size_t i = 0;
    while (i < units.size()) {
      std::size_t j = i;  // extend the run while consecutive units are gapless
      if (align_snap_pause_sec_ > 0.0) {
        while (j + 1 < units.size()) {
          const double gap = units[j + 1].start - units[j].end;
          if (gap > align_snap_pause_sec_ ||
              boundary_near_gap(units[j].end, units[j + 1].start)) {
            break;
          }
          ++j;
        }
      }
      const double mid = 0.5 * (units[i].start + units[j].end);
      std::size_t ti = 0;
      while (ti + 1 < turns.size() && mid >= turns[ti].end) ++ti;
      for (std::size_t k = i; k <= j; ++k) slices[ti] += units[k].text;
      i = j + 1;
    }
    std::vector<Entry> out;
    out.reserve(turns.size());
    for (std::size_t k = 0; k < turns.size(); ++k) {
      if (slices[k].empty()) continue;
      out.push_back({turns[k].start, turns[k].end, turns[k].speaker,
                     turns[k].speaker_id, std::move(slices[k]), t.id});
    }
    if (!out.empty()) return out;
  }

  // Proportional codepoint allocation across turns (cumulative to avoid drift).
  const std::vector<std::size_t> offs = Utf8Offsets(t.text);
  const int ncp = static_cast<int>(offs.size()) - 1;
  const double total = t.end - t.start;
  std::vector<Entry> out;
  out.reserve(turns.size());  // Pre-allocate to avoid repeated reallocations
  int cp_used = 0;
  for (std::size_t k = 0; k < turns.size(); ++k) {
    int cp_end;
    if (k + 1 == turns.size() || total <= 0.0) {
      cp_end = ncp;
    } else {
      const double frac = (turns[k].end - t.start) / total;
      cp_end = static_cast<int>(std::llround(frac * ncp));
      cp_end = std::max(cp_used, std::min(ncp, cp_end));
    }
    std::string slice =
        t.text.substr(offs[cp_used], offs[cp_end] - offs[cp_used]);
    out.push_back({turns[k].start, turns[k].end, turns[k].speaker,
                   turns[k].speaker_id, std::move(slice), t.id});
    cp_used = cp_end;
  }
  return out;
}

void ComprehensiveTimeline::ReprojectText(const TextSeg& t,
                                          std::vector<Revision>* out) {
  std::vector<Entry> pcs = SplitTextByDiar(t);
  auto it = pieces_.find(t.id);
  const bool changed = (it == pieces_.end()) || !EntriesEqual(it->second, pcs);
  pieces_[t.id] = pcs;
  // Emit a revision only when the projected result changed, not on every raw
  // update. The revision carries ALL the (possibly multiple) pieces for this
  // text id, keyed by text_id so the consumer replaces them as a unit.
  if (changed && out) out->push_back({t.start, t.end, std::move(pcs)});
}

void ComprehensiveTimeline::ReprojectRange(double start, double end,
                                           std::vector<Revision>* out) {
  for (const auto& t : texts_) {
    if (Overlap(t.start, t.end, start, end) > 0.0) ReprojectText(t, out);
  }
}

std::vector<ComprehensiveTimeline::Revision>
ComprehensiveTimeline::UpsertSpeaker(double start, double end,
                                     const std::string& speaker, float conf) {
  SpeakerSeg s;
  s.start = start;
  s.end = end;
  s.speaker = speaker;
  s.conf = conf;
  // Insert keeping speakers_ ordered by start (stable, simple; small N).
  auto pos = std::lower_bound(
      speakers_.begin(), speakers_.end(), start,
      [](const SpeakerSeg& a, double v) { return a.start < v; });
  speakers_.insert(pos, std::move(s));

  std::vector<Revision> revs;
  // Pre-allocate to avoid repeated reallocations
  revs.reserve(1);  // Typically 0 or 1 revision
  ReprojectRange(start, end, &revs);
  return revs;
}

std::vector<ComprehensiveTimeline::Revision> ComprehensiveTimeline::UpsertText(
    long id, double start, double end, const std::string& text) {
  auto it = std::find_if(texts_.begin(), texts_.end(),
                         [&](const TextSeg& t) { return t.id == id; });
  if (it == texts_.end()) {
    TextSeg t;
    t.id = id;
    t.start = start;
    t.end = end;
    t.text = text;
    // Keep texts_ ordered by start.
    auto pos = std::lower_bound(
        texts_.begin(), texts_.end(), start,
        [](const TextSeg& a, double v) { return a.start < v; });
    it = texts_.insert(pos, std::move(t));
  } else {
    it->start = start;
    it->end = end;
    it->text = text;
  }
  std::vector<Revision> revs;
  // Pre-allocate to avoid repeated reallocations
  revs.reserve(1);  // Typically 0 or 1 revision
  ReprojectText(*it, &revs);
  return revs;
}

std::vector<ComprehensiveTimeline::Revision>
ComprehensiveTimeline::ReplaceSpeakers(const std::vector<SpeakerInput>& segs) {
  // Diarization delivers its whole current segment view (global derivation), so
  // replace the speaker set wholesale and re-project ALL text. Emit a revision
  // for every text whose attributed result changed.
  speakers_.clear();
  speakers_.reserve(
      segs.size());  // Pre-allocate to avoid repeated reallocations
  for (const auto& s : segs) {
    SpeakerSeg seg;
    seg.start = s.start;
    seg.end = s.end;
    seg.speaker = s.speaker;
    seg.conf = s.conf;
    seg.speaker_id = s.speaker_id;
    if (!s.speaker_id.empty()) seen_speaker_ids_.insert(s.speaker_id);
    auto pos = std::lower_bound(
        speakers_.begin(), speakers_.end(), s.start,
        [](const SpeakerSeg& a, double v) { return a.start < v; });
    speakers_.insert(pos, std::move(seg));
  }
  std::vector<Revision> revs;
  // Pre-allocate to avoid repeated reallocations
  revs.reserve(texts_.size());  // Maximum possible revisions
  for (const auto& t : texts_) ReprojectText(t, &revs);
  return revs;
}

std::vector<std::string> ComprehensiveTimeline::AllSpeakerIds() const {
  return {seen_speaker_ids_.begin(), seen_speaker_ids_.end()};
}

std::map<std::string, std::string> ComprehensiveTimeline::SpeakerLabelIds()
    const {
  std::map<std::string, std::string> out;
  for (const auto& s : speakers_) {
    if (!s.speaker_id.empty()) out[s.speaker] = s.speaker_id;
  }
  return out;
}

void ComprehensiveTimeline::AddVad(double start, double end) {
  VadSeg v{start, end};
  auto pos =
      std::lower_bound(vad_.begin(), vad_.end(), start,
                       [](const VadSeg& a, double s) { return a.start < s; });
  vad_.insert(pos, v);
}

std::vector<ComprehensiveTimeline::Entry> ComprehensiveTimeline::Snapshot()
    const {
  // The comprehensive view is the accuracy-first projection of every finalized
  // ASR text segment through the current diarization view. It may coalesce
  // adjacent pieces created by one source text segment, but it preserves
  // text_id boundaries so ASR finalization evidence remains visible to the
  // terminal timeline JSON. Presentation-level speaker-turn grouping belongs
  // in a separate consumer view.
  std::vector<Entry> all;
  // Pre-allocate to avoid repeated reallocations
  all.reserve(texts_.size() * 2);  // Estimate maximum entries
  for (const auto& kv : pieces_) {
    for (const auto& e : kv.second) all.push_back(e);
  }
  std::sort(all.begin(), all.end(),
            [](const Entry& a, const Entry& b) { return a.start < b.start; });

  std::vector<Entry> out;
  for (const auto& e : all) {
    if (!out.empty() && out.back().speaker == e.speaker &&
        out.back().speaker_id == e.speaker_id && out.back().text_id == e.text_id) {
      out.back().end = std::max(out.back().end, e.end);
      out.back().text += e.text;
    } else {
      out.push_back(e);
    }
  }
  return out;
}

std::vector<ComprehensiveTimeline::RawTextSeg>
ComprehensiveTimeline::SnapshotRawTexts() const {
  std::vector<RawTextSeg> out;
  out.reserve(texts_.size());
  for (const auto& t : texts_) {
    out.push_back({t.id, t.start, t.end, t.text});
  }
  return out;
}

std::vector<ComprehensiveTimeline::Revision> ComprehensiveTimeline::UpsertAlign(
    long text_id, double start, double end,
    const std::vector<AlignUnitSeg>& units) {
  AlignGroup g;
  g.text_id = text_id;
  g.start = start;
  g.end = end;
  g.units = units;
  align_[text_id] = std::move(g);  // idempotent replace by id
  // Re-project the matching text now that exact per-unit timestamps refine its
  // diarization split (it was time-proportional before alignment arrived).
  std::vector<Revision> revs;
  for (const auto& t : texts_) {
    if (t.id == text_id) {
      ReprojectText(t, &revs);
      break;
    }
  }
  return revs;
}

std::vector<ComprehensiveTimeline::AlignGroup>
ComprehensiveTimeline::SnapshotAlign() const {
  std::vector<AlignGroup> out;
  out.reserve(align_.size());
  for (const auto& kv : align_) out.push_back(kv.second);
  std::sort(out.begin(), out.end(),
            [](const AlignGroup& a, const AlignGroup& b) {
              return a.start < b.start;
            });
  return out;
}

void ComprehensiveTimeline::Clear() {
  speakers_.clear();
  texts_.clear();
  vad_.clear();
  pieces_.clear();
  align_.clear();
}

void ComprehensiveTimeline::CleanupOldData(double keep_until_sec) {
  // Remove speaker segments that end before keep_until_sec
  auto speaker_it = speakers_.begin();
  while (speaker_it != speakers_.end()) {
    if (speaker_it->end <= keep_until_sec) {
      speaker_it = speakers_.erase(speaker_it);
    } else {
      ++speaker_it;
    }
  }

  // Remove text segments that end before keep_until_sec
  auto text_it = texts_.begin();
  while (text_it != texts_.end()) {
    if (text_it->end <= keep_until_sec) {
      // Remove from pieces_ map as well
      pieces_.erase(text_it->id);
      text_it = texts_.erase(text_it);
    } else {
      ++text_it;
    }
  }

  // Remove VAD segments that end before keep_until_sec
  auto vad_it = vad_.begin();
  while (vad_it != vad_.end()) {
    if (vad_it->end <= keep_until_sec) {
      vad_it = vad_.erase(vad_it);
    } else {
      ++vad_it;
    }
  }

  // Clean up pieces_ map for any remaining texts
  std::vector<long> texts_to_remove;
  for (const auto& kv : pieces_) {
    bool found = false;
    for (const auto& t : texts_) {
      if (t.id == kv.first) {
        found = true;
        break;
      }
    }
    if (!found) {
      texts_to_remove.push_back(kv.first);
    }
  }
  for (long id : texts_to_remove) {
    pieces_.erase(id);
  }
}

}  // namespace pipeline
}  // namespace orator
