/// @file Status.h
/// @brief Error codes and status handling for ADS1115 driver
#pragma once

#include <cstdint>

namespace ADS1115 {

/// @brief Error codes for all ADS1115 operations.
enum class Err : uint8_t {
  OK = 0,                    ///< Operation successful
  NOT_INITIALIZED,           ///< begin() not called
  INVALID_CONFIG,            ///< Invalid configuration parameter
  I2C_ERROR,                 ///< I2C communication failure
  TIMEOUT,                   ///< Operation timed out
  INVALID_PARAM,             ///< Invalid parameter value
  DEVICE_NOT_FOUND,          ///< Device did not acknowledge a probe/read when absence is definite
  CONVERSION_NOT_READY,      ///< Conversion not yet complete
  MEASUREMENT_NOT_READY = CONVERSION_NOT_READY, ///< Alias for cross-library uniformity
  BUSY,                      ///< Device is busy with conversion
  IN_PROGRESS,               ///< Operation scheduled; call tick() to complete
  I2C_NACK_ADDR,             ///< I2C address phase was not acknowledged
  I2C_NACK_DATA,             ///< I2C data phase was not acknowledged
  I2C_TIMEOUT,               ///< I2C transaction timed out
  I2C_BUS,                   ///< I2C bus or arbitration error
  OFFLINE,                   ///< Passive health state reached the configured failure threshold
  UNSUPPORTED_OPERATION,     ///< Requested operation is not valid for the current driver/device mode
  READBACK_MISMATCH,         ///< Strict register read-back did not match the expected writable fields
  HARDWARE_CONFIG_DIRTY,     ///< Hardware/cache synchronization is known dirty or stale
  CLOCK_STALLED,             ///< Supplied monotonic timebase did not advance while waiting
  CANCELLED,                 ///< Operation was cancelled by the caller
  CONFIG_UNKNOWN,            ///< Typed result is unsafe because hardware configuration is not verified
  RESULT_NOT_AVAILABLE,      ///< No unconsumed terminal result is available
  TOKEN_MISMATCH,            ///< Requested result token does not match the pending terminal result
  INDETERMINATE              ///< Hardware effect or final state cannot be confirmed
};

/// @brief Status structure returned by all fallible operations.
struct Status {
  Err code = Err::OK;
  int32_t detail = 0;        ///< Implementation-specific detail such as raw transport code or observed register value
  const char* msg = "";      ///< Static string describing the error

  constexpr Status() = default;
  constexpr Status(Err codeIn, int32_t detailIn, const char* msgIn)
      : code(codeIn), detail(detailIn), msg(msgIn) {}
  
  /// @return true if operation succeeded
  constexpr bool ok() const { return code == Err::OK; }

  /// @return true if the status matches the provided error code
  constexpr bool is(Err err) const { return code == err; }
  
  /// @return true if operation in progress (not a failure)
  constexpr bool inProgress() const { return code == Err::IN_PROGRESS; }

  /// @return true if operation succeeded
  explicit constexpr operator bool() const { return ok(); }
  
  /// Create a success status
  static constexpr Status Ok() { return Status{Err::OK, 0, "OK"}; }
  
  /// Create an error status
  static constexpr Status Error(Err err, const char* message, int32_t detailCode = 0) {
    return Status{err, detailCode, message};
  }
};

} // namespace ADS1115
