#pragma once

#include <atomic>
#include <cstdio>

// ------------------------------------------------------------------
// Level-based logging for Orator.
//
// Usage:
//   LOG_INFO("server listening on port %d", port);
//   LOG_ERROR("lws_create_context failed");
//   LOG_DEBUG("RECEIVE: len=%zu", n);
//
// Control at runtime with SetOratorLogLevel(), or define ORATOR_LOG_LEVEL
// before including this header to set the compile-time floor.
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

inline std::atomic<int>& OratorRuntimeLogLevelStorage() {
  static std::atomic<int> level{ORATOR_LOG_LEVEL};
  return level;
}

inline void SetOratorLogLevel(int level) {
  if (level < ORATOR_LOG_DEBUG) level = ORATOR_LOG_DEBUG;
  if (level > ORATOR_LOG_ERROR) level = ORATOR_LOG_ERROR;
  OratorRuntimeLogLevelStorage().store(level, std::memory_order_relaxed);
}

inline int OratorLogLevel_Runtime() {
  return OratorRuntimeLogLevelStorage().load(std::memory_order_relaxed);
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
