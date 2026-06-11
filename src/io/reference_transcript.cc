#include "io/reference_transcript.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

namespace orator {
namespace io {

double ParseTimestamp(const std::string& hhmmss) {
  int h = 0, m = 0, s = 0;
  char c1 = 0, c2 = 0;
  std::istringstream iss(hhmmss);
  iss >> h >> c1 >> m >> c2 >> s;
  if (!iss || c1 != ':' || c2 != ':') return -1.0;
  return h * 3600.0 + m * 60.0 + s;
}

namespace {

std::string Trim(const std::string& s) {
  size_t b = 0, e = s.size();
  while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
  while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
  return s.substr(b, e - b);
}

// A header line looks like "HH:MM:SS Name". The speaker is taken as everything
// after the first whitespace token so malformed timestamps (e.g. a 3-digit
// seconds field like "00:34:087" seen in real data) don't leak into the name.
bool ParseHeader(const std::string& line, double* time, std::string* speaker) {
  // First whitespace-delimited token must look like a timestamp.
  size_t sp = line.find_first_of(" \t");
  if (sp == std::string::npos) return false;
  const std::string ts = line.substr(0, sp);

  // Validate H:M:S shape: digits separated by two colons.
  size_t c1 = ts.find(':');
  if (c1 == std::string::npos) return false;
  size_t c2 = ts.find(':', c1 + 1);
  if (c2 == std::string::npos) return false;
  auto all_digits = [](const std::string& s) {
    if (s.empty()) return false;
    for (char ch : s)
      if (!std::isdigit(static_cast<unsigned char>(ch))) return false;
    return true;
  };
  const std::string hh = ts.substr(0, c1);
  const std::string mm = ts.substr(c1 + 1, c2 - c1 - 1);
  std::string ss = ts.substr(c2 + 1);
  if (!all_digits(hh) || !all_digits(mm) || !all_digits(ss)) return false;
  // Tolerate a malformed 3-digit seconds field by keeping the first two digits.
  if (ss.size() > 2) ss = ss.substr(0, 2);

  const double t = ParseTimestamp(hh + ":" + mm + ":" + ss);
  if (t < 0) return false;

  std::string name = Trim(line.substr(sp + 1));
  if (name.empty()) return false;
  *time = t;
  *speaker = name;
  return true;
}

}  // namespace

ReferenceTranscript ParseReferenceTranscript(const std::string& path) {
  std::ifstream f(path);
  if (!f.is_open()) {
    throw std::runtime_error("Cannot open reference transcript: " + path);
  }

  ReferenceTranscript out;
  std::string line;
  ReferenceUtterance* current = nullptr;

  while (std::getline(f, line)) {
    const std::string trimmed = Trim(line);
    if (trimmed.empty()) continue;

    double t = 0.0;
    std::string speaker;
    if (ParseHeader(trimmed, &t, &speaker)) {
      ReferenceUtterance u;
      u.start_sec = t;
      u.speaker = speaker;
      out.utterances.push_back(std::move(u));
      current = &out.utterances.back();
    } else if (current != nullptr) {
      if (!current->text.empty()) current->text += " ";
      current->text += trimmed;
    }
  }

  return out;
}

std::vector<std::string> ReferenceTranscript::Speakers() const {
  std::vector<std::string> speakers;
  std::unordered_set<std::string> seen;
  for (const auto& u : utterances) {
    if (seen.insert(u.speaker).second) speakers.push_back(u.speaker);
  }
  return speakers;
}

std::vector<core::DiarSegment> ReferenceTranscript::ToDiarSegments(
    double tail_sec) const {
  std::vector<std::string> speakers = Speakers();
  auto speaker_index = [&](const std::string& name) {
    for (size_t i = 0; i < speakers.size(); ++i) {
      if (speakers[i] == name) return static_cast<int>(i);
    }
    return -1;
  };

  std::vector<core::DiarSegment> segs;
  for (size_t i = 0; i < utterances.size(); ++i) {
    core::DiarSegment seg;
    seg.start_sec = utterances[i].start_sec;
    seg.end_sec = (i + 1 < utterances.size())
                      ? utterances[i + 1].start_sec
                      : utterances[i].start_sec + tail_sec;
    if (seg.end_sec < seg.start_sec) seg.end_sec = seg.start_sec;
    seg.local_speaker = speaker_index(utterances[i].speaker);
    seg.speaker_id = utterances[i].speaker;
    seg.confidence = 1.0f;
    segs.push_back(seg);
  }
  return segs;
}

core::Transcript ReferenceTranscript::ToTranscript(double tail_sec) const {
  core::Transcript tr;
  for (size_t i = 0; i < utterances.size(); ++i) {
    core::AsrToken tok;
    tok.start_sec = utterances[i].start_sec;
    tok.end_sec = (i + 1 < utterances.size())
                      ? utterances[i + 1].start_sec
                      : utterances[i].start_sec + tail_sec;
    if (tok.end_sec < tok.start_sec) tok.end_sec = tok.start_sec;
    tok.text = utterances[i].text;  // no speaker label (ASR-like)
    tr.tokens.push_back(std::move(tok));
  }
  return tr;
}

}  // namespace io
}  // namespace orator
