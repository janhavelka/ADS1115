#include <cinttypes>

#include "ADS1115/ADS1115.h"
#include "Ads1115IdfI2cTransport.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {

constexpr char TAG[] = "ads1115_basic";
constexpr gpio_num_t I2C_SDA = GPIO_NUM_8;
constexpr gpio_num_t I2C_SCL = GPIO_NUM_9;
constexpr uint32_t I2C_FREQ_HZ = 400000;
constexpr uint8_t ADS1115_ADDR = 0x48;

void logStatus(const char* label, const ADS1115::Status& status) {
  ESP_LOGI(TAG, "%s: code=%u detail=%" PRId32 " msg=%s",
           label, static_cast<unsigned>(status.code), status.detail, status.msg);
}

}  // namespace

extern "C" void app_main() {
  i2c_master_bus_config_t busConfig = {};
  busConfig.i2c_port = I2C_NUM_0;
  busConfig.sda_io_num = I2C_SDA;
  busConfig.scl_io_num = I2C_SCL;
  busConfig.clk_source = I2C_CLK_SRC_DEFAULT;
  busConfig.glitch_ignore_cnt = 7;
  busConfig.flags.enable_internal_pullup = true;

  Ads1115IdfI2cTransport transport;
  esp_err_t err = i2c_new_master_bus(&busConfig, &transport.bus);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "i2c_new_master_bus failed: %s", esp_err_to_name(err));
    return;
  }

  i2c_device_config_t devConfig = {};
  devConfig.dev_addr_length = I2C_ADDR_BIT_LEN_7;
  devConfig.device_address = ADS1115_ADDR;
  devConfig.scl_speed_hz = I2C_FREQ_HZ;

  err = i2c_master_bus_add_device(transport.bus, &devConfig, &transport.dev);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "i2c_master_bus_add_device failed: %s", esp_err_to_name(err));
    (void)i2c_del_master_bus(transport.bus);
    return;
  }
  transport.address = ADS1115_ADDR;

  ADS1115::ADS1115 adc;
  ADS1115::Config cfg;
  cfg.i2cWrite = ads1115IdfWrite;
  cfg.i2cWriteRead = ads1115IdfWriteRead;
  cfg.i2cUser = &transport;
  cfg.nowMs = ads1115IdfNowMs;
  cfg.cooperativeYield = ads1115IdfYield;
  cfg.i2cAddress = ADS1115_ADDR;
  cfg.i2cTimeoutMs = 50;
  cfg.mux = ADS1115::Mux::AIN0_GND;
  cfg.gain = ADS1115::Gain::FSR_2_048V;
  cfg.dataRate = ADS1115::DataRate::SPS_128;
  cfg.mode = ADS1115::Mode::SINGLE_SHOT;

  ADS1115::Status st = adc.begin(cfg);
  if (!st.ok()) {
    logStatus("ADS1115 begin failed", st);
    (void)i2c_master_bus_rm_device(transport.dev);
    (void)i2c_del_master_bus(transport.bus);
    return;
  }

  int16_t raw = 0;
  st = adc.readBlocking(raw, 250);
  if (st.ok()) {
    ESP_LOGI(TAG, "raw=%d voltage=%.6f V", static_cast<int>(raw),
             static_cast<double>(adc.rawToVoltage(raw)));
  } else {
    logStatus("readBlocking failed", st);
  }

  st = adc.startConversion();
  if (st.inProgress()) {
    bool ready = false;
    const uint32_t startMs = ads1115IdfNowMs(nullptr);
    while (!ready && (ads1115IdfNowMs(nullptr) - startMs) < 250U) {
      adc.tick(ads1115IdfNowMs(nullptr));
      st = adc.readConversionReady(ready);
      if (!st.ok()) {
        logStatus("readConversionReady failed", st);
        break;
      }
      vTaskDelay(pdMS_TO_TICKS(1));
    }
    if (ready && adc.readRaw(raw).ok()) {
      ESP_LOGI(TAG, "tick-driven raw=%d voltage=%.6f V", static_cast<int>(raw),
               static_cast<double>(adc.rawToVoltage(raw)));
    }
  }

  adc.end();
  (void)i2c_master_bus_rm_device(transport.dev);
  (void)i2c_del_master_bus(transport.bus);
}
