#include <Arduino.h>
#include <Wire.h>

// UART Debug Serial
HardwareSerial DebugSerial(PA10, PA9);

#define WAKE_PIN  PA0   // WKUP1 pin is PA0 on STM32G070

// The clock config function provided in the project
extern "C" void SystemClock_Config(void);

void enterShutdownMode() {
  DebugSerial.println("Preparing to enter SHUTDOWN mode...");
  DebugSerial.flush();
  DebugSerial.end();
  Wire.end();

  // Configure all pins to ANALOG to prevent leakage, except WAKE_PIN
  const uint8_t allPins[] = {
    PA1, PA2, PA3, PA4, PA5, PA6, PA7, PA8, PA9, PA10, PA11, PA12, PA13, PA14, PA15,
    PB0, PB1, PB2, PB3, PB4, PB5, PB6, PB7, PB8, PB9,
    PC6, PC14, PC15
  };
  for (uint8_t pin : allPins) {
    pinMode(pin, INPUT_ANALOG);
  }

  // Enable Power clock
  __HAL_RCC_PWR_CLK_ENABLE();

  // Enable the internal pull-down on PA0 (WKUP1) during Shutdown/Standby 
  // to prevent it from floating and triggering an immediate wakeup.
  HAL_PWREx_EnableGPIOPullDown(PWR_GPIO_A, PWR_GPIO_BIT_0);
  HAL_PWREx_EnablePullUpPullDownConfig();

  // Disable all wakeup pins first
  HAL_PWR_DisableWakeUpPin(PWR_WAKEUP_PIN1);

  // Clear wakeup flags
  __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WUF1);

  // Enable WKUP1 (PA0) on RISING edge (assuming active-high button press)
  HAL_PWR_EnableWakeUpPin(PWR_WAKEUP_PIN1_HIGH);

  // Enter Shutdown Mode (Deepest Low Power State)
  HAL_PWREx_EnterSHUTDOWNMode();
}

void setup() {
  // Initialize UART
  DebugSerial.begin(115200);
  delay(1000); 

  DebugSerial.println("\n--- STM32G070 Shutdown Mode Test ---");
  DebugSerial.println("Boot up / Wakeup Reset complete.");

  // Configure PA0 (WAKE_PIN) as input with internal pull-down for active-high wake logic
  pinMode(WAKE_PIN, INPUT_PULLDOWN);

  // 5-second delay before sleeping to allow easy reprogramming
  DebugSerial.println("Waiting 5 seconds before entering shutdown...");
  for (int i = 5; i > 0; i--) {
    DebugSerial.print(i);
    DebugSerial.print("... ");
    delay(1000);
  }
  
  enterShutdownMode();
}

void loop() {
  // We will never reach here because Shutdown mode wakes up via system reset.
}
