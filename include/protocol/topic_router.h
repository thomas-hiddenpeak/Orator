#pragma once

// TopicRouter (Spec 004 Phase 9): publish/subscribe topic routing engine.
//
// Matches published messages against subscriber TopicPatterns and returns the
// list of Delivery targets. Thread-safe via internal mutex. Does not perform
// actual message delivery — it is a pure routing layer that returns a list of
// (subscriber, effective QoS) pairs for the caller to dispatch.
//
// QoS negotiation: effective QoS = min(publisher_qos, subscriber_requested_qos).
// A subscriber requesting EXACTLY_ONCE cannot get better than what the publisher
// offers, and a publisher at AT_MOST_ONCE cannot guarantee more.

#include <algorithm>
#include <cstdint>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "protocol/topic.h"

namespace orator {
namespace protocol {

enum class QoS : uint8_t {
  AT_MOST_ONCE = 0,  // fire-and-forget
  AT_LEAST_ONCE = 1, // ACK-based
  EXACTLY_ONCE = 2,  // dedup window
};

struct Subscription {
  long sub_id = 0;
  std::string pipeline_name;
  TopicPattern pattern;
  QoS requested_qos = QoS::AT_MOST_ONCE;
  bool no_local = false;
};

struct Delivery {
  long sub_id = 0;
  std::string pipeline_name;
  QoS effective_qos = QoS::AT_MOST_ONCE;
};

class TopicRouter {
 public:
  using SubscriptionId = long;

  // Subscribe a pipeline to a topic pattern.
  // Returns a subscription id for later unsubscribe.
  SubscriptionId Subscribe(const TopicPattern& pattern,
                           const std::string& pipeline_name, QoS qos = QoS::AT_MOST_ONCE,
                           bool no_local = false);

  // Remove a subscription by id.
  void Unsubscribe(SubscriptionId sub_id);

  // Route a published message to all matching subscribers.
  // Returns list of Delivery targets (subscriber + effective QoS).
  // `publisher_name` is used for no_local filtering.
  // Effective QoS = min(publisher_qos, subscriber_requested_qos).
  std::vector<Delivery> Route(const Topic& topic, const std::string& publisher_name,
                              QoS publisher_qos);

  // Return all subscriptions (for inspection).
  std::vector<Subscription> GetAllSubscriptions() const;

  // Remove all subscriptions for a pipeline (called on deregistration).
  void RemovePipeline(const std::string& pipeline_name);

 private:
  struct SubEntry {
    SubscriptionId id;
    Subscription sub;
  };

  std::vector<SubEntry> subscriptions_;
  SubscriptionId next_sub_id_ = 1;
  mutable std::mutex mutex_;
};

}  // namespace protocol
}  // namespace orator
