// Verifies stable ASR text IDs across partial events, final events, and the
// typed text sink without loading a model.

#include <cstdio>
#include <numeric>
#include <string>
#include <vector>

#include "core/stages.h"
#include "core/time_base.h"
#include "pipeline/asr_worker.h"
#include "pipeline/comprehensive_timeline.h"

namespace {

class FakeAsr final : public orator::core::IAsr {
 public:
  void Initialize(const orator::core::AsrConfig&) override {}
  void LoadWeights(const std::string&) override {}
  void Reset() override {}
  orator::core::Transcript Transcribe(
      const orator::core::AudioChunk&) override {
    return {};
  }
  void set_max_new_tokens(int) override {}
  void StreamReset(long base_sample) override {
    reset_positions.push_back(base_sample);
    current_ = "speech";
  }
  std::string StreamChunk(const float* pcm, int n, cudaStream_t) override {
    chunk_sizes.push_back(n);
    fed_samples.insert(fed_samples.end(), pcm, pcm + n);
    return current_;
  }
  std::string StreamFinalize(cudaStream_t) override { return current_; }
  int stream_audio_tokens() const override { return 1; }
  std::string name() const override { return "fake_asr"; }

  std::vector<long> reset_positions;
  std::vector<int> chunk_sizes;
  std::vector<float> fed_samples;

 private:
  std::string current_;
};

struct FinalRecord {
  long id = -1;
  double start = 0.0;
  double end = 0.0;
  std::string text;

  bool operator==(const FinalRecord&) const = default;
};

struct GateTrace {
  std::vector<long> reset_positions;
  std::vector<int> chunk_sizes;
  std::vector<float> fed_samples;
  std::vector<std::string> events;
  std::vector<FinalRecord> finals;
  size_t events_before_evidence = 0;
};

GateTrace RunPublicationSchedule(int schedule) {
  FakeAsr asr;
  orator::pipeline::ComprehensiveTimeline evidence;
  orator::pipeline::AsrWorker::Params params;
  params.sample_rate = 100;
  params.segment_sec = 0.0;
  params.asr_vad_gate = true;
  params.asr_vad_lead_ms = 100;
  params.asr_vad_gate_chunk_ms = 100;
  params.asr_vad_trail_sec = 0.1;
  params.asr_vad_min_overlap_sec = 0.01;

  GateTrace trace;
  orator::pipeline::AsrWorker worker(
      &asr, params,
      [&trace](const std::string& event) { trace.events.push_back(event); },
      orator::core::TimeBase(100), /*stream=*/0, &evidence);
  worker.set_text_sink([&trace](long id, double start, double end,
                                const std::string& text, bool final) {
    if (final) trace.finals.push_back({id, start, end, text});
  });

  std::vector<float> audio(100);
  std::iota(audio.begin(), audio.end(), 0.0f);
  const auto publish_final = [&evidence] {
    evidence.DepositVad({0.2, 0.5});
    evidence.UpdateVadState(false, 1.0);
    evidence.AdvanceVadHorizon(1.0);
  };

  if (schedule == 0) {
    publish_final();
    worker.ProcessSpan(audio.data(), static_cast<int>(audio.size()));
  } else if (schedule == 1) {
    worker.ProcessSpan(audio.data(), 10);
    trace.events_before_evidence = trace.events.size();
    publish_final();
    worker.ProcessSpan(audio.data() + 10, 90);
  } else {
    evidence.UpdateVadState(true, 0.8, 0.2, 0.45);
    worker.ProcessSpan(audio.data(), 50);
    publish_final();
    worker.ProcessSpan(audio.data() + 50, 50);
  }
  worker.Finalize();

  trace.reset_positions = asr.reset_positions;
  trace.chunk_sizes = asr.chunk_sizes;
  trace.fed_samples = asr.fed_samples;
  return trace;
}

}  // namespace

int main() {
  int failures = 0;
#define CHECK(condition, message)         \
  do {                                    \
    if (!(condition)) {                   \
      std::printf("FAIL: %s\n", message); \
      ++failures;                         \
    }                                     \
  } while (0)

  FakeAsr asr;
  orator::pipeline::AsrWorker::Params params;
  params.sample_rate = 100;
  params.segment_sec = 0.0;
  params.asr_vad_gate = false;

  std::vector<std::string> events;
  std::vector<long> final_sink_ids;
  orator::pipeline::AsrWorker worker(
      &asr, params,
      [&events](const std::string& event) { events.push_back(event); },
      orator::core::TimeBase(100));
  worker.set_text_sink([&final_sink_ids](long id, double, double,
                                         const std::string&, bool final) {
    if (final) final_sink_ids.push_back(id);
  });

  const std::vector<float> audio(10, 0.25f);
  worker.ProcessSpan(audio.data(), static_cast<int>(audio.size()));
  worker.Finalize();
  worker.ProcessSpan(audio.data(), static_cast<int>(audio.size()));
  worker.Finalize();

  CHECK(final_sink_ids.size() == 2, "two final sink records emitted");
  if (final_sink_ids.size() == 2) {
    CHECK(final_sink_ids[0] == 0, "first final sink ID is zero");
    CHECK(final_sink_ids[1] == 1, "second final sink ID is one");
  }

  bool final_zero = false;
  bool final_one = false;
  for (const auto& event : events) {
    if (event.find("\"type\":\"asr\"") == std::string::npos) continue;
    if (event.find("\"text_id\":0") != std::string::npos) final_zero = true;
    if (event.find("\"text_id\":1") != std::string::npos) final_one = true;
  }
  CHECK(final_zero, "first final event reuses sink ID zero");
  CHECK(final_one, "second final event reuses sink ID one");

  {
    FakeAsr silent_asr;
    orator::pipeline::ComprehensiveTimeline evidence;
    evidence.AdvanceVadHorizon(1.0);
    params.asr_vad_gate = true;
    std::vector<std::string> silent_events;
    int silent_finals = 0;
    orator::pipeline::AsrWorker silent_worker(
        &silent_asr, params,
        [&silent_events](const std::string& event) {
          silent_events.push_back(event);
        },
        orator::core::TimeBase(100), /*stream=*/0, &evidence);
    silent_worker.set_text_sink(
        [&silent_finals](long, double, double, const std::string&, bool final) {
          if (final) ++silent_finals;
        });
    silent_worker.ProcessSpan(audio.data(), static_cast<int>(audio.size()));
    silent_worker.Finalize();

    CHECK(silent_worker.processed_samples() == static_cast<long>(audio.size()),
          "ASR accounts for silence skipped from typed VAD evidence");
    CHECK(silent_events.empty(),
          "confirmed silence from typed VAD evidence emits no ASR event");
    CHECK(silent_finals == 0,
          "confirmed silence from typed VAD evidence deposits no ASR final");
  }

  {
    const GateTrace early = RunPublicationSchedule(0);
    const GateTrace late = RunPublicationSchedule(1);
    const GateTrace active = RunPublicationSchedule(2);

    CHECK(late.events_before_evidence == 0,
          "unclassified audio remains buffered without provisional text");
    CHECK(early.reset_positions == late.reset_positions &&
              early.reset_positions == active.reset_positions,
          "VAD publication order preserves ASR reset positions");
    CHECK(early.chunk_sizes == late.chunk_sizes &&
              early.chunk_sizes == active.chunk_sizes,
          "VAD publication order preserves fixed decoder chunks");
    CHECK(early.fed_samples == late.fed_samples &&
              early.fed_samples == active.fed_samples,
          "VAD publication order preserves the exact decoder input samples");
    CHECK(early.events == late.events && early.events == active.events,
          "VAD publication order preserves live and final ASR events");
    CHECK(early.finals == late.finals && early.finals == active.finals,
          "VAD publication order preserves typed ASR finals");
    CHECK(early.reset_positions == std::vector<long>{10},
          "ASR reset starts at VAD onset minus TOML lead");
    CHECK(early.chunk_sizes == std::vector<int>({10, 10, 10, 10}),
          "decided speech uses the TOML fixed feed quantum");
    CHECK(early.fed_samples.size() == 40 &&
              early.fed_samples.front() == 10.0f &&
              early.fed_samples.back() == 49.0f,
          "only lead plus finalized VAD speech reaches the decoder");
    CHECK(early.finals.size() == 1 && early.finals[0].start == 0.1 &&
              early.finals[0].end == 0.5,
          "typed final uses deterministic lead and endpoint time codes");
  }

  {
    FakeAsr silence_asr;
    orator::pipeline::ComprehensiveTimeline silence_evidence;
    silence_evidence.UpdateVadState(false, 1.0);
    silence_evidence.AdvanceVadHorizon(1.0);
    params.sample_rate = 100;
    params.asr_vad_gate = true;
    params.asr_vad_lead_ms = 100;
    params.asr_vad_gate_chunk_ms = 100;
    std::vector<std::string> silence_events;
    std::vector<FinalRecord> silence_finals;
    orator::pipeline::AsrWorker silence_worker(
        &silence_asr, params,
        [&silence_events](const std::string& event) {
          silence_events.push_back(event);
        },
        orator::core::TimeBase(100), /*stream=*/0, &silence_evidence);
    silence_worker.set_text_sink(
        [&silence_finals](long id, double start, double end,
                          const std::string& text, bool final) {
          if (final) silence_finals.push_back({id, start, end, text});
        });
    std::vector<float> silence(100, 0.0f);
    silence_worker.ProcessSpan(silence.data(),
                               static_cast<int>(silence.size()));
    silence_worker.Finalize();
    CHECK(silence_asr.reset_positions.empty() &&
              silence_asr.fed_samples.empty() && silence_events.empty() &&
              silence_finals.empty(),
          "confirmed silence never reaches ASR or emits a transcript");
  }

  {
    params.sample_rate = 100;
    params.segment_sec = 0.0;
    params.asr_vad_gate = true;
    params.asr_vad_lead_ms = 100;
    params.asr_vad_gate_chunk_ms = 100;
    params.asr_vad_trail_sec = 0.1;
    params.asr_vad_min_overlap_sec = 0.01;
    std::vector<float> sequence(100);
    std::iota(sequence.begin(), sequence.end(), 0.0f);

    FakeAsr short_gap_asr;
    orator::pipeline::ComprehensiveTimeline short_gap_evidence;
    short_gap_evidence.DepositVad({0.2, 0.4});
    short_gap_evidence.DepositVad({0.45, 0.6});
    short_gap_evidence.AdvanceVadHorizon(1.0);
    std::vector<FinalRecord> short_gap_finals;
    orator::pipeline::AsrWorker short_gap_worker(
        &short_gap_asr, params, [](const std::string&) {},
        orator::core::TimeBase(100), /*stream=*/0, &short_gap_evidence);
    short_gap_worker.set_text_sink(
        [&short_gap_finals](long id, double start, double end,
                            const std::string& text, bool final) {
          if (final) short_gap_finals.push_back({id, start, end, text});
        });
    short_gap_worker.ProcessSpan(sequence.data(), 100);
    short_gap_worker.Finalize();
    CHECK(
        short_gap_asr.reset_positions == std::vector<long>{10} &&
            short_gap_finals.size() == 1,
        "speech returning within the trailing interval keeps one ASR session");

    FakeAsr long_gap_asr;
    orator::pipeline::ComprehensiveTimeline long_gap_evidence;
    long_gap_evidence.DepositVad({0.2, 0.4});
    long_gap_evidence.DepositVad({0.55, 0.7});
    long_gap_evidence.AdvanceVadHorizon(1.0);
    std::vector<FinalRecord> long_gap_finals;
    orator::pipeline::AsrWorker long_gap_worker(
        &long_gap_asr, params, [](const std::string&) {},
        orator::core::TimeBase(100), /*stream=*/0, &long_gap_evidence);
    long_gap_worker.set_text_sink(
        [&long_gap_finals](long id, double start, double end,
                           const std::string& text, bool final) {
          if (final) long_gap_finals.push_back({id, start, end, text});
        });
    long_gap_worker.ProcessSpan(sequence.data(), 100);
    long_gap_worker.Finalize();
    CHECK(
        long_gap_asr.reset_positions == std::vector<long>({10, 45}) &&
            long_gap_finals.size() == 2,
        "speech after the trailing interval starts a new lead-backed session");
  }

  if (failures == 0) {
    std::printf("test_asr_worker PASSED\n");
    return 0;
  }
  std::printf("test_asr_worker FAILED (%d checks)\n", failures);
  return 1;
}
