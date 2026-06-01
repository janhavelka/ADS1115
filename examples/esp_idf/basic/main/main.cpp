#include <cstddef>
#include <cstdint>
#include <cstdio>

#include "ADS1115/ADS1115.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

namespace {

static constexpr i2c_port_num_t I2C_PORT = I2C_NUM_0;
static constexpr gpio_num_t I2C_SDA = GPIO_NUM_8;
static constexpr gpio_num_t I2C_SCL = GPIO_NUM_9;
static constexpr uint32_t I2C_FREQ_HZ = 400000U;
static constexpr uint32_t I2C_TIMEOUT_MS = 50U;
static constexpr uint8_t ADS1115_ADDR = 0x48U;

struct I2cBusContext {
  i2c_master_bus_handle_t bus = nullptr;
  i2c_master_dev_handle_t dev = nullptr;
  SemaphoreHandle_t lock = nullptr;
};

I2cBusContext busContext;
ADS1115::ADS1115 adc;

// ESP-IDF's master I2C API reports broad esp_err_t values here; this example
// cannot prove address-NACK versus data-NACK precision. Production adapters
// should refine this mapping only when their platform/driver exposes reliable
// fault classification.
ADS1115::Status mapEspError(esp_err_t err) {
  if (err == ESP_OK) {
    return ADS1115::Status::Ok();
  }
  if (err == ESP_ERR_TIMEOUT) {
    return ADS1115::Status::Error(ADS1115::Err::I2C_TIMEOUT, "ESP-IDF I2C timeout", err);
  }
  if (err == ESP_ERR_INVALID_STATE || err == ESP_ERR_INVALID_ARG) {
    return ADS1115::Status::Error(ADS1115::Err::I2C_BUS, "ESP-IDF I2C bus error", err);
  }
  return ADS1115::Status::Error(ADS1115::Err::I2C_ERROR, "ESP-IDF I2C error", err);
}

bool lockBus(I2cBusContext& ctx, uint32_t timeoutMs) {
  if (ctx.lock == nullptr) {
    return false;
  }
  return xSemaphoreTake(ctx.lock, pdMS_TO_TICKS(timeoutMs)) == pdTRUE;
}

void unlockBus(I2cBusContext& ctx) {
  if (ctx.lock != nullptr) {
    xSemaphoreGive(ctx.lock);
  }
}

ADS1115::Status idfWrite(uint8_t addr, const uint8_t* data, size_t len,
                         uint32_t timeoutMs, void* user) {
  I2cBusContext* ctx = static_cast<I2cBusContext*>(user);
  if (ctx == nullptr || ctx->dev == nullptr || data == nullptr || len == 0) {
    return ADS1115::Status::Error(ADS1115::Err::INVALID_PARAM, "Invalid IDF write");
  }
  if (addr != ADS1115_ADDR) {
    return ADS1115::Status::Error(ADS1115::Err::INVALID_CONFIG, "Unexpected I2C address", addr);
  }
  if (!lockBus(*ctx, timeoutMs)) {
    return ADS1115::Status::Error(ADS1115::Err::I2C_TIMEOUT, "I2C lock timeout");
  }
  const esp_err_t err = i2c_master_transmit(ctx->dev, data, len, static_cast<int>(timeoutMs));
  unlockBus(*ctx);
  return mapEspError(err);
}

ADS1115::Status idfWriteRead(uint8_t addr, const uint8_t* txData, size_t txLen,
                             uint8_t* rxData, size_t rxLen, uint32_t timeoutMs,
                             void* user) {
  I2cBusContext* ctx = static_cast<I2cBusContext*>(user);
  if (ctx == nullptr || ctx->dev == nullptr || txData == nullptr || txLen == 0 ||
      rxData == nullptr || rxLen == 0) {
    return ADS1115::Status::Error(ADS1115::Err::INVALID_PARAM, "Invalid IDF write-read");
  }
  if (addr != ADS1115_ADDR) {
    return ADS1115::Status::Error(ADS1115::Err::INVALID_CONFIG, "Unexpected I2C address", addr);
  }
  if (!lockBus(*ctx, timeoutMs)) {
    return ADS1115::Status::Error(ADS1115::Err::I2C_TIMEOUT, "I2C lock timeout");
  }
  const esp_err_t err = i2c_master_transmit_receive(ctx->dev, txData, txLen, rxData, rxLen,
                                                    static_cast<int>(timeoutMs));
  unlockBus(*ctx);
  return mapEspError(err);
}

uint32_t idfNowMs(void*) {
  return static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
}

void idfYield(void*) {
  taskYIELD();
}

bool initBus() {
  busContext.lock = xSemaphoreCreateMutex();
  if (busContext.lock == nullptr) {
    return false;
  }

  i2c_master_bus_config_t busConfig = {};
  busConfig.i2c_port = I2C_PORT;
  busConfig.sda_io_num = I2C_SDA;
  busConfig.scl_io_num = I2C_SCL;
  busConfig.clk_source = I2C_CLK_SRC_DEFAULT;
  busConfig.glitch_ignore_cnt = 7;
  // Example convenience only. Production boards should size external pull-ups
  // for bus capacitance, speed, voltage domain, and sink-current limits.
  busConfig.flags.enable_internal_pullup = true;

  esp_err_t err = i2c_new_master_bus(&busConfig, &busContext.bus);
  if (err != ESP_OK) {
    std::printf("i2c_new_master_bus failed: %s\n", esp_err_to_name(err));
    return false;
  }

  i2c_device_config_t devConfig = {};
  devConfig.dev_addr_length = I2C_ADDR_BIT_LEN_7;
  devConfig.device_address = ADS1115_ADDR;
  devConfig.scl_speed_hz = I2C_FREQ_HZ;

  err = i2c_master_bus_add_device(busContext.bus, &devConfig, &busContext.dev);
  if (err != ESP_OK) {
    std::printf("i2c_master_bus_add_device failed: %s\n", esp_err_to_name(err));
    return false;
  }
  return true;
}

}  // namespace

extern "C" void app_main(void) {
  if (!initBus()) {
    std::printf("ADS1115 IDF example: I2C init failed\n");
    return;
  }

  ADS1115::Config cfg;
  cfg.i2cWrite = idfWrite;
  cfg.i2cWriteRead = idfWriteRead;
  cfg.i2cUser = &busContext;
  cfg.nowMs = idfNowMs;
  cfg.cooperativeYield = idfYield;
  cfg.i2cAddress = ADS1115_ADDR;
  cfg.i2cTimeoutMs = I2C_TIMEOUT_MS;
  cfg.strictInitVerify = true;

  ADS1115::Status st = adc.begin(cfg);
  if (!st.ok()) {
    std::printf("ADS1115 begin failed: code=%u detail=%ld msg=%s\n",
                static_cast<unsigned>(st.code), static_cast<long>(st.detail), st.msg);
    return;
  }

  while (true) {
    adc.tick(idfNowMs(nullptr));
    int16_t raw = 0;
    st = adc.readBlocking(raw, 200);
    if (st.ok()) {
      std::printf("ADS1115 raw=%d volts=%.6f\n", static_cast<int>(raw),
                  static_cast<double>(adc.rawToVoltage(raw)));
    } else {
      std::printf("ADS1115 read failed: code=%u detail=%ld msg=%s\n",
                  static_cast<unsigned>(st.code), static_cast<long>(st.detail), st.msg);
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
