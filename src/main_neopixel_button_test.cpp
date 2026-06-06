#include <Arduino.h>
#include <AceButton.h>
#include <Adafruit_NeoPixel.h>
#include <Wire.h>

#include <IP5306.h>

using namespace ace_button;

#define NUM_BUTTONS 6
#define NUMPIXELS   6

#define PIXEL_PIN   PA4
#define LED_CTRL    PA5
#define BUZZER_PIN  PA8

const uint8_t buttonPins[NUM_BUTTONS] = {
  PB2,
  PA3,
  PC6,
  PA1,
  PA15,
  PA2
};

HardwareSerial DebugSerial(PA10, PA9);
Adafruit_NeoPixel pixels(NUMPIXELS, PIXEL_PIN, NEO_GRB + NEO_KHZ800);
ButtonConfig buttonConfig;
AceButton button0(&buttonConfig, buttonPins[0], HIGH, 0);
AceButton button1(&buttonConfig, buttonPins[1], HIGH, 1);
AceButton button2(&buttonConfig, buttonPins[2], HIGH, 2);
AceButton button3(&buttonConfig, buttonPins[3], HIGH, 3);
AceButton button4(&buttonConfig, buttonPins[4], HIGH, 4);
AceButton button5(&buttonConfig, buttonPins[5], HIGH, 5);
IP5306 charger;

AceButton* buttons[NUM_BUTTONS] = {
  &button0,
  &button1,
  &button2,
  &button3,
  &button4,
  &button5
};

static void setButtonLed(uint8_t idx, uint32_t color) {
  if (idx >= NUMPIXELS) return;
  pixels.setPixelColor(idx, color);
  pixels.show();
}

static void beepShort() {
  tone(BUZZER_PIN, 4000, 50);
}

static void handleEvent(AceButton* button, uint8_t eventType, uint8_t /*buttonState*/) {
  uint8_t idx = button->getId();

  switch (eventType) {
    case AceButton::kEventClicked:
      setButtonLed(idx, pixels.Color(0, 150, 0));
      beepShort();
      break;

    case AceButton::kEventLongPressed:
      setButtonLed(idx, pixels.Color(150, 150, 0));
      beepShort();
      break;
  }
}

static void printPowerStatus() {
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

static void printStartupSummary() {
  uint8_t ll = 0;
  uint8_t sys0 = 0;
  IP5306::PowerMode mode;

  if (!charger.getLightLoadShutdownTime(ll) ||
      !charger.readRegister(IP5306::REG_SYS_0, sys0) ||
      !charger.getPowerMode(mode)) {
    DebugSerial.println("Failed to read startup summary.");
    return;
  }

  DebugSerial.print("Startup: mode=");
  DebugSerial.print(
      mode == IP5306::PowerMode::Normal ? "Normal" :
      mode == IP5306::PowerMode::Standby ? "Standby" :
      mode == IP5306::PowerMode::PowerDown ? "PowerDown" : "Unknown");
  DebugSerial.print(" light_load_shutdown=");
  DebugSerial.print(ll);
  DebugSerial.print(" boost_output=");
  DebugSerial.println((sys0 >> 1) & 0x01);
}

void setup() {
  DebugSerial.begin(115200);
  delay(200);

  DebugSerial.println();
  DebugSerial.println(">>> STM32G070 NEOPIXEL BUTTON TEST <<<");
  beepShort();
  delay(200);
  pinMode(LED_CTRL, OUTPUT);
  digitalWrite(LED_CTRL, LOW);
  delay(10);

  Wire.begin();
  delay(10);

  if (charger.begin()) {
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
    printStartupSummary();
    printPowerStatus();
  } else {
    DebugSerial.println("IP5306 not detected or not responding.");
  }

  for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
    pinMode(buttonPins[i], INPUT_PULLUP);
  }

  pixels.begin();
  pixels.clear();
  pixels.show();

  buttonConfig.setEventHandler(handleEvent);
  buttonConfig.setFeature(ButtonConfig::kFeatureClick);
  buttonConfig.setFeature(ButtonConfig::kFeatureLongPress);
  buttonConfig.setLongPressDelay(800);

}

void loop() {
  for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
    buttons[i]->check();
  }
  delay(4);
}
