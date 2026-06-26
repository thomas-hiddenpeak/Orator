#include <cstdio>
#include <string>
#include <vector>

#include "protocol/topic_router.h"

using orator::protocol::QoS;
using orator::protocol::Topic;
using orator::protocol::TopicPattern;
using orator::protocol::TopicRouter;

static int g_fail = 0;
#define CHECK(cond, msg)                \
  do {                                  \
    if (!(cond)) {                      \
      std::printf("FAIL: %s\n", msg);   \
      ++g_fail;                         \
    }                                   \
  } while (0)

int main() {
  std::printf("Testing protocol::TopicRouter (Spec 004 Phase 9)...\n");

  // ---- 1. Subscribe creates subscription, returns valid id ----
  {
    TopicRouter router;
    TopicPattern p{"audio/raw"};
    long id = router.Subscribe(p, "pipeline_a", QoS::AT_LEAST_ONCE);
    CHECK(id > 0, "Subscribe returns positive id");

    auto subs = router.GetAllSubscriptions();
    CHECK(subs.size() == 1, "GetAllSubscriptions returns one entry");
    CHECK(subs[0].sub_id == id, "subscription id matches");
    CHECK(subs[0].pipeline_name == "pipeline_a", "pipeline name recorded");
    CHECK(subs[0].requested_qos == QoS::AT_LEAST_ONCE, "QoS recorded");
  }

  // ---- 2. Route with exact topic match delivers to subscriber ----
  {
    TopicRouter router;
    router.Subscribe(TopicPattern{"audio/raw"}, "pipeline_a");
    auto deliveries = router.Route(Topic{"audio/raw"}, "publisher_x",
                                   QoS::AT_MOST_ONCE);
    CHECK(deliveries.size() == 1, "exact match produces one delivery");
    CHECK(deliveries[0].pipeline_name == "pipeline_a",
          "delivery targets correct pipeline");
  }

  // ---- 3. Route with '+' wildcard ----
  {
    TopicRouter router;
    router.Subscribe(TopicPattern{"vad/+"}, "vad_listener");
    auto deliveries = router.Route(Topic{"vad/speech_segment"}, "vad_pipeline",
                                   QoS::AT_MOST_ONCE);
    CHECK(deliveries.size() == 1, "'+' wildcard matches one level");
    CHECK(deliveries[0].pipeline_name == "vad_listener", "correct subscriber");
  }

  // ---- 4. Route with '#' wildcard ----
  {
    TopicRouter router;
    router.Subscribe(TopicPattern{"system/#"}, "monitor");

    // Two-level topic: system/pipeline/online
    auto d1 = router.Route(Topic{"system/pipeline/online"}, "sys",
                            QoS::AT_MOST_ONCE);
    CHECK(d1.size() == 1, "'#' matches system/pipeline/online");

    // Another two-level topic: system/pipeline/offline
    auto d2 = router.Route(Topic{"system/pipeline/offline"}, "sys",
                            QoS::AT_MOST_ONCE);
    CHECK(d2.size() == 1, "'#' matches system/pipeline/offline");

    // Single-level topic: system (zero remaining levels)
    auto d3 = router.Route(Topic{"system"}, "sys", QoS::AT_MOST_ONCE);
    CHECK(d3.size() == 1, "'#' matches zero remaining levels");
  }

  // ---- 5. Route with no match produces no delivery ----
  {
    TopicRouter router;
    router.Subscribe(TopicPattern{"audio/raw"}, "pipeline_a");
    auto deliveries = router.Route(Topic{"vad/speech_segment"}, "vad",
                                   QoS::AT_MOST_ONCE);
    CHECK(deliveries.empty(), "non-matching topic produces no delivery");
  }

  // ---- 6. no_local=true: publisher does NOT receive its own message ----
  {
    TopicRouter router;
    router.Subscribe(TopicPattern{"audio/#"}, "pipeline_a", QoS::AT_MOST_ONCE,
                     /*no_local=*/true);
    auto deliveries = router.Route(Topic{"audio/raw"}, "pipeline_a",
                                   QoS::AT_MOST_ONCE);
    CHECK(deliveries.empty(), "no_local=true blocks self-delivery");
  }

  // ---- 7. no_local=false: publisher DOES receive its own message ----
  {
    TopicRouter router;
    router.Subscribe(TopicPattern{"audio/#"}, "pipeline_a", QoS::AT_MOST_ONCE,
                     /*no_local=*/false);
    auto deliveries = router.Route(Topic{"audio/raw"}, "pipeline_a",
                                   QoS::AT_MOST_ONCE);
    CHECK(deliveries.size() == 1, "no_local=false allows self-delivery");
  }

  // ---- 8. Fan-out: multiple subscribers all receive ----
  {
    TopicRouter router;
    router.Subscribe(TopicPattern{"audio/raw"}, "pipeline_a");
    router.Subscribe(TopicPattern{"audio/raw"}, "pipeline_b");
    router.Subscribe(TopicPattern{"audio/+"}, "pipeline_c");
    auto deliveries = router.Route(Topic{"audio/raw"}, "publisher",
                                   QoS::AT_MOST_ONCE);
    CHECK(deliveries.size() == 3, "fan-out delivers to all three subscribers");
  }

  // ---- 9. Unsubscribe removes subscription ----
  {
    TopicRouter router;
    long id = router.Subscribe(TopicPattern{"audio/raw"}, "pipeline_a");
    CHECK(router.Route(Topic{"audio/raw"}, "pub", QoS::AT_MOST_ONCE).size() == 1,
          "delivers before unsubscribe");

    router.Unsubscribe(id);
    CHECK(router.Route(Topic{"audio/raw"}, "pub", QoS::AT_MOST_ONCE).empty(),
          "no delivery after unsubscribe");
  }

  // ---- 10. RemovePipeline removes all subscriptions for that pipeline ----
  {
    TopicRouter router;
    router.Subscribe(TopicPattern{"audio/raw"}, "pipeline_a");
    router.Subscribe(TopicPattern{"vad/+"}, "pipeline_a");
    router.Subscribe(TopicPattern{"audio/raw"}, "pipeline_b");

    CHECK(router.GetAllSubscriptions().size() == 3, "three subscriptions exist");

    router.RemovePipeline("pipeline_a");
    CHECK(router.GetAllSubscriptions().size() == 1,
          "only pipeline_b remains after RemovePipeline");
    CHECK(router.GetAllSubscriptions()[0].pipeline_name == "pipeline_b",
          "remaining subscription belongs to pipeline_b");
  }

  // ---- 11. Effective QoS is min of publisher and subscriber QoS ----
  {
    TopicRouter router;

    // Subscriber wants EXACTLY_ONCE, publisher offers AT_MOST_ONCE -> min = 0
    router.Subscribe(TopicPattern{"qos/test"}, "high_qos", QoS::EXACTLY_ONCE);
    auto d1 = router.Route(Topic{"qos/test"}, "pub", QoS::AT_MOST_ONCE);
    CHECK(d1.size() == 1 && d1[0].effective_qos == QoS::AT_MOST_ONCE,
          "min(EXACTLY_ONCE, AT_MOST_ONCE) = AT_MOST_ONCE");

    // Subscriber wants AT_MOST_ONCE, publisher offers EXACTLY_ONCE -> min = 0
    router.Subscribe(TopicPattern{"qos/test2"}, "low_qos", QoS::AT_MOST_ONCE);
    auto d2 = router.Route(Topic{"qos/test2"}, "pub", QoS::EXACTLY_ONCE);
    CHECK(d2.size() == 1 && d2[0].effective_qos == QoS::AT_MOST_ONCE,
          "min(AT_MOST_ONCE, EXACTLY_ONCE) = AT_MOST_ONCE");

    // Subscriber wants AT_LEAST_ONCE, publisher offers EXACTLY_ONCE -> min = 1
    router.Subscribe(TopicPattern{"qos/test3"}, "mid_qos", QoS::AT_LEAST_ONCE);
    auto d3 = router.Route(Topic{"qos/test3"}, "pub", QoS::EXACTLY_ONCE);
    CHECK(d3.size() == 1 && d3[0].effective_qos == QoS::AT_LEAST_ONCE,
          "min(AT_LEAST_ONCE, EXACTLY_ONCE) = AT_LEAST_ONCE");

    // Both AT_LEAST_ONCE -> min = 1
    router.Subscribe(TopicPattern{"qos/test4"}, "same_qos", QoS::AT_LEAST_ONCE);
    auto d4 = router.Route(Topic{"qos/test4"}, "pub", QoS::AT_LEAST_ONCE);
    CHECK(d4.size() == 1 && d4[0].effective_qos == QoS::AT_LEAST_ONCE,
          "min(AT_LEAST_ONCE, AT_LEAST_ONCE) = AT_LEAST_ONCE");
  }

  // ---- 12. Multiple subscriptions with different patterns ----
  {
    TopicRouter router;
    router.Subscribe(TopicPattern{"audio/#"}, "audio_consumer");
    router.Subscribe(TopicPattern{"vad/+"}, "vad_consumer");
    router.Subscribe(TopicPattern{"system/#"}, "monitor");

    // audio/raw matches audio/# only
    auto d1 = router.Route(Topic{"audio/raw"}, "pub", QoS::AT_MOST_ONCE);
    CHECK(d1.size() == 1 && d1[0].pipeline_name == "audio_consumer",
          "audio/raw -> audio_consumer only");

    // vad/speech_segment matches vad/+ only
    auto d2 = router.Route(Topic{"vad/speech_segment"}, "pub",
                            QoS::AT_MOST_ONCE);
    CHECK(d2.size() == 1 && d2[0].pipeline_name == "vad_consumer",
          "vad/speech_segment -> vad_consumer only");

    // system/pipeline/online matches system/# only
    auto d3 = router.Route(Topic{"system/pipeline/online"}, "pub",
                            QoS::AT_MOST_ONCE);
    CHECK(d3.size() == 1 && d3[0].pipeline_name == "monitor",
          "system/pipeline/online -> monitor only");
  }

  if (g_fail == 0) {
    std::printf("TopicRouter test PASSED\n");
    return 0;
  }
  std::printf("TopicRouter test FAILED (%d checks)\n", g_fail);
  return 1;
}
