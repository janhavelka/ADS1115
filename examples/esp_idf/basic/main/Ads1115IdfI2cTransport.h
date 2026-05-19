#pragma once

#include <cstddef>
#include <cstdint>

#include "ADS1115/Config.h"
#include "driver/i2c_master.h"

struct Ads1115IdfI2cTransport {
  i2c_master_bus_handle_t bus = nullptr;
  i2c_master_dev_handle_t dev = nullptr;
  uint8_t address = 0x48;
};

ADS1115::Status ads1115IdfWrite(uint8_t addr, const uint8_t* data, size_t len,
                                uint32_t timeoutMs, void* user);
ADS1115::Status ads1115IdfWriteRead(uint8_t addr, const uint8_t* txData, size_t txLen,
                                    uint8_t* rxData, size_t rxLen, uint32_t timeoutMs,
                                    void* user);
uint32_t ads1115IdfNowMs(void* user);
void ads1115IdfYield(void* user);
