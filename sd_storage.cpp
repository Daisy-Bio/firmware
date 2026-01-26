/**
 * @file sd_storage.cpp
 * @brief SD card storage operations implementation
 */

#include "sd_storage.h"
#include "config.h"
#include <SD.h>
#include <SPI.h>


// Global SD status
bool g_sdOK = false;

// SD test list storage
char g_sdTestNames[MAX_SD_TESTS][21];
char g_sdTestFiles[MAX_SD_TESTS][32];
int g_sdTestCount = 0;

int clampInt(int v, int lo, int hi) {
  if (v < lo)
    return lo;
  if (v > hi)
    return hi;
  return v;
}

bool endsWithIgnoreCase(const char *s, const char *suffix) {
  size_t ls = strlen(s), lf = strlen(suffix);
  if (lf > ls)
    return false;
  const char *p = s + (ls - lf);
  for (size_t i = 0; i < lf; i++) {
    char a = p[i], b = suffix[i];
    if (a >= 'a' && a <= 'z')
      a -= 32;
    if (b >= 'a' && b <= 'z')
      b -= 32;
    if (a != b)
      return false;
  }
  return true;
}

bool initSDCard() {
  SPI.begin(SD_SCLK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
  if (!SD.begin(SD_CS_PIN, SPI)) {
    Serial.println("SD init failed");
    return false;
  }
  Serial.println("SD card OK");
  return true;
}

void ensureTestsDir() {
  if (!g_sdOK)
    return;
  if (!SD.exists("/TESTS"))
    SD.mkdir("/TESTS");
}

void clearSDTestList() {
  g_sdTestCount = 0;
  for (int i = 0; i < MAX_SD_TESTS; i++) {
    g_sdTestNames[i][0] = '\0';
    g_sdTestFiles[i][0] = '\0';
  }
}

void filenameToDisplayName(const char *filename, char *out20) {
  int j = 0;
  for (int i = 0; filename[i] != '\0' && j < 20; i++) {
    if (filename[i] == '.')
      break;
    out20[j++] = filename[i];
  }
  out20[j] = '\0';
}

void buildTestPath(char *out, size_t outSz, const char *base16) {
  char base[17];
  strncpy(base, base16, 16);
  base[16] = 0;
  // trim trailing underscores
  for (int i = 15; i >= 0; i--) {
    if (base[i] == '_')
      base[i] = 0;
    else
      break;
  }
  if (base[0] == 0)
    strncpy(base, "TEST", sizeof(base) - 1);

  snprintf(out, outSz, "/TESTS/%s.TXT", base);
}

void scanTestsOnSD() {
  clearSDTestList();
  if (!g_sdOK)
    return;
  if (!SD.exists("/TESTS"))
    return;

  File dir = SD.open("/TESTS");
  if (!dir || !dir.isDirectory()) {
    if (dir)
      dir.close();
    return;
  }

  while (true) {
    File f = dir.openNextFile();
    if (!f)
      break;

    if (!f.isDirectory()) {
      const char *fn = f.name();
      const char *base = strrchr(fn, '/');
      base = (base) ? (base + 1) : fn;

      if (endsWithIgnoreCase(base, ".TXT")) {
        if (g_sdTestCount < MAX_SD_TESTS) {
          strncpy(g_sdTestFiles[g_sdTestCount], base,
                  sizeof(g_sdTestFiles[g_sdTestCount]) - 1);
          g_sdTestFiles[g_sdTestCount]
                       [sizeof(g_sdTestFiles[g_sdTestCount]) - 1] = '\0';

          filenameToDisplayName(base, g_sdTestNames[g_sdTestCount]);
          g_sdTestCount++;
        }
      }
    }
    f.close();
  }
  dir.close();
}

static void parseKeyValueLineIntoParams(const String &line, TestParams &p) {
  int eq = line.indexOf('=');
  if (eq <= 0)
    return;

  String key = line.substring(0, eq);
  String val = line.substring(eq + 1);
  key.trim();
  val.trim();
  int iv = val.toInt();

  if (key == "Temp_Init_Denat")
    p.Temp_Init_Denat = clampInt(iv, 25, 125);
  else if (key == "Time_Init_Denat")
    p.Time_Init_Denat = clampInt(iv, 0, 600);
  else if (key == "Temp_Denat")
    p.Temp_Denat = clampInt(iv, 25, 125);
  else if (key == "Time_Denat")
    p.Time_Denat = clampInt(iv, 0, 600);
  else if (key == "Temp_Anneal")
    p.Temp_Anneal = clampInt(iv, 25, 125);
  else if (key == "Time_Anneal")
    p.Time_Anneal = clampInt(iv, 0, 600);
  else if (key == "Temp_Extension")
    p.Temp_Extension = clampInt(iv, 25, 125);
  else if (key == "Time_Extension")
    p.Time_Extension = clampInt(iv, 0, 600);
  else if (key == "Num_Cycles")
    p.Num_Cycles = clampInt(iv, 1, 99);
  else if (key == "Temp_Final_Ext")
    p.Temp_Final_Ext = clampInt(iv, 25, 125);
  else if (key == "Time_Final_Ext")
    p.Time_Final_Ext = clampInt(iv, 0, 600);
}

bool loadSelectedTestForView(TestParams &out, const char *filename) {
  out = TestParams(); // defaults
  if (!g_sdOK || filename[0] == '\0')
    return false;

  char path[64];
  snprintf(path, sizeof(path), "/TESTS/%s", filename);

  File f = SD.open(path, FILE_READ);
  if (!f)
    return false;

  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length() == 0)
      continue;
    parseKeyValueLineIntoParams(line, out);
  }
  f.close();
  return true;
}

bool saveTestParamsToSD(const char *base16, const TestParams &params) {
  if (!g_sdOK)
    return false;
  ensureTestsDir();

  char path[64];
  buildTestPath(path, sizeof(path), base16);

  // FILE_WRITE appends; remove first to overwrite cleanly
  if (SD.exists(path))
    SD.remove(path);

  File f = SD.open(path, FILE_WRITE);
  if (!f)
    return false;

  f.printf("Temp_Init_Denat=%d\n", params.Temp_Init_Denat);
  f.printf("Time_Init_Denat=%d\n", params.Time_Init_Denat);
  f.printf("Temp_Denat=%d\n", params.Temp_Denat);
  f.printf("Time_Denat=%d\n", params.Time_Denat);
  f.printf("Temp_Anneal=%d\n", params.Temp_Anneal);
  f.printf("Time_Anneal=%d\n", params.Time_Anneal);
  f.printf("Temp_Extension=%d\n", params.Temp_Extension);
  f.printf("Time_Extension=%d\n", params.Time_Extension);
  f.printf("Num_Cycles=%d\n", params.Num_Cycles);
  f.printf("Temp_Final_Ext=%d\n", params.Temp_Final_Ext);
  f.printf("Time_Final_Ext=%d\n", params.Time_Final_Ext);

  f.close();
  return true;
}

void deleteTestFile(const char *filename) {
  if (!g_sdOK)
    return;
  if (filename[0] == '\0')
    return;

  char path[64];
  snprintf(path, sizeof(path), "/TESTS/%s", filename);
  SD.remove(path);
}
