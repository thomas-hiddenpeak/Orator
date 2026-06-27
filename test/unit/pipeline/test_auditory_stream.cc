#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include <sys/stat.h>

#include "core/registry.h"
#include "core/stages.h"
#include "core/types.h"
#include "pipeline/auditory_stream.h"

using orator::core::AsrConfig;
using orator::core::AudioChunk;
using orator::core::DiarizationConfig;
using orator::core::DiarizationFrames;
using orator::core::IAsr;
using orator::core::IDiarizer;
using orator::core::IVad;
using orator::core::Registry;
using orator::core::Transcript;
using orator::core::VadConfig;
using orator::core::VadSegmentResult;
using orator::pipeline::AuditoryStream;

static int g_fail = 0;
#define CHECK(cond, msg)              \
  do {                                \
    if (!(cond)) {                    \
      std::printf("FAIL: %s\n", msg); \
      ++g_fail;                       \
    } else {                          \
      std::printf("PASS: %s\n", msg); \
    }                                 \
  } while (0)

// ============================================================================
// Mock model implementations
//
// These implement core::IDiarizer, core::IAsr, and core::IVad with
// call-tracking counters and no-op behavior. Workers now call interface
// methods directly (no dynamic_cast). Mocks are usable through the full
// pipeline provided they implement all pure virtual methods on the interface.
//
// All test cases below use empty model paths (no workers created), which
// exercises the AuditoryStream controller shell without requiring GPU compute
// or concrete model implementations.
// ============================================================================

class TestDiarizer : public IDiarizer {
 public:
  int init_count = 0;
  int load_count = 0;
  int reset_count = 0;
  int process_count = 0;

  void Initialize(const DiarizationConfig& /*config*/) override {
    ++init_count;
  }
  void LoadWeights(const std::string& /*path*/) override { ++load_count; }
  void Reset() override { ++reset_count; }
  DiarizationFrames ProcessChunk(const AudioChunk& /*chunk*/) override {
    ++process_count;
    return DiarizationFrames{};
  }
  DiarizationFrames StreamAudio(const float* /*samples*/, int /*n*/,
                                bool /*final*/,
                                cudaStream_t /*stream*/) override {
    return DiarizationFrames{};
  }
  int max_speakers() const override { return 4; }
  double frame_period_sec() const override { return 0.032; }
  std::string name() const override { return "test_diarizer"; }
};

class TestAsr : public IAsr {
 public:
  int init_count = 0;
  int load_count = 0;
  int reset_count = 0;
  int transcribe_count = 0;

  void Initialize(const AsrConfig& /*config*/) override { ++init_count; }
  void LoadWeights(const std::string& /*path*/) override { ++load_count; }
  void Reset() override { ++reset_count; }
  Transcript Transcribe(const AudioChunk& /*audio*/) override {
    ++transcribe_count;
    return Transcript{};
  }
  void set_max_new_tokens(int /*max_tokens*/) override {}
  void StreamReset(long /*base_sample*/) override {}
  std::string StreamChunk(const float* /*pcm*/, int /*n*/,
                          cudaStream_t /*stream*/) override {
    return "";
  }
  std::string StreamFinalize(cudaStream_t /*stream*/) override { return ""; }
  int stream_audio_tokens() const override { return 0; }
  std::string name() const override { return "test_asr"; }
};

class TestVad : public IVad {
 public:
  int init_count = 0;
  int load_count = 0;
  int reset_count = 0;

  void Initialize(const VadConfig& /*config*/) override { ++init_count; }
  void LoadWeights(const std::string& /*path*/) override { ++load_count; }
  void Reset() override { ++reset_count; }
  void Push(const float* /*samples*/, int /*n*/) override {}
  void DrainSegments(bool /*finalize*/,
                     std::vector<VadSegmentResult>* /*segments*/) override {}
  bool is_in_speech() const override { return false; }
  double compute_sec() const override { return 0.0; }
  std::string name() const override { return "test_vad"; }
};

// ============================================================================
// Test helpers
// ============================================================================

// Returns a config with all pipelines disabled so tests exercise only the
// controller shell (no workers, no GPU compute, no model loading).
static AuditoryStream::Config MakeTestConfig() {
  AuditoryStream::Config cfg;
  cfg.diarizer_weights = "";  // disable diarization pipeline
  cfg.asr_model_dir = "";     // disable ASR pipeline
  cfg.vad_model = "";         // disable VAD model path
  cfg.vad_stream = false;     // disable VAD stream thread
  cfg.storage_disk_path = "/tmp/orator_test/";
  cfg.gpu_telemetry_interval_sec = 0.0;
  return cfg;
}

// ============================================================================
// Test cases
// ============================================================================

int main() {
  std::printf("Testing AuditoryStream (pipeline orchestration layer)...\n\n");

  // Register mock models in the registry under unique keys so they don't
  // interfere with built-in registrations (sortformer, qwen3_asr).
  Registry<IDiarizer>::Instance().Register(
      "test_diarizer", [] { return std::make_unique<TestDiarizer>(); });
  Registry<IAsr>::Instance().Register(
      "test_asr", [] { return std::make_unique<TestAsr>(); });
  Registry<IVad>::Instance().Register(
      "test_vad", [] { return std::make_unique<TestVad>(); });

  // Verify mock implementations satisfy their interface contracts.
  {
    std::printf("--- Mock contract verification ---\n");
    TestDiarizer diar;
    CHECK(diar.max_speakers() == 4, "TestDiarizer::max_speakers() == 4");
    CHECK(diar.frame_period_sec() == 0.032,
          "TestDiarizer::frame_period_sec() == 0.032");
    CHECK(diar.name() == "test_diarizer",
          "TestDiarizer::name() == \"test_diarizer\"");
    CHECK(diar.init_count == 0, "TestDiarizer init_count starts at 0");
    diar.Initialize(DiarizationConfig{});
    CHECK(diar.init_count == 1, "TestDiarizer Initialize increments count");
    diar.LoadWeights("");
    CHECK(diar.load_count == 1, "TestDiarizer LoadWeights increments count");
    diar.Reset();
    CHECK(diar.reset_count == 1, "TestDiarizer Reset increments count");
    auto frames = diar.ProcessChunk(AudioChunk{});
    CHECK(diar.process_count == 1,
          "TestDiarizer ProcessChunk increments count");
    CHECK(frames.probs.empty(),
          "TestDiarizer ProcessChunk returns empty frames");
    CHECK(frames.num_frames == 0, "TestDiarizer ProcessChunk num_frames == 0");

    TestAsr asr;
    CHECK(asr.name() == "test_asr", "TestAsr::name() == \"test_asr\"");
    CHECK(asr.init_count == 0, "TestAsr init_count starts at 0");
    asr.Initialize(AsrConfig{});
    CHECK(asr.init_count == 1, "TestAsr Initialize increments count");
    asr.LoadWeights("");
    CHECK(asr.load_count == 1, "TestAsr LoadWeights increments count");
    asr.Reset();
    CHECK(asr.reset_count == 1, "TestAsr Reset increments count");
    auto transcript = asr.Transcribe(AudioChunk{});
    CHECK(asr.transcribe_count == 1, "TestAsr Transcribe increments count");
    CHECK(transcript.tokens.empty(),
          "TestAsr Transcribe returns empty transcript");

    TestVad vad;
    CHECK(vad.name() == "test_vad", "TestVad::name() == \"test_vad\"");
    CHECK(vad.init_count == 0, "TestVad init_count starts at 0");
    vad.Initialize(VadConfig{});
    CHECK(vad.init_count == 1, "TestVad Initialize increments count");
    vad.LoadWeights("");
    CHECK(vad.load_count == 1, "TestVad LoadWeights increments count");
    vad.Reset();
    CHECK(vad.reset_count == 1, "TestVad Reset increments count");
    std::printf("\n");
  }

  // Create temp directory for the storage backend.
  mkdir("/tmp/orator_test/", 0755);

  // ------------------------------------------------------------------
  // Test 1: Lifecycle test
  //   Start -> PushAudio(small data) -> EmitTimeline(finalize=true)
  //   Verifies the basic controller lifecycle does not crash and produces
  //   a valid timeline JSON.
  // ------------------------------------------------------------------
  {
    std::printf("--- Test 1: Lifecycle ---\n");
    std::string last_emit;
    AuditoryStream stream(
        MakeTestConfig(),
        [&last_emit](const std::string& json) { last_emit = json; });
    stream.Start();
    // Push 10 ms of silence at 16 kHz.
    std::vector<float> silence(160, 0.0f);
    stream.PushAudio(silence.data(), static_cast<int>(silence.size()));
    stream.EmitTimeline(true);
    CHECK(!last_emit.empty(), "EmitTimeline produced output");
    CHECK(last_emit.find("\"type\":\"timeline\"") != std::string::npos,
          "output is a timeline JSON");
    CHECK(last_emit.find("\"tracks\"") != std::string::npos,
          "timeline contains tracks array");
    std::printf("\n");
  }

  // ------------------------------------------------------------------
  // Test 2: Reset test
  //   Start -> PushAudio -> Reset -> Start -> PushAudio -> EmitTimeline
  //   Verifies clean restart: Reset clears state and a new session can
  //   begin without issues.
  // ------------------------------------------------------------------
  {
    std::printf("--- Test 2: Reset ---\n");
    std::string last_emit;
    AuditoryStream stream(
        MakeTestConfig(),
        [&last_emit](const std::string& json) { last_emit = json; });
    stream.Start();
    std::vector<float> silence(160, 0.0f);
    stream.PushAudio(silence.data(), static_cast<int>(silence.size()));
    stream.Reset();  // Clear state, start new session.
    // After Reset, push new data and finalize.
    stream.PushAudio(silence.data(), static_cast<int>(silence.size()));
    stream.EmitTimeline(true);
    CHECK(!last_emit.empty(), "EmitTimeline after Reset produced output");
    CHECK(last_emit.find("\"type\":\"timeline\"") != std::string::npos,
          "output after Reset is a timeline JSON");
    std::printf("\n");
  }

  // ------------------------------------------------------------------
  // Test 3: Empty push test
  //   Start -> PushAudio(zero samples) -> EmitTimeline
  //   Verifies that pushing zero-length audio does not cause issues.
  // ------------------------------------------------------------------
  {
    std::printf("--- Test 3: Empty push ---\n");
    std::string last_emit;
    AuditoryStream stream(
        MakeTestConfig(),
        [&last_emit](const std::string& json) { last_emit = json; });
    stream.Start();
    stream.PushAudio(nullptr, 0);  // zero samples, no-op
    stream.EmitTimeline(true);
    CHECK(!last_emit.empty(), "EmitTimeline after empty push produced output");
    std::printf("\n");
  }

  // ------------------------------------------------------------------
  // Test 4: Double finalize test
  //   Start -> PushAudio -> EmitTimeline(true) -> EmitTimeline(true)
  //   Verifies that calling EmitTimeline(finalize=true) twice is safe
  //   (idempotent after the first finalize stops all workers).
  // ------------------------------------------------------------------
  {
    std::printf("--- Test 4: Double finalize ---\n");
    std::string last_emit;
    AuditoryStream stream(
        MakeTestConfig(),
        [&last_emit](const std::string& json) { last_emit = json; });
    stream.Start();
    std::vector<float> silence(160, 0.0f);
    stream.PushAudio(silence.data(), static_cast<int>(silence.size()));
    stream.EmitTimeline(true);  // first finalize
    CHECK(!last_emit.empty(), "first EmitTimeline produced output");
    std::string first = last_emit;
    last_emit.clear();
    stream.EmitTimeline(true);  // second finalize (should be safe, no crash)
    CHECK(!last_emit.empty(), "second EmitTimeline produced output");
    std::printf("\n");
  }

  // ------------------------------------------------------------------
  // Test 5: Edge case — all pipelines disabled
  //   Config with empty diarizer_weights, empty asr_model_dir, and
  //   vad_stream=false. Verifies the controller handles the degenerate
  //   case where no pipeline workers are created.
  // ------------------------------------------------------------------
  {
    std::printf("--- Test 5: All pipelines disabled ---\n");
    std::string last_emit;
    AuditoryStream stream(
        MakeTestConfig(),
        [&last_emit](const std::string& json) { last_emit = json; });
    stream.Start();
    std::vector<float> silence(160, 0.0f);
    stream.PushAudio(silence.data(), static_cast<int>(silence.size()));
    stream.EmitTimeline(true);
    CHECK(!last_emit.empty(),
          "EmitTimeline with all pipelines disabled produced output");
    CHECK(last_emit.find("\"tracks\"") != std::string::npos,
          "timeline contains tracks array (empty)");
    std::printf("\n");
  }

  // Cleanup temp files created by the storage backend.
  std::remove("/tmp/orator_test/session_0.000000.dat");
  std::remove("/tmp/orator_test/");

  if (g_fail == 0) {
    std::printf("AuditoryStream test PASSED\n");
    return 0;
  }
  std::printf("AuditoryStream test FAILED (%d checks)\n", g_fail);
  return 1;
}
