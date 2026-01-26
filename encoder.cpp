/**
 * @file encoder.cpp
 * @brief Rotary encoder and button handling implementation
 */

#include "encoder.h"
#include "config.h"

// Encoder state
volatile int32_t g_pulseCount = 0;
volatile uint8_t g_lastAB = 0;

// Quadrature state table
static const int8_t QDELTA[16] = {0,  -1, +1, 0,  +1, 0,  0,  -1,
                                  -1, 0,  0,  +1, 0,  +1, -1, 0};

void IRAM_ATTR encoderISR() {
  uint8_t a = (uint8_t)digitalRead(ENCODER_A_PIN);
  uint8_t b = (uint8_t)digitalRead(ENCODER_B_PIN);
  uint8_t newAB = (a << 1) | b;
  uint8_t idx = (g_lastAB << 2) | newAB;
  g_pulseCount += QDELTA[idx];
  g_lastAB = newAB;
}

ButtonEvent readButtonEvent() {
  // Debounced press-edge + single/double click detector.
  // Returns BE_DOUBLE immediately on 2nd click.
  // Returns BE_SINGLE only after the double-click window expires.
  static bool lastStable = true;
  static bool lastRead = true;
  static uint32_t lastChangeMs = 0;

  static bool pendingSingle = false;
  static uint32_t firstClickMs = 0;
  static const uint32_t DOUBLE_MS = 350;

  bool now = digitalRead(ENCODER_SW_PIN);

  if (now != lastRead) {
    lastRead = now;
    lastChangeMs = millis();
  }

  // debounce 25ms
  if ((millis() - lastChangeMs) > 25) {
    if (now != lastStable) {
      lastStable = now;
      // Detect press (active-low)
      if (lastStable == false) {
        uint32_t t = millis();
        if (!pendingSingle) {
          pendingSingle = true;
          firstClickMs = t;
        } else {
          // second click
          if ((t - firstClickMs) <= DOUBLE_MS) {
            pendingSingle = false;
            return BE_DOUBLE;
          } else {
            // too late; treat as new first click
            pendingSingle = true;
            firstClickMs = t;
          }
        }
      }
    }
  }

  // If single-click pending and window expired, emit single
  if (pendingSingle && (millis() - firstClickMs) > DOUBLE_MS) {
    pendingSingle = false;
    return BE_SINGLE;
  }

  return BE_NONE;
}

int32_t getDetentDelta() {
  static int32_t lastDetent = 0;
  int32_t pulses;
  noInterrupts();
  pulses = g_pulseCount;
  interrupts();
  int32_t detent = pulses / PULSES_PER_DETENT;
  int32_t delta = detent - lastDetent;
  if (delta != 0)
    lastDetent = detent;
  return delta;
}

void initEncoderState() {
  g_lastAB = ((uint8_t)digitalRead(ENCODER_A_PIN) << 1) |
             (uint8_t)digitalRead(ENCODER_B_PIN);
}
