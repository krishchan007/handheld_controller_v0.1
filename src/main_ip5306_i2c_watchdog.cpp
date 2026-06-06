#include <Arduino.h>
#include <Wire.h>

#include <IP5306.h>

namespace {
HardwareSerial DebugSerial(PA10, PA9);
IP5306 charger;
constexpr IP5306::PowerMode kStartupMode = IP5306::PowerMode::Normal;  // Switch to Normal / Standby

bool initI2C() {
  Wire.begin();
  delay(10);
  return charger.begin();
}

bool applyStartupConfig() {
  bool ok = true;

  if (!charger.setPowerMode(kStartupMode)) {
    DebugSerial.println("setPowerMode() failed.");
    ok = false;
  } else {
    DebugSerial.println("setPowerMode() verified.");
  }

  if (!charger.setBoostOffOnLongPress(true)) {
    DebugSerial.println("setBoostOffOnLongPress() failed.");
    ok = false;
  } else {
    DebugSerial.println("setBoostOffOnLongPress() verified.");
  }

  if (!charger.setFlashlightOnLongPress(false)) {
    DebugSerial.println("setFlashlightOnLongPress() failed.");
    ok = false;
  } else {
    DebugSerial.println("setFlashlightOnLongPress() verified.");
  }

  if (!charger.setLongPressTime(false)) {
    DebugSerial.println("setLongPressTime() failed.");
    ok = false;
  } else {
    DebugSerial.println("setLongPressTime() verified.");
  }

  if (!charger.disableLightLoadShutdown()) {
    DebugSerial.println("disableLightLoadShutdown() failed.");
    ok = false;
  } else {
    DebugSerial.println("disableLightLoadShutdown() verified.");
  }

  return ok;
}

void printStatusLine() {
  uint8_t sys0 = 0;
  uint8_t read0 = 0;
  uint8_t read1 = 0;
  uint8_t read2 = 0;
  uint8_t read4 = 0;
  uint8_t ll = 0;
  IP5306::PowerMode mode;

  if (!charger.readRegister(IP5306::REG_SYS_0, sys0) ||
      !charger.readRegister(IP5306::REG_READ_0, read0) ||
      !charger.readRegister(IP5306::REG_READ_1, read1) ||
      !charger.readRegister(IP5306::REG_READ_2, read2) ||
      !charger.readRegister(IP5306::REG_READ_4, read4) ||
      !charger.getLightLoadShutdownTime(ll) ||
      !charger.getPowerMode(mode)) {
    DebugSerial.println("I2C read failed.");
    return;
  }

  DebugSerial.print("SYS0=0x");
  if (sys0 < 0x10) DebugSerial.print('0');
  DebugSerial.print(sys0, HEX);
  DebugSerial.print(" VIN=");
  DebugSerial.print(((read0 >> 3) & 0x01) ? "VIN" : "BAT");
  DebugSerial.print(" BFULL=");
  DebugSerial.print(((read1 >> 3) & 0x01) ? "1" : "0");
  DebugSerial.print(" LLOAD=");
  DebugSerial.print(((read2 >> 2) & 0x01) ? "1" : "0");
  DebugSerial.print(" LEDs=");
  DebugSerial.print(static_cast<uint8_t>((~(read4 >> 4)) & 0x0F), BIN);
  DebugSerial.print(" BOOT=");
  DebugSerial.print((sys0 >> 1) & 0x01);
  DebugSerial.print(" POL=");
  DebugSerial.print((sys0 >> 2) & 0x01);
  DebugSerial.print(" CHG=");
  DebugSerial.print((sys0 >> 4) & 0x01);
  DebugSerial.print(" BOOST=");
  DebugSerial.print((sys0 >> 5) & 0x01);
  DebugSerial.print(" LL=");
  DebugSerial.print(ll);
  bool boostOffOnLongPress = false;
  if (charger.getBoostOffOnLongPress(boostOffOnLongPress)) {
    DebugSerial.print(" BOOST_OFF_LONG=");
    DebugSerial.print(boostOffOnLongPress ? "1" : "0");
  }
  bool flashlightOnLongPress = false;
  if (charger.getFlashlightOnLongPress(flashlightOnLongPress)) {
    DebugSerial.print(" FLASH_LONG=");
    DebugSerial.print(flashlightOnLongPress ? "1" : "0");
  }
  bool threeSeconds = false;
  if (charger.getLongPressTime(threeSeconds)) {
    DebugSerial.print(" LP_TIME=");
    DebugSerial.print(threeSeconds ? "3s" : "2s");
  }
  DebugSerial.print(" MODE=");
  DebugSerial.println(
      mode == IP5306::PowerMode::Normal ? "Normal" :
      mode == IP5306::PowerMode::Standby ? "Standby" :
      mode == IP5306::PowerMode::PowerDown ? "PowerDown" : "Unknown");
}
}  // namespace

void setup() {
  DebugSerial.begin(115200);
  delay(200);

  DebugSerial.println();
  DebugSerial.println(">>> IP5306 I2C WATCHDOG TEST <<<");

  if (!initI2C()) {
    DebugSerial.println("IP5306 not detected.");
    return;
  }

  DebugSerial.print("IP5306 detected at 0x");
  DebugSerial.println(charger.address(), HEX);

  applyStartupConfig();
  printStatusLine();
}

void loop() {
  static uint32_t lastPoll = 0;
  const uint32_t now = millis();
  if (now - lastPoll < 1000) {
    delay(10);
    return;
  }
  lastPoll = now;

  uint8_t probe = 0;
  if (!charger.readRegister(IP5306::REG_SYS_0, probe)) {
    DebugSerial.println("I2C lost, reinitializing...");
    if (!initI2C()) {
      DebugSerial.println("Reinit failed.");
      return;
    }
    DebugSerial.print("Re-detected IP5306 at 0x");
    DebugSerial.println(charger.address(), HEX);
    applyStartupConfig();
    printStatusLine();
    return;
  }

  printStatusLine();
}
