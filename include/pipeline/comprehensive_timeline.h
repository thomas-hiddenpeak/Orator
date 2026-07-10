#pragma once

// ComprehensiveTimeline (Spec 004): the native, stateful container that holds
// the three independent pipelines' tracks on one common time base (seconds;
// origin = absolute sample 0). It is a PURE CONTAINER + ALIGNMENT layer: it
// only RECEIVES each pipeline's data + meta + time codes and never modifies,
// infers, or back-fills another pipeline's content.
//
//   - diarization -> ReplaceSpeakers(segs)        (who / when)   [diar track]
//   - ASR         -> UpsertText(id,start,end,text) (what / when)  [asr track]
//   - VAD         -> AddVad(start,end)             (speech segs)  [vad track]
//
// The COMPREHENSIVE VIEW is a DERIVED PRODUCT of this container, for accurate
// terminal output and human browsing -- it is not a fourth pipeline. The view
// answers "who said what when" by applying available pipeline evidence on the
// common time base: ASR supplies accepted finalized text_id spans, diarization
// supplies speaker ownership, and forced-alignment deposits can refine internal
// text boundaries. Each ASR text segment is placed onto the diarization speaker
// turns it overlaps, and when a text segment crosses a diarization boundary its
// text is split at that boundary (proportional to time, since the ASR model
// emits no per-word timestamps unless forced alignment is present). Where
// diarization has not covered a span the speaker is honestly "unknown" (never
// borrowed from a neighbour). The view preserves source ASR text_id/final
// boundaries in terminal snapshots; presentation-level speaker-turn grouping
// belongs in a separate consumer view.
//
// Because the container is stateful and revisable, a text placed against
// incomplete diarization is re-projected when diarization later covers the
// span; each change is returned as a Revision the controller pushes to the WS
// consumer.
//
#include <functional>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace orator {

// Forward declaration needed by friend declarations below.
namespace protocol {
class Message;
}

namespace pipeline {

class ComprehensiveTimeline {
 public:
  // One view entry: a time span attributed to a speaker with their (possibly
  // diarization-split) text slice. Multiple entries may share a text_id when
  // one ASR segment is split across several diarization turns.
  struct Entry {
    double start = 0.0;
    double end = 0.0;
    std::string speaker;  // "speaker_<n>" or resolved id, "unknown" if none yet
    std::string speaker_id;  // resolved global voiceprint id for this interval
    std::string text;
    long text_id = -1;  // source text-segment id (for revision tracking)
    // Speaker-evidence diagnostics for this exact comprehensive interval. These
    // fields do not change attribution; they expose how much raw diar evidence
    // supports the selected speaker so downstream views can flag weak regions.
    double diar_overlap_sec = 0.0;
    double diar_total_overlap_sec = 0.0;
    double diar_coverage_ratio = 0.0;
    double diar_total_coverage_ratio = 0.0;
    double diar_max_gap_sec = 0.0;
    int diar_island_count = 0;
    std::string speaker_support = "none";  // none | weak | strong
  };

  // A revision: the consumer should replace its comprehensive entries whose
  // text_id matches with these entries (or insert if new). dirty_start/end
  // bound the affected time range for convenience.
  struct Revision {
    double dirty_start = 0.0;
    double dirty_end = 0.0;
    std::vector<Entry> entries;  // the new state of the changed entries
  };

  // One raw text segment as stored internally, exposed for serialization.
  // This is the ASR track data (who/when) before diarization attribution.
  struct RawTextSeg {
    long id = -1;
    double start = 0.0;
    double end = 0.0;
    std::string text;
  };

  // Snapshot all raw text segments for the ASR track in Serialize().
  // Returns the text segments ordered by start time.
  std::vector<RawTextSeg> SnapshotRawTexts() const;

  // One VAD speech segment.
  struct VadSeg {
    double start = 0.0;
    double end = 0.0;
  };

  // One forced-alignment unit (word/character) with its time span on the common
  // clock. A REFINEMENT of an ASR text segment: per-unit timestamps the ASR
  // engine itself does not emit. Times are already on the common time base.
  struct AlignUnitSeg {
    double start = 0.0;
    double end = 0.0;
    std::string text;
  };

  // One aligned ASR segment: the source text_id, its bounds, and the per-unit
  // timestamps, for the serialized align track.
  struct AlignGroup {
    long text_id = -1;
    double start = 0.0;
    double end = 0.0;
    std::vector<AlignUnitSeg> units;
  };

  // The comprehensive view: ASR text spans projected through diarization
  // ownership, time-ordered, preserving ASR text_id/final boundaries while
  // coalescing only adjacent pieces from the same source text segment.
  std::vector<Entry> Snapshot() const;

  // Maximum gap between adjacent forced-alignment units that is still treated
  // as one coherent utterance run for speaker attribution. A lower value lets
  // the view split at short but real turn-change pauses.
  void set_align_snap_pause_sec(double sec);
  void set_align_boundary_split_tolerance_sec(double sec);
  void set_speaker_support_min_coverage_ratio(double ratio);
  void set_speaker_support_max_gap_sec(double sec);
  void set_speaker_support_max_islands(int count);

  // The recorded VAD speech segments (sorted), for the serialized vad track.
  std::vector<VadSeg> SnapshotVad() const { return vad_; }

  // Map of diarization speaker LABEL ("speaker_<n>") -> resolved global
  // voiceprint id (Spec 010), for serializing the global identity onto the
  // comprehensive view's speaker turns. Empty ids are omitted.
  std::map<std::string, std::string> SpeakerLabelIds() const;

  // Every distinct global voiceprint id assigned to a speaker turn over the
  // whole session (accumulated; not just the current diarizer window). This is
  // what a management UI should list, since the transcript accumulates ids the
  // same way while SpeakerLabelIds() only reflects the <=N current slots.
  std::vector<std::string> AllSpeakerIds() const;

  // The forced-alignment groups (one per aligned text segment, ordered by
  // start), for the serialized align track. Each refines an ASR segment into
  // per-unit timestamps on the common time base.
  std::vector<AlignGroup> SnapshotAlign() const;

  void Clear();

 protected:
  // ---------------------------------------------------------------------------
  // INTERNAL mutation API — visible only to protocol bridge (friend) and
  // test subclasses. Production pipeline data MUST arrive through
  // ProtocolTimeline subscriptions; never call these directly.
  // ---------------------------------------------------------------------------

  // Friend the three protocol subscription bridge functions.
  friend void HandleVadSubscription(ComprehensiveTimeline&, std::mutex&,
                                    const orator::protocol::Message&);
  friend void HandleDiarSubscription(
      ComprehensiveTimeline&, std::mutex&, const orator::protocol::Message&,
      const std::function<void(const std::string&)>&);
  friend void HandleAsrSubscription(
      ComprehensiveTimeline&, std::mutex&, const orator::protocol::Message&,
      const std::function<void(const std::string&)>&);
  friend void HandleAlignSubscription(
      ComprehensiveTimeline&, std::mutex&, const orator::protocol::Message&,
      const std::function<void(const std::string&)>&);
  // Deposit a speaker segment (who/when). Returns revisions caused by
  // attribution changes in overlapping text segments.
  std::vector<Revision> UpsertSpeaker(double start, double end,
                                      const std::string& speaker, float conf);

  // Replace the ENTIRE speaker set in one call and re-project all text.
  struct SpeakerInput {
    double start;
    double end;
    std::string speaker;
    float conf;
    std::string speaker_id;  // resolved global voiceprint id ("" if none)
  };
  std::vector<Revision> ReplaceSpeakers(const std::vector<SpeakerInput>& segs);

  // Deposit or replace a text segment (what/when), keyed by a stable id.
  std::vector<Revision> UpsertText(long id, double start, double end,
                                   const std::string& text);

  // Deposit a VAD speech segment.
  void AddVad(double start, double end);

  // Deposit (or replace) the forced-alignment units for one ASR text segment,
  // keyed by its source text_id. Idempotent: re-depositing the same id replaces
  // its units. Times must already be on the common time base. Re-projects the
  // matching text so the comprehensive view splits it at diarization boundaries
  // by each unit's exact timestamp (instead of the time-proportional fallback);
  // returns the resulting revisions.
  std::vector<Revision> UpsertAlign(long text_id, double start, double end,
                                    const std::vector<AlignUnitSeg>& units);

  // Clean up old data to prevent memory accumulation
  void CleanupOldData(double keep_until_sec);

 private:
  struct SpeakerSeg {
    double start = 0.0;
    double end = 0.0;
    std::string speaker;
    float conf = 0.0f;
    std::string speaker_id;  // resolved global voiceprint id ("" if none)
  };
  struct SpeakerAttr {
    std::string speaker;
    std::string speaker_id;
  };
  struct SpeakerSupport {
    double overlap_sec = 0.0;
    double total_overlap_sec = 0.0;
    double coverage_ratio = 0.0;
    double total_coverage_ratio = 0.0;
    double max_gap_sec = 0.0;
    int island_count = 0;
    std::string level = "none";
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
  SpeakerAttr AttributeInterval(double start, double end) const;
  SpeakerSupport ComputeSpeakerSupport(double start, double end,
                                       const std::string& speaker,
                                       const std::string& speaker_id) const;
  Entry MakeEntry(double start, double end, const std::string& speaker,
                  const std::string& speaker_id, std::string text,
                  long text_id) const;
  void MergeEntrySupport(Entry* dst, const Entry& src) const;
  // Split one text segment at the diarization-track boundaries it crosses,
  // allocating the text to each speaker turn proportionally by time. Returns
  // the resulting view entries (>=1) for this text id.
  std::vector<Entry> SplitTextByDiar(const TextSeg& t) const;
  // Re-project text segments overlapping [start,end); update pieces_ and
  // collect those whose projection changed into `out`.
  void ReprojectRange(double start, double end, std::vector<Revision>* out);
  // Re-project a single text segment by id; update its pieces_; if new or
  // changed, append a revision to `out`.
  void ReprojectText(const TextSeg& t, std::vector<Revision>* out);

  std::vector<SpeakerSeg> speakers_;  // diar track: who/when (overlaps allowed)
  std::set<std::string> seen_speaker_ids_;  // every global id ever assigned
  std::vector<TextSeg> texts_;              // asr track: what/when, keyed by id
  std::vector<VadSeg> vad_;                 // vad track: speech segments
  double align_snap_pause_sec_ = 0.25;
  double align_boundary_split_tolerance_sec_ = 0.08;
  double speaker_support_min_coverage_ratio_ = 0.50;
  double speaker_support_max_gap_sec_ = 1.00;
  int speaker_support_max_islands_ = 1;
  // align track: per-unit timestamps refining an ASR segment, keyed by text_id.
  std::map<long, AlignGroup> align_;
  // Current diarization-split projection per text id (kept in sync).
  std::map<long, std::vector<Entry>> pieces_;
};

}  // namespace pipeline
}  // namespace orator
