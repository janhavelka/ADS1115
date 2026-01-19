/// @file test_basic.cpp
/// @brief Basic unit tests for ADS1115 driver

#include <cstdio>
#include <cassert>

// Include stubs first
#include "Arduino.h"
#include "Wire.h"

// Stub implementations
SerialClass Serial;
TwoWire Wire;

// Include driver
#include "ADS1115/Status.h"
#include "ADS1115/Config.h"

using namespace ADS1115;

// ============================================================================
// Test Helpers
// ============================================================================

static int testsPassed = 0;
static int testsFailed = 0;

#define TEST(name) void test_##name()
#define RUN_TEST(name) do { \
  printf("Running %s... ", #name); \
  test_##name(); \
  printf("PASSED\n"); \
  testsPassed++; \
} catch (...) { \
  printf("FAILED\n"); \
  testsFailed++; \
}

#define ASSERT_TRUE(x) assert(x)
#define ASSERT_FALSE(x) assert(!(x))
#define ASSERT_EQ(a, b) assert((a) == (b))
#define ASSERT_NE(a, b) assert((a) != (b))

// ============================================================================
// Tests
// ============================================================================

TEST(status_ok) {
  Status st = Status::Ok();
  ASSERT_TRUE(st.ok());
  ASSERT_EQ(st.code, Err::OK);
}

TEST(status_error) {
  Status st = Status::Error(Err::I2C_ERROR, "Test error", 42);
  ASSERT_FALSE(st.ok());
  ASSERT_EQ(st.code, Err::I2C_ERROR);
  ASSERT_EQ(st.detail, 42);
}

TEST(status_in_progress) {
  Status st = Status{Err::IN_PROGRESS, 0, "In progress"};
  ASSERT_FALSE(st.ok());
  ASSERT_TRUE(st.inProgress());
}

TEST(config_defaults) {
  Config cfg;
  ASSERT_EQ(cfg.i2cWrite, nullptr);
  ASSERT_EQ(cfg.i2cWriteRead, nullptr);
  ASSERT_EQ(cfg.i2cAddress, 0x48);
  ASSERT_EQ(cfg.i2cTimeoutMs, 50);
  ASSERT_EQ(static_cast<uint8_t>(cfg.mux), static_cast<uint8_t>(Mux::AIN0_GND));
  ASSERT_EQ(static_cast<uint8_t>(cfg.gain), static_cast<uint8_t>(Gain::FSR_2_048V));
  ASSERT_EQ(static_cast<uint8_t>(cfg.dataRate), static_cast<uint8_t>(DataRate::SPS_128));
  ASSERT_EQ(static_cast<uint8_t>(cfg.mode), static_cast<uint8_t>(Mode::SINGLE_SHOT));
  ASSERT_EQ(static_cast<uint8_t>(cfg.compQueue), static_cast<uint8_t>(ComparatorQueue::DISABLE));
  ASSERT_EQ(cfg.compThresholdHigh, 0x7FFF);
  ASSERT_EQ(cfg.compThresholdLow, static_cast<int16_t>(0x8000));
  ASSERT_EQ(cfg.offlineThreshold, 5);
}

// ============================================================================
// Main
// ============================================================================

int main() {
  printf("\n=== ADS1115 Unit Tests ===\n\n");
  
  RUN_TEST(status_ok);
  RUN_TEST(status_error);
  RUN_TEST(status_in_progress);
  RUN_TEST(config_defaults);
  
  printf("\n=== Results: %d passed, %d failed ===\n\n", testsPassed, testsFailed);
  
  return testsFailed > 0 ? 1 : 0;
}
