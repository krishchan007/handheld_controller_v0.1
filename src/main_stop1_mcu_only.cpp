#include <Arduino.h>
#include <Wire.h>

#define WAKE_PIN  PA0

HardwareSerial DebugSerial(PA10, PA9);

// The clock config function provided in the project
extern "C" void SystemClock_Config(void);

// Empty ISR for the wake pin.
void wakeUpISR() {
  // Do nothing, just waking up the core
}

void setup() {
  // Initialize UART
  DebugSerial.begin(115200);
  delay(1000); 

  // Initialize Wire to configure I2C pins correctly
  Wire.begin();
  delay(10);

  DebugSerial.println("\n--- STM32G070 MCU-Only Stop 1 Mode Test ---");
  DebugSerial.println("Boot up complete.");

  // Configure PA0 (WAKE_PIN) as input with internal pull-down for active-high wake logic
  pinMode(WAKE_PIN, INPUT_PULLDOWN);

  // Attach wake interrupt on PA0 rising edge (when button is pulled high)
  attachInterrupt(digitalPinToInterrupt(WAKE_PIN), wakeUpISR, RISING);

  // --- POWER SAVING: Configure pins to prevent leakage ---
  
  // 1. Force the NeoPixel Power Control pin (PA5) HIGH to cut power to the LEDs.
  // (Assuming active-low P-MOSFET control. If it is active-high, write LOW).
  // pinMode(PA5, OUTPUT);
  // digitalWrite(PA5, HIGH); 

  // 2. Set all other unused pins to ANALOG mode to disable digital input buffers.
  // This prevents floating voltages from causing "shoot-through" leakage current.
  const uint8_t unusedPins[] = {
    PA1, PA2, PA3, PA4, PA5, PA6, PA7, PA8, PA11,PA9,PA10, PA12,PA13,PA14,PA15,     // Port A (excluding PA0/Wake, PA5/LED_CTRL, PA9/PA10 UART)
    PB0, PB1, PB2, PB3, PB4, PB5, PB6, PB7, PB8, PB9,   // Port B
    PC6, PC14,PC15                                            // Port C
  };
  for (uint8_t pin : unusedPins) {
    pinMode(pin, INPUT_ANALOG);
  }

  // We add a 5-second delay before sleeping. This gives you a 5-second 
  // window to upload new code easily without needing to hold the Reset button!
  DebugSerial.println("Waiting 5 seconds before entering sleep...");
  for (int i = 5; i > 0; i--) {
    DebugSerial.print(i);
    DebugSerial.print("... ");
    delay(1000);
  }
  
  DebugSerial.println("\nEntering Stop 1 mode now.");
  DebugSerial.flush(); // Ensure all UART data is transmitted before sleeping

// 1. Shut down the I2C peripheral hardware gates cleanly
  Wire.end(); 


  // =======================================================================
  // DIAGNOSTIC TEST: Disable PA0 right before sleep to check for leakage
  // =======================================================================
  detachInterrupt(digitalPinToInterrupt(WAKE_PIN)); // Detach the interrupt
  pinMode(WAKE_PIN, INPUT_ANALOG);                 // Kill the internal pull-down
  // =======================================================================


  
  // 2. Clear any pending wake-up flags in the power controller
  __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WUF);

  // 3. Enable the Flash Power Down feature SPECIFICALLY for Sleep/Stop modes
  // This is safe; it instructs the hardware controller to turn off flash only *during* sleep.
  // Set the Sleep Power Down bit in the Flash Access Control Register
HAL_PWREx_EnableFlashPowerDown(PWR_FLASHPD_STOP);

  // 4. Suspend the SysTick timer so its interrupts don't wake the MCU prematurely
  HAL_SuspendTick(); 

  // 5. Enter Stop 1 mode using the HAL (Low Power Regulator ON = Stop 1)
  HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI); 

  // ==========================================
  // --- MCU IS ASLEEP HERE UNTIL PA0 GOES HIGH ---
  // ==========================================

  // --- MCU WAKES UP HERE ---
// Clear the Sleep Power Down bit to restore standard flash operations
HAL_PWREx_DisableFlashPowerDown(PWR_FLASHPD_STOP);
  // After waking up from Stop mode, the system clock is switched to the HSI (16 MHz).
  // We need to re-initialize the clock to go back to 64 MHz via the PLL.
  SystemClock_Config();

  // Resume the SysTick timer
  HAL_ResumeTick();

  DebugSerial.println("MCU woke up successfully!");
}

void loop() {
  // Print a heartbeat every 2 seconds after waking up to show it is active.
  DebugSerial.println("MCU is active.");
  delay(2000);
}
