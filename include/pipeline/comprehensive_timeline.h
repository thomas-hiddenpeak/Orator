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
// characteristics (common time base, no invented content).
//
// Because the container is stateful and revisable, a text placed against
// incomplete diarization is re-projected when diarization later covers the span;
// each change is returned as a Revision the controller pushes to the WS consumer.
//
// Lightweight event subscription (extensibility for future pipeline growth):
// each mutation method fires a typed event after the data change completes.
// Subscribers receive the event payload (event type + affected time range) via
// a callback. Events are dispatched synchronously, outside the internal lock,
// after the mutation is fully committed. A subscriber's callback receives a
// consistent view but may see subsequent mutations by the time it runs.

#include <functional>
#include <map>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace orator {
namespace pipeline {

class ComprehensiveTimeline {
 public:
  // Lightweight event types — each mutation fires zero or more events.
  // Subscribers use these to react to timeline changes without polling.
  enum class Event {
    VAD_ADDED,          // a VAD speech segment was deposited
    SPEAKER_REPLACED,   // the entire speaker set was replaced
    TEXT_UPSERTED,      // a text segment was deposited or replaced
  };

  // Event payload: what changed and the affected time range.
  // Subscribers use `type` to switch behavior and `[start,end)` to scope
  // any follow-up read (e.g. `SnapshotVadSince(start)`).
  struct EventPayload {
    Event type;
    double start = 0.0;
    double end = 0.0;
  };

  using EventHandler = std::function<void(const EventPayload&)>;
  // Opaque subscription handle for unsubscribe.
  using SubscriptionId = long;

  // Subscribe to events. Returns a subscription id for later unsubscribe.
  // The handler is called synchronously after each mutation completes,
  // outside the internal lock. The handler receives a consistent view but
  // may see subsequent mutations by the time it runs.
  SubscriptionId Subscribe(EventHandler handler);

  // Remove a subscription. Safe to call from within an event handler.
  void Unsubscribe(SubscriptionId id);

  // Dispatch collected events to subscribers. MUST be called outside
  // comp_mutex_ to avoid deadlock if a handler reads back.
  // Callers of mutation methods should invoke this after releasing the lock.
  // Returns true if any events were dispatched.
  bool DispatchEvents();

  // One view entry: a time span attributed to a speaker with their (possibly
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

  // The comprehensive view: diarization-driven speaker turns with their text,
  // time-ordered, consecutive same-speaker entries coalesced. Derived product.
  std::vector<Entry> Snapshot() const;

  // The recorded VAD speech segments (sorted), for the serialized vad track.
  std::vector<VadSeg> SnapshotVad() const { return vad_; }

  void Clear();

  // ---------------------------------------------------------------------------
  // INTERNAL mutation API — called by AuditoryStream protocol bridge callbacks
  // and unit tests. Production pipeline data MUST arrive through
  // ProtocolTimeline subscriptions; these methods are the sink for that path.
  // ---------------------------------------------------------------------------

  // Deposit a speaker segment (who/when). Returns the revisions caused by any
  // change in attribution of overlapping text segments.
  std::vector<Revision> UpsertSpeaker(double start, double end,
                                      const std::string& speaker, float conf);

  // Replace the ENTIRE speaker set in one call and re-project all text.
  struct SpeakerInput {
    double start;
    double end;
    std::string speaker;
    float conf;
  };
  std::vector<Revision> ReplaceSpeakers(const std::vector<SpeakerInput>& segs);

  // Deposit or replace a text segment (what/when), keyed by a stable id.
  std::vector<Revision> UpsertText(long id, double start, double end,
                                   const std::string& text);

  // Deposit a VAD speech segment.
  void AddVad(double start, double end);

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

  void fire_event_(Event type, double start, double end);

  struct HandlerEntry {
    SubscriptionId id;
    EventHandler handler;
  };
  std::vector<HandlerEntry> handlers_;
  // Collected during mutation, dispatched after lock release.
  std::vector<EventPayload> pending_events_;
  long next_subscription_id_ = 1;
};

}  // namespace pipeline
}  // namespace orator
