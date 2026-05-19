#include "Ads1115IdfI2cTransport.h"

#include <climits>

#include "esp_err.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {

ADS1115::Status mapEspError(esp_err_t err) {
  if (err == ESP_OK) {
    return ADS1115::Status::Ok();
  }
  if (err == ESP_ERR_TIMEOUT) {
    return ADS1115::Status::Error(ADS1115::Err::I2C_TIMEOUT, "ESP-IDF I2C timeout", err);
  }
#if defined(ESP_ERR_INVALID_RESPONSE)
  if (err == ESP_ERR_INVALID_RESPONSE) {
    return ADS1115::Status::Error(ADS1115::Err::I2C_ERROR, "ESP-IDF I2C invalid response", err);
  }
#endif
  if (err == ESP_ERR_INVALID_ARG || err == ESP_ERR_INVALID_STATE) {
    return ADS1115::Status::Error(ADS1115::Err::I2C_BUS, "ESP-IDF I2C bus error", err);
  }
  return ADS1115::Status::Error(ADS1115::Err::I2C_ERROR, "ESP-IDF I2C error", err);
}

ADS1115::Status resolveTransfer(void* user, uint8_t addr, uint32_t timeoutMs,
                                Ads1115IdfI2cTransport*& transport,
                                int& timeoutOut) {
  transport = static_cast<Ads1115IdfI2cTransport*>(user);
  if (transport == nullptr || transport->dev == nullptr) {
    return ADS1115::Status::Error(ADS1115::Err::INVALID_CONFIG, "IDF I2C transport missing");
  }
  if (addr != transport->address) {
    return ADS1115::Status::Error(ADS1115::Err::INVALID_CONFIG, "IDF I2C address mismatch", addr);
  }
  if (timeoutMs == 0 || timeoutMs > static_cast<uint32_t>(INT_MAX)) {
    return ADS1115::Status::Error(ADS1115::Err::INVALID_CONFIG, "Invalid IDF I2C timeout",
                                  static_cast<int32_t>(timeoutMs));
  }
  timeoutOut = static_cast<int>(timeoutMs);
  return ADS1115::Status::Ok();
}

}  // namespace

ADS1115::Status ads1115IdfWrite(uint8_t addr, const uint8_t* data, size_t len,
                                uint32_t timeoutMs, void* user) {
  if (data == nullptr || len == 0) {
    return ADS1115::Status::Error(ADS1115::Err::INVALID_PARAM, "Invalid IDF I2C write buffers");
  }
  Ads1115IdfI2cTransport* transport = nullptr;
  int timeout = 0;
  ADS1115::Status st = resolveTransfer(user, addr, timeoutMs, transport, timeout);
  if (!st.ok()) {
    return st;
  }
  return mapEspError(i2c_master_transmit(transport->dev, data, len, timeout));
}

ADS1115::Status ads1115IdfWriteRead(uint8_t addr, const uint8_t* txData, size_t txLen,
                                    uint8_t* rxData, size_t rxLen, uint32_t timeoutMs,
                                    void* user) {
  if (txData == nullptr || txLen == 0 || rxData == nullptr || rxLen == 0) {
    return ADS1115::Status::Error(ADS1115::Err::INVALID_PARAM, "Invalid IDF I2C read buffers");
  }
  Ads1115IdfI2cTransport* transport = nullptr;
  int timeout = 0;
  ADS1115::Status st = resolveTransfer(user, addr, timeoutMs, transport, timeout);
  if (!st.ok()) {
    return st;
  }
  return mapEspError(i2c_master_transmit_receive(transport->dev, txData, txLen,
                                                 rxData, rxLen, timeout));
}

uint32_t ads1115IdfNowMs(void*) {
  return static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
}

void ads1115IdfYield(void*) {
  taskYIELD();
}
