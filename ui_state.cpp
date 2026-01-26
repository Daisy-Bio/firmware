/**
 * @file ui_state.cpp
 * @brief UI state machine implementation
 */

#include "ui_state.h"
#include "config.h"
#include "sd_storage.h"
#include "ui_draw.h"


// Current state
UIState g_state = UIState::MAIN_MENU;

// Global params
TestParams g_createParams;
TestParams g_activeParams;

// Main menu
const char *MAIN_MENU_ITEMS[] = {"Run Test", "Create Test", "SD Card"};
int g_mainMenuIndex = 0;

// Run Test menu
int g_runTestMenuIndex = 0;

// Live Test
unsigned long g_liveTestStartMs = 0;
unsigned long g_liveLastDrawMs = 0;
char g_liveTestName[17] = "TEST";
char g_livePhase[13] = "Idle";
int g_liveTargetC = 95;
int g_liveActualC = 25;

// Create Test menu
const char *CREATE_MENU_ITEMS[] = {"Init Denat Temp",
                                   "Init Denat Time",
                                   "Denat Temp",
                                   "Denat Time",
                                   "Anneal Temp",
                                   "Anneal Time",
                                   "Extension Temp",
                                   "Extension Time",
                                   "Number of Cycles",
                                   "Final Ext Temp",
                                   "Final Ext Time",
                                   "Save Test",
                                   "Back"};
int g_createMenuIndex = 0;
int g_createMenuTop = 0;

// Edit state
int g_editMenuIndex = 0;
int *g_editPtr = nullptr;
int g_editMin = 0;
int g_editMax = 0;
const char *g_editUnit = "";
char g_editLabel[21] = {0};

// SD test list
int g_sdMenuIndex = 0;
int g_sdMenuTop = 0;

// Selected test context
int g_selectedTestIdx = -1;
char g_selectedTestName[21] = {0};
char g_selectedTestFile[32] = {0};

// Actions menu
const char *SD_ACTION_ITEMS[] = {"Load Test", "View Test", "Delete Test",
                                 "Back"};
int g_sdActionIndex = 0;
int g_sdActionTop = 0;

// View menu
TestParams g_viewParams;
char g_viewLines[VIEW_ITEM_MAX][21];
int g_viewCount = 0;
int g_viewIndex = 0;
int g_viewTop = 0;

// Save name entry
char g_nameBuf[17] = "TEST1___________";
uint8_t g_namePos = 0;
bool g_nameEditMode = false;
bool g_nameAccepted = false;
uint8_t g_saveNameMenuIndex = 0;
uint32_t g_nameBlinkMs = 0;
bool g_nameBlinkOn = true;

// ================== UTILITY FUNCTIONS ==================

void clampMenuTopN(int &top, int index, int count, int visibleRows) {
  if (count <= 0) {
    top = 0;
    return;
  }

  if (index < 0)
    index = 0;
  if (index > count - 1)
    index = count - 1;

  if (index < top)
    top = index;
  if (index > top + (visibleRows - 1))
    top = index - (visibleRows - 1);

  int maxTop = count - visibleRows;
  if (maxTop < 0)
    maxTop = 0;
  if (top < 0)
    top = 0;
  if (top > maxTop)
    top = maxTop;
}

int sdMenuItemCountIncludingBack() { return g_sdTestCount + 1; }
bool sdIsBackSelected() {
  return (g_sdMenuIndex == sdMenuItemCountIncludingBack() - 1);
}

void resetNameEntry() {
  strncpy(g_nameBuf, "TEST1___________", 16);
  g_nameBuf[16] = 0;
  g_namePos = 0;
  g_nameEditMode = false;
  g_nameAccepted = false;
  g_saveNameMenuIndex = 0;
  g_nameBlinkMs = millis();
  g_nameBlinkOn = true;
}

bool setupEditForCreateMenuIndex(int menuIdx) {
  if (menuIdx < 0 || menuIdx >= CREATE_MENU_COUNT - 1)
    return false;

  g_editMenuIndex = menuIdx;
  strncpy(g_editLabel, CREATE_MENU_ITEMS[menuIdx], 20);
  g_editLabel[20] = '\0';

  g_editPtr = nullptr;
  g_editMin = 0;
  g_editMax = 0;
  g_editUnit = "";

  switch (menuIdx) {
  case 0:
    g_editPtr = &g_createParams.Temp_Init_Denat;
    g_editMin = 25;
    g_editMax = 125;
    g_editUnit = "C";
    break;
  case 1:
    g_editPtr = &g_createParams.Time_Init_Denat;
    g_editMin = 0;
    g_editMax = 600;
    g_editUnit = "s";
    break;
  case 2:
    g_editPtr = &g_createParams.Temp_Denat;
    g_editMin = 25;
    g_editMax = 125;
    g_editUnit = "C";
    break;
  case 3:
    g_editPtr = &g_createParams.Time_Denat;
    g_editMin = 0;
    g_editMax = 600;
    g_editUnit = "s";
    break;
  case 4:
    g_editPtr = &g_createParams.Temp_Anneal;
    g_editMin = 25;
    g_editMax = 125;
    g_editUnit = "C";
    break;
  case 5:
    g_editPtr = &g_createParams.Time_Anneal;
    g_editMin = 0;
    g_editMax = 600;
    g_editUnit = "s";
    break;
  case 6:
    g_editPtr = &g_createParams.Temp_Extension;
    g_editMin = 25;
    g_editMax = 125;
    g_editUnit = "C";
    break;
  case 7:
    g_editPtr = &g_createParams.Time_Extension;
    g_editMin = 0;
    g_editMax = 600;
    g_editUnit = "s";
    break;
  case 8:
    g_editPtr = &g_createParams.Num_Cycles;
    g_editMin = 1;
    g_editMax = 99;
    g_editUnit = "";
    break;
  case 9:
    g_editPtr = &g_createParams.Temp_Final_Ext;
    g_editMin = 25;
    g_editMax = 125;
    g_editUnit = "C";
    break;
  case 10:
    g_editPtr = &g_createParams.Time_Final_Ext;
    g_editMin = 0;
    g_editMax = 600;
    g_editUnit = "s";
    break;
  default:
    return false;
  }
  return (g_editPtr != nullptr);
}

static void setViewLine(int idx, const char *s) {
  strncpy(g_viewLines[idx], s, 20);
  g_viewLines[idx][20] = '\0';
}

void buildViewLinesFromParams(const TestParams &p) {
  g_viewCount = 0;
  for (int i = 0; i < VIEW_ITEM_MAX; i++)
    g_viewLines[i][0] = '\0';

  char b[21];

  snprintf(b, sizeof(b), "InitDenT=%dC", p.Temp_Init_Denat);
  setViewLine(g_viewCount++, b);
  snprintf(b, sizeof(b), "InitDenS=%ds", p.Time_Init_Denat);
  setViewLine(g_viewCount++, b);

  snprintf(b, sizeof(b), "DenatT=%dC", p.Temp_Denat);
  setViewLine(g_viewCount++, b);
  snprintf(b, sizeof(b), "DenatS=%ds", p.Time_Denat);
  setViewLine(g_viewCount++, b);

  snprintf(b, sizeof(b), "AnnealT=%dC", p.Temp_Anneal);
  setViewLine(g_viewCount++, b);
  snprintf(b, sizeof(b), "AnnealS=%ds", p.Time_Anneal);
  setViewLine(g_viewCount++, b);

  snprintf(b, sizeof(b), "ExtT=%dC", p.Temp_Extension);
  setViewLine(g_viewCount++, b);
  snprintf(b, sizeof(b), "ExtS=%ds", p.Time_Extension);
  setViewLine(g_viewCount++, b);

  snprintf(b, sizeof(b), "Cycles=%d", p.Num_Cycles);
  setViewLine(g_viewCount++, b);

  snprintf(b, sizeof(b), "FinalExtT=%dC", p.Temp_Final_Ext);
  setViewLine(g_viewCount++, b);
  snprintf(b, sizeof(b), "FinalExtS=%ds", p.Time_Final_Ext);
  setViewLine(g_viewCount++, b);

  if (g_viewCount < VIEW_ITEM_MAX)
    setViewLine(g_viewCount++, "Back");
  else {
    setViewLine(VIEW_ITEM_MAX - 1, "Back");
    g_viewCount = VIEW_ITEM_MAX;
  }
}

// ================== ENTER STATE ==================

void enterState(UIState s) {
  g_state = s;

  switch (g_state) {
  case UIState::MAIN_MENU:
    drawMainMenu();
    break;

  case UIState::RUN_TEST:
    g_runTestMenuIndex = 0;
    drawRunTestScreen();
    break;

  case UIState::LIVE_TEST:
    g_liveTestStartMs = millis();
    g_liveLastDrawMs = 0;
    strncpy(g_liveTestName, "TEST", sizeof(g_liveTestName));
    g_liveTestName[16] = '\0';
    strncpy(g_livePhase, "Running", sizeof(g_livePhase));
    g_livePhase[12] = '\0';
    g_liveTargetC = 95;
    g_liveActualC = 25;
    drawLiveTestScreen();
    break;

  case UIState::CREATE_TEST_MENU:
    drawCreateTestMenu();
    break;

  case UIState::CREATE_EDIT_PARAM:
    drawCreateEditScreen();
    break;

  case UIState::SAVE_TEST_NAME:
    drawSaveNameScreen();
    break;

  case UIState::SD_TEST_LIST:
    scanTestsOnSD();
    g_sdMenuIndex = 0;
    g_sdMenuTop = 0;
    drawSDTestListMenu();
    break;

  case UIState::SD_TEST_ACTIONS:
    g_sdActionIndex = 0;
    g_sdActionTop = 0;
    drawSDTestActionsMenu();
    break;

  case UIState::SD_TEST_VIEW:
    loadSelectedTestForView(g_viewParams, g_selectedTestFile);
    buildViewLinesFromParams(g_viewParams);
    g_viewIndex = 0;
    g_viewTop = 0;
    drawViewMenu();
    break;
  }
}
