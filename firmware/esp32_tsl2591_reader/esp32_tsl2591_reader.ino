#include <Arduino.h>
#include <Wire.h>
#include <stdarg.h>

static const int SDA_PIN = 8;
static const int SCL_PIN = 9;
static const uint8_t TSL2591_ADDR = 0x29;
static const uint8_t TSL2591_CMD = 0xA0;

static void logPrint(const char *s) {
  Serial.print(s);
  Serial0.print(s);
}

static void logPrintln(const char *s) {
  Serial.println(s);
  Serial0.println(s);
}

static void logPrintf(const char *fmt, ...) {
  char buf[192];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  Serial.print(buf);
  Serial0.print(buf);
}

static bool i2cWrite8(uint8_t addr, uint8_t reg, uint8_t value) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

static bool i2cReadBytes(uint8_t addr, uint8_t reg, uint8_t *buf, size_t n) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((int)addr, (int)n) != (int)n) return false;
  for (size_t i = 0; i < n; i++) buf[i] = Wire.read();
  return true;
}

static bool i2cPresent(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

static bool tslWrite8(uint8_t reg, uint8_t value) {
  return i2cWrite8(TSL2591_ADDR, TSL2591_CMD | reg, value);
}

static bool tslRead16(uint8_t reg, uint16_t &value) {
  uint8_t raw[2] = {0, 0};
  if (!i2cReadBytes(TSL2591_ADDR, TSL2591_CMD | reg, raw, 2)) return false;
  value = ((uint16_t)raw[1] << 8) | raw[0];
  return true;
}

static bool tslConfigure() {
  bool ok = true;
  ok = tslWrite8(0x00, 0x03) && ok;  // ENABLE: PON + AEN
  ok = tslWrite8(0x01, 0x00) && ok;  // CONTROL: low gain, 100 ms integration
  delay(120);
  return ok;
}

static void scanI2C() {
  logPrint("# I2C:");
  for (uint8_t addr = 1; addr < 127; addr++) {
    if (i2cPresent(addr)) {
      logPrint(" 0x");
      if (addr < 16) logPrint("0");
      logPrintf("%X", addr);
    }
  }
  logPrintln("");
}

void setup() {
  Serial.begin(115200);
  Serial0.begin(115200);
  delay(800);

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000);

  logPrintln("# ESP32-S3 TSL2591 reader");
  logPrintln("# Wiring: 3V3->VCC/VIN, GND->GND, IO9->SCL, IO8->SDA");
  scanI2C();

  bool present = i2cPresent(TSL2591_ADDR);
  bool configured = present && tslConfigure();
  logPrintf("# TSL2591 addr=0x29 present=%d configured=%d\n", present ? 1 : 0, configured ? 1 : 0);
  logPrintln("t_ms,present,configured,full,ir,visible");
}

void loop() {
  static uint32_t lastScan = 0;
  uint16_t full = 0;
  uint16_t ir = 0;
  uint16_t visible = 0;
  bool present = i2cPresent(TSL2591_ADDR);
  bool configured = present;

  if (present) {
    configured = tslConfigure();
    bool ok0 = tslRead16(0x14, full);
    bool ok1 = tslRead16(0x16, ir);
    configured = configured && ok0 && ok1;
    visible = (full > ir) ? (uint16_t)(full - ir) : 0;
  }

  logPrintf("%lu,%d,%d,%u,%u,%u\n",
            (unsigned long)millis(),
            present ? 1 : 0,
            configured ? 1 : 0,
            full,
            ir,
            visible);

  if (millis() - lastScan > 5000) {
    scanI2C();
    lastScan = millis();
  }

  delay(100);
}
