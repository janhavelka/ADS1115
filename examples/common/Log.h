/**
 * @file Log.h
 * @brief Simple serial logging macros for examples.
 *
 * NOT part of the library API. The library itself does not log.
 * These macros are for example/application code only.
 */

#pragma once

#include <Arduino.h>

#include "examples/common/BuildConfig.h"

// Compile-time validation
#if LOG_LEVEL < 0 || LOG_LEVEL > 2
#error "LOG_LEVEL must be 0-2 (0=off, 1=error, 2=warning/info)"
#endif

#ifndef LOG_SERIAL
#define LOG_SERIAL Serial
#endif

#define LOG_COLOR_RESET  "\033[0m"
#define LOG_COLOR_RED    "\033[31m"
#define LOG_COLOR_GREEN  "\033[32m"
#define LOG_COLOR_YELLOW "\033[33m"
#define LOG_COLOR_CYAN   "\033[36m"
#define LOG_COLOR_GRAY   "\033[90m"
#define LOG_COLOR_RESULT(ok) ((ok) ? LOG_COLOR_GREEN : LOG_COLOR_RED)
#define LOG_COLOR_STATE(online, failures) \
  ((online) ? (((failures) > 0U) ? LOG_COLOR_YELLOW : LOG_COLOR_GREEN) : LOG_COLOR_RED)

inline const char* log_bool_str(bool value) { return value ? "yes" : "no"; }

// Colorize only the severity tag; keep message text in terminal default color.
#define LOG_PRINT_WITH_TAG(tagColor, tag, fmt, ...) \
  LOG_SERIAL.printf(tagColor "[" tag "]" LOG_COLOR_RESET " " fmt "\n", ##__VA_ARGS__)

/// @brief Log error message (level >= 1)
#define LOGE(fmt, ...) \
  do { \
    if (LOG_LEVEL >= 1) LOG_PRINT_WITH_TAG(LOG_COLOR_RED, "E", fmt, ##__VA_ARGS__); \
  } while (0)

/// @brief Log warning message (level >= 2)
#define LOGW(fmt, ...) \
  do { \
    if (LOG_LEVEL >= 2) LOG_PRINT_WITH_TAG(LOG_COLOR_YELLOW, "W", fmt, ##__VA_ARGS__); \
  } while (0)

/// @brief Log info message (level >= 2)
#define LOGI(fmt, ...) \
  do { \
    if (LOG_LEVEL >= 2) LOG_PRINT_WITH_TAG(LOG_COLOR_CYAN, "I", fmt, ##__VA_ARGS__); \
  } while (0)

// Conditional verbose logging (runtime switch)
#define LOGV(verbose, fmt, ...) \
  do { \
    if (verbose) { \
      LOG_PRINT_WITH_TAG(LOG_COLOR_GRAY, "V", fmt, ##__VA_ARGS__); \
    } \
  } while (0)
