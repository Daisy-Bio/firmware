/**
 * @file oled.cpp
 * @brief US2066 OLED display driver implementation
 */

#include "oled.h"
#include "config.h"
#include <Wire.h>

void oledWrite(uint8_t control, const uint8_t *data, size_t len) {
  Wire.beginTransmission(OLED_ADDR);
  Wire.write(control);
  Wire.write(data, len);
  Wire.endTransmission();
}

void oledCmd(uint8_t c) {
  oledWrite(CTRL_CMD, &c, 1);
  delayMicroseconds(50);
}

void oledData(uint8_t d) { oledWrite(CTRL_DATA, &d, 1); }

void oledSetCursor(uint8_t col, uint8_t row) {
  if (row > 3)
    row = 3;
  if (col > 19)
    col = 19;
  oledCmd(0x80 | (ROW_ADDR[row] + col));
}

void oledClearAndHome() {
  oledCmd(0x01);
  delay(3);
  oledCmd(0x02);
  delay(3);
}

void oledHardwareReset() {
  if (OLED_RESET_PIN < 0)
    return;
  pinMode((uint8_t)OLED_RESET_PIN, OUTPUT);
  digitalWrite((uint8_t)OLED_RESET_PIN, LOW);
  delay(50);
  digitalWrite((uint8_t)OLED_RESET_PIN, HIGH);
  delay(100);
}

void oledInitOnce() {
  // US2066 initialization sequence for I2C, 3.3V mode
  oledCmd(0x2A);  // Extended instruction set (RE=1)
  oledCmd(0x71);  // Function selection A
  oledData(0x00); // CRITICAL: Send as DATA byte for 3.3V mode

  // IMPORTANT: This command while still in the extended instruction set
  // stabilizes orientation / mirroring behavior on some US2066 modules.
  oledCmd(0x06);

  oledCmd(0x28); // Fundamental instruction set (RE=0)
  oledCmd(0x08); // Display off

  oledClearAndHome();

  oledCmd(0x06); // Entry mode: increment, no shift
  oledCmd(0x0C); // Display on, cursor off, blink off
}

void oledInitDeterministic() {
  delay(200);
  oledHardwareReset();
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(100000);
  delay(50);
  oledInitOnce();
  delay(20);
  oledInitOnce();
}

void oledWriteLine(uint8_t row, const char *text) {
  char buf[21];
  memset(buf, ' ', 20);
  buf[20] = '\0';
  size_t n = strlen(text);
  if (n > 20)
    n = 20;
  memcpy(buf, text, n);
  for (uint8_t col = 0; col < 20; col++) {
    oledSetCursor(col, row);
    oledData((uint8_t)buf[col]);
  }
}
