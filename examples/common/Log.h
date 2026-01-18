/// @file Log.h
/// @brief Logging macros for examples only (NOT part of library)
#pragma once

#include <Arduino.h>

// Simple logging macros for examples
// DO NOT use these in library code!

#define LOGI(fmt, ...) Serial.printf("[I] " fmt "\n", ##__VA_ARGS__)
#define LOGW(fmt, ...) Serial.printf("[W] " fmt "\n", ##__VA_ARGS__)
#define LOGE(fmt, ...) Serial.printf("[E] " fmt "\n", ##__VA_ARGS__)
#define LOGD(fmt, ...) Serial.printf("[D] " fmt "\n", ##__VA_ARGS__)

// Conditional verbose logging
#define LOGV(verbose, fmt, ...) do { if (verbose) { Serial.printf("[V] " fmt "\n", ##__VA_ARGS__); } } while(0)
