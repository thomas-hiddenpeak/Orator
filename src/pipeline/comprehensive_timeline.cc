#include "pipeline/comprehensive_timeline.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

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
  for (std::size_t i = 0; i < s.size();) {
    o.push_back(i);
    const unsigned char c = static_cast<unsigned char>(s[i]);
    int adv = 1;
    if (c >= 0xF0) adv = 4;
    else if (c >= 0xE0) adv = 3;
    else if (c >= 0xC0) adv = 2;
    i += adv;
  }
  o.push_back(s.size());
  return o;
}

bool EntriesEqual(const std::vector<ComprehensiveTimeline::Entry>& a,
                  const std::vector<ComprehensiveTimeline::Entry>& b) {
  if (a.size() != b.size()) return false;
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (a[i].speaker != b[i].speaker || a[i].text != b[i].text ||
        std::abs(a[i].start - b[i].start) > 1e-9 ||
        std::abs(a[i].end - b[i].end) > 1e-9) {
      return false;
    }
  }
  return true;
}
}  // namespace

std::string ComprehensiveTimeline::AttributeInterval(double start,
                                                     double end) const {
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
  return best != nullptr ? best->speaker : "unknown";
}

std::vector<ComprehensiveTimeline::Entry> ComprehensiveTimeline::SplitTextByDiar(
    const TextSeg& t) const {
  // The comprehensive VIEW's boundaries come from the DIARIZATION TRACK. Collect
  // the diarization boundary times that fall strictly inside this text segment,
  // partition [t.start,t.end) by them, attribute each sub-interval to its
  // max-overlap speaker, merge consecutive same-speaker sub-intervals, then
  // allocate the text's characters to each resulting turn proportionally by
  // time (the ASR model emits no per-word timestamps, so a time-proportional
  // split is the faithful approximation).
  std::vector<double> bounds = {t.start, t.end};
  for (const auto& s : speakers_) {
    if (s.start > t.start + 1e-9 && s.start < t.end - 1e-9) bounds.push_back(s.start);
    if (s.end > t.start + 1e-9 && s.end < t.end - 1e-9) bounds.push_back(s.end);
  }
  std::sort(bounds.begin(), bounds.end());
  bounds.erase(std::unique(bounds.begin(), bounds.end(),
                           [](double a, double b) { return std::abs(a - b) < 1e-9; }),
               bounds.end());

  struct Turn {
    double start;
    double end;
    std::string speaker;
  };
  std::vector<Turn> turns;
  for (std::size_t i = 0; i + 1 < bounds.size(); ++i) {
    const double s = bounds[i];
    const double e = bounds[i + 1];
    if (e - s <= 1e-9) continue;
    const std::string spk = AttributeInterval(s, e);
    if (!turns.empty() && turns.back().speaker == spk) {
      turns.back().end = e;
    } else {
      turns.push_back({s, e, spk});
    }
  }

  if (turns.empty()) {
    return {Entry{t.start, t.end, "unknown", t.text, t.id}};
  }
  if (turns.size() == 1) {
    return {Entry{turns[0].start, turns[0].end, turns[0].speaker, t.text, t.id}};
  }

  // Proportional codepoint allocation across turns (cumulative to avoid drift).
  const std::vector<std::size_t> offs = Utf8Offsets(t.text);
  const int ncp = static_cast<int>(offs.size()) - 1;
  const double total = t.end - t.start;
  std::vector<Entry> out;
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
                   std::move(slice), t.id});
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

void ComprehensiveTimeline::AddVad(double start, double end) {
  VadSeg v{start, end};
  auto pos = std::lower_bound(
      vad_.begin(), vad_.end(), start,
      [](const VadSeg& a, double s) { return a.start < s; });
  vad_.insert(pos, v);
}

std::vector<ComprehensiveTimeline::Entry> ComprehensiveTimeline::Snapshot()
    const {
  // The comprehensive view is the diarization-split projection of every text
  // segment, time-ordered, with consecutive same-speaker entries coalesced.
  std::vector<Entry> all;
  for (const auto& kv : pieces_) {
    for (const auto& e : kv.second) all.push_back(e);
  }
  std::sort(all.begin(), all.end(),
            [](const Entry& a, const Entry& b) { return a.start < b.start; });

  std::vector<Entry> out;
  for (const auto& e : all) {
    if (!out.empty() && out.back().speaker == e.speaker) {
      out.back().end = std::max(out.back().end, e.end);
      out.back().text += e.text;
    } else {
      out.push_back(e);
    }
  }
  return out;
}

std::vector<ComprehensiveTimeline::RawTextSeg> ComprehensiveTimeline::SnapshotRawTexts()
    const {
  std::vector<RawTextSeg> out;
  out.reserve(texts_.size());
  for (const auto& t : texts_) {
    out.push_back({t.id, t.start, t.end, t.text});
  }
  return out;
}

void ComprehensiveTimeline::Clear() {
  speakers_.clear();
  texts_.clear();
  vad_.clear();
  pieces_.clear();
}

}  // namespace pipeline
}  // namespace orator
