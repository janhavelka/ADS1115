#pragma once

#include <cstddef>
#include <cstdint>

#include "ADS1115/Config.h"
#include "driver/i2c_master.h"
#include "esp_err.h"

struct Ads1115IdfI2cTransport {
  i2c_master_bus_handle_t bus = nullptr;
  i2c_master_dev_handle_t dev = nullptr;
  uint8_t address = 0x48;
  esp_err_t lastError = ESP_OK;
};

Ads1115IdfI2cTransport& ads1115IdfTransportContext();
bool ads1115IdfInitI2c(int sda, int scl, uint32_t freqHz, uint16_t timeoutMs,
                       uint8_t address);
void ads1115IdfDeinitI2c();
esp_err_t ads1115IdfLastError();

ADS1115::Status ads1115IdfWrite(uint8_t addr, const uint8_t* data, size_t len,
                                uint32_t timeoutMs, void* user);
ADS1115::Status ads1115IdfWriteRead(uint8_t addr, const uint8_t* txData, size_t txLen,
                                    uint8_t* rxData, size_t rxLen, uint32_t timeoutMs,
                                    void* user);
uint32_t ads1115IdfNowMs(void* user);
void ads1115IdfYield(void* user);
