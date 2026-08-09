#pragma once

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace orator {
namespace io {

struct AsrStreamStateRecord {
  std::int64_t end_sample = 0;
  int chunk_id = 0;
  bool final_tail = false;
  bool unfixed_prefix = false;
  int max_new_tokens = 0;
  int unfixed_chunks = 0;
  int unfixed_tokens = 0;
  std::vector<int> raw_decoded_token_ids;
  std::vector<int> retained_prefix_token_ids;
  std::string retained_prefix_text;
  std::vector<int> generated_token_ids;
  std::string generated_text;
  std::string continuation_text;
};

// Opt-in JSONL evidence writer for the inactive accumulated ASR state machine.
// It records raw state only and performs no transcript comparison or scoring.
class AsrStreamStateTrace {
 public:
  explicit AsrStreamStateTrace(const std::string& path);

  void BeginSegment(std::int64_t base_sample);
  void Write(const AsrStreamStateRecord& record);

 private:
  std::ofstream output_;
  std::int64_t base_sample_ = 0;
  int segment_id_ = -1;
};

}  // namespace io
}  // namespace orator
