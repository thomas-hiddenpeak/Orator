#pragma once

#include <cstdio>
#include <cstdlib>

// ------------------------------------------------------------------
// Level-based logging for Orator.
//
// Usage:
//   LOG_INFO("server listening on port %d", port);
//   LOG_ERROR("lws_create_context failed");
//   LOG_DEBUG("RECEIVE: len=%zu", n);
//
// Control at compile time via LOG_LEVEL env var at program start
// (runtime), or by defining ORATOR_LOG_LEVEL before including this
// header (compile-time floor).
//
// Levels: 0=DEBUG, 1=INFO, 2=WARN, 3=ERROR
// ------------------------------------------------------------------

enum OratorLogLevel {
  ORATOR_LOG_DEBUG = 0,
  ORATOR_LOG_INFO = 1,
  ORATOR_LOG_WARN = 2,
  ORATOR_LOG_ERROR = 3,
};

// Compile-time floor – everything below this is stripped.
#ifndef ORATOR_LOG_LEVEL
#define ORATOR_LOG_LEVEL ORATOR_LOG_DEBUG
#endif

// Runtime gate: check env var once per call (fine for infrequent logging).
// If the env is not set, all levels at or above the compile-time floor pass.
inline int OratorLogLevel_Runtime() {
  static const int level = []() -> int {
    const char* e = std::getenv("ORATOR_LOG_LEVEL");
    if (!e) return ORATOR_LOG_LEVEL;
    int v = std::atoi(e);
    return (v < ORATOR_LOG_DEBUG)   ? static_cast<int>(ORATOR_LOG_DEBUG)
           : (v > ORATOR_LOG_ERROR) ? static_cast<int>(ORATOR_LOG_ERROR)
                                    : v;
  }();
  return level;
}

#define LOG_DEBUG(...)                                \
  do {                                                \
    if (ORATOR_LOG_LEVEL <= ORATOR_LOG_DEBUG &&       \
        OratorLogLevel_Runtime() <= ORATOR_LOG_DEBUG) \
      std::fprintf(stderr, "[DEBUG] " __VA_ARGS__);   \
  } while (0)

#define LOG_INFO(...)                                \
  do {                                               \
    if (ORATOR_LOG_LEVEL <= ORATOR_LOG_INFO &&       \
        OratorLogLevel_Runtime() <= ORATOR_LOG_INFO) \
      std::fprintf(stderr, "[INFO] " __VA_ARGS__);   \
  } while (0)

#define LOG_WARN(...)                                \
  do {                                               \
    if (ORATOR_LOG_LEVEL <= ORATOR_LOG_WARN &&       \
        OratorLogLevel_Runtime() <= ORATOR_LOG_WARN) \
      std::fprintf(stderr, "[WARN] " __VA_ARGS__);   \
  } while (0)

#define LOG_ERROR(...)                                \
  do {                                                \
    if (ORATOR_LOG_LEVEL <= ORATOR_LOG_ERROR &&       \
        OratorLogLevel_Runtime() <= ORATOR_LOG_ERROR) \
      std::fprintf(stderr, "[ERROR] " __VA_ARGS__);   \
  } while (0)
