#include <cstdio>
#include <string>
#include <vector>

#include "protocol/schema.h"
#include "protocol/topic.h"

using orator::protocol::Field;
using orator::protocol::FieldType;
using orator::protocol::Schema;
using orator::protocol::SchemaRegistry;
using orator::protocol::Topic;
using orator::protocol::TopicPattern;
using orator::protocol::TopicSchema;

static int g_fail = 0;

#define CHECK(cond, msg)              \
  do {                                \
    if (!(cond)) {                    \
      std::printf("FAIL: %s\n", msg); \
      ++g_fail;                       \
    }                                 \
  } while (0)

// ---------------------------------------------------------------------------
// Topic parsing
// ---------------------------------------------------------------------------

static void test_topic_single_level() {
  Topic t{"audio"};
  CHECK(t.level_count() == 1, "single level count");
  CHECK(t.levels()[0] == "audio", "single level value");
  CHECK(t.to_string() == "audio", "single level to_string");
}

static void test_topic_multi_level() {
  Topic t{"system/pipeline/online"};
  CHECK(t.level_count() == 3, "multi level count");
  CHECK(t.levels()[0] == "system", "level 0");
  CHECK(t.levels()[1] == "pipeline", "level 1");
  CHECK(t.levels()[2] == "online", "level 2");
  CHECK(t.to_string() == "system/pipeline/online", "multi level to_string");
}

static void test_topic_empty() {
  Topic t{""};
  CHECK(t.level_count() == 0, "empty topic has zero levels");
  CHECK(t.to_string().empty(), "empty topic to_string is empty");
}

// ---------------------------------------------------------------------------
// Topic comparison
// ---------------------------------------------------------------------------

static void test_topic_equality() {
  Topic a{"audio/raw"};
  Topic b{"audio/raw"};
  Topic c{"audio/pcm"};
  CHECK(a == b, "same string topics are equal");
  CHECK(!(a == c), "different topics are not equal");
}

static void test_topic_ordering() {
  Topic a{"audio/pcm"};
  Topic b{"audio/raw"};
  Topic c{"system/pipeline"};
  CHECK(a < b, "audio/pcm < audio/raw");
  CHECK(b < c, "audio/raw < system/pipeline");
  CHECK(!(b < a), "audio/raw is not < audio/pcm");
}

// ---------------------------------------------------------------------------
// TopicPattern exact match
// ---------------------------------------------------------------------------

static void test_pattern_exact_match() {
  TopicPattern p{"audio/raw"};
  CHECK(p.Matches(Topic{"audio/raw"}), "exact match succeeds");
  CHECK(!p.Matches(Topic{"audio/pcm"}), "exact mismatch fails");
  CHECK(!p.Matches(Topic{"audio/raw/extra"}), "extra level fails");
}

// ---------------------------------------------------------------------------
// TopicPattern '+' wildcard
// ---------------------------------------------------------------------------

static void test_pattern_plus_wildcard() {
  TopicPattern p{"audio/+"};
  CHECK(p.Matches(Topic{"audio/raw"}), "+ matches raw");
  CHECK(p.Matches(Topic{"audio/pcm"}), "+ matches pcm");
  CHECK(!p.Matches(Topic{"audio"}), "+ requires exactly one level");
  CHECK(!p.Matches(Topic{"audio/raw/extra"}), "+ does not match extra levels");
}

static void test_pattern_plus_middle() {
  TopicPattern p{"system/+/online"};
  CHECK(p.Matches(Topic{"system/pipeline/online"}), "+ in middle matches");
  CHECK(!p.Matches(Topic{"system/pipeline/offline"}),
        "+ does not change last level");
}

// ---------------------------------------------------------------------------
// TopicPattern '#' wildcard
// ---------------------------------------------------------------------------

static void test_pattern_hash_wildcard() {
  TopicPattern p{"system/#"};
  CHECK(p.Matches(Topic{"system"}), "# matches zero levels");
  CHECK(p.Matches(Topic{"system/pipeline"}), "# matches one level");
  CHECK(p.Matches(Topic{"system/pipeline/online"}), "# matches two levels");
  CHECK(p.Matches(Topic{"system/a/b/c/d"}), "# matches many levels");
  CHECK(!p.Matches(Topic{"audio/raw"}), "# does not match wrong prefix");
}

// ---------------------------------------------------------------------------
// TopicPattern non-match cases
// ---------------------------------------------------------------------------

static void test_pattern_non_matches() {
  TopicPattern p{"a/b/c"};
  CHECK(!p.Matches(Topic{"a/b"}), "too few levels");
  CHECK(!p.Matches(Topic{"a/b/c/d"}), "too many levels");
  CHECK(!p.Matches(Topic{"x/y/z"}), "completely different");

  TopicPattern p2{""};
  CHECK(p2.Matches(Topic{""}), "empty pattern matches empty topic");
  CHECK(!p2.Matches(Topic{"a"}), "empty pattern does not match non-empty");
}

// ---------------------------------------------------------------------------
// Standard topic constants
// ---------------------------------------------------------------------------

static void test_standard_topics() {
  CHECK(orator::protocol::kAudioRaw.to_string() == "audio/raw", "kAudioRaw");
  CHECK(orator::protocol::kVadSpeechSegment.to_string() == "vad/speech_segment",
        "kVadSpeechSegment");
  CHECK(orator::protocol::kAsrTranscript.to_string() == "asr/transcript",
        "kAsrTranscript");
  CHECK(orator::protocol::kAsrTranscriptPartial.to_string() ==
            "asr/transcript_partial",
        "kAsrTranscriptPartial");
  CHECK(orator::protocol::kDiarSpeakerSegment.to_string() ==
            "diar/speaker_segment",
        "kDiarSpeakerSegment");
  CHECK(orator::protocol::kSystemPipelineOnline.to_string() ==
            "system/pipeline/online",
        "kSystemPipelineOnline");
  CHECK(orator::protocol::kSystemPipelineOffline.to_string() ==
            "system/pipeline/offline",
        "kSystemPipelineOffline");
  CHECK(orator::protocol::kSystemGpuTelemetry.to_string() ==
            "system/gpu_telemetry",
        "kSystemGpuTelemetry");
}

// ---------------------------------------------------------------------------
// SchemaRegistry register + get (latest version)
// ---------------------------------------------------------------------------

static void test_registry_register_get_latest() {
  SchemaRegistry reg;
  TopicSchema ts;
  ts.topic = Topic{"audio/raw"};
  ts.version = 1;
  ts.schema.fields.push_back(Field{"data", FieldType::BYTES, false, {}});
  reg.Register(std::move(ts));

  TopicSchema const* got = reg.Get(Topic{"audio/raw"});
  CHECK(got != nullptr, "get returns non-null for registered topic");
  CHECK(got->version == 1, "latest version is 1");
  CHECK(got->schema.fields.size() == 1, "schema has one field");
  CHECK(got->schema.fields[0].name == "data", "field name is data");
}

// ---------------------------------------------------------------------------
// SchemaRegistry register + get (specific version)
// ---------------------------------------------------------------------------

static void test_registry_get_specific_version() {
  SchemaRegistry reg;

  // Register v1
  TopicSchema ts1;
  ts1.topic = Topic{"audio/raw"};
  ts1.version = 1;
  ts1.schema.fields.push_back(Field{"data", FieldType::BYTES, false, {}});
  reg.Register(std::move(ts1));

  // Register v2
  TopicSchema ts2;
  ts2.topic = Topic{"audio/raw"};
  ts2.version = 2;
  ts2.schema.fields.push_back(Field{"data", FieldType::BYTES, false, {}});
  ts2.schema.fields.push_back(Field{"timestamp", FieldType::INT64, false, {}});
  reg.Register(std::move(ts2));

  TopicSchema const* v1 = reg.Get(Topic{"audio/raw"}, 1);
  CHECK(v1 != nullptr, "v1 exists");
  CHECK(v1->version == 1, "v1 version correct");
  CHECK(v1->schema.fields.size() == 1, "v1 has one field");

  TopicSchema const* v2 = reg.Get(Topic{"audio/raw"}, 2);
  CHECK(v2 != nullptr, "v2 exists");
  CHECK(v2->version == 2, "v2 version correct");
  CHECK(v2->schema.fields.size() == 2, "v2 has two fields");

  // Latest (version=0) returns v2
  TopicSchema const* latest = reg.Get(Topic{"audio/raw"}, 0);
  CHECK(latest != nullptr, "latest exists");
  CHECK(latest->version == 2, "latest is v2");
}

// ---------------------------------------------------------------------------
// SchemaRegistry version tracking (two versions for same topic)
// ---------------------------------------------------------------------------

static void test_registry_version_tracking() {
  SchemaRegistry reg;

  TopicSchema ts1;
  ts1.topic = Topic{"asr/transcript"};
  ts1.version = 1;
  ts1.schema.fields.push_back(Field{"text", FieldType::STRING, false, {}});
  reg.Register(std::move(ts1));

  TopicSchema ts2;
  ts2.topic = Topic{"asr/transcript"};
  ts2.version = 2;
  ts2.schema.fields.push_back(Field{"text", FieldType::STRING, false, {}});
  ts2.schema.fields.push_back(Field{"confidence", FieldType::FLOAT, true, {}});
  reg.Register(std::move(ts2));

  // Both versions accessible
  CHECK(reg.Get(Topic{"asr/transcript"}, 1) != nullptr, "v1 accessible");
  CHECK(reg.Get(Topic{"asr/transcript"}, 2) != nullptr, "v2 accessible");
  CHECK(reg.Get(Topic{"asr/transcript"}, 3) == nullptr, "v3 does not exist");
}

// ---------------------------------------------------------------------------
// GetAll returns all registered schemas
// ---------------------------------------------------------------------------

static void test_registry_get_all() {
  SchemaRegistry reg;

  TopicSchema ts1;
  ts1.topic = Topic{"audio/raw"};
  ts1.version = 1;
  reg.Register(std::move(ts1));

  TopicSchema ts2;
  ts2.topic = Topic{"audio/raw"};
  ts2.version = 2;
  reg.Register(std::move(ts2));

  TopicSchema ts3;
  ts3.topic = Topic{"system/pipeline/online"};
  ts3.version = 1;
  reg.Register(std::move(ts3));

  auto all = reg.GetAll();
  CHECK(all.size() == 3, "GetAll returns 3 schemas");
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main() {
  std::printf("Testing protocol types (Spec 004 Phase 7)...\n");

  test_topic_single_level();
  test_topic_multi_level();
  test_topic_empty();
  test_topic_equality();
  test_topic_ordering();
  test_pattern_exact_match();
  test_pattern_plus_wildcard();
  test_pattern_plus_middle();
  test_pattern_hash_wildcard();
  test_pattern_non_matches();
  test_standard_topics();
  test_registry_register_get_latest();
  test_registry_get_specific_version();
  test_registry_version_tracking();
  test_registry_get_all();

  if (g_fail == 0) {
    std::printf("PASS\n");
    return 0;
  }
  std::printf("FAIL (%d checks)\n", g_fail);
  return 1;
}
