// Unit tests for core/registry.h (Registry template) and
// model/builtin_registration.h (EnsureBuiltinsRegistered).
//
// No GPU required — these tests exercise the registration mechanism only.
// For builtin registrations we check Contains() and Keys() but do NOT call
// Create() (the factories construct real model objects that need GPU + weights).

#include <cstdio>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "core/registry.h"
#include "core/stages.h"
#include "model/builtin_registration.h"

using orator::core::Registry;
using orator::core::Registrar;

static int g_fail = 0;
#define CHECK(cond, msg)              \
  do {                                \
    if (!(cond)) {                    \
      std::printf("FAIL: %s\n", msg); \
      ++g_fail;                       \
    }                                 \
  } while (0)

// ---------------------------------------------------------------------------
// A minimal test interface for exercising Registry without GPU dependencies.
// ---------------------------------------------------------------------------
struct ITest {
  virtual ~ITest() = default;
  virtual int Value() const = 0;
};

struct TestA : ITest {
  int Value() const override { return 42; }
};

struct TestB : ITest {
  int Value() const override { return 99; }
};

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main() {
  std::printf("Testing core::Registry and model::EnsureBuiltinsRegistered...\n");

  // ------------------------------------------------------------------
  // 1. Registry: Register, Contains, Create, Keys with a custom interface.
  // ------------------------------------------------------------------
  {
    std::printf("  Test 1: Registry<ITest> basic operations\n");
    auto& reg = Registry<ITest>::Instance();

    // Initially empty.
    CHECK(!reg.Contains("test_a"), "Registry empty before registration");
    CHECK(!reg.Contains("test_b"), "Registry empty before registration");
    CHECK(reg.Keys().empty(), "Registry::Keys() empty before registration");

    // Register two factories.
    reg.Register("test_a", [] { return std::make_unique<TestA>(); });
    reg.Register("test_b", [] { return std::make_unique<TestB>(); });

    // Contains after registration.
    CHECK(reg.Contains("test_a"), "Contains('test_a') after registration");
    CHECK(reg.Contains("test_b"), "Contains('test_b') after registration");
    CHECK(!reg.Contains("nonexistent"), "Contains('nonexistent') false");

    // Create and verify.
    auto a = reg.Create("test_a");
    CHECK(a != nullptr, "Create('test_a') returns non-null");
    CHECK(a->Value() == 42, "Create('test_a')->Value() == 42");

    auto b = reg.Create("test_b");
    CHECK(b != nullptr, "Create('test_b') returns non-null");
    CHECK(b->Value() == 99, "Create('test_b')->Value() == 99");

    // Keys() returns all registered keys.
    auto keys = reg.Keys();
    CHECK(keys.size() == 2, "Keys() size == 2");
    std::set<std::string> key_set(keys.begin(), keys.end());
    CHECK(key_set.count("test_a") == 1, "Keys() contains 'test_a'");
    CHECK(key_set.count("test_b") == 1, "Keys() contains 'test_b'");

    std::printf("  PASS\n");
  }

  // ------------------------------------------------------------------
  // 2. Registry: Create throws for unknown key.
  // ------------------------------------------------------------------
  {
    std::printf("  Test 2: Registry throws for unknown key\n");
    auto& reg = Registry<ITest>::Instance();
    bool threw = false;
    try {
      reg.Create("does_not_exist");
    } catch (const std::runtime_error&) {
      threw = true;
    }
    CHECK(threw, "Create('does_not_exist') throws std::runtime_error");
    std::printf("  PASS\n");
  }

  // ------------------------------------------------------------------
  // 3. Registrar helper (static-init registration).
  // ------------------------------------------------------------------
  {
    std::printf("  Test 3: Registrar helper\n");
    // Register via Registrar (simulates static-init pattern).
    Registrar<ITest> r("registrar_test", [] {
      return std::make_unique<TestA>();
    });
    auto& reg = Registry<ITest>::Instance();
    CHECK(reg.Contains("registrar_test"),
          "Contains('registrar_test') after Registrar");
    auto obj = reg.Create("registrar_test");
    CHECK(obj != nullptr, "Create('registrar_test') returns non-null");
    CHECK(obj->Value() == 42, "Create('registrar_test')->Value() == 42");
    std::printf("  PASS\n");
  }

  // ------------------------------------------------------------------
  // 4. EnsureBuiltinsRegistered registers "sortformer" and "qwen3_asr".
  // ------------------------------------------------------------------
  {
    std::printf("  Test 4: EnsureBuiltinsRegistered\n");
    orator::model::EnsureBuiltinsRegistered();

    // Check IDiarizer registry.
    auto& diar_reg = Registry<orator::core::IDiarizer>::Instance();
    CHECK(diar_reg.Contains("sortformer"),
          "IDiarizer registry contains 'sortformer'");
    CHECK(!diar_reg.Contains("nonexistent_diar"),
          "IDiarizer registry does not contain unknown key");

    // Check IAsr registry.
    auto& asr_reg = Registry<orator::core::IAsr>::Instance();
    CHECK(asr_reg.Contains("qwen3_asr"),
          "IAsr registry contains 'qwen3_asr'");
    CHECK(!asr_reg.Contains("nonexistent_asr"),
          "IAsr registry does not contain unknown key");

    // Keys() for each registry.
    auto diar_keys = diar_reg.Keys();
    CHECK(diar_keys.size() >= 1, "IDiarizer Keys() size >= 1");
    bool found_sortformer = false;
    for (const auto& k : diar_keys)
      if (k == "sortformer") found_sortformer = true;
    CHECK(found_sortformer, "IDiarizer Keys() contains 'sortformer'");

    auto asr_keys = asr_reg.Keys();
    CHECK(asr_keys.size() >= 1, "IAsr Keys() size >= 1");
    bool found_qwen = false;
    for (const auto& k : asr_keys)
      if (k == "qwen3_asr") found_qwen = true;
    CHECK(found_qwen, "IAsr Keys() contains 'qwen3_asr'");

    std::printf("  PASS\n");
  }

  // ------------------------------------------------------------------
  // 5. EnsureBuiltinsRegistered is idempotent (second call is a no-op).
  // ------------------------------------------------------------------
  {
    std::printf("  Test 5: EnsureBuiltinsRegistered idempotent\n");
    // Capture keys before second call.
    auto& diar_reg = Registry<orator::core::IDiarizer>::Instance();
    auto keys_before = diar_reg.Keys();

    // Call again.
    orator::model::EnsureBuiltinsRegistered();

    auto keys_after = diar_reg.Keys();
    CHECK(keys_before.size() == keys_after.size(),
          "Keys unchanged after second EnsureBuiltinsRegistered call");
    std::printf("  PASS\n");
  }

  // ------------------------------------------------------------------
  // Summary
  // ------------------------------------------------------------------
  if (g_fail == 0) {
    std::printf("core::Registry and model::EnsureBuiltinsRegistered test PASSED\n");
    return 0;
  }
  std::printf("core::Registry and model::EnsureBuiltinsRegistered test FAILED (%d checks)\n",
              g_fail);
  return 1;
}
