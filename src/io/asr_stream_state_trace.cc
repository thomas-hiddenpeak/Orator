#include "io/asr_stream_state_trace.h"

#include <stdexcept>

#include "io/json_escape.h"

namespace orator {
namespace io {
namespace {

void WriteTokenIds(std::ostream& output, const std::vector<int>& token_ids) {
  output << '[';
  for (size_t i = 0; i < token_ids.size(); ++i) {
    if (i != 0) output << ',';
    output << token_ids[i];
  }
  output << ']';
}

}  // namespace

AsrStreamStateTrace::AsrStreamStateTrace(const std::string& path)
    : output_(path, std::ios::out | std::ios::trunc) {
  if (path.empty() || !output_.is_open()) {
    throw std::runtime_error("failed to open ASR stream-state trace: " + path);
  }
}

void AsrStreamStateTrace::BeginSegment(std::int64_t base_sample) {
  base_sample_ = base_sample;
  ++segment_id_;
}

void AsrStreamStateTrace::Write(const AsrStreamStateRecord& record) {
  if (segment_id_ < 0 || record.end_sample < base_sample_) {
    throw std::logic_error("invalid ASR stream-state trace extent");
  }
  if (record.retained_prefix_token_ids.size() >
      record.raw_decoded_token_ids.size()) {
    throw std::logic_error("invalid ASR stream-state rollback boundary");
  }

  output_ << "{\"schema_version\":1,\"segment_id\":" << segment_id_
          << ",\"chunk_id\":" << record.chunk_id
          << ",\"base_sample\":" << base_sample_
          << ",\"end_sample\":" << record.end_sample
          << ",\"num_samples\":" << (record.end_sample - base_sample_)
          << ",\"final_tail\":" << (record.final_tail ? "true" : "false")
          << ",\"unfixed_prefix\":"
          << (record.unfixed_prefix ? "true" : "false")
          << ",\"max_new_tokens\":" << record.max_new_tokens
          << ",\"unfixed_chunks\":" << record.unfixed_chunks
          << ",\"unfixed_tokens\":" << record.unfixed_tokens
          << ",\"rollback_tokens_applied\":"
          << (record.raw_decoded_token_ids.size() -
              record.retained_prefix_token_ids.size())
          << ",\"raw_decoded_token_ids\":";
  WriteTokenIds(output_, record.raw_decoded_token_ids);
  output_ << ",\"retained_prefix_token_ids\":";
  WriteTokenIds(output_, record.retained_prefix_token_ids);
  output_ << ",\"retained_prefix_text\":\""
          << JsonEscape(record.retained_prefix_text)
          << "\",\"generated_token_ids\":";
  WriteTokenIds(output_, record.generated_token_ids);
  output_ << ",\"generated_text\":\"" << JsonEscape(record.generated_text)
          << "\",\"continuation_text\":\""
          << JsonEscape(record.continuation_text) << "\"}\n";
  output_.flush();
  if (!output_) {
    throw std::runtime_error("failed to write ASR stream-state trace");
  }
}

}  // namespace io
}  // namespace orator
