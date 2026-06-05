#include <Arduino.h>
#include <Wire.h>

#include <IP5306.h>

namespace {
HardwareSerial DebugSerial(PA10, PA9);
IP5306 charger;

constexpr uint8_t kShortPressBit = 0;
constexpr uint8_t kLongPressBit = 1;
constexpr uint8_t kDoublePressBit = 2;

bool readPressFlags(uint8_t &flags) {
  return charger.readRegister(IP5306::REG_READ_3, flags);
}

void printFlagState(uint8_t flags) {
  DebugSerial.print("READ3=0x");
  if (flags < 0x10) {
    DebugSerial.print('0');
  }
  DebugSerial.print(flags, HEX);
  DebugSerial.print(" -> ");

  bool printed = false;
  if (flags & (1U << kShortPressBit)) {
    DebugSerial.print("SHORT_PRESS ");
    printed = true;
  }
  if (flags & (1U << kLongPressBit)) {
    DebugSerial.print("LONG_PRESS ");
    printed = true;
  }
  if (flags & (1U << kDoublePressBit)) {
    DebugSerial.print("DOUBLE_PRESS ");
    printed = true;
  }

  if (!printed) {
    DebugSerial.print("no_key_event");
  }

  DebugSerial.println();
}

void clearReportedFlags(uint8_t flags) {
  if (flags & (1U << kShortPressBit)) {
    charger.writeRegister(IP5306::REG_READ_3, static_cast<uint8_t>(1U << kShortPressBit));
  }
  if (flags & (1U << kLongPressBit)) {
    charger.writeRegister(IP5306::REG_READ_3, static_cast<uint8_t>(1U << kLongPressBit));
  }
  if (flags & (1U << kDoublePressBit)) {
    charger.writeRegister(IP5306::REG_READ_3, static_cast<uint8_t>(1U << kDoublePressBit));
  }
}
}  // namespace

void setup() {
  DebugSerial.begin(115200);
  delay(200);

  DebugSerial.println();
  DebugSerial.println("--- IP5306 Key Test ---");

  Wire.begin();
  delay(10);

if (charger.begin()) {

  IP5306::PowerMode currentMode;

  if (charger.getPowerMode(currentMode)) {
    DebugSerial.print("Current power mode: ");
    DebugSerial.println(
        currentMode == IP5306::PowerMode::Normal    ? "Normal" :
        currentMode == IP5306::PowerMode::Standby   ? "Standby" :
        currentMode == IP5306::PowerMode::PowerDown ? "PowerDown" :
                                                      "Unknown");

    if (currentMode != IP5306::PowerMode::Standby) {
      DebugSerial.println("Changing power mode to Standby...");

      if (!charger.setPowerMode(IP5306::PowerMode::Standby)) {
        DebugSerial.println("Power mode write failed or did not verify.");
      } else {
        IP5306::PowerMode newMode;
        if (charger.getPowerMode(newMode)) {
          DebugSerial.print("Power mode now: ");
          DebugSerial.println(
              newMode == IP5306::PowerMode::Normal    ? "Normal" :
              newMode == IP5306::PowerMode::Standby   ? "Standby" :
              newMode == IP5306::PowerMode::PowerDown ? "PowerDown" :
                                                        "Unknown");
        }
      }
    } else {
      DebugSerial.println("Already in Standby mode.");
    }

  } else {
    DebugSerial.println("Failed to read current power mode.");
  }

  DebugSerial.print("IP5306 detected at 0x");
  DebugSerial.println(charger.address(), HEX);
  DebugSerial.println("Watching READ3 for short/long/double key events...");

} else {
  DebugSerial.println("IP5306 not detected or not responding.");
}
}

void loop() {
  static uint8_t lastFlags = 0xFF;
  uint8_t flags = 0;

  if (!readPressFlags(flags)) {
    DebugSerial.println("Failed to read key status register.");
    delay(250);
    return;
  }

  if (flags != lastFlags) {
    printFlagState(flags);
    clearReportedFlags(flags);
    lastFlags = flags;
  }

  delay(100);
}
