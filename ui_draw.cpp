/**
 * @file ui_draw.cpp
 * @brief UI drawing functions implementation
 */

#include "ui_draw.h"
#include "config.h"
#include "oled.h"
#include "sd_storage.h"
#include "ui_state.h"


void drawMainMenu() {
  oledWriteLine(0, "Main Menu");
  char l1[21], l2[21], l3[21];
  snprintf(l1, sizeof(l1), "%c %s", (g_mainMenuIndex == 0 ? '>' : ' '),
           MAIN_MENU_ITEMS[0]);
  snprintf(l2, sizeof(l2), "%c %s", (g_mainMenuIndex == 1 ? '>' : ' '),
           MAIN_MENU_ITEMS[1]);
  snprintf(l3, sizeof(l3), "%c %s", (g_mainMenuIndex == 2 ? '>' : ' '),
           MAIN_MENU_ITEMS[2]);
  oledWriteLine(1, l1);
  oledWriteLine(2, l2);
  oledWriteLine(3, l3);
}

void drawRunTestScreen() {
  oledWriteLine(0, "Run Test");

  char l1[21], l2[21];
  snprintf(l1, sizeof(l1), "%c Start", (g_runTestMenuIndex == 0 ? '>' : ' '));
  snprintf(l2, sizeof(l2), "%c Back", (g_runTestMenuIndex == 1 ? '>' : ' '));

  oledWriteLine(1, l1);
  oledWriteLine(2, l2);
  oledWriteLine(3, "");
}

void drawLiveTestScreen() {
  unsigned long elapsedS = (millis() - g_liveTestStartMs) / 1000UL;

  char line0[21], line1[21], line2[21];
  snprintf(line0, sizeof(line0), "%-20s", g_liveTestName);

  char tbuf[6];
  snprintf(tbuf, sizeof(tbuf), "%lus", (unsigned long)elapsedS);
  snprintf(line1, sizeof(line1), "%-10s %9s", g_livePhase, tbuf);

  snprintf(line2, sizeof(line2), "Tgt:%3dC Act:%3dC", g_liveTargetC,
           g_liveActualC);

  oledWriteLine(0, line0);
  oledWriteLine(1, line1);
  oledWriteLine(2, line2);
  oledWriteLine(3, "> Cancel Test");
}

void drawCreateTestMenu() {
  oledWriteLine(0, "Create Test:");
  clampMenuTopN(g_createMenuTop, g_createMenuIndex, CREATE_MENU_COUNT, 3);

  for (int row = 1; row <= 3; row++) {
    int idx = g_createMenuTop + (row - 1);
    if (idx >= CREATE_MENU_COUNT) {
      oledWriteLine(row, "");
      continue;
    }

    char line[21];
    bool sel = (idx == g_createMenuIndex);
    snprintf(line, sizeof(line), "%c %s", sel ? '>' : ' ',
             CREATE_MENU_ITEMS[idx]);
    oledWriteLine(row, line);
  }
}

void drawCreateEditScreen() {
  oledWriteLine(0, "Set Parameter:");
  oledWriteLine(1, g_editLabel);

  char vline[21];
  int v = (g_editPtr) ? *g_editPtr : 0;
  if (g_editUnit && g_editUnit[0] != '\0') {
    snprintf(vline, sizeof(vline), "%d %s", v, g_editUnit);
  } else {
    snprintf(vline, sizeof(vline), "%d", v);
  }
  oledWriteLine(2, vline);
  oledWriteLine(3, "Press=Accept");
}

void drawSaveNameScreen() {
  oledWriteLine(0, "clk:edit dblclk:OK");

  char nameLine[21];
  memset(nameLine, ' ', 20);
  nameLine[20] = 0;

  for (int i = 0; i < 16; i++) {
    char c = g_nameBuf[i];

    if (!g_nameAccepted && !g_nameEditMode && (i == (int)g_namePos)) {
      nameLine[i] = (g_nameBlinkOn ? '_' : ' ');
    } else {
      nameLine[i] = c;
    }
  }
  oledWriteLine(1, nameLine);

  if (!g_nameAccepted) {
    oledWriteLine(2, "");
    oledWriteLine(3, "");
    return;
  }

  char line2[21], line3[21];
  snprintf(line2, sizeof(line2), "%sSave Test",
           (g_saveNameMenuIndex == 0) ? "> " : "  ");
  snprintf(line3, sizeof(line3), "%sBack",
           (g_saveNameMenuIndex == 1) ? "> " : "  ");
  oledWriteLine(2, line2);
  oledWriteLine(3, line3);
}

void drawSDTestListMenu() {
  int count = sdMenuItemCountIncludingBack();
  clampMenuTopN(g_sdMenuTop, g_sdMenuIndex, count, 3);

  oledWriteLine(0, "SD Tests:");

  if (g_sdTestCount == 0) {
    oledWriteLine(1, "  (none found)");
    oledWriteLine(2, "");
    oledWriteLine(3, "> Back");
    return;
  }

  for (int row = 1; row < 4; row++) {
    int idx = g_sdMenuTop + (row - 1);
    if (idx >= count) {
      oledWriteLine(row, "");
      continue;
    }

    char line[21];
    bool sel = (idx == g_sdMenuIndex);

    if (idx == count - 1)
      snprintf(line, sizeof(line), "%c %s", sel ? '>' : ' ', "Back");
    else
      snprintf(line, sizeof(line), "%c %s", sel ? '>' : ' ',
               g_sdTestNames[idx]);

    oledWriteLine(row, line);
  }
}

void drawSDTestActionsMenu() {
  char header[21];
  snprintf(header, sizeof(header), "%s", g_selectedTestName);
  oledWriteLine(0, header);

  clampMenuTopN(g_sdActionTop, g_sdActionIndex, SD_ACTION_COUNT, 3);

  for (int row = 1; row <= 3; row++) {
    int idx = g_sdActionTop + (row - 1);
    if (idx >= SD_ACTION_COUNT) {
      oledWriteLine(row, "");
      continue;
    }

    char line[21];
    bool sel = (idx == g_sdActionIndex);
    snprintf(line, sizeof(line), "%c %s", sel ? '>' : ' ',
             SD_ACTION_ITEMS[idx]);
    oledWriteLine(row, line);
  }
}

void drawViewMenu() {
  char header[21];
  snprintf(header, sizeof(header), "View:%s", g_selectedTestName);
  header[20] = '\0';
  oledWriteLine(0, header);

  if (g_viewCount <= 0) {
    oledWriteLine(1, "> Back");
    oledWriteLine(2, "");
    oledWriteLine(3, "");
    return;
  }

  if (g_viewIndex < 0)
    g_viewIndex = 0;
  if (g_viewIndex > g_viewCount - 1)
    g_viewIndex = g_viewCount - 1;

  clampMenuTopN(g_viewTop, g_viewIndex, g_viewCount, 3);

  for (int row = 1; row < 4; row++) {
    int idx = g_viewTop + (row - 1);
    if (idx >= g_viewCount) {
      oledWriteLine(row, "");
      continue;
    }

    char line[21];
    bool sel = (idx == g_viewIndex);
    snprintf(line, sizeof(line), "%c %s", sel ? '>' : ' ', g_viewLines[idx]);
    oledWriteLine(row, line);
  }
}
