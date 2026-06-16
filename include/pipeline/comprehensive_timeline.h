#pragma once

// ComprehensiveTimeline (Spec 004): the native, stateful container that holds the
// three independent pipelines' tracks on one common time base (seconds; origin =
// absolute sample 0). It is a PURE CONTAINER + ALIGNMENT layer: it only RECEIVES
// each pipeline's data + meta + time codes and never modifies, infers, or
// back-fills another pipeline's content.
//
//   - diarization -> ReplaceSpeakers(segs)        (who / when)   [diar track]
//   - ASR         -> UpsertText(id,start,end,text) (what / when)  [asr track]
//   - VAD         -> AddVad(start,end)             (speech segs)  [vad track]
//
// The COMPREHENSIVE VIEW is a DERIVED PRODUCT of this container, for human
// browsing -- it is not a fourth pipeline. The view answers "who said what when"
// and its boundaries come from the DIARIZATION TRACK: each ASR text segment is
// placed onto the diarization speaker turns it overlaps, and when a text segment
// crosses a diarization boundary its text is split at that boundary (proportional
// to time, since the ASR model emits no per-word timestamps). The view never
// re-segments by ASR's own coarse segmentation. Where diarization has not covered
// a span the speaker is honestly "unknown" (never borrowed from a neighbour). The
// view is a faithful interactive form of the container and carries the same
// characteristics (common time base, no content invention).
//
// Because the container is stateful and revisable, a text placed against
// incomplete diarization is re-projected when diarization later covers the span;
// each change is returned as a Revision the controller pushes to the WS consumer.

#include <map>
#include <string>
#include <utility>
#include <vector>

namespace orator {
namespace pipeline {

class ComprehensiveTimeline {
 public:
  // One view entry: a time span attributed to a speaker with its (possibly
  // diarization-split) text slice. Multiple entries may share a text_id when one
  // ASR segment is split across several diarization turns.
  struct Entry {
    double start = 0.0;
    double end = 0.0;
    std::string speaker;  // "speaker_<n>" or resolved id, "unknown" if none yet
    std::string text;
    long text_id = -1;    // source text-segment id (for revision tracking)
  };

  // A revision: the consumer should replace its comprehensive entries whose
  // text_id matches with these entries (or insert if new). dirty_start/end bound
  // the affected time range for convenience.
  struct Revision {
    double dirty_start = 0.0;
    double dirty_end = 0.0;
    std::vector<Entry> entries;  // the new state of the changed entries
  };

  // Deposit a speaker segment (who/when). Returns the revisions caused by any
  // change in attribution of overlapping text segments.
  std::vector<Revision> UpsertSpeaker(double start, double end,
                                      const std::string& speaker, float conf);

  // Replace the ENTIRE speaker set in one call and re-project all text. This is
  // the delivery model for diarization, whose segments are a GLOBAL derivation
  // from frames (boundaries shift as frames arrive), so there is no stable
  // per-segment event to upsert -- the pipeline delivers its whole current
  // segment view and the timeline re-projects. Returns revisions for every text
  // segment whose attribution changed.
  struct SpeakerInput {
    double start;
    double end;
    std::string speaker;
    float conf;
  };
  std::vector<Revision> ReplaceSpeakers(const std::vector<SpeakerInput>& segs);

  // Deposit or replace a text segment (what/when), keyed by a stable id. Returns
  // a revision for this segment if its diarization-split projection changed.
  std::vector<Revision> UpsertText(long id, double start, double end,
                                   const std::string& text);

  // Deposit a VAD speech segment (the VAD pipeline's data: a voice-activity
  // region on the common time base). The vad track is a pure data track; it does
  // not modify the diar/asr tracks or drive the view's boundaries.
  void AddVad(double start, double end);

  // One VAD speech segment.
  struct VadSeg {
    double start = 0.0;
    double end = 0.0;
  };

  // The comprehensive view: diarization-driven speaker turns with their text,
  // time-ordered, consecutive same-speaker entries coalesced. Derived product.
  std::vector<Entry> Snapshot() const;

  // The recorded VAD speech segments (sorted), for the serialized vad track.
  std::vector<VadSeg> SnapshotVad() const { return vad_; }

  void Clear();

 private:
  struct SpeakerSeg {
    double start = 0.0;
    double end = 0.0;
    std::string speaker;
    float conf = 0.0f;
  };
  struct TextSeg {
    long id = -1;
    double start = 0.0;
    double end = 0.0;
    std::string text;
  };

  // Attribute the interval [start,end) to the max-time-overlap speaker (tighter
  // span then higher confidence on ties), or "unknown" if no diarization covers
  // it. Used per sub-interval when splitting a text by diarization boundaries.
  std::string AttributeInterval(double start, double end) const;
  // Split one text segment at the diarization-track boundaries it crosses,
  // allocating the text to each speaker turn proportionally by time. Returns the
  // resulting view entries (>=1) for this text id.
  std::vector<Entry> SplitTextByDiar(const TextSeg& t) const;
  // Re-project text segments overlapping [start,end); update pieces_ and collect
  // those whose projection changed into `out`.
  void ReprojectRange(double start, double end, std::vector<Revision>* out);
  // Re-project a single text segment by id; update its pieces_; if new or
  // changed, append a revision to `out`.
  void ReprojectText(const TextSeg& t, std::vector<Revision>* out);

  std::vector<SpeakerSeg> speakers_;  // diar track: who/when (overlaps allowed)
  std::vector<TextSeg> texts_;        // asr track: what/when, keyed by id
  std::vector<VadSeg> vad_;           // vad track: speech segments
  // Current diarization-split projection per text id (kept in sync).
  std::map<long, std::vector<Entry>> pieces_;
};

}  // namespace pipeline
}  // namespace orator
