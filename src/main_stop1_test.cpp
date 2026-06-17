#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <RadioLib.h>

// UART Debug Serial
HardwareSerial DebugSerial(PA10, PA9);

// LoRa SX1262 Pin Definitions
#define LORA_NSS  PB0
#define LORA_SCK  PB3
#define LORA_MISO PB4
#define LORA_MOSI PB5
#define LORA_BUSY PB1
#define LORA_DIO1 PA0   // Shared with Wake Pin
#define LORA_RST  PB7
#define LORA_RXEN PA7
#define LORA_TXEN PB6

#define WAKE_PIN  PA0   // Shared with LORA_DIO1

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
  int state = radio.begin();
  if (state == RADIOLIB_ERR_NONE) {
    DebugSerial.println("Success! SX1262 detected.");
    // Set RF switch pins (RXEN, TXEN)
    radio.setRfSwitchPins(LORA_RXEN, LORA_TXEN);
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
    pinMode(LORA_RXEN, INPUT_ANALOG);
    pinMode(LORA_TXEN, INPUT_ANALOG);
  }
}

void setup() {
  // Initialize UART
  DebugSerial.begin(115200);
  delay(1000); 

  // Initialize Wire to configure I2C pins correctly
  Wire.begin();
  delay(10);

  DebugSerial.println("\n--- STM32G070 Stop 1 Mode + LoRa Sleep Test ---");
  DebugSerial.println("Boot up complete.");

  // Initialize and check LoRa SX1262 module
  initLoRa();

  // Configure PA0 (WAKE_PIN) as input with internal pull-down for active-high wake logic
  pinMode(WAKE_PIN, INPUT_PULLDOWN);

  // Attach wake interrupt on PA0 rising edge (when button is pulled high)
  attachInterrupt(digitalPinToInterrupt(WAKE_PIN), wakeUpISR, RISING);

  // --- POWER SAVING: Configure pins to prevent leakage ---
  
  // Force the NeoPixel Power Control pin (PA5) HIGH to cut power to the LEDs.
  pinMode(PA5, OUTPUT);
  digitalWrite(PA5, HIGH); 

  // Set all unused pins to ANALOG mode to disable digital input buffers.
  const uint8_t unusedPins[] = {
    PA4, PA6, PA8,                     // Port A unused
    PB8, PB9,                          // Port B unused
    PC14, PC15                         // Port C unused
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

  // We add a 5-second delay before sleeping. This gives you a 5-second 
  // window to upload new code easily without needing to hold the Reset button!
  DebugSerial.println("Waiting 5 seconds before entering sleep...");
  for (int i = 5; i > 0; i--) {
    DebugSerial.print(i);
    DebugSerial.print("... ");
    delay(1000);
  }
  
  if (loraPresent) {
    DebugSerial.println("\nPutting LoRa module to sleep (Cold start - lowest current)...");
    // sleep(false) triggers cold start sleep (retains no configuration, ~160 nA current)
    int loraSleepState = radio.sleep(false);
    if (loraSleepState == RADIOLIB_ERR_NONE) {
      DebugSerial.println("LoRa module is now in deep sleep.");
    } else {
      DebugSerial.print("Failed to put LoRa to sleep, code: ");
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
    // Re-initialize LoRa module since it was in a cold-start sleep mode
    DebugSerial.println("Waking up and re-initializing LoRa module...");
    int state = radio.begin();
    if (state == RADIOLIB_ERR_NONE) {
      DebugSerial.println("LoRa module woke up and re-initialized successfully!");
      radio.setRfSwitchPins(LORA_RXEN, LORA_TXEN);
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
