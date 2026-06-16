#pragma once

// ComprehensiveTimeline (Spec 004): the native, stateful comprehensive view on
// one common time base (seconds; origin = absolute sample 0).
//
// Each independent pipeline deposits its result on the common time base:
//   - diarization -> UpsertSpeaker(start,end,speaker,conf)  (who / when)
//   - ASR         -> UpsertText(id,start,end,text)          (what / when)
//   - endpoints   -> MarkEndpoint(time)                     (markers)
//
// The correspondence "who said what" is derived purely by TIME ALIGNMENT:
// diarization only says who is speaking when; it never maps text. A text segment
// is attributed to the speaker segment with the greatest time overlap. This is a
// PURE TIME-ALIGNMENT layer: it NEVER infers, substitutes, or back-fills another
// pipeline's content. If diarization has not covered a span, the speaker is
// honestly "unknown" (not borrowed from the nearest segment). Because the
// structure is stateful and revisable, an early "unknown" or attribution made
// against incomplete diarization is corrected in place when diarization later
// covers the span -- each such change is returned as a Revision so the
// controller can push it to the WS consumer. Updates are incremental: an upsert
// re-projects only the text segments overlapping the changed time range.
//
// No per-word timestamps are required (the ASR model does not emit them);
// attribution is at text-segment granularity, and ASR segments are produced at
// speech endpoints so they are mostly single-speaker.

#include <string>
#include <vector>

namespace orator {
namespace pipeline {

class ComprehensiveTimeline {
 public:
  // One comprehensive entry: a time span attributed to a speaker with its text.
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
  // the revision for this segment if its attributed entry is new or changed.
  std::vector<Revision> UpsertText(long id, double start, double end,
                                   const std::string& text);

  // Record an endpoint marker. Endpoints are a PURE MARKER track: they are
  // serialized into the timeline document (see SnapshotEndpoints) but never
  // modify, split, or re-segment the speaker/text attribution.
  void MarkEndpoint(double time);

  // The current comprehensive view, time-ordered, consecutive same-speaker
  // entries coalesced. Used for the final timeline snapshot.
  std::vector<Entry> Snapshot() const;

  // The recorded endpoint marker times (sorted), for the serialized endpoint
  // track in the timeline document (Spec 004 FR7).
  std::vector<double> SnapshotEndpoints() const { return endpoints_; }

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

  // Attribute one text segment to the max-time-overlap speaker (or "unknown").
  std::string AttributeSpeaker(const TextSeg& t) const;
  // Re-project text segments overlapping [start,end); update view_ entries and
  // collect those whose attribution changed into `out`.
  void ReprojectRange(double start, double end, std::vector<Revision>* out);
  // Re-project a single text segment by id; update its view_ entry; if new or
  // changed, append a revision to `out`.
  void ReprojectText(const TextSeg& t, std::vector<Revision>* out);

  std::vector<SpeakerSeg> speakers_;  // who/when (overlaps allowed)
  std::vector<TextSeg> texts_;        // what/when, keyed by id
  std::vector<double> endpoints_;     // markers
  // Current attributed entry per text id (the projection kept in sync).
  std::vector<Entry> view_;           // one entry per text segment, by text_id
};

}  // namespace pipeline
}  // namespace orator
