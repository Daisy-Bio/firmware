/**
 * @file oled.h
 * @brief US2066 OLED display driver interface
 */

#ifndef OLED_H
#define OLED_H

#include <Arduino.h>

/**
 * @brief Write data to the OLED via I2C
 */
void oledWrite(uint8_t control, const uint8_t *data, size_t len);

/**
 * @brief Send a command byte to the OLED
 */
void oledCmd(uint8_t c);

/**
 * @brief Send a data byte to the OLED
 */
void oledData(uint8_t d);

/**
 * @brief Set the cursor position on the OLED
 */
void oledSetCursor(uint8_t col, uint8_t row);

/**
 * @brief Clear the display and return cursor to home
 */
void oledClearAndHome();

/**
 * @brief Perform hardware reset of the OLED
 */
void oledHardwareReset();

/**
 * @brief Initialize the OLED once
 */
void oledInitOnce();

/**
 * @brief Full deterministic initialization sequence
 */
void oledInitDeterministic();

/**
 * @brief Write a line of text to the specified row (0-3)
 */
void oledWriteLine(uint8_t row, const char *text);

#endif // OLED_H
