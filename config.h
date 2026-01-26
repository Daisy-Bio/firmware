/**
 * @file config.h
 * @brief Hardware pin definitions and constants for Mira PCR device
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ================== PIN DEFINITIONS ==================

// Rotary Encoder
static constexpr uint8_t ENCODER_A_PIN = 33;
static constexpr uint8_t ENCODER_B_PIN = 32;
static constexpr uint8_t ENCODER_SW_PIN = 25;

// I2C (OLED)
static constexpr uint8_t I2C_SDA_PIN = 21;
static constexpr uint8_t I2C_SCL_PIN = 22;
static constexpr int OLED_RESET_PIN = 13;
static constexpr uint8_t OLED_ADDR = 0x3C;

// SD Card (SPI)
static constexpr uint8_t SD_CS_PIN = 2;
static constexpr uint8_t SD_MOSI_PIN = 17;
static constexpr uint8_t SD_MISO_PIN = 19;
static constexpr uint8_t SD_SCLK_PIN = 18;

// ================== OLED CONSTANTS ==================
static constexpr uint8_t CTRL_CMD = 0x00;
static constexpr uint8_t CTRL_DATA = 0x40;
static const uint8_t ROW_ADDR[4] = {0x00, 0x20, 0x40, 0x60};

// ================== ENCODER CONSTANTS ==================
static constexpr int PULSES_PER_DETENT = 8;

// ================== NAME ENTRY CONSTANTS ==================
static const char *NAME_CHARS = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_()[]";
static constexpr uint8_t NAME_CHARS_COUNT = 42;

#endif // CONFIG_H
