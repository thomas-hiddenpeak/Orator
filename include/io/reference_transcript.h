#pragma once

// Parser for diarized-transcript reference files of the form:
//
//   HH:MM:SS SpeakerName
//   <utterance text...>
//   HH:MM:SS SpeakerName
//   <utterance text...>
//
// Used to (a) drive the timeline-fusion path with real data and (b) provide a
// ground-truth speaker track to validate diarization-dependent stages.

#include <string>
#include <vector>

#include "core/types.h"

namespace orator {
namespace io {

struct ReferenceUtterance {
  double start_sec = 0.0;
  std::string speaker;
  std::string text;
};

struct ReferenceTranscript {
  std::vector<ReferenceUtterance> utterances;

  // Distinct speaker names in first-appearance order.
  std::vector<std::string> Speakers() const;

  // Ground-truth diarization: each utterance becomes a segment spanning until
  // the next utterance's start (the last extends by tail_sec).
  std::vector<core::DiarSegment> ToDiarSegments(double tail_sec = 5.0) const;

  // ASR-style transcript WITHOUT speaker labels (one token per utterance).
  // Mirrors what an ASR system would emit: text + timing, no speaker info.
  core::Transcript ToTranscript(double tail_sec = 5.0) const;
};

// Parses the reference file. Throws std::runtime_error on open failure.
ReferenceTranscript ParseReferenceTranscript(const std::string& path);

// Parses "HH:MM:SS" into seconds; returns -1 on malformed input.
double ParseTimestamp(const std::string& hhmmss);

}  // namespace io
}  // namespace orator
