/**
 * @file Mira_Firmware.ino
 * @brief Main firmware for Mira PCR device
 *
 * Modular refactor - all functionality split into separate modules:
 * - config.h: Pin definitions and constants
 * - test_params.h: PCR test parameter structure
 * - oled.h/cpp: US2066 OLED display driver
 * - encoder.h/cpp: Rotary encoder and button handling
 * - sd_storage.h/cpp: SD card operations
 * - ui_state.h/cpp: UI state machine
 * - ui_draw.h/cpp: Screen drawing functions
 */

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <Wire.h>
#include <string.h>


// Optional: reduce current spikes/noise during boot
#include <WiFi.h>
#include <esp_bt.h>

// Module includes
#include "config.h"
#include "encoder.h"
#include "oled.h"
#include "sd_storage.h"
#include "test_params.h"
#include "ui_draw.h"
#include "ui_state.h"


// ================== SETUP ==================
void setup() {
  // Power-up stabilization delay
  delay(2000);

  // Reduce current spikes/noise during boot
  WiFi.mode(WIFI_OFF);
  btStop();

  Serial.begin(115200);
  delay(200);

  // Initialize OLED
  oledInitDeterministic();

  // Initialize encoder pins
  pinMode(ENCODER_A_PIN, INPUT_PULLUP);
  pinMode(ENCODER_B_PIN, INPUT_PULLUP);
  pinMode(ENCODER_SW_PIN, INPUT_PULLUP);

  initEncoderState();
  attachInterrupt(digitalPinToInterrupt(ENCODER_A_PIN), encoderISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENCODER_B_PIN), encoderISR, CHANGE);

  // Initialize SD card
  g_sdOK = initSDCard();
  ensureTestsDir();

  // Initialize params to defaults
  g_createParams = TestParams();
  g_activeParams = g_createParams;

  // Enter main menu
  enterState(UIState::MAIN_MENU);
}

// ================== LOOP ==================
void loop() {
  const int32_t delta = getDetentDelta();
  const ButtonEvent btn = readButtonEvent();

  // ---------------- SAVE NAME BLINK REFRESH ----------------
  static uint32_t lastNameBlinkMs = 0;
  if (g_state == UIState::SAVE_TEST_NAME && !g_nameAccepted) {
    uint32_t now = millis();
    if (now - lastNameBlinkMs >= 350) {
      lastNameBlinkMs = now;
      g_nameBlinkOn = !g_nameBlinkOn;
      drawSaveNameScreen();
    }
  }

  // ---------------- LIVE TEST REFRESH ----------------
  if (g_state == UIState::LIVE_TEST) {
    uint32_t now = millis();
    if (g_liveLastDrawMs == 0 || (now - g_liveLastDrawMs) >= 1000) {
      g_liveLastDrawMs = now;

      // Placeholder: simple dummy actual temp "ramps" toward target
      if (g_liveActualC < g_liveTargetC)
        g_liveActualC++;
      else if (g_liveActualC > g_liveTargetC)
        g_liveActualC--;

      drawLiveTestScreen();
    }
  }

  // ---------------- ROTATION ----------------
  if (delta != 0) {
    int32_t steps = delta;
    while (steps != 0) {
      const int step = (steps > 0) ? 1 : -1;
      steps -= step;

      if (g_state == UIState::MAIN_MENU) {
        int ni = (int)g_mainMenuIndex + step;
        if (ni < 0)
          ni = 0;
        if (ni > 2)
          ni = 2;
        if (ni != (int)g_mainMenuIndex) {
          g_mainMenuIndex = ni;
          drawMainMenu();
        }
      } else if (g_state == UIState::RUN_TEST) {
        int ni = (int)g_runTestMenuIndex + step;
        if (ni < 0)
          ni = 0;
        if (ni > 1)
          ni = 1;
        if (ni != (int)g_runTestMenuIndex) {
          g_runTestMenuIndex = ni;
          drawRunTestScreen();
        }
      } else if (g_state == UIState::LIVE_TEST) {
        // Single item; rotation does nothing
      } else if (g_state == UIState::CREATE_TEST_MENU) {
        int ni = (int)g_createMenuIndex + step;
        if (ni < 0)
          ni = 0;
        if (ni > CREATE_MENU_COUNT - 1)
          ni = CREATE_MENU_COUNT - 1;
        if (ni != (int)g_createMenuIndex) {
          g_createMenuIndex = ni;
          drawCreateTestMenu();
        }
      } else if (g_state == UIState::CREATE_EDIT_PARAM) {
        if (g_editPtr) {
          int v = *g_editPtr;
          v += step;
          v = clampInt(v, g_editMin, g_editMax);
          *g_editPtr = v;
          drawCreateEditScreen();
        }
      } else if (g_state == UIState::SAVE_TEST_NAME) {
        if (!g_nameAccepted) {
          if (!g_nameEditMode) {
            int np = (int)g_namePos + step;
            if (np < 0)
              np = 0;
            if (np > 15)
              np = 15;
            g_namePos = (uint8_t)np;
          } else {
            char cur = g_nameBuf[g_namePos];
            int idx = 0;
            for (int i = 0; i < NAME_CHARS_COUNT; i++) {
              if (NAME_CHARS[i] == cur) {
                idx = i;
                break;
              }
            }
            idx += step;
            if (idx < 0)
              idx = NAME_CHARS_COUNT - 1;
            if (idx >= NAME_CHARS_COUNT)
              idx = 0;
            g_nameBuf[g_namePos] = NAME_CHARS[idx];
          }
          drawSaveNameScreen();
        } else {
          int ni = (int)g_saveNameMenuIndex + step;
          if (ni < 0)
            ni = 0;
          if (ni > 1)
            ni = 1;
          g_saveNameMenuIndex = (uint8_t)ni;
          drawSaveNameScreen();
        }
      } else if (g_state == UIState::SD_TEST_LIST) {
        int count = sdMenuItemCountIncludingBack();
        int ni = (int)g_sdMenuIndex + step;
        if (ni < 0)
          ni = 0;
        if (ni > count - 1)
          ni = count - 1;
        if (ni != (int)g_sdMenuIndex) {
          g_sdMenuIndex = ni;
          drawSDTestListMenu();
        }
      } else if (g_state == UIState::SD_TEST_ACTIONS) {
        int ni = (int)g_sdActionIndex + step;
        if (ni < 0)
          ni = 0;
        if (ni > SD_ACTION_COUNT - 1)
          ni = SD_ACTION_COUNT - 1;
        if (ni != (int)g_sdActionIndex) {
          g_sdActionIndex = ni;
          drawSDTestActionsMenu();
        }
      } else if (g_state == UIState::SD_TEST_VIEW) {
        int ni = (int)g_viewIndex + step;
        if (ni < 0)
          ni = 0;
        if (ni > g_viewCount - 1)
          ni = g_viewCount - 1;
        if (ni != (int)g_viewIndex) {
          g_viewIndex = ni;
          drawViewMenu();
        }
      }
    }
  }

  // ---------------- BUTTON ----------------
  if (btn != BE_NONE) {
    if (g_state == UIState::MAIN_MENU) {
      if (g_mainMenuIndex == 0) {
        enterState(UIState::RUN_TEST);
      } else if (g_mainMenuIndex == 1) {
        g_createMenuIndex = 0;
        g_createMenuTop = 0;
        enterState(UIState::CREATE_TEST_MENU);
      } else {
        enterState(UIState::SD_TEST_LIST);
      }
    } else if (g_state == UIState::RUN_TEST) {
      if (btn == BE_SINGLE) {
        if (g_runTestMenuIndex == 0) {
          enterState(UIState::LIVE_TEST);
        } else {
          enterState(UIState::MAIN_MENU);
        }
      }
    } else if (g_state == UIState::LIVE_TEST) {
      if (btn == BE_SINGLE) {
        enterState(UIState::RUN_TEST);
      }
    } else if (g_state == UIState::CREATE_TEST_MENU) {
      const char *item = CREATE_MENU_ITEMS[g_createMenuIndex];
      if (strcmp(item, "Back") == 0) {
        enterState(UIState::MAIN_MENU);
      } else if (strcmp(item, "Save Test") == 0) {
        resetNameEntry();
        enterState(UIState::SAVE_TEST_NAME);
      } else {
        if (setupEditForCreateMenuIndex(g_createMenuIndex)) {
          enterState(UIState::CREATE_EDIT_PARAM);
        }
      }
    } else if (g_state == UIState::SAVE_TEST_NAME) {
      if (!g_nameAccepted) {
        if (btn == BE_SINGLE) {
          g_nameEditMode = !g_nameEditMode;
          drawSaveNameScreen();
        } else if (btn == BE_DOUBLE) {
          g_nameAccepted = true;
          g_nameEditMode = false;
          g_saveNameMenuIndex = 0;
          drawSaveNameScreen();
        }
      } else {
        if (btn == BE_SINGLE) {
          if (g_saveNameMenuIndex == 0) {
            bool ok = saveTestParamsToSD(g_nameBuf, g_createParams);
            enterState(ok ? UIState::MAIN_MENU : UIState::CREATE_TEST_MENU);
          } else {
            enterState(UIState::CREATE_TEST_MENU);
          }
        } else if (btn == BE_DOUBLE) {
          g_nameAccepted = false;
          g_nameEditMode = false;
          drawSaveNameScreen();
        }
      }
    } else if (g_state == UIState::CREATE_EDIT_PARAM) {
      enterState(UIState::CREATE_TEST_MENU);
    } else if (g_state == UIState::SD_TEST_LIST) {
      if (sdIsBackSelected()) {
        enterState(UIState::MAIN_MENU);
      } else if (g_sdTestCount > 0) {
        g_selectedTestIdx = g_sdMenuIndex;

        strncpy(g_selectedTestName, g_sdTestNames[g_selectedTestIdx],
                sizeof(g_selectedTestName) - 1);
        g_selectedTestName[sizeof(g_selectedTestName) - 1] = '\0';

        strncpy(g_selectedTestFile, g_sdTestFiles[g_selectedTestIdx],
                sizeof(g_selectedTestFile) - 1);
        g_selectedTestFile[sizeof(g_selectedTestFile) - 1] = '\0';

        enterState(UIState::SD_TEST_ACTIONS);
      }
    } else if (g_state == UIState::SD_TEST_ACTIONS) {
      const char *item = SD_ACTION_ITEMS[g_sdActionIndex];

      if (strcmp(item, "Back") == 0) {
        enterState(UIState::SD_TEST_LIST);
      } else if (strcmp(item, "Delete Test") == 0) {
        deleteTestFile(g_selectedTestFile);
        enterState(UIState::SD_TEST_LIST);
      } else if (strcmp(item, "View Test") == 0) {
        enterState(UIState::SD_TEST_VIEW);
      } else if (strcmp(item, "Load Test") == 0) {
        TestParams tp;
        if (loadSelectedTestForView(tp, g_selectedTestFile)) {
          g_activeParams = tp;
          g_createParams = tp;
        }
        enterState(UIState::SD_TEST_LIST);
      }
    } else if (g_state == UIState::SD_TEST_VIEW) {
      if (g_viewIndex == g_viewCount - 1) {
        enterState(UIState::SD_TEST_ACTIONS);
      }
    }
  }

  delay(5);
}
