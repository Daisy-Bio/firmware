enum ButtonEvent { BE_NONE, BE_SINGLE, BE_DOUBLE };

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <string.h>

// Optional: reduce current spikes/noise during boot
#include <WiFi.h>
#include <esp_bt.h>

// ================== UI STATE MACHINE (define EARLY) ==================
enum class UIState : uint8_t {
  MAIN_MENU,
  RUN_TEST,          // placeholder screen for now
  LIVE_TEST,
  CREATE_TEST_MENU,     // parameter list
  CREATE_EDIT_PARAM,    // edit value screen
  SAVE_TEST_NAME,       // filename entry + save
  SD_TEST_LIST,
  SD_TEST_ACTIONS,
  SD_TEST_VIEW,
};
static void enterState(UIState s);
// =====================================================================

// ================== PIN DEFINITIONS ==================
static constexpr uint8_t ENCODER_A_PIN  = 33;
static constexpr uint8_t ENCODER_B_PIN  = 32;
static constexpr uint8_t ENCODER_SW_PIN = 25;

static constexpr uint8_t I2C_SDA_PIN    = 21;
static constexpr uint8_t I2C_SCL_PIN    = 22;
static constexpr int     OLED_RESET_PIN = 13;
static constexpr uint8_t OLED_ADDR      = 0x3C;

static constexpr uint8_t SD_CS_PIN   = 2;
static constexpr uint8_t SD_MOSI_PIN = 17;
static constexpr uint8_t SD_MISO_PIN = 19;
static constexpr uint8_t SD_SCLK_PIN = 18;
// =====================================================

// ================== TEST PARAMS TYPE (MUST BE EARLY) ==================
struct TestParams {
  int Temp_Init_Denat   = 95;
  int Time_Init_Denat   = 120;
  int Temp_Denat        = 95;
  int Time_Denat        = 10;
  int Temp_Anneal       = 60;
  int Time_Anneal       = 20;
  int Temp_Extension    = 72;
  int Time_Extension    = 20;
  int Num_Cycles        = 45;
  int Temp_Final_Ext    = 72;
  int Time_Final_Ext    = 240;
};
// =====================================================================

// Draft params being edited in Create Test flow
static TestParams g_createParams;
// Active params (used for Run/Load). For now, we copy loaded tests here.
static TestParams g_activeParams;

// ================== SAVE TEST NAME ENTRY ==================
static char g_nameBuf[17] = "TEST1___________"; // 16 chars, '_' are placeholders
static uint8_t g_namePos = 0;                   // 0..15
static bool g_nameEditMode = false;             // false=move, true=edit
static bool g_nameAccepted = false;            // filename accepted, now in Save/Back menu
static uint8_t g_saveNameMenuIndex = 0;        // 0=Save Test, 1=Back
static uint32_t g_nameBlinkMs = 0;
static bool g_nameBlinkOn = true;

static const char* NAME_CHARS = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_()[]";
static constexpr uint8_t NAME_CHARS_COUNT = 42;

static void resetNameEntry() {
  // start with "TEST" + digits + fill underscores
  strncpy(g_nameBuf, "TEST1___________", 16);
  g_nameBuf[16] = 0;
  g_namePos = 0;
  g_nameEditMode = false;
  g_nameAccepted = false;
  g_saveNameMenuIndex = 0;
  g_nameBlinkMs = millis();
  g_nameBlinkOn = true;
}

static void drawSaveNameScreen();
static bool saveCreateParamsToSD(const char* base16);

// Helper: build /TESTS/<name>.TXT path from base16 (trim underscores)
static void buildTestPath(char* out, size_t outSz, const char* base16) {
  char base[17];
  strncpy(base, base16, 16);
  base[16]=0;
  // trim trailing underscores
  for (int i=15;i>=0;i--) {
    if (base[i]=='_') base[i]=0;
    else break;
  }
  if (base[0]==0) strncpy(base, "TEST", sizeof(base)-1);

  snprintf(out, outSz, "/TESTS/%s.TXT", base);
}


// ================== OLED (US2066) ==================
static constexpr uint8_t CTRL_CMD  = 0x00;
static constexpr uint8_t CTRL_DATA = 0x40;
static const uint8_t ROW_ADDR[4] = { 0x00, 0x20, 0x40, 0x60 };

static void oledWrite(uint8_t control, const uint8_t* data, size_t len) {
  Wire.beginTransmission(OLED_ADDR);
  Wire.write(control);
  Wire.write(data, len);
  Wire.endTransmission();
}
static void oledCmd(uint8_t c) { oledWrite(CTRL_CMD, &c, 1); delayMicroseconds(50); }
static void oledData(uint8_t d) { oledWrite(CTRL_DATA, &d, 1); }
static void oledSetCursor(uint8_t col, uint8_t row) {
  if (row > 3) row = 3;
  if (col > 19) col = 19;
  oledCmd(0x80 | (ROW_ADDR[row] + col));
}
static void oledClearAndHome() { oledCmd(0x01); delay(3); oledCmd(0x02); delay(3); }

static void oledHardwareReset() {
  if (OLED_RESET_PIN < 0) return;
  pinMode((uint8_t)OLED_RESET_PIN, OUTPUT);
  digitalWrite((uint8_t)OLED_RESET_PIN, LOW);
  delay(50);
  digitalWrite((uint8_t)OLED_RESET_PIN, HIGH);
  delay(100);
}
static void oledInitOnce() {
  // US2066 initialization sequence for I2C, 3.3V mode
  oledCmd(0x2A);    // Extended instruction set (RE=1)
  oledCmd(0x71);    // Function selection A
  oledData(0x00);   // CRITICAL: Send as DATA byte for 3.3V mode

  // IMPORTANT: This command while still in the extended instruction set
  // stabilizes orientation / mirroring behavior on some US2066 modules.
  oledCmd(0x06);

  oledCmd(0x28);    // Fundamental instruction set (RE=0)
  oledCmd(0x08);    // Display off

  oledClearAndHome();

  oledCmd(0x06);    // Entry mode: increment, no shift
  oledCmd(0x0C);    // Display on, cursor off, blink off
}
static void oledInitDeterministic() {
  delay(200);
  oledHardwareReset();
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(100000);
  delay(50);
  oledInitOnce();
  delay(20);
  oledInitOnce();
}
static void oledWriteLine(uint8_t row, const char* text) {
  char buf[21];
  memset(buf, ' ', 20);
  buf[20] = '\0';
  size_t n = strlen(text);
  if (n > 20) n = 20;
  memcpy(buf, text, n);
  for (uint8_t col = 0; col < 20; col++) {
    oledSetCursor(col, row);
    oledData((uint8_t)buf[col]);
  }
}

// ================== ENCODER ==================
static constexpr int PULSES_PER_DETENT = 8;
volatile int32_t g_pulseCount = 0;
volatile uint8_t g_lastAB = 0;

static const int8_t QDELTA[16] = {
  0, -1, +1,  0,
  +1,  0,  0, -1,
  -1,  0,  0, +1,
   0, +1, -1,  0
};

static void IRAM_ATTR encoderISR() {
  uint8_t a = (uint8_t)digitalRead(ENCODER_A_PIN);
  uint8_t b = (uint8_t)digitalRead(ENCODER_B_PIN);
  uint8_t newAB = (a << 1) | b;
  uint8_t idx = (g_lastAB << 2) | newAB;
  g_pulseCount += QDELTA[idx];
  g_lastAB = newAB;
}

static ButtonEvent readButtonEvent() {
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

  if (now != lastRead) { lastRead = now; lastChangeMs = millis(); }

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

static int32_t getDetentDelta() {
  static int32_t lastDetent = 0;
  int32_t pulses;
  noInterrupts(); pulses = g_pulseCount; interrupts();
  int32_t detent = pulses / PULSES_PER_DETENT;
  int32_t delta = detent - lastDetent;
  if (delta != 0) lastDetent = detent;
  return delta;
}

// ================== SD ==================
static bool g_sdOK = false;

static bool initSDCard() {
  SPI.begin(SD_SCLK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
  if (!SD.begin(SD_CS_PIN, SPI)) { Serial.println("SD init failed"); return false; }
  Serial.println("SD card OK");
  return true;
}

static void ensureTestsDir() {
  if (!g_sdOK) return;
  if (!SD.exists("/TESTS")) SD.mkdir("/TESTS");
}

// ================== UI MODELS ==================
static UIState g_state = UIState::MAIN_MENU;

// Main menu
static const char* MAIN_MENU_ITEMS[] = { "Run Test", "Create Test", "SD Card" };
static constexpr int MAIN_MENU_COUNT = sizeof(MAIN_MENU_ITEMS) / sizeof(MAIN_MENU_ITEMS[0]);
static int g_mainMenuIndex = 0;

// Run Test menu
static int g_runTestMenuIndex = 0;

// Live Test (placeholder)
static unsigned long g_liveTestStartMs = 0;
static unsigned long g_liveLastDrawMs = 0;
static char g_liveTestName[17] = "TEST";
static char g_livePhase[13] = "Idle";
static int g_liveTargetC = 95;
static int g_liveActualC = 25;


// Create Test menu (expanded)
static const char* CREATE_MENU_ITEMS[] = {
  "Init Denat Temp",
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
  "Back"
};
static constexpr int CREATE_MENU_COUNT = sizeof(CREATE_MENU_ITEMS) / sizeof(CREATE_MENU_ITEMS[0]);
static int g_createMenuIndex = 0;
static int g_createMenuTop   = 0;

// Edit state context
static int g_editMenuIndex = 0;   // which CREATE menu item is being edited (0..10)
static int* g_editPtr = nullptr;  // points into g_createParams
static int g_editMin = 0;
static int g_editMax = 0;
static const char* g_editUnit = "";
static char g_editLabel[21] = {0};

// SD test list
static constexpr int MAX_SD_TESTS = 40;
static char g_sdTestNames[MAX_SD_TESTS][21];
static char g_sdTestFiles[MAX_SD_TESTS][32];
static int  g_sdTestCount = 0;

static int g_sdMenuIndex = 0;
static int g_sdMenuTop   = 0;

// Selected test context
static int  g_selectedTestIdx = -1;
static char g_selectedTestName[21] = {0};
static char g_selectedTestFile[32] = {0};

// Actions menu (Delete above Back, Back last)
static const char* SD_ACTION_ITEMS[] = {
  "Load Test",
  "View Test",
  "Delete Test",
  "Back"
};
static constexpr int SD_ACTION_COUNT = 4;
static int g_sdActionIndex = 0;
static int g_sdActionTop   = 0;

// View menu lines
static TestParams g_viewParams;
static constexpr int VIEW_ITEM_MAX = 16;
static char g_viewLines[VIEW_ITEM_MAX][21];
static int  g_viewCount = 0;
static int  g_viewIndex = 0;
static int  g_viewTop   = 0;

// ================== UTIL ==================
static void clampMenuTopN(int &top, int index, int count, int visibleRows) {
  if (count <= 0) { top = 0; return; }

  if (index < 0) index = 0;
  if (index > count - 1) index = count - 1;

  if (index < top) top = index;
  if (index > top + (visibleRows - 1)) top = index - (visibleRows - 1);

  int maxTop = count - visibleRows;
  if (maxTop < 0) maxTop = 0;
  if (top < 0) top = 0;
  if (top > maxTop) top = maxTop;
}

static int clampInt(int v, int lo, int hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

static bool endsWithIgnoreCase(const char* s, const char* suffix) {
  size_t ls = strlen(s), lf = strlen(suffix);
  if (lf > ls) return false;
  const char* p = s + (ls - lf);
  for (size_t i = 0; i < lf; i++) {
    char a = p[i], b = suffix[i];
    if (a >= 'a' && a <= 'z') a -= 32;
    if (b >= 'a' && b <= 'z') b -= 32;
    if (a != b) return false;
  }
  return true;
}

static void clearSDTestList() {
  g_sdTestCount = 0;
  for (int i = 0; i < MAX_SD_TESTS; i++) {
    g_sdTestNames[i][0] = '\0';
    g_sdTestFiles[i][0] = '\0';
  }
}

static void filenameToDisplayName(const char* filename, char* out20) {
  int j = 0;
  for (int i = 0; filename[i] != '\0' && j < 20; i++) {
    if (filename[i] == '.') break;
    out20[j++] = filename[i];
  }
  out20[j] = '\0';
}

static void scanTestsOnSD() {
  clearSDTestList();
  if (!g_sdOK) return;
  if (!SD.exists("/TESTS")) return;

  File dir = SD.open("/TESTS");
  if (!dir || !dir.isDirectory()) {
    if (dir) dir.close();
    return;
  }

  while (true) {
    File f = dir.openNextFile();
    if (!f) break;

    if (!f.isDirectory()) {
      const char* fn = f.name();
      const char* base = strrchr(fn, '/');
      base = (base) ? (base + 1) : fn;

      if (endsWithIgnoreCase(base, ".TXT")) {
        if (g_sdTestCount < MAX_SD_TESTS) {
          strncpy(g_sdTestFiles[g_sdTestCount], base, sizeof(g_sdTestFiles[g_sdTestCount]) - 1);
          g_sdTestFiles[g_sdTestCount][sizeof(g_sdTestFiles[g_sdTestCount]) - 1] = '\0';

          filenameToDisplayName(base, g_sdTestNames[g_sdTestCount]);
          g_sdTestCount++;
        }
      }
    }
    f.close();
  }
  dir.close();
}

// Backwards-compatible name used in some UI states
static void readSDTestsList() {
  scanTestsOnSD();
}


static int sdMenuItemCountIncludingBack() { return g_sdTestCount + 1; }
static bool sdIsBackSelected() { return (g_sdMenuIndex == sdMenuItemCountIncludingBack() - 1); }

// ================== SD: DELETE ==================
static void deleteSelectedTestFile() {
  if (!g_sdOK) return;
  if (g_selectedTestFile[0] == '\0') return;

  char path[64];
  snprintf(path, sizeof(path), "/TESTS/%s", g_selectedTestFile);
  SD.remove(path);
}

// ================== SD: LOAD FOR VIEW ==================
static void parseKeyValueLineIntoParams(const String& line, TestParams& p) {
  int eq = line.indexOf('=');
  if (eq <= 0) return;

  String key = line.substring(0, eq);
  String val = line.substring(eq + 1);
  key.trim(); val.trim();
  int iv = val.toInt();

  if (key == "Temp_Init_Denat")      p.Temp_Init_Denat   = clampInt(iv, 25, 125);
  else if (key == "Time_Init_Denat") p.Time_Init_Denat   = clampInt(iv, 0, 600);
  else if (key == "Temp_Denat")      p.Temp_Denat        = clampInt(iv, 25, 125);
  else if (key == "Time_Denat")      p.Time_Denat        = clampInt(iv, 0, 600);
  else if (key == "Temp_Anneal")     p.Temp_Anneal       = clampInt(iv, 25, 125);
  else if (key == "Time_Anneal")     p.Time_Anneal       = clampInt(iv, 0, 600);
  else if (key == "Temp_Extension")  p.Temp_Extension    = clampInt(iv, 25, 125);
  else if (key == "Time_Extension")  p.Time_Extension    = clampInt(iv, 0, 600);
  else if (key == "Num_Cycles")      p.Num_Cycles        = clampInt(iv, 1, 99);
  else if (key == "Temp_Final_Ext")  p.Temp_Final_Ext    = clampInt(iv, 25, 125);
  else if (key == "Time_Final_Ext")  p.Time_Final_Ext    = clampInt(iv, 0, 600);
}

static bool loadSelectedTestForView(TestParams& out) {
  out = TestParams(); // defaults
  if (!g_sdOK || g_selectedTestFile[0] == '\0') return false;

  char path[64];
  snprintf(path, sizeof(path), "/TESTS/%s", g_selectedTestFile);

  File f = SD.open(path, FILE_READ);
  if (!f) return false;

  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;
    parseKeyValueLineIntoParams(line, out);
  }
  f.close();
  return true;
}

static void setViewLine(int idx, const char* s) {
  strncpy(g_viewLines[idx], s, 20);
  g_viewLines[idx][20] = '\0';
}

static void buildViewLinesFromParams(const TestParams& p) {
  g_viewCount = 0;
  for (int i = 0; i < VIEW_ITEM_MAX; i++) g_viewLines[i][0] = '\0';

  char b[21];

  snprintf(b, sizeof(b), "InitDenT=%dC", p.Temp_Init_Denat);     setViewLine(g_viewCount++, b);
  snprintf(b, sizeof(b), "InitDenS=%ds", p.Time_Init_Denat);     setViewLine(g_viewCount++, b);

  snprintf(b, sizeof(b), "DenatT=%dC", p.Temp_Denat);            setViewLine(g_viewCount++, b);
  snprintf(b, sizeof(b), "DenatS=%ds", p.Time_Denat);            setViewLine(g_viewCount++, b);

  snprintf(b, sizeof(b), "AnnealT=%dC", p.Temp_Anneal);          setViewLine(g_viewCount++, b);
  snprintf(b, sizeof(b), "AnnealS=%ds", p.Time_Anneal);          setViewLine(g_viewCount++, b);

  snprintf(b, sizeof(b), "ExtT=%dC", p.Temp_Extension);          setViewLine(g_viewCount++, b);
  snprintf(b, sizeof(b), "ExtS=%ds", p.Time_Extension);          setViewLine(g_viewCount++, b);

  snprintf(b, sizeof(b), "Cycles=%d", p.Num_Cycles);             setViewLine(g_viewCount++, b);

  snprintf(b, sizeof(b), "FinalExtT=%dC", p.Temp_Final_Ext);     setViewLine(g_viewCount++, b);
  snprintf(b, sizeof(b), "FinalExtS=%ds", p.Time_Final_Ext);     setViewLine(g_viewCount++, b);

  if (g_viewCount < VIEW_ITEM_MAX) setViewLine(g_viewCount++, "Back");
  else { setViewLine(VIEW_ITEM_MAX - 1, "Back"); g_viewCount = VIEW_ITEM_MAX; }
}

// ================== CREATE: PARAM POINTER MAPPING ==================
static bool setupEditForCreateMenuIndex(int menuIdx) {
  // menuIdx is 0..(CREATE_MENU_COUNT-1)
  // last item is Back -> not editable
  if (menuIdx < 0 || menuIdx >= CREATE_MENU_COUNT - 1) return false;

  g_editMenuIndex = menuIdx;
  strncpy(g_editLabel, CREATE_MENU_ITEMS[menuIdx], 20);
  g_editLabel[20] = '\0';

  g_editPtr = nullptr;
  g_editMin = 0;
  g_editMax = 0;
  g_editUnit = "";

  // Temps: 25..125 C
  // Times: 0..600 s
  // Cycles: 1..99
  switch (menuIdx) {
    case 0:  g_editPtr = &g_createParams.Temp_Init_Denat;  g_editMin = 25; g_editMax = 125; g_editUnit = "C"; break;
    case 1:  g_editPtr = &g_createParams.Time_Init_Denat;  g_editMin = 0;  g_editMax = 600; g_editUnit = "s"; break;
    case 2:  g_editPtr = &g_createParams.Temp_Denat;       g_editMin = 25; g_editMax = 125; g_editUnit = "C"; break;
    case 3:  g_editPtr = &g_createParams.Time_Denat;       g_editMin = 0;  g_editMax = 600; g_editUnit = "s"; break;
    case 4:  g_editPtr = &g_createParams.Temp_Anneal;      g_editMin = 25; g_editMax = 125; g_editUnit = "C"; break;
    case 5:  g_editPtr = &g_createParams.Time_Anneal;      g_editMin = 0;  g_editMax = 600; g_editUnit = "s"; break;
    case 6:  g_editPtr = &g_createParams.Temp_Extension;   g_editMin = 25; g_editMax = 125; g_editUnit = "C"; break;
    case 7:  g_editPtr = &g_createParams.Time_Extension;   g_editMin = 0;  g_editMax = 600; g_editUnit = "s"; break;
    case 8:  g_editPtr = &g_createParams.Num_Cycles;       g_editMin = 1;  g_editMax = 99;  g_editUnit = "";  break;
    case 9:  g_editPtr = &g_createParams.Temp_Final_Ext;   g_editMin = 25; g_editMax = 125; g_editUnit = "C"; break;
    case 10: g_editPtr = &g_createParams.Time_Final_Ext;   g_editMin = 0;  g_editMax = 600; g_editUnit = "s"; break;
    default: return false;
  }
  return (g_editPtr != nullptr);
}

// ================== DRAW ==================
static void drawMainMenu() {
  oledWriteLine(0, "Main Menu");
  char l1[21], l2[21], l3[21];
  snprintf(l1, sizeof(l1), "%c %s", (g_mainMenuIndex == 0 ? '>' : ' '), MAIN_MENU_ITEMS[0]);
  snprintf(l2, sizeof(l2), "%c %s", (g_mainMenuIndex == 1 ? '>' : ' '), MAIN_MENU_ITEMS[1]);
  snprintf(l3, sizeof(l3), "%c %s", (g_mainMenuIndex == 2 ? '>' : ' '), MAIN_MENU_ITEMS[2]);
  oledWriteLine(1, l1);
  oledWriteLine(2, l2);
  oledWriteLine(3, l3);
}

static void drawRunTestScreen() {
  oledWriteLine(0, "Run Test");

  char l1[21], l2[21];
  snprintf(l1, sizeof(l1), "%c Start", (g_runTestMenuIndex == 0 ? '>' : ' '));
  snprintf(l2, sizeof(l2), "%c Back",  (g_runTestMenuIndex == 1 ? '>' : ' '));

  oledWriteLine(1, l1);
  oledWriteLine(2, l2);
  oledWriteLine(3, "");
}


static void drawLiveTestScreen() {
  // Update once per second (caller throttles)
  unsigned long elapsedS = (millis() - g_liveTestStartMs) / 1000UL;

  char line0[21], line1[21], line2[21];
  snprintf(line0, sizeof(line0), "%-20s", g_liveTestName);

  // Phase + time (compact)
  char tbuf[6];
  snprintf(tbuf, sizeof(tbuf), "%lus", (unsigned long)elapsedS);
  snprintf(line1, sizeof(line1), "%-10s %9s", g_livePhase, tbuf);

  // Target and Actual
  snprintf(line2, sizeof(line2), "Tgt:%3dC Act:%3dC", g_liveTargetC, g_liveActualC);

  oledWriteLine(0, line0);
  oledWriteLine(1, line1);
  oledWriteLine(2, line2);
  oledWriteLine(3, "> Cancel Test");
}

static void drawCreateTestMenu() {
  oledWriteLine(0, "Create Test:");
  clampMenuTopN(g_createMenuTop, g_createMenuIndex, CREATE_MENU_COUNT, 3);

  for (int row = 1; row <= 3; row++) {
    int idx = g_createMenuTop + (row - 1);
    if (idx >= CREATE_MENU_COUNT) { oledWriteLine(row, ""); continue; }

    char line[21];
    bool sel = (idx == g_createMenuIndex);
    snprintf(line, sizeof(line), "%c %s", sel ? '>' : ' ', CREATE_MENU_ITEMS[idx]);
    oledWriteLine(row, line);
  }
}

static void drawCreateEditScreen() {
  // Screen layout:
  // Row0: "Set Parameter:"
  // Row1: <label>
  // Row2: <value + unit>
  // Row3: "Press=Accept"
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

static void drawSDTestListMenu() {
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
    if (idx >= count) { oledWriteLine(row, ""); continue; }

    char line[21];
    bool sel = (idx == g_sdMenuIndex);

    if (idx == count - 1) snprintf(line, sizeof(line), "%c %s", sel ? '>' : ' ', "Back");
    else                 snprintf(line, sizeof(line), "%c %s", sel ? '>' : ' ', g_sdTestNames[idx]);

    oledWriteLine(row, line);
  }
}

static void drawSDTestActionsMenu() {
  char header[21];
  snprintf(header, sizeof(header), "%s", g_selectedTestName);
  oledWriteLine(0, header);

  clampMenuTopN(g_sdActionTop, g_sdActionIndex, SD_ACTION_COUNT, 3);

  for (int row = 1; row <= 3; row++) {
    int idx = g_sdActionTop + (row - 1);
    if (idx >= SD_ACTION_COUNT) { oledWriteLine(row, ""); continue; }

    char line[21];
    bool sel = (idx == g_sdActionIndex);
    snprintf(line, sizeof(line), "%c %s", sel ? '>' : ' ', SD_ACTION_ITEMS[idx]);
    oledWriteLine(row, line);
  }
}

static void drawViewMenu() {
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

  if (g_viewIndex < 0) g_viewIndex = 0;
  if (g_viewIndex > g_viewCount - 1) g_viewIndex = g_viewCount - 1;

  clampMenuTopN(g_viewTop, g_viewIndex, g_viewCount, 3);

  for (int row = 1; row < 4; row++) {
    int idx = g_viewTop + (row - 1);
    if (idx >= g_viewCount) { oledWriteLine(row, ""); continue; }

    char line[21];
    bool sel = (idx == g_viewIndex);
    snprintf(line, sizeof(line), "%c %s", sel ? '>' : ' ', g_viewLines[idx]);
    oledWriteLine(row, line);
  }
}
// ================== SAVE TEST NAME SCREEN + SAVE FUNCTION ==================
static void drawSaveNameScreen() {
  // Blink timing handled in loop() for consistent refresh

  // Line 0: fixed instructions
  oledWriteLine(0, "clk:edit dblclk:OK");

  // Line 1: 16-char name area with ONE blinking character cursor
  // - Rotating moves the blinking cell (clamped 0..15)
  // - Clicking toggles EDIT for that cell; rotation changes the character
  // - The active cell blinks by blanking it; if stored char is space, we blink '_'
  char nameLine[21];
  memset(nameLine, ' ', 20);
  nameLine[20] = 0;

  for (int i = 0; i < 16; i++) {
    char c = g_nameBuf[i];

    if (!g_nameAccepted && !g_nameEditMode && (i == (int)g_namePos)) {
      // Blink the ACTIVE CELL by toggling underscore vs blank
      nameLine[i] = (g_nameBlinkOn ? '_' : ' ');
    } else {
      // Show stored character
      nameLine[i] = c;
    }
  }
  oledWriteLine(1, nameLine);

  // Lines 2-3: Save/Back menu appears only after filename accepted (dblclk)
  if (!g_nameAccepted) {
    oledWriteLine(2, "");
    oledWriteLine(3, "");
    return;
  }

  char line2[21], line3[21];
  snprintf(line2, sizeof(line2), "%sSave Test", (g_saveNameMenuIndex == 0) ? "> " : "  ");
  snprintf(line3, sizeof(line3), "%sBack",     (g_saveNameMenuIndex == 1) ? "> " : "  ");
  oledWriteLine(2, line2);
  oledWriteLine(3, line3);
}

static bool saveCreateParamsToSD(const char* base16) {
  if (!g_sdOK) return false;
  ensureTestsDir();

  char path[64];
  buildTestPath(path, sizeof(path), base16);

  // FILE_WRITE appends; remove first to overwrite cleanly
  if (SD.exists(path)) SD.remove(path);

  File f = SD.open(path, FILE_WRITE);
  if (!f) return false;

  f.printf("Temp_Init_Denat=%d\n",  g_createParams.Temp_Init_Denat);
  f.printf("Time_Init_Denat=%d\n",  g_createParams.Time_Init_Denat);
  f.printf("Temp_Denat=%d\n",       g_createParams.Temp_Denat);
  f.printf("Time_Denat=%d\n",       g_createParams.Time_Denat);
  f.printf("Temp_Anneal=%d\n",      g_createParams.Temp_Anneal);
  f.printf("Time_Anneal=%d\n",      g_createParams.Time_Anneal);
  f.printf("Temp_Extension=%d\n",   g_createParams.Temp_Extension);
  f.printf("Time_Extension=%d\n",   g_createParams.Time_Extension);
  f.printf("Num_Cycles=%d\n",       g_createParams.Num_Cycles);
  f.printf("Temp_Final_Ext=%d\n",   g_createParams.Temp_Final_Ext);
  f.printf("Time_Final_Ext=%d\n",   g_createParams.Time_Final_Ext);

  f.close();
  return true;
}
// ===========================================================================

// ================== ENTER STATE ==================
static void enterState(UIState s) {
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
      // Placeholder values (replace with real run engine later)
      strncpy(g_liveTestName, "TEST", sizeof(g_liveTestName));
      g_liveTestName[16] = '\0';
      strncpy(g_livePhase, "Running", sizeof(g_livePhase));
      g_livePhase[12] = '\0';
      g_liveTargetC = 95;
      g_liveActualC = 25;
      drawLiveTestScreen();
      break;


    case UIState::CREATE_TEST_MENU:
      // Keep current g_createMenuIndex/top (don’t reset every time; feels nicer)
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
      loadSelectedTestForView(g_viewParams);
      buildViewLinesFromParams(g_viewParams);
      g_viewIndex = 0;
      g_viewTop = 0;
      drawViewMenu();
      break;
  }
}

// ================== SETUP / LOOP ==================
void setup() {
  // ---- Power-up stabilization delay ----
  delay(2000);

  // Optional: reduce current spikes/noise during boot
  WiFi.mode(WIFI_OFF);
  btStop();

  Serial.begin(115200);
  delay(200);

  oledInitDeterministic();

  pinMode(ENCODER_A_PIN, INPUT_PULLUP);
  pinMode(ENCODER_B_PIN, INPUT_PULLUP);
  pinMode(ENCODER_SW_PIN, INPUT_PULLUP);

  g_lastAB = ((uint8_t)digitalRead(ENCODER_A_PIN) << 1) | (uint8_t)digitalRead(ENCODER_B_PIN);
  attachInterrupt(digitalPinToInterrupt(ENCODER_A_PIN), encoderISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENCODER_B_PIN), encoderISR, CHANGE);

  g_sdOK = initSDCard();
  ensureTestsDir();

  // Initialize Create Test draft params to defaults
  g_createParams = TestParams();
  g_activeParams = g_createParams;

  enterState(UIState::MAIN_MENU);
}

void loop() {
  const int32_t delta = getDetentDelta();
  const ButtonEvent btn = readButtonEvent();

  // ---------------- SAVE NAME BLINK REFRESH ----------------
  // The name-entry screen needs periodic redraws so the blinking cursor is visible even when idle.
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

    // Placeholder: simple dummy actual temp "ramps" toward target for visibility
    if (g_liveActualC < g_liveTargetC) g_liveActualC++;
    else if (g_liveActualC > g_liveTargetC) g_liveActualC--;

    drawLiveTestScreen();
  }
}



  // ---------------- ROTATION ----------------
  if (delta != 0) {
    // Apply *every* detent step. (If the loop is busy and multiple detents accumulate,
    // we still move one menu item / one character per detent.)
    int32_t steps = delta;
    while (steps != 0) {
      const int step = (steps > 0) ? 1 : -1;
      steps -= step;

      if (g_state == UIState::MAIN_MENU) {
        // Rotation moves the cursor through the top-level items (one detent = one move).
        int ni = (int)g_mainMenuIndex + step;
        if (ni < 0) ni = 0;
        if (ni > 2) ni = 2; // 3 items: Run Test, Create Test, SD Card
        if (ni != (int)g_mainMenuIndex) {
          g_mainMenuIndex = (uint8_t)ni;
          drawMainMenu();
        }
      }
      else if (g_state == UIState::RUN_TEST) {
        int ni = (int)g_runTestMenuIndex + step;
        if (ni < 0) ni = 0;
        if (ni > 1) ni = 1; // Start, Back
        if (ni != (int)g_runTestMenuIndex) {
          g_runTestMenuIndex = (uint8_t)ni;
          drawRunTestScreen();
        }
      }
      else if (g_state == UIState::LIVE_TEST) {
        // Single item; rotation does nothing for now.
      }
      else if (g_state == UIState::CREATE_TEST_MENU) {
        int ni = (int)g_createMenuIndex + step;
        if (ni < 0) ni = 0;
        if (ni > CREATE_MENU_COUNT - 1) ni = CREATE_MENU_COUNT - 1;
        if (ni != (int)g_createMenuIndex) { g_createMenuIndex = (uint8_t)ni; drawCreateTestMenu(); }
      }
      else if (g_state == UIState::CREATE_EDIT_PARAM) {
        if (g_editPtr) {
          int v = *g_editPtr;
          v += step;
          v = clampInt(v, g_editMin, g_editMax);
          *g_editPtr = v;
          drawCreateEditScreen();
        }
      }
      else if (g_state == UIState::SAVE_TEST_NAME) {
        if (!g_nameAccepted) {
          if (!g_nameEditMode) {
            // MOVE mode: move the blinking cell across 16 cells
            int np = (int)g_namePos + step;
            if (np < 0) np = 0;
            if (np > 15) np = 15;
            g_namePos = (uint8_t)np;
          } else {
            // EDIT mode: cycle the character at the current cell
            char cur = g_nameBuf[g_namePos];
            int idx = 0;
            for (int i = 0; i < NAME_CHARS_COUNT; i++) { if (NAME_CHARS[i] == cur) { idx = i; break; } }
            idx += step;
            if (idx < 0) idx = NAME_CHARS_COUNT - 1;
            if (idx >= NAME_CHARS_COUNT) idx = 0;
            g_nameBuf[g_namePos] = NAME_CHARS[idx];
          }
          drawSaveNameScreen();
        } else {
          // After filename accepted: select Save Test / Back
          int ni = (int)g_saveNameMenuIndex + step;
          if (ni < 0) ni = 0;
          if (ni > 1) ni = 1;
          g_saveNameMenuIndex = (uint8_t)ni;
          drawSaveNameScreen();
        }
      }
      else if (g_state == UIState::SD_TEST_LIST) {
        int count = sdMenuItemCountIncludingBack();
        int ni = (int)g_sdMenuIndex + step;
        if (ni < 0) ni = 0;
        if (ni > count - 1) ni = count - 1;
        if (ni != (int)g_sdMenuIndex) { g_sdMenuIndex = (uint8_t)ni; drawSDTestListMenu(); }
      }
      else if (g_state == UIState::SD_TEST_ACTIONS) {
        int ni = (int)g_sdActionIndex + step;
        if (ni < 0) ni = 0;
        if (ni > SD_ACTION_COUNT - 1) ni = SD_ACTION_COUNT - 1;
        if (ni != (int)g_sdActionIndex) { g_sdActionIndex = (uint8_t)ni; drawSDTestActionsMenu(); }
      }
      else if (g_state == UIState::SD_TEST_VIEW) {
        int ni = (int)g_viewIndex + step;
        if (ni < 0) ni = 0;
        if (ni > g_viewCount - 1) ni = g_viewCount - 1;
        if (ni != (int)g_viewIndex) { g_viewIndex = (uint8_t)ni; drawViewMenu(); }
      }
    }
  }

  // ---------------- BUTTON ----------------
  if (btn != BE_NONE) {
    if (g_state == UIState::MAIN_MENU) {
      // Single click selects the highlighted item.
      if (g_mainMenuIndex == 0) {
        enterState(UIState::RUN_TEST);
      } else if (g_mainMenuIndex == 1) {
        g_createMenuIndex = 0;
        g_createMenuTop = 0;
        enterState(UIState::CREATE_TEST_MENU);
      } else { // 2
        enterState(UIState::SD_TEST_LIST);
      }
    }
    else if (g_state == UIState::RUN_TEST) {
      if (btn == BE_SINGLE) {
        if (g_runTestMenuIndex == 0) {
          enterState(UIState::LIVE_TEST);
        } else {
          enterState(UIState::MAIN_MENU);
        }
      }
    }
    else if (g_state == UIState::LIVE_TEST) {
      if (btn == BE_SINGLE) {
        // Cancel Test (placeholder)
        enterState(UIState::RUN_TEST);
      }
    }
    else if (g_state == UIState::CREATE_TEST_MENU) {
      const char* item = CREATE_MENU_ITEMS[g_createMenuIndex];
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
    }
    else if (g_state == UIState::SAVE_TEST_NAME) {
      if (!g_nameAccepted) {
        if (btn == BE_SINGLE) {
          // First click enters EDIT mode; next click accepts the character and returns to MOVE mode
          g_nameEditMode = !g_nameEditMode;
          drawSaveNameScreen();
        } else if (btn == BE_DOUBLE) {
          // Double-click accepts the filename (does NOT save)
          g_nameAccepted = true;
          g_nameEditMode = false;
          g_saveNameMenuIndex = 0; // cursor lands on "Save Test"
          drawSaveNameScreen();
        }
      } else {
        if (btn == BE_SINGLE) {
          if (g_saveNameMenuIndex == 0) {
            bool ok = saveCreateParamsToSD(g_nameBuf);
            enterState(ok ? UIState::MAIN_MENU : UIState::CREATE_TEST_MENU);
          } else {
            enterState(UIState::CREATE_TEST_MENU);
          }
        } else if (btn == BE_DOUBLE) {
          // Optional: double-click returns to filename editing
          g_nameAccepted = false;
          g_nameEditMode = false;
          drawSaveNameScreen();
        }
      }
    }
    else if (g_state == UIState::CREATE_EDIT_PARAM) {
      enterState(UIState::CREATE_TEST_MENU);
    }
    else if (g_state == UIState::SD_TEST_LIST) {
      if (sdIsBackSelected()) {
        enterState(UIState::MAIN_MENU);
      } else if (g_sdTestCount > 0) {
        g_selectedTestIdx = g_sdMenuIndex;

        strncpy(g_selectedTestName, g_sdTestNames[g_selectedTestIdx], sizeof(g_selectedTestName) - 1);
        g_selectedTestName[sizeof(g_selectedTestName) - 1] = '\0';

        strncpy(g_selectedTestFile, g_sdTestFiles[g_selectedTestIdx], sizeof(g_selectedTestFile) - 1);
        g_selectedTestFile[sizeof(g_selectedTestFile) - 1] = '\0';

        enterState(UIState::SD_TEST_ACTIONS);
      }
    }
    else if (g_state == UIState::SD_TEST_ACTIONS) {
      const char* item = SD_ACTION_ITEMS[g_sdActionIndex];

      if (strcmp(item, "Back") == 0) {
        enterState(UIState::SD_TEST_LIST);
      }
      else if (strcmp(item, "Delete Test") == 0) {
        deleteSelectedTestFile();
        enterState(UIState::SD_TEST_LIST);
      }
      else if (strcmp(item, "View Test") == 0) {
        enterState(UIState::SD_TEST_VIEW);
      }
      else if (strcmp(item, "Load Test") == 0) {
        TestParams tp;
        if (loadSelectedTestForView(tp)) {
          g_activeParams = tp;
          g_createParams = tp;
        }
        enterState(UIState::SD_TEST_LIST);
      }
    }
    else if (g_state == UIState::SD_TEST_VIEW) {
      if (g_viewIndex == g_viewCount - 1) {
        enterState(UIState::SD_TEST_ACTIONS);
      }
    }
  }

  delay(5);
}