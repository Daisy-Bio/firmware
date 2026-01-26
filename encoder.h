/**
 * @file encoder.h
 * @brief Rotary encoder and button handling interface
 */

#ifndef ENCODER_H
#define ENCODER_H

#include <Arduino.h>

/**
 * @brief Button event types
 */
enum ButtonEvent { BE_NONE, BE_SINGLE, BE_DOUBLE };

/**
 * @brief Encoder ISR - call from attachInterrupt
 */
void IRAM_ATTR encoderISR();

/**
 * @brief Read button event (debounced, single/double click detection)
 */
ButtonEvent readButtonEvent();

/**
 * @brief Get the number of detent steps since last call
 */
int32_t getDetentDelta();

/**
 * @brief Initialize encoder state (call in setup before attachInterrupt)
 */
void initEncoderState();

// Encoder state - exposed for ISR access
extern volatile int32_t g_pulseCount;
extern volatile uint8_t g_lastAB;

#endif // ENCODER_H
