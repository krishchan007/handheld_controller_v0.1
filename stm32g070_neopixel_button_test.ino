#include <STM32FreeRTOS.h>
#include <AceButton.h>
#include <Adafruit_NeoPixel.h>

using namespace ace_button;

#define NUM_BUTTONS 6
#define NUMPIXELS   6

// Change this to your NeoPixel data pin
#define PIXEL_PIN   PA4
#define LED_CTRL    PA5
#define BUZZER_PIN  PA8

// Button pins
const uint8_t buttonPins[NUM_BUTTONS] = {
  PB2,  // BTN1
  PA3,  // BTN2
  PC6,  // BTN3
  PA1,  // BTN4
  PA15, // BTN5
  PA2   // BTN6
};

Adafruit_NeoPixel pixels(NUMPIXELS, PIXEL_PIN, NEO_GRB + NEO_KHZ800);

ButtonConfig buttonConfig;
AceButton buttons[NUM_BUTTONS] = {
  AceButton(&buttonConfig, buttonPins[0], HIGH, 0),
  AceButton(&buttonConfig, buttonPins[1], HIGH, 1),
  AceButton(&buttonConfig, buttonPins[2], HIGH, 2),
  AceButton(&buttonConfig, buttonPins[3], HIGH, 3),
  AceButton(&buttonConfig, buttonPins[4], HIGH, 4),
  AceButton(&buttonConfig, buttonPins[5], HIGH, 5)
};

static void setButtonLed(uint8_t idx, uint32_t color) {
  if (idx >= NUMPIXELS) return;
  pixels.setPixelColor(idx, color);
  pixels.show();
}

static void beepShort() {
  tone(BUZZER_PIN, 4000, 50);   // 4 kHz for 50 ms
}

static void handleEvent(AceButton* button, uint8_t eventType, uint8_t /*buttonState*/) {
  uint8_t idx = button->getId();

  switch (eventType) {
    case AceButton::kEventClicked:
      // Short press -> green
      setButtonLed(idx, pixels.Color(0, 150, 0));
      beepShort();
      break;

    case AceButton::kEventLongPressed:
      // Long press -> yellow
      setButtonLed(idx, pixels.Color(150, 150, 0));
      beepShort();
      break;
  }
}

void buttonTask(void *pvParameters) {
  (void) pvParameters;

  for (;;) {
    for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
      buttons[i].check();
    }
    vTaskDelay(pdMS_TO_TICKS(4));
  }
}

void setup() {
  // pinMode(LED_CTRL, OUTPUT);
  // digitalWrite(LED_CTRL, LOW);

  for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
    pinMode(buttonPins[i], INPUT_PULLUP);
  }

  pixels.begin();
  pixels.clear();
  pixels.show();

  buttonConfig.setEventHandler(handleEvent);
  buttonConfig.setFeature(ButtonConfig::kFeatureClick);
  buttonConfig.setFeature(ButtonConfig::kFeatureLongPress);

  // Adjust if you want a different long-press time
  buttonConfig.setLongPressDelay(800);

  xTaskCreate(
    buttonTask,
    "Buttons",
    256,
    nullptr,
    1,
    nullptr
  );

  vTaskStartScheduler();
}

void loop() {
  // Not used when FreeRTOS scheduler is running
}