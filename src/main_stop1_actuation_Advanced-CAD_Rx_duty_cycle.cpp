#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <RadioLib.h>

// UART Debug Serial
HardwareSerial DebugSerial(PA10, PA9);

// LoRa Actuator Pin Definitions
static const uint8_t LORA_SCK  = PB3;
static const uint8_t LORA_MISO = PB4;
static const uint8_t LORA_MOSI = PB5;
static const uint8_t LORA_NSS  = PB8;
static const uint8_t LORA_BUSY = PA8;
static const uint8_t LORA_DIO1 = PA7;
static const uint8_t LORA_RST  = PB7;

#define WAKE_PIN  PA0

// Create LoRa instance using RadioLib (CS, IRQ/DIO1, RST, BUSY)
SX1262 radio = new Module(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY);

// The clock config function provided in the project
extern "C" void SystemClock_Config(void);

// Empty ISR for the wake pin.
void wakeUpISR() {
  // Do nothing, just waking up the core
}

bool loraPresent = false;

void initLoRa() {
  DebugSerial.print("Initializing SPI for LoRa... ");
  SPI.setMOSI(LORA_MOSI);
  SPI.setMISO(LORA_MISO);
  SPI.setSCLK(LORA_SCK);
  SPI.begin();
  DebugSerial.println("done.");

  DebugSerial.print("Checking LoRa SX1262 communication... ");
  // begin() performs a communication check by writing to SX1262 registers
  int state = radio.begin(865.0, 125.0, 12, 8, RADIOLIB_SX126X_SYNC_WORD_PRIVATE, 10, 8, 0.0, false);
  if (state == RADIOLIB_ERR_NONE) {
    DebugSerial.println("Success! SX1262 detected.");
    loraPresent = true;
  } else {
    DebugSerial.print("Failed to communicate, code: ");
    DebugSerial.println(state);
    
    // LoRa module is not present, clean up SPI and set pins to ANALOG to prevent leakage
    SPI.end();
    pinMode(LORA_NSS, INPUT_ANALOG);
    pinMode(LORA_SCK, INPUT_ANALOG);
    pinMode(LORA_MISO, INPUT_ANALOG);
    pinMode(LORA_MOSI, INPUT_ANALOG);
    pinMode(LORA_BUSY, INPUT_ANALOG);
    pinMode(LORA_RST, INPUT_ANALOG);
  }
}

void setup() {
  // Initialize UART
  DebugSerial.begin(115200);
  delay(1000); 

  // Initialize Wire to configure I2C pins correctly
  Wire.begin();
  delay(10);

  DebugSerial.println("\n--- STM32G070 Stop 1 Mode + LoRa Advanced CAD Rx Duty Cycle Sleep Test ---");
  DebugSerial.println("Boot up complete.");

  // Initialize and check LoRa SX1262 module
  initLoRa();

  // Configure PA0 (WAKE_PIN) as input with internal pull-down for active-high wake logic
  pinMode(WAKE_PIN, INPUT_PULLDOWN);

  // Attach wake interrupt on PA0 rising edge (when button is pulled high)
  attachInterrupt(digitalPinToInterrupt(WAKE_PIN), wakeUpISR, RISING);

  // --- POWER SAVING: Configure pins to prevent leakage ---
  
  // Set all unused pins to ANALOG mode to disable digital input buffers.
  // We keep PA9/PA10 (UART), PB3/PB4/PB5/PB7/PB8 (LoRa SPI/Control), PA8 (LoRa BUSY), PA7 (LoRa DIO1), PA0 (Wake Pin) active.
  const uint8_t unusedPins[] = {
    PA1, PA2, PA3, PA4, PA5, PA6, PA11, PA12, PA15,    // Port A unused (excl PA7, PA8, PA9, PA10, PA0)
    PB0, PB1, PB2, PB6, PB9,                           // Port B unused (excl PB3, PB4, PB5, PB7, PB8)
    PC6, PC14, PC15                                    // Port C unused
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
  
  if (loraPresent) {
    DebugSerial.println("\nPutting LoRa module to Advanced CAD / Rx Duty Cycle mode...");
    // 10 ms (10000 us) RX period, 1000 ms (1000000 us) sleep period
    int loraSleepState = radio.startReceiveDutyCycle(10000, 1000000);
    if (loraSleepState == RADIOLIB_ERR_NONE) {
      DebugSerial.println("LoRa module is now in RX Duty Cycle mode.");
    } else {
      DebugSerial.print("Failed to put LoRa to RX Duty Cycle mode, code: ");
      DebugSerial.println(loraSleepState);
    }
  }

  DebugSerial.println("Entering Stop 1 mode now.");
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
  HAL_PWREx_EnableFlashPowerDown(PWR_FLASHPD_STOP);

  // 4. Suspend the SysTick timer so its interrupts don't wake the MCU prematurely
  HAL_SuspendTick(); 

  // 5. Enter Stop 1 mode using the HAL (Low Power Regulator ON = Stop 1)
  HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI); 

  // ==========================================
  // --- MCU IS ASLEEP HERE ---
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

  if (loraPresent) {
    // Re-initialize LoRa module
    DebugSerial.println("Waking up and re-initializing LoRa module...");
    int state = radio.begin(865.0, 125.0, 12, 8, RADIOLIB_SX126X_SYNC_WORD_PRIVATE, 10, 8, 0.0, false);
    if (state == RADIOLIB_ERR_NONE) {
      DebugSerial.println("LoRa module woke up and re-initialized successfully!");
    } else {
      DebugSerial.print("LoRa wake up failed, code: ");
      DebugSerial.println(state);
    }
  }
}

void loop() {
  // Print a heartbeat every 2 seconds after waking up to show it is active.
  DebugSerial.println("MCU is active.");
  delay(2000);
}
