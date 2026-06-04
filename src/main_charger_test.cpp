#include <Arduino.h>
#include <Wire.h>

#include <IP5306.h>

namespace {
HardwareSerial DebugSerial(PA10, PA9);
IP5306 charger;

struct RegisterInfo {
  uint8_t address;
  const char *name;
};

constexpr RegisterInfo kRegisters[] = {
    {IP5306::REG_SYS_0, "SYS_CTL0"},
    {IP5306::REG_SYS_1, "SYS_CTL1"},
    {IP5306::REG_SYS_2, "SYS_CTL2"},
    {IP5306::REG_CHG_0, "CHG_CTL0"},
    {IP5306::REG_CHG_1, "CHG_CTL1"},
    {IP5306::REG_CHG_2, "CHG_CTL2"},
    {IP5306::REG_CHG_3, "CHG_CTL3"},
    {IP5306::REG_CHG_4, "CHG_CTL4"},
    {IP5306::REG_READ_0, "READ0"},
    {IP5306::REG_READ_1, "READ1"},
    {IP5306::REG_READ_2, "READ2"},
    {IP5306::REG_READ_3, "READ3"},
    {IP5306::REG_READ_4, "READ4"},
};

void printRegisterDump() {
  DebugSerial.println("IP5306 register dump:");

  for (const auto &reg : kRegisters) {
    uint8_t value = 0;
    DebugSerial.print("  0x");
    if (reg.address < 0x10) {
      DebugSerial.print('0');
    }
    DebugSerial.print(reg.address, HEX);
    DebugSerial.print(" (");
    DebugSerial.print(reg.name);
    DebugSerial.print(") = ");

    if (charger.readRegister(reg.address, value)) {
      DebugSerial.print("0x");
      if (value < 0x10) {
        DebugSerial.print('0');
      }
      DebugSerial.println(value, HEX);
    } else {
      DebugSerial.println("read failed");
    }
  }
}
}  // namespace

void setup() {
  DebugSerial.begin(115200);
  delay(200);

  DebugSerial.println();
  DebugSerial.println("--- IP5306 Charger Test ---");

  Wire.begin();
  delay(10);

  if (charger.begin()) {
    DebugSerial.print("IP5306 detected at 0x");
    DebugSerial.println(charger.address(), HEX);
    printRegisterDump();
  } else {
    DebugSerial.println("IP5306 not detected or not responding.");
  }
}

void loop() {
  delay(5000);
  printRegisterDump();
}
