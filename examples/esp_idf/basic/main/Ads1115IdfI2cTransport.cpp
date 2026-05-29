#include "Ads1115IdfI2cTransport.h"

#include <climits>

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {

Ads1115IdfI2cTransport gTransport;

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

Ads1115IdfI2cTransport& ads1115IdfTransportContext() {
  return gTransport;
}

bool ads1115IdfInitI2c(int sda, int scl, uint32_t freqHz, uint16_t timeoutMs,
                       uint8_t address) {
  (void)timeoutMs;
  ads1115IdfDeinitI2c();

  i2c_master_bus_config_t busConfig = {};
  busConfig.i2c_port = I2C_NUM_0;
  busConfig.sda_io_num = static_cast<gpio_num_t>(sda);
  busConfig.scl_io_num = static_cast<gpio_num_t>(scl);
  busConfig.clk_source = I2C_CLK_SRC_DEFAULT;
  busConfig.glitch_ignore_cnt = 7;
  busConfig.flags.enable_internal_pullup = true;

  esp_err_t err = i2c_new_master_bus(&busConfig, &gTransport.bus);
  if (err != ESP_OK) {
    gTransport.lastError = err;
    return false;
  }

  i2c_device_config_t devConfig = {};
  devConfig.dev_addr_length = I2C_ADDR_BIT_LEN_7;
  devConfig.device_address = address;
  devConfig.scl_speed_hz = freqHz;

  err = i2c_master_bus_add_device(gTransport.bus, &devConfig, &gTransport.dev);
  if (err != ESP_OK) {
    (void)i2c_del_master_bus(gTransport.bus);
    gTransport.bus = nullptr;
    gTransport.lastError = err;
    return false;
  }

  gTransport.address = address;
  gTransport.lastError = ESP_OK;
  return true;
}

void ads1115IdfDeinitI2c() {
  if (gTransport.dev != nullptr) {
    (void)i2c_master_bus_rm_device(gTransport.dev);
    gTransport.dev = nullptr;
  }
  if (gTransport.bus != nullptr) {
    (void)i2c_del_master_bus(gTransport.bus);
    gTransport.bus = nullptr;
  }
}

esp_err_t ads1115IdfLastError() {
  return gTransport.lastError;
}

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
  transport->lastError = i2c_master_transmit(transport->dev, data, len, timeout);
  return mapEspError(transport->lastError);
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
  transport->lastError = i2c_master_transmit_receive(transport->dev, txData, txLen,
                                                     rxData, rxLen, timeout);
  return mapEspError(transport->lastError);
}

uint32_t ads1115IdfNowMs(void*) {
  return static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
}

void ads1115IdfYield(void*) {
  taskYIELD();
}
