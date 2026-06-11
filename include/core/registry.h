#pragma once

// Generic name->factory registry enabling runtime-swappable implementations.
//
// Each interface type gets its own registry instance. Concrete implementations
// self-register (see model/builtin_registration.cc) under a string key, and the
// pipeline instantiates by key from config. Adding a new model never requires
// editing the pipeline or any other consumer.

#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace orator {
namespace core {

template <typename Interface>
class Registry {
 public:
  using Factory = std::function<std::unique_ptr<Interface>()>;

  static Registry& Instance() {
    static Registry instance;
    return instance;
  }

  void Register(const std::string& key, Factory factory) {
    factories_[key] = std::move(factory);
  }

  bool Contains(const std::string& key) const {
    return factories_.find(key) != factories_.end();
  }

  std::unique_ptr<Interface> Create(const std::string& key) const {
    auto it = factories_.find(key);
    if (it == factories_.end()) {
      throw std::runtime_error("Registry: unknown key '" + key + "'");
    }
    return (it->second)();
  }

  std::vector<std::string> Keys() const {
    std::vector<std::string> keys;
    keys.reserve(factories_.size());
    for (const auto& kv : factories_) keys.push_back(kv.first);
    return keys;
  }

 private:
  Registry() = default;
  std::unordered_map<std::string, Factory> factories_;
};

// Helper that registers a factory at static-init time.
template <typename Interface>
struct Registrar {
  Registrar(const std::string& key,
            typename Registry<Interface>::Factory factory) {
    Registry<Interface>::Instance().Register(key, std::move(factory));
  }
};

}  // namespace core
}  // namespace orator
