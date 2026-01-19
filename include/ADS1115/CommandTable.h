/// @file CommandTable.h
/// @brief Register addresses and bit definitions for ADS1115
#pragma once

#include <cstdint>

namespace ADS1115 {
namespace cmd {

// ============================================================================
// Register Addresses (pointer register)
// ============================================================================

static constexpr uint8_t REG_CONVERSION = 0x00;  ///< Conversion result (16-bit, read-only)
static constexpr uint8_t REG_CONFIG     = 0x01;  ///< Configuration register (16-bit, R/W)
static constexpr uint8_t REG_LO_THRESH  = 0x02;  ///< Low threshold (16-bit, R/W)
static constexpr uint8_t REG_HI_THRESH  = 0x03;  ///< High threshold (16-bit, R/W)

// ============================================================================
// Default Register Values
// ============================================================================

static constexpr uint16_t CONVERSION_DEFAULT = 0x0000;
static constexpr uint16_t CONFIG_DEFAULT     = 0x8583;
static constexpr uint16_t LO_THRESH_DEFAULT  = 0x8000;
static constexpr uint16_t HI_THRESH_DEFAULT  = 0x7FFF;

// ============================================================================
// Register Field Masks
// ============================================================================

static constexpr uint16_t MASK_CONVERSION = 0xFFFF;
static constexpr uint16_t MASK_LO_THRESH  = 0xFFFF;
static constexpr uint16_t MASK_HI_THRESH  = 0xFFFF;

static constexpr uint16_t MASK_OS        = 0x8000;
static constexpr uint16_t MASK_MUX       = 0x7000;
static constexpr uint16_t MASK_PGA       = 0x0E00;
static constexpr uint16_t MASK_MODE      = 0x0100;
static constexpr uint16_t MASK_DR        = 0x00E0;
static constexpr uint16_t MASK_COMP_MODE = 0x0010;
static constexpr uint16_t MASK_COMP_POL  = 0x0008;
static constexpr uint16_t MASK_COMP_LAT  = 0x0004;
static constexpr uint16_t MASK_COMP_QUE  = 0x0003;

// ============================================================================
// Bit Positions
// ============================================================================

static constexpr uint8_t BIT_OS        = 15;  ///< Operational status / single-shot start
static constexpr uint8_t BIT_MUX       = 12;  ///< Mux (3 bits: 14:12)
static constexpr uint8_t BIT_PGA       = 9;   ///< PGA (3 bits: 11:9)
static constexpr uint8_t BIT_MODE      = 8;   ///< Mode (1 bit)
static constexpr uint8_t BIT_DR        = 5;   ///< Data rate (3 bits: 7:5)
static constexpr uint8_t BIT_COMP_MODE = 4;   ///< Comparator mode (1 bit)
static constexpr uint8_t BIT_COMP_POL  = 3;   ///< Comparator polarity (1 bit)
static constexpr uint8_t BIT_COMP_LAT  = 2;   ///< Comparator latch (1 bit)
static constexpr uint8_t BIT_COMP_QUE  = 0;   ///< Comparator queue (2 bits: 1:0)

// ============================================================================
// Field Values
// ============================================================================

// OS bit meanings
static constexpr uint16_t OS_BUSY  = 0x0000;  ///< Read: conversion in progress
static constexpr uint16_t OS_IDLE  = 0x8000;  ///< Read: no conversion in progress
static constexpr uint16_t OS_START = 0x8000;  ///< Write: start single conversion

// MUX field values
static constexpr uint16_t MUX_AIN0_AIN1 = 0x0000;
static constexpr uint16_t MUX_AIN0_AIN3 = 0x1000;
static constexpr uint16_t MUX_AIN1_AIN3 = 0x2000;
static constexpr uint16_t MUX_AIN2_AIN3 = 0x3000;
static constexpr uint16_t MUX_AIN0_GND  = 0x4000;
static constexpr uint16_t MUX_AIN1_GND  = 0x5000;
static constexpr uint16_t MUX_AIN2_GND  = 0x6000;
static constexpr uint16_t MUX_AIN3_GND  = 0x7000;

// PGA field values
static constexpr uint16_t PGA_6_144V = 0x0000;
static constexpr uint16_t PGA_4_096V = 0x0200;
static constexpr uint16_t PGA_2_048V = 0x0400;
static constexpr uint16_t PGA_1_024V = 0x0600;
static constexpr uint16_t PGA_0_512V = 0x0800;
static constexpr uint16_t PGA_0_256V = 0x0A00;

// MODE field values
static constexpr uint16_t MODE_CONTINUOUS  = 0x0000;
static constexpr uint16_t MODE_SINGLE_SHOT = 0x0100;

// DR field values
static constexpr uint16_t DR_8SPS   = 0x0000;
static constexpr uint16_t DR_16SPS  = 0x0020;
static constexpr uint16_t DR_32SPS  = 0x0040;
static constexpr uint16_t DR_64SPS  = 0x0060;
static constexpr uint16_t DR_128SPS = 0x0080;
static constexpr uint16_t DR_250SPS = 0x00A0;
static constexpr uint16_t DR_475SPS = 0x00C0;
static constexpr uint16_t DR_860SPS = 0x00E0;

// COMP_MODE field values
static constexpr uint16_t COMP_MODE_TRADITIONAL = 0x0000;
static constexpr uint16_t COMP_MODE_WINDOW      = 0x0010;

// COMP_POL field values
static constexpr uint16_t COMP_POL_ACTIVE_LOW  = 0x0000;
static constexpr uint16_t COMP_POL_ACTIVE_HIGH = 0x0008;

// COMP_LAT field values
static constexpr uint16_t COMP_LAT_NON_LATCHING = 0x0000;
static constexpr uint16_t COMP_LAT_LATCHING     = 0x0004;

// COMP_QUE field values
static constexpr uint16_t COMP_QUE_ASSERT_1 = 0x0000;
static constexpr uint16_t COMP_QUE_ASSERT_2 = 0x0001;
static constexpr uint16_t COMP_QUE_ASSERT_4 = 0x0002;
static constexpr uint16_t COMP_QUE_DISABLE  = 0x0003;

} // namespace cmd
} // namespace ADS1115
