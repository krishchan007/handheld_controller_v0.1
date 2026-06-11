#include <Arduino.h>
#include <Wire.h>

// UART Debug Serial
HardwareSerial DebugSerial(PA10, PA9);

#define WAKE_PIN  PA0   // Wake up button pin

// The clock config function provided in the project
extern "C" void SystemClock_Config(void);

// Empty ISR for the wake pin.
void wakeUpISR() {
  // Do nothing, just waking up the core
}

void enterStop1Mode() {
  // 1. Flush and terminate Debug Serial to prevent UART leakage
  DebugSerial.println("Entering Stop 1 mode now...");
  DebugSerial.flush();
  DebugSerial.end();

  // 2. Terminate I2C to release pins
  Wire.end();

  // 3. Configure UART and I2C pins to ANALOG to prevent leakage
  pinMode(PA9, INPUT_ANALOG);   // UART TX
  pinMode(PA10, INPUT_ANALOG);  // UART RX
  pinMode(PA11, INPUT_ANALOG);  // I2C SCL
  pinMode(PA12, INPUT_ANALOG);  // I2C SDA

  // 4. Configure SWD pins to ANALOG to prevent debug line leakage
  pinMode(PA13, INPUT_ANALOG);  // SWDIO
  pinMode(PA14, INPUT_ANALOG);  // SWCLK

  // 5. Disable debugging in Stop mode to allow the debug block to power down
  HAL_DBGMCU_DisableDBGStopMode();

  // 6. Disable peripheral clocks to ensure no internal leakage
  __HAL_RCC_I2C1_CLK_DISABLE();
  __HAL_RCC_I2C2_CLK_DISABLE();
  __HAL_RCC_USART1_CLK_DISABLE();
  __HAL_RCC_USART2_CLK_DISABLE();
  __HAL_RCC_SPI1_CLK_DISABLE();
  __HAL_RCC_ADC_CLK_DISABLE();

  // 7. Suspend the SysTick timer
  HAL_SuspendTick(); 

  // 8. Enter Stop 1 mode (Low Power Regulator ON = Stop 1)
  HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI); 

  // --- MCU IS ASLEEP HERE ---

  // --- MCU WAKES UP HERE ---

  // 1. Re-initialize the system clock to 64 MHz via the PLL
  SystemClock_Config();

  // 2. Resume the SysTick timer
  HAL_ResumeTick();

  // 3. Re-enable peripheral clocks
  __HAL_RCC_I2C1_CLK_ENABLE();
  __HAL_RCC_USART1_CLK_ENABLE();

  // 4. Re-initialize I2C
  Wire.begin();

  // 5. Re-initialize UART
  DebugSerial.begin(115200);
  DebugSerial.println("MCU woke up successfully!");
}

void setup() {
  // Initialize UART
  DebugSerial.begin(115200);
  delay(1000); 

  // Initialize Wire to configure I2C pins correctly
  Wire.begin();
  delay(10);

  DebugSerial.println("\n--- STM32G070 Ultra Low Power Stop 1 Mode Test ---");
  DebugSerial.println("Boot up complete.");

  // Configure PA0 (WAKE_PIN) as input with internal pull-down for active-high wake logic
  pinMode(WAKE_PIN, INPUT_PULLDOWN);

  // Attach wake interrupt on PA0 rising edge (when button is pulled high)
  attachInterrupt(digitalPinToInterrupt(WAKE_PIN), wakeUpISR, RISING);

  // --- POWER SAVING: Configure unused pins to prevent leakage ---
  
  // Force the NeoPixel Power Control pin (PA5) HIGH to cut power to the LEDs.
  pinMode(PA5, OUTPUT);
  digitalWrite(PA5, HIGH); 

  // Set all unused and unpopulated LoRa pins to ANALOG mode
  const uint8_t unusedPins[] = {
    PA4, PA6, PA7, PA8,
    PB0, PB1, PB3, PB4, PB5, PB6, PB7, PB8, PB9,
    PC14, PC15 // LSE Oscillator pins
    // Note: UART (PA9/10), I2C (PA11/12), and Wake (PA0) are handled separately.
  };
  for (uint8_t pin : unusedPins) {
    pinMode(pin, INPUT_ANALOG);
  }

  // Set button pins (which have 10k external pull-ups) to high-impedance INPUT mode
  // to prevent any leakage current through the resistors.
  const uint8_t buttonPins[] = {
    PB2,  // BTN1
    PA3,  // BTN2
    PC6,  // BTN3
    PA1,  // BTN4
    PA15, // BTN5
    PA2   // BTN6
  };
  for (uint8_t pin : buttonPins) {
    pinMode(pin, INPUT);
  }

  // 5-second delay before sleeping to allow easy reprogramming
  DebugSerial.println("Waiting 5 seconds before entering sleep...");
  for (int i = 5; i > 0; i--) {
    DebugSerial.print(i);
    DebugSerial.print("... ");
    delay(1000);
  }
  
  // Enter low power mode
  enterStop1Mode();
}

void loop() {
  // Print a heartbeat every 2 seconds after waking up to show it is active.
  DebugSerial.println("MCU is active.");
  delay(2000);
}
