/**
 * @file ui_state.h
 * @brief UI state machine and global state variables
 */

#ifndef UI_STATE_H
#define UI_STATE_H

#include "test_params.h"
#include <Arduino.h>


// ================== UI STATE MACHINE ==================
enum class UIState : uint8_t {
  MAIN_MENU,
  RUN_TEST,
  LIVE_TEST,
  CREATE_TEST_MENU,
  CREATE_EDIT_PARAM,
  SAVE_TEST_NAME,
  SD_TEST_LIST,
  SD_TEST_ACTIONS,
  SD_TEST_VIEW,
};

// Current state
extern UIState g_state;

// ================== GLOBAL PARAMS ==================
extern TestParams g_createParams;
extern TestParams g_activeParams;

// ================== MENU STATE ==================

// Main menu
extern const char *MAIN_MENU_ITEMS[];
static constexpr int MAIN_MENU_COUNT = 3;
extern int g_mainMenuIndex;

// Run Test menu
extern int g_runTestMenuIndex;

// Live Test
extern unsigned long g_liveTestStartMs;
extern unsigned long g_liveLastDrawMs;
extern char g_liveTestName[17];
extern char g_livePhase[13];
extern int g_liveTargetC;
extern int g_liveActualC;

// Create Test menu
extern const char *CREATE_MENU_ITEMS[];
static constexpr int CREATE_MENU_COUNT = 13;
extern int g_createMenuIndex;
extern int g_createMenuTop;

// Edit state
extern int g_editMenuIndex;
extern int *g_editPtr;
extern int g_editMin;
extern int g_editMax;
extern const char *g_editUnit;
extern char g_editLabel[21];

// SD test list
extern int g_sdMenuIndex;
extern int g_sdMenuTop;

// Selected test context
extern int g_selectedTestIdx;
extern char g_selectedTestName[21];
extern char g_selectedTestFile[32];

// Actions menu
extern const char *SD_ACTION_ITEMS[];
static constexpr int SD_ACTION_COUNT = 4;
extern int g_sdActionIndex;
extern int g_sdActionTop;

// View menu
extern TestParams g_viewParams;
static constexpr int VIEW_ITEM_MAX = 16;
extern char g_viewLines[VIEW_ITEM_MAX][21];
extern int g_viewCount;
extern int g_viewIndex;
extern int g_viewTop;

// Save name entry
extern char g_nameBuf[17];
extern uint8_t g_namePos;
extern bool g_nameEditMode;
extern bool g_nameAccepted;
extern uint8_t g_saveNameMenuIndex;
extern uint32_t g_nameBlinkMs;
extern bool g_nameBlinkOn;

// ================== FUNCTIONS ==================

/**
 * @brief Enter a new UI state
 */
void enterState(UIState s);

/**
 * @brief Reset name entry state
 */
void resetNameEntry();

/**
 * @brief Setup edit for create menu index
 */
bool setupEditForCreateMenuIndex(int menuIdx);

/**
 * @brief Build view lines from params
 */
void buildViewLinesFromParams(const TestParams &p);

/**
 * @brief Clamp menu top for scrolling
 */
void clampMenuTopN(int &top, int index, int count, int visibleRows);

/**
 * @brief Get SD menu item count including Back
 */
int sdMenuItemCountIncludingBack();

/**
 * @brief Check if Back is selected in SD menu
 */
bool sdIsBackSelected();

#endif // UI_STATE_H
