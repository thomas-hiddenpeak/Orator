#include "protocol/topic_router.h"

#include <algorithm>

namespace orator {
namespace protocol {

TopicRouter::SubscriptionId TopicRouter::Subscribe(
    const TopicPattern& pattern, const std::string& pipeline_name, QoS qos,
    bool no_local) {
  std::lock_guard<std::mutex> lock(mutex_);
  SubEntry entry;
  entry.id = next_sub_id_++;
  entry.sub.sub_id = entry.id;
  entry.sub.pipeline_name = pipeline_name;
  entry.sub.pattern = pattern;
  entry.sub.requested_qos = qos;
  entry.sub.no_local = no_local;
  // Capture the id before the move: reading entry.id after std::move(entry)
  // would be a use-after-move (bugprone-use-after-move).
  const SubscriptionId id = entry.id;
  subscriptions_.push_back(std::move(entry));
  return id;
}

void TopicRouter::Unsubscribe(SubscriptionId sub_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  subscriptions_.erase(
      std::remove_if(subscriptions_.begin(), subscriptions_.end(),
                     [sub_id](SubEntry const& e) { return e.id == sub_id; }),
      subscriptions_.end());
}

std::vector<Delivery> TopicRouter::Route(const Topic& topic,
                                         const std::string& publisher_name,
                                         QoS publisher_qos) {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<Delivery> deliveries;

  for (SubEntry const& entry : subscriptions_) {
    // no_local: skip if subscriber is the same pipeline as the publisher.
    if (entry.sub.no_local && entry.sub.pipeline_name == publisher_name)
      continue;

    if (!entry.sub.pattern.Matches(topic)) continue;

    Delivery d;
    d.sub_id = entry.sub.sub_id;
    d.pipeline_name = entry.sub.pipeline_name;
    // Effective QoS = min(publisher_qos, subscriber_requested_qos).
    uint8_t pub = static_cast<uint8_t>(publisher_qos);
    uint8_t sub = static_cast<uint8_t>(entry.sub.requested_qos);
    d.effective_qos = static_cast<QoS>(std::min(pub, sub));
    deliveries.push_back(d);
  }

  return deliveries;
}

std::vector<Subscription> TopicRouter::GetAllSubscriptions() const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<Subscription> subs;
  subs.reserve(subscriptions_.size());
  for (SubEntry const& e : subscriptions_) {
    subs.push_back(e.sub);
  }
  return subs;
}

void TopicRouter::RemovePipeline(const std::string& pipeline_name) {
  std::lock_guard<std::mutex> lock(mutex_);
  subscriptions_.erase(
      std::remove_if(subscriptions_.begin(), subscriptions_.end(),
                     [&pipeline_name](SubEntry const& e) {
                       return e.sub.pipeline_name == pipeline_name;
                     }),
      subscriptions_.end());
}

}  // namespace protocol
}  // namespace orator
