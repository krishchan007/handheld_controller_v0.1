#include <Arduino.h>
#include <Wire.h>

#include <IP5306.h>

namespace {
HardwareSerial DebugSerial(PA10, PA9);
IP5306 charger;

void printResetCause() {
  const uint32_t csr = RCC->CSR;
  DebugSerial.print("Reset cause CSR=0x");
  DebugSerial.println(csr, HEX);
  RCC->CSR |= RCC_CSR_RMVF;
}

bool reinitWire() {
  Wire.end();
  delay(20);
  Wire.begin();
  delay(20);
  return charger.begin();
}

void printSys2State() {
  uint8_t raw = 0;
  uint8_t ll = 0;

  if (!charger.readRegister(IP5306::REG_SYS_2, raw)) {
    DebugSerial.println("Failed to read SYS_2.");
    return;
  }

  if (!charger.getLightLoadShutdownTime(ll)) {
    DebugSerial.println("Failed to decode light-load shutdown setting.");
    return;
  }

  DebugSerial.print("SYS_2=0x");
  if (raw < 0x10) {
    DebugSerial.print('0');
  }
  DebugSerial.print(raw, HEX);
  DebugSerial.print(" light_load_shutdown=");
  DebugSerial.println(ll);
}

void printPowerStatus() {
  uint8_t sys0 = 0;
  uint8_t read0 = 0;
  uint8_t read1 = 0;
  uint8_t read2 = 0;
  uint8_t read4 = 0;
  IP5306::PowerMode mode;

  if (!charger.readRegister(IP5306::REG_SYS_0, sys0) ||
      !charger.readRegister(IP5306::REG_READ_0, read0) ||
      !charger.readRegister(IP5306::REG_READ_1, read1) ||
      !charger.readRegister(IP5306::REG_READ_2, read2) ||
      !charger.readRegister(IP5306::REG_READ_4, read4) ||
      !charger.getPowerMode(mode)) {
    DebugSerial.println("Failed to read power status.");
    return;
  }

  const bool boost_output = (sys0 >> 1) & 0x01;
  const bool power_on_load = (sys0 >> 2) & 0x01;
  const bool charger_enabled = (sys0 >> 4) & 0x01;
  const bool boost_enabled = (sys0 >> 5) & 0x01;
  const bool vin_present = (read0 >> 3) & 0x01;
  const bool battery_full = (read1 >> 3) & 0x01;
  const bool light_load = (read2 >> 2) & 0x01;
  const uint8_t leds = static_cast<uint8_t>((~(read4 >> 4)) & 0x0F);

  DebugSerial.print("SYS0=0x");
  if (sys0 < 0x10) DebugSerial.print('0');
  DebugSerial.print(sys0, HEX);
  DebugSerial.print(" VIN=");
  DebugSerial.print(vin_present ? "VIN" : "BAT");
  DebugSerial.print(" BFULL=");
  DebugSerial.print(battery_full ? "1" : "0");
  DebugSerial.print(" LLOAD=");
  DebugSerial.print(light_load ? "1" : "0");
  DebugSerial.print(" LEDs=");
  DebugSerial.print(leds, BIN);
  DebugSerial.print(" BOOT=");
  DebugSerial.print(boost_output ? "1" : "0");
  DebugSerial.print(" POL=");
  DebugSerial.print(power_on_load ? "1" : "0");
  DebugSerial.print(" CHG=");
  DebugSerial.print(charger_enabled ? "1" : "0");
  DebugSerial.print(" BOOST=");
  DebugSerial.print(boost_enabled ? "1" : "0");
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
  DebugSerial.println(">>> IP5306 LIGHT LOAD SHUTDOWN TEST <<<");
  printResetCause();

  Wire.begin();
  delay(10);

  if (!charger.begin()) {
    DebugSerial.println("IP5306 not detected or not responding.");
    return;
  }

  DebugSerial.print("IP5306 detected at 0x");
  DebugSerial.println(charger.address(), HEX);

  if (charger.setNormalMode()) {
    DebugSerial.println("setNormalMode() write verified.");
  } else {
    DebugSerial.println("setNormalMode() failed or did not verify.");
  }

  if (charger.disableLightLoadShutdown()) {
    DebugSerial.println("disableLightLoadShutdown() write verified.");
  } else {
    DebugSerial.println("disableLightLoadShutdown() failed or did not verify.");
  }

  printSys2State();
  printPowerStatus();
}

void loop() {
  delay(5000);
  uint8_t probe = 0;
  if (!charger.readRegister(IP5306::REG_SYS_2, probe)) {
    DebugSerial.println("I2C failed, attempting Wire reinit...");
    if (!reinitWire()) {
      DebugSerial.println("Wire reinit failed.");
      return;
    }
    DebugSerial.println("Wire reinit succeeded.");
  }
  printSys2State();
  printPowerStatus();
}
