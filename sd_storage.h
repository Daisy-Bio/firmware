/**
 * @file sd_storage.h
 * @brief SD card storage operations interface
 */

#ifndef SD_STORAGE_H
#define SD_STORAGE_H

#include "test_params.h"
#include <Arduino.h>


// Global SD status
extern bool g_sdOK;

// SD test list storage
static constexpr int MAX_SD_TESTS = 40;
extern char g_sdTestNames[MAX_SD_TESTS][21];
extern char g_sdTestFiles[MAX_SD_TESTS][32];
extern int g_sdTestCount;

/**
 * @brief Initialize the SD card
 */
bool initSDCard();

/**
 * @brief Ensure /TESTS directory exists
 */
void ensureTestsDir();

/**
 * @brief Scan /TESTS directory for .TXT files
 */
void scanTestsOnSD();

/**
 * @brief Clear the SD test list
 */
void clearSDTestList();

/**
 * @brief Load test parameters from file
 */
bool loadSelectedTestForView(TestParams &out, const char *filename);

/**
 * @brief Save test parameters to SD
 */
bool saveTestParamsToSD(const char *base16, const TestParams &params);

/**
 * @brief Delete a test file
 */
void deleteTestFile(const char *filename);

/**
 * @brief Convert filename to display name (strip extension)
 */
void filenameToDisplayName(const char *filename, char *out20);

/**
 * @brief Build full path from base name
 */
void buildTestPath(char *out, size_t outSz, const char *base16);

/**
 * @brief Clamp integer to range
 */
int clampInt(int v, int lo, int hi);

/**
 * @brief Check if string ends with suffix (case insensitive)
 */
bool endsWithIgnoreCase(const char *s, const char *suffix);

#endif // SD_STORAGE_H
