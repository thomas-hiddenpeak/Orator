#include "pipeline/comprehensive_timeline.h"

#include <algorithm>

namespace orator {
namespace pipeline {

namespace {
double Overlap(double a0, double a1, double b0, double b1) {
  return std::max(0.0, std::min(a1, b1) - std::max(a0, b0));
}
}  // namespace

std::string ComprehensiveTimeline::AttributeSpeaker(const TextSeg& t) const {
  double best_overlap = 0.0;
  double best_span = 0.0;
  float best_conf = -1.0f;
  const SpeakerSeg* best = nullptr;
  for (const auto& s : speakers_) {
    const double ov = Overlap(t.start, t.end, s.start, s.end);
    if (ov <= 0.0) continue;
    const double span = s.end - s.start;
    bool better = ov > best_overlap + 1e-9;
    if (!better && ov > best_overlap - 1e-9 && best != nullptr) {
      // Near-equal overlap: prefer the more SPECIFIC speaker segment (shorter
      // span = more tightly localized to this time), then higher confidence.
      // This makes a tight, high-confidence speaker turn win over a long,
      // loosely-overlapping one -- the right answer for dense multi-speaker
      // discussion where a short turn is embedded in a longer one.
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
  if (best != nullptr) return best->speaker;

  // No time overlap with any speaker segment: return "unknown". The
  // comprehensive layer is a PURE TIME-ALIGNMENT layer (Spec 004 invariant): it
  // never infers, substitutes, or back-fills another pipeline's content. If
  // diarization has not covered this time, the speaker is honestly unknown; the
  // text keeps its accurate time and a later diarization upsert covering the
  // span revises the attribution. We do NOT borrow the nearest speaker (that
  // would be the comprehensive layer guessing on diarization's behalf).
  return "unknown";
}

void ComprehensiveTimeline::ReprojectText(const TextSeg& t,
                                          std::vector<Revision>* out) {
  const std::string spk = AttributeSpeaker(t);

  // Find this text's entry in view_ (entries are 1:1 with text ids).
  auto it = std::find_if(view_.begin(), view_.end(),
                         [&](const Entry& e) { return e.text_id == t.id; });
  if (it == view_.end()) {
    Entry e;
    e.start = t.start;
    e.end = t.end;
    e.speaker = spk;
    e.text = t.text;
    e.text_id = t.id;
    view_.push_back(e);
    if (out) out->push_back({t.start, t.end, {e}});
    return;
  }

  // Existing entry: emit a revision only if the ATTRIBUTED RESULT changed
  // (speaker, text, or timing), not on every raw update.
  const bool changed = it->speaker != spk || it->text != t.text ||
                       it->start != t.start || it->end != t.end;
  it->start = t.start;
  it->end = t.end;
  it->speaker = spk;
  it->text = t.text;
  if (changed && out) out->push_back({t.start, t.end, {*it}});
}

void ComprehensiveTimeline::ReprojectRange(double start, double end,
                                           std::vector<Revision>* out) {
  for (const auto& t : texts_) {
    if (Overlap(t.start, t.end, start, end) > 0.0) ReprojectText(t, out);
  }
}

std::vector<ComprehensiveTimeline::Revision> ComprehensiveTimeline::UpsertSpeaker(
    double start, double end, const std::string& speaker, float conf) {
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
  ReprojectText(*it, &revs);
  return revs;
}

std::vector<ComprehensiveTimeline::Revision> ComprehensiveTimeline::ReplaceSpeakers(
    const std::vector<SpeakerInput>& segs) {
  // Diarization delivers its whole current segment view (global derivation), so
  // replace the speaker set wholesale and re-project ALL text. Emit a revision
  // for every text whose attributed result changed.
  speakers_.clear();
  for (const auto& s : segs) {
    SpeakerSeg seg;
    seg.start = s.start;
    seg.end = s.end;
    seg.speaker = s.speaker;
    seg.conf = s.conf;
    auto pos = std::lower_bound(
        speakers_.begin(), speakers_.end(), s.start,
        [](const SpeakerSeg& a, double v) { return a.start < v; });
    speakers_.insert(pos, std::move(seg));
  }
  std::vector<Revision> revs;
  for (const auto& t : texts_) ReprojectText(t, &revs);
  return revs;
}

void ComprehensiveTimeline::MarkEndpoint(double time) {
  auto pos = std::lower_bound(endpoints_.begin(), endpoints_.end(), time);
  endpoints_.insert(pos, time);
}

std::vector<ComprehensiveTimeline::Entry> ComprehensiveTimeline::Snapshot()
    const {
  // Order entries by time, then coalesce consecutive same-speaker entries.
  std::vector<Entry> ordered = view_;
  std::sort(ordered.begin(), ordered.end(),
            [](const Entry& a, const Entry& b) { return a.start < b.start; });

  std::vector<Entry> out;
  for (const auto& e : ordered) {
    if (!out.empty() && out.back().speaker == e.speaker) {
      out.back().end = std::max(out.back().end, e.end);
      if (!e.text.empty()) {
        if (!out.back().text.empty()) out.back().text += " ";
        out.back().text += e.text;
      }
    } else {
      out.push_back(e);
    }
  }
  return out;
}

void ComprehensiveTimeline::Clear() {
  speakers_.clear();
  texts_.clear();
  endpoints_.clear();
  view_.clear();
}

}  // namespace pipeline
}  // namespace orator
