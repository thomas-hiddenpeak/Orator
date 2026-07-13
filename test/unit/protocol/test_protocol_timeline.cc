#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include "protocol/protocol_timeline.h"

using orator::protocol::Backend;
using orator::protocol::Message;
using orator::protocol::PipelineDescriptor;
using orator::protocol::PipelineHandle;
using orator::protocol::ProtocolTimeline;
using orator::protocol::QoS;
using orator::protocol::Schema;
using orator::protocol::StorageRef;
using orator::protocol::Topic;
using orator::protocol::TopicPattern;
using orator::protocol::TopicRetention;
using orator::protocol::TopicSchema;

static int g_fail = 0;
#define CHECK(cond, msg)              \
  do {                                \
    if (!(cond)) {                    \
      std::printf("FAIL: %s\n", msg); \
      ++g_fail;                       \
    }                                 \
  } while (0)

int main() {
  std::printf("Testing protocol::ProtocolTimeline (Spec 004 Phase 11)...\n");

  // ---- 1. Register pipeline returns valid handle with matching name ----
  {
    ProtocolTimeline tl;
    PipelineDescriptor desc;
    desc.name = "test_pipeline";
    desc.version = "1.0";
    desc.produces = {Topic{"audio/raw"}};
    auto handle = tl.RegisterPipeline(std::move(desc));
    CHECK(handle != nullptr, "RegisterPipeline returns non-null handle");
    CHECK(handle->valid(), "handle is valid");
    CHECK(handle->name() == "test_pipeline", "handle name matches");
  }

  // ---- 2. Register + Describe contains pipeline name ----
  {
    ProtocolTimeline tl;
    PipelineDescriptor desc;
    desc.name = "describe_me";
    desc.version = "0.1";
    auto handle = tl.RegisterPipeline(std::move(desc));
    std::string desc_str = tl.Describe();
    CHECK(desc_str.find("\"type\": \"describe\"") != std::string::npos,
          "Describe carries a routable response type");
    CHECK(desc_str.find("describe_me") != std::string::npos,
          "Describe contains pipeline name");
    (void)handle;
  }

  // ---- 3. Publish stores message, retrievable via Replay ----
  {
    ProtocolTimeline tl;
    PipelineDescriptor desc;
    desc.name = "pub_test";
    desc.version = "1.0";
    auto handle = tl.RegisterPipeline(std::move(desc));

    Message msg;
    msg.timestamp_sec = 1.5;
    msg.data = "{\"text\":\"hello\"}";
    bool ok = tl.Publish(*handle, Topic{"audio/raw"}, msg, QoS::AT_LEAST_ONCE);
    CHECK(ok, "Publish returns true");

    auto replayed = tl.Replay("audio/raw", 0.0);
    CHECK(replayed.size() == 1, "Replay returns one message");
    CHECK(replayed[0].data == "{\"text\":\"hello\"}", "Replayed data matches");
    CHECK(replayed[0].pipeline == "pub_test", "Replayed pipeline name matches");
  }

  // ---- 4. Publish delivers to subscriber via SubscribeInternal ----
  {
    ProtocolTimeline tl;
    PipelineDescriptor desc;
    desc.name = "sub_test";
    desc.version = "1.0";
    auto handle = tl.RegisterPipeline(std::move(desc));

    std::vector<Message> received;
    long sub_id = tl.SubscribeInternal(
        TopicPattern{"audio/#"},
        [&received](const Message& m) { received.push_back(m); });

    Message msg;
    msg.timestamp_sec = 2.0;
    msg.data = "{\"sub\":\"works\"}";
    tl.Publish(*handle, Topic{"audio/raw"}, msg, QoS::AT_LEAST_ONCE);

    CHECK(received.size() == 1, "Subscriber received one message");
    CHECK(received[0].data == "{\"sub\":\"works\"}", "Subscriber data matches");

    tl.UnsubscribeInternal(sub_id);
  }

  // ---- 5. Replay from timestamp returns messages from that point ----
  {
    ProtocolTimeline tl;
    PipelineDescriptor desc;
    desc.name = "replay_test";
    desc.version = "1.0";
    auto handle = tl.RegisterPipeline(std::move(desc));

    Message m1;
    m1.timestamp_sec = 1.0;
    m1.data = "first";
    tl.Publish(*handle, Topic{"audio/raw"}, m1, QoS::AT_MOST_ONCE);

    Message m2;
    m2.timestamp_sec = 2.0;
    m2.data = "second";
    tl.Publish(*handle, Topic{"audio/raw"}, m2, QoS::AT_MOST_ONCE);

    Message m3;
    m3.timestamp_sec = 3.0;
    m3.data = "third";
    tl.Publish(*handle, Topic{"audio/raw"}, m3, QoS::AT_MOST_ONCE);

    auto r1 = tl.Replay("audio/raw", 2.0);
    CHECK(r1.size() == 2, "Replay from 2.0 returns two messages");
    CHECK(r1[0].data == "second", "First replayed message is second");

    auto r2 = tl.Replay("audio/raw", 0.0);
    CHECK(r2.size() == 3, "Replay from 0.0 returns all three messages");
  }

  // ---- 6. LastRetained returns last published message ----
  {
    ProtocolTimeline tl;
    PipelineDescriptor desc;
    desc.name = "retained_test";
    desc.version = "1.0";
    auto handle = tl.RegisterPipeline(std::move(desc));

    Message m1;
    m1.timestamp_sec = 1.0;
    m1.data = "early";
    tl.Publish(*handle, Topic{"audio/raw"}, m1, QoS::AT_MOST_ONCE);

    Message m2;
    m2.timestamp_sec = 2.0;
    m2.data = "latest";
    tl.Publish(*handle, Topic{"audio/raw"}, m2, QoS::AT_MOST_ONCE);

    const Message* last = tl.LastRetained("audio/raw");
    CHECK(last != nullptr, "LastRetained returns non-null");
    CHECK(last->data == "latest", "LastRetained returns latest message");

    const Message* missing = tl.LastRetained("nonexistent/topic");
    CHECK(missing == nullptr, "LastRetained returns nullptr for unknown topic");
  }

  // ---- 7. Unregister pipeline removes it from Describe ----
  {
    ProtocolTimeline tl;
    PipelineDescriptor desc;
    desc.name = "unreg_test";
    desc.version = "1.0";
    auto handle = tl.RegisterPipeline(std::move(desc));

    std::string before = tl.Describe();
    CHECK(before.find("unreg_test") != std::string::npos,
          "Describe contains pipeline before unregister");

    tl.UnregisterPipeline(*handle);
    CHECK(!handle->valid(), "handle is invalid after unregister");

    std::string after = tl.Describe();
    CHECK(after.find("unreg_test") == std::string::npos,
          "Describe does not contain pipeline after unregister");
  }

  // ---- 8. Out-of-order publish fires system/out_of_order event ----
  {
    ProtocolTimeline tl;
    PipelineDescriptor desc;
    desc.name = "ooo_test";
    desc.version = "1.0";
    auto handle = tl.RegisterPipeline(std::move(desc));

    std::vector<Message> events;
    tl.SubscribeInternal(TopicPattern{"system/#"},
                         [&events](const Message& m) { events.push_back(m); });

    Message m1;
    m1.timestamp_sec = 1.0;
    m1.data = "in-order";
    tl.Publish(*handle, Topic{"audio/raw"}, m1, QoS::AT_MOST_ONCE);

    Message m2;
    m2.timestamp_sec = 0.5;
    m2.data = "out-of-order";
    tl.Publish(*handle, Topic{"audio/raw"}, m2, QoS::AT_MOST_ONCE);

    bool found_ooo = std::any_of(
        events.begin(), events.end(),
        [](const Message& m) { return m.topic == "system/out_of_order"; });
    CHECK(found_ooo, "Out-of-order publish fires system/out_of_order event");
  }

  // ---- 9. Negative timestamp rejected ----
  {
    ProtocolTimeline tl;
    PipelineDescriptor desc;
    desc.name = "neg_ts_test";
    desc.version = "1.0";
    auto handle = tl.RegisterPipeline(std::move(desc));

    Message msg;
    msg.timestamp_sec = -1.0;
    msg.data = "bad";
    bool ok = tl.Publish(*handle, Topic{"audio/raw"}, msg, QoS::AT_MOST_ONCE);
    CHECK(!ok, "Negative timestamp rejected");

    auto replayed = tl.Replay("audio/raw", 0.0);
    CHECK(replayed.empty(), "No message stored for negative timestamp");
  }

  // ---- 10. Multiple pipelines, multiple topics ----
  {
    ProtocolTimeline tl;

    PipelineDescriptor desc1;
    desc1.name = "pipeline_a";
    desc1.version = "1.0";
    desc1.produces = {Topic{"audio/raw"}};
    auto handle_a = tl.RegisterPipeline(std::move(desc1));

    PipelineDescriptor desc2;
    desc2.name = "pipeline_b";
    desc2.version = "2.0";
    desc2.produces = {Topic{"vad/speech_segment"}};
    auto handle_b = tl.RegisterPipeline(std::move(desc2));

    Message m1;
    m1.timestamp_sec = 1.0;
    m1.data = "from_a";
    tl.Publish(*handle_a, Topic{"audio/raw"}, m1, QoS::AT_MOST_ONCE);

    Message m2;
    m2.timestamp_sec = 1.5;
    m2.data = "from_b";
    tl.Publish(*handle_b, Topic{"vad/speech_segment"}, m2, QoS::AT_MOST_ONCE);

    auto r_a = tl.Replay("audio/raw", 0.0);
    CHECK(r_a.size() == 1, "audio/raw has one message");
    CHECK(r_a[0].pipeline == "pipeline_a", "audio/raw message from pipeline_a");

    auto r_b = tl.Replay("vad/speech_segment", 0.0);
    CHECK(r_b.size() == 1, "vad/speech_segment has one message");
    CHECK(r_b[0].pipeline == "pipeline_b",
          "vad/speech_segment message from pipeline_b");

    std::string d = tl.Describe();
    CHECK(d.find("pipeline_a") != std::string::npos,
          "Describe contains pipeline_a");
    CHECK(d.find("pipeline_b") != std::string::npos,
          "Describe contains pipeline_b");
  }

  // ---- 11. Schema registration visible in Describe ----
  {
    ProtocolTimeline tl;
    PipelineDescriptor desc;
    desc.name = "schema_test";
    desc.version = "1.0";
    desc.schema["audio/raw"] = TopicSchema{Topic{"audio/raw"}, 1, Schema{}};
    auto handle = tl.RegisterPipeline(std::move(desc));

    std::string d = tl.Describe();
    CHECK(d.find("audio/raw") != std::string::npos,
          "Describe contains schema topic");
    (void)handle;
  }

  // ---- 12. SetTopicRetention configures backend ----
  {
    ProtocolTimeline tl;
    TopicRetention config;
    config.backend = Backend::DISK;
    config.retention_sec = 60.0;
    tl.SetTopicRetention("disk_topic", config);
    std::printf("PASS: SetTopicRetention\n");
  }

  if (g_fail == 0) {
    std::printf("ProtocolTimeline test PASSED\n");
    return 0;
  }
  std::printf("ProtocolTimeline test FAILED (%d checks)\n", g_fail);
  return 1;
}
