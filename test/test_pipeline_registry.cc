#include <cstdio>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "protocol/pipeline_registry.h"

using orator::protocol::PipelineDescriptor;
using orator::protocol::PipelineHandle;
using orator::protocol::PipelineRegistry;

static int g_fail = 0;
#define CHECK(cond, msg)                                 \
  do {                                                   \
    if (!(cond)) {                                       \
      std::printf("FAIL: %s\n", msg);                    \
      ++g_fail;                                          \
    } else {                                             \
      std::printf("PASS: %s\n", msg);                    \
    }                                                    \
  } while (0)

int main() {
  std::printf("Testing PipelineRegistry (Spec 004 Phase 8)...\n\n");

  // ---- 1. Register a pipeline: handle is valid, name matches ----
  {
    PipelineRegistry registry;
    PipelineDescriptor desc;
    desc.name = "asr";
    desc.version = "1.0.0";

    auto handle = registry.Register(std::move(desc));
    CHECK(handle->valid(), "registered handle is valid");
    CHECK(handle->name() == "asr", "handle name matches descriptor");
  }

  // ---- 2. Register duplicate name: throws ----
  {
    PipelineRegistry registry;
    PipelineDescriptor desc1;
    desc1.name = "vad";
    desc1.version = "1.0.0";
    auto handle1 = registry.Register(std::move(desc1));

    PipelineDescriptor desc2;
    desc2.name = "vad";
    desc2.version = "2.0.0";
    bool threw = false;
    try {
      registry.Register(std::move(desc2));
    } catch (const std::runtime_error&) {
      threw = true;
    }
    CHECK(threw, "duplicate name throws std::runtime_error");
  }

  // ---- 3. Describe() returns all registered pipelines ----
  {
    PipelineRegistry registry;
    PipelineDescriptor d1;
    d1.name = "asr";
    d1.version = "1.0.0";
    auto h1 = registry.Register(std::move(d1));

    PipelineDescriptor d2;
    d2.name = "diar";
    d2.version = "0.5.0";
    auto h2 = registry.Register(std::move(d2));

    auto descs = registry.Describe();
    CHECK(descs.size() == 2, "Describe returns 2 pipelines");

    // Verify both names are present.
    bool has_asr = false, has_diar = false;
    for (const auto& d : descs) {
      if (d.name == "asr") has_asr = true;
      if (d.name == "diar") has_diar = true;
    }
    CHECK(has_asr && has_diar, "Describe contains both asr and diar");
  }

  // ---- 4. Heartbeat updates timestamp ----
  {
    PipelineRegistry registry;
    PipelineDescriptor desc;
    desc.name = "heartbeat_test";
    desc.version = "1.0.0";
    auto handle = registry.Register(std::move(desc));

    // Before sleeping, heartbeat is recent.
    auto unhealthy_before = registry.HealthCheck(0.5);
    CHECK(unhealthy_before.empty(), "fresh pipeline is healthy before sleep");

    // Sleep briefly.
    std::this_thread::sleep_for(std::chrono::milliseconds(600));

    // Now it should be unhealthy (600ms > 500ms timeout).
    auto unhealthy_after = registry.HealthCheck(0.5);
    CHECK(!unhealthy_after.empty() && unhealthy_after[0] == "heartbeat_test",
          "pipeline is unhealthy after timeout");

    // Heartbeat should reset it.
    handle->Heartbeat();

    auto unhealthy_after_hb = registry.HealthCheck(0.5);
    CHECK(unhealthy_after_hb.empty(), "heartbeat restores health");
  }

  // ---- 5. Unregister removes pipeline, Describe() no longer includes it ----
  {
    PipelineRegistry registry;
    PipelineDescriptor desc;
    desc.name = "to_remove";
    desc.version = "1.0.0";
    auto handle = registry.Register(std::move(desc));

    CHECK(registry.Describe().size() == 1, "one pipeline before unregister");

    registry.Unregister(*handle);
    CHECK(!handle->valid(), "handle is invalid after unregister");
    CHECK(registry.Describe().empty(), "Describe is empty after unregister");
  }

  // ---- 6. RAII: handle destructor auto-unregisters ----
  {
    PipelineRegistry registry;
    {
      PipelineDescriptor desc;
      desc.name = "raii_pipeline";
      desc.version = "1.0.0";
      auto handle = registry.Register(std::move(desc));
      CHECK(registry.Describe().size() == 1, "pipeline exists inside scope");
      // handle goes out of scope here -> destructor calls Unregister
    }
    CHECK(registry.Describe().empty(), "pipeline gone after handle scope exit");
  }

  // ---- 7. Disabled pipeline (enabled=false) registers but is marked disabled ----
  {
    PipelineRegistry registry;
    PipelineDescriptor desc;
    desc.name = "disabled";
    desc.version = "1.0.0";
    desc.enabled = false;
    auto handle = registry.Register(std::move(desc));

    CHECK(handle->valid(), "disabled pipeline handle is valid");
    CHECK(handle->descriptor().enabled == false, "descriptor.enabled is false");

    auto descs = registry.Describe();
    CHECK(descs.size() == 1, "disabled pipeline appears in Describe");
    CHECK(descs[0].enabled == false, "Describe reflects enabled=false");
  }

  // ---- 8. Move semantics: PipelineHandle move constructor works ----
  {
    PipelineRegistry registry;
    PipelineDescriptor desc;
    desc.name = "movable";
    desc.version = "1.0.0";
    auto handle1 = registry.Register(std::move(desc));

    PipelineHandle handle2(std::move(*handle1));
    CHECK(!handle1->valid(), "source handle is invalid after move");
    CHECK(handle2.valid(), "moved handle is valid");
    CHECK(handle2.name() == "movable", "moved handle preserves name");

    // Verify the pipeline is still registered.
    CHECK(registry.Describe().size() == 1, "pipeline still registered after move");
  }

  // ---- 9. System event callback fires on register/unregister ----
  {
    PipelineRegistry registry;
    std::vector<std::string> events;
    registry.OnSystemEvent([&events](const std::string& topic,
                                     const std::string& data) {
      events.push_back(topic + "=" + data);
    });

    PipelineDescriptor desc;
    desc.name = "event_test";
    desc.version = "3.0.0";
    {
      auto handle = registry.Register(std::move(desc));
      CHECK(events.size() == 1, "one event fired on register");
      CHECK(events[0].find("online") != std::string::npos,
            "event topic contains 'online'");
      CHECK(events[0].find("event_test") != std::string::npos,
            "event data contains pipeline name");

      registry.Unregister(*handle);
      CHECK(events.size() == 2, "two events total after unregister");
      CHECK(events[1].find("offline") != std::string::npos,
            "event topic contains 'offline'");
    }
  }

  // ---- 10. Empty name throws ----
  {
    PipelineRegistry registry;
    PipelineDescriptor desc;
    desc.name = "";
    desc.version = "1.0.0";
    bool threw = false;
    try {
      registry.Register(std::move(desc));
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    CHECK(threw, "empty name throws std::invalid_argument");
  }

  if (g_fail == 0) {
    std::printf("\nPipelineRegistry test PASSED\n");
    return 0;
  }
  std::printf("\nPipelineRegistry test FAILED (%d checks)\n", g_fail);
  return 1;
}
