// =============================================================================
// main_stop1_actuation.cpp
//
// Actuation Module – Ultra Low Power Sleep / Wake
// ------------------------------------------------
// Strategy:
//   1. Put the SX1262 LoRa module to cold-start deep sleep (~160 nA).
//   2. Tear down all active peripherals (SPI, I2C, UART) and set their
//      pins to ANALOG to prevent leakage.
//   3. Disable peripheral clocks and the debug block so nothing holds
//      the supply rails up inside the MCU.
//   4. Enter STM32G070 Stop 1 mode – only the wake-up interrupt path stays
//      alive.  Supply current target: < 5 µA total system.
//
// Wake source:
//   PA0 rising edge (WAKE_PIN = LORA_DIO1).  Connect a push-button that
//   pulls PA0 HIGH when pressed (external pull-down keeps it LOW at rest).
//
// After wake:
//   Re-initialise the clock to 64 MHz, bring all peripherals back up,
//   re-initialise the LoRa radio, then drop into the normal loop.
//
// Pinout (matches the LoRa actuator example):
//   LORA_NSS  → PB0     LORA_BUSY → PB1
//   LORA_SCK  → PB3     LORA_MISO → PB4    LORA_MOSI → PB5
//   LORA_RST  → PB7     LORA_RXEN → PA7    LORA_TXEN → PB6
//   LORA_DIO1 → PA0     (shared with WAKE_PIN)
//   UART TX   → PA9     UART RX   → PA10
//   I2C SDA   → PA12    I2C SCL   → PA11
//   LED CTRL  → PA5     NEOPIXEL  → PA4
//   BUZZER    → PA8
//   BTN1–6    → PB2, PA3, PC6, PA1, PA15, PA2
// =============================================================================

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <RadioLib.h>

// ---------------------------------------------------------------------------
// Hardware definitions
// ---------------------------------------------------------------------------
#define LORA_NSS  PB0
#define LORA_SCK  PB3
#define LORA_MISO PB4
#define LORA_MOSI PB5
#define LORA_BUSY PB1
#define LORA_DIO1 PA0   // Also the WAKE_PIN (rising-edge interrupt source)
#define LORA_RST  PB7
#define LORA_RXEN PA7
#define LORA_TXEN PB6

#define WAKE_PIN  PA0   // Shared with LORA_DIO1

// Debug UART
HardwareSerial DebugSerial(PA10, PA9);

// LoRa radio instance (CS, IRQ/DIO1, RST, BUSY)
SX1262 radio = new Module(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY);

// Pull in the project's 64 MHz PLL clock init
extern "C" void SystemClock_Config(void);

// ---------------------------------------------------------------------------
// Interrupt service routine – intentionally empty.
// Its only purpose is to give the NVIC something to vector to so the MCU
// exits Stop 1 cleanly (WFI returns when any enabled EXTI fires).
// ---------------------------------------------------------------------------
void wakeUpISR() {
    // Nothing to do here – the interrupt itself is the event.
}

// ---------------------------------------------------------------------------
// initLoRa()
// Sets up SPI and calls RadioLib begin().  Called on first boot AND after
// waking from a cold-start sleep (full re-initialisation required).
// ---------------------------------------------------------------------------
bool initLoRa() {
    DebugSerial.print("[LoRa] Configuring SPI... ");
    SPI.setMOSI(LORA_MOSI);
    SPI.setMISO(LORA_MISO);
    SPI.setSCLK(LORA_SCK);
    SPI.begin();
    DebugSerial.println("done.");

    DebugSerial.print("[LoRa] Initialising SX1262... ");
    int state = radio.begin();
    if (state == RADIOLIB_ERR_NONE) {
        radio.setRfSwitchPins(LORA_RXEN, LORA_TXEN);
        DebugSerial.println("OK.");
        return true;
    } else {
        DebugSerial.print("FAILED (code ");
        DebugSerial.print(state);
        DebugSerial.println(").");
        return false;
    }
}

// ---------------------------------------------------------------------------
// enterStop1Mode()
//
// Full power-down sequence:
//   • LoRa cold-start sleep
//   • SPI end + pins → ANALOG
//   • UART end + pins → ANALOG
//   • I2C end + pins → ANALOG
//   • SWD pins → ANALOG (prevents debug-line leakage)
//   • Peripheral clock gating
//   • Debug block power-down in Stop mode
//   • SysTick suspend
//   • HAL Stop 1 entry  ← MCU sleeps here
//   • Full wake-up sequence on return
// ---------------------------------------------------------------------------
void enterStop1Mode() {

    // ------------------------------------------------------------------
    // 1. Put LoRa SX1262 into cold-start deep sleep (~160 nA).
    //    cold-start (false) means the module forgets its configuration
    //    and needs a full radio.begin() on wake.
    // ------------------------------------------------------------------
    DebugSerial.print("[LoRa] Entering cold-start deep sleep... ");
    int loraSleepState = radio.sleep(false);
    if (loraSleepState == RADIOLIB_ERR_NONE) {
        DebugSerial.println("OK.");
    } else {
        DebugSerial.print("FAILED (code ");
        DebugSerial.print(loraSleepState);
        DebugSerial.println(") – continuing anyway.");
    }

    // ------------------------------------------------------------------
    // 2. Bring SPI down and set all SPI / LoRa control pins to ANALOG.
    //    ANALOG mode disables the digital input buffer, eliminating any
    //    leakage current through a floating or partially-driven pin.
    // ------------------------------------------------------------------
    SPI.end();
    // Set SPI data/control lines to ANALOG to eliminate leakage.
    // LORA_BUSY (PB1) and LORA_RST (PB7) are intentionally EXCLUDED:
    //   • INPUT_ANALOG disables the digital input buffer, so RadioLib would
    //     always read BUSY=0 on wake and race ahead before the SX1262
    //     finishes its cold-sleep boot → RADIOLIB_ERR_CHIP_NOT_FOUND (-2).
    //   • We keep RST as OUTPUT HIGH (safe deasserted idle) so we can drive
    //     a proper reset pulse before the first SPI transaction on wake.
    // NOTE: LORA_DIO1 / WAKE_PIN (PA0) is kept as INPUT_PULLDOWN for the
    //       wake-up interrupt – do NOT set it to ANALOG.
    const uint8_t loraPins[] = {
        LORA_NSS, LORA_SCK, LORA_MISO, LORA_MOSI, LORA_RXEN, LORA_TXEN
    };
    for (uint8_t pin : loraPins) {
        pinMode(pin, INPUT_ANALOG);
    }
    // Hold RST deasserted (HIGH) and keep BUSY readable during sleep.
    pinMode(LORA_RST,  OUTPUT); digitalWrite(LORA_RST, HIGH);
    pinMode(LORA_BUSY, INPUT);

    // ------------------------------------------------------------------
    // 3. Cut power to the NeoPixel strip.
    //    LED_CTRL (PA5) drives a P-MOSFET gate – HIGH = MOSFET off =
    //    no current to the LEDs.
    // ------------------------------------------------------------------
    pinMode(PA5, OUTPUT);
    digitalWrite(PA5, HIGH);

    // ------------------------------------------------------------------
    // 4. Set button pins to plain INPUT (high-impedance).
    //    The board has external 10 kΩ pull-ups, so INPUT_PULLUP would
    //    force the STM32's weak internal pull-up to fight the external
    //    resistor, wasting current.  Plain INPUT avoids that.
    // ------------------------------------------------------------------
    const uint8_t buttonPins[] = { PB2, PA3, PC6, PA1, PA15, PA2 };
    for (uint8_t pin : buttonPins) {
        pinMode(pin, INPUT);
    }

    // ------------------------------------------------------------------
    // 5. Set remaining unused / unpopulated pins to ANALOG.
    //    (NeoPixel data PA4, buzzer PA8 are inactive but could float)
    // ------------------------------------------------------------------
    const uint8_t unusedPins[] = { PA4, PA6, PA8, PB8, PB9, PC14, PC15 };
    for (uint8_t pin : unusedPins) {
        pinMode(pin, INPUT_ANALOG);
    }

    // ------------------------------------------------------------------
    // 6. Terminate I2C and set its pins to ANALOG.
    // ------------------------------------------------------------------
    Wire.end();
    pinMode(PA11, INPUT_ANALOG);  // I2C SCL
    pinMode(PA12, INPUT_ANALOG);  // I2C SDA

    // ------------------------------------------------------------------
    // 7. Flush and terminate the debug UART; set its pins to ANALOG.
    // ------------------------------------------------------------------
    DebugSerial.println("[MCU] Entering Stop 1 mode. Goodbye.");
    DebugSerial.flush();
    DebugSerial.end();
    pinMode(PA9,  INPUT_ANALOG);  // UART TX
    pinMode(PA10, INPUT_ANALOG);  // UART RX

    // ------------------------------------------------------------------
    // 8. Set SWD debug pins to ANALOG.
    //    This allows the debug domain to fully power-gate in Stop mode.
    //    WARNING: After this you cannot attach a debugger without a
    //    hardware reset.  Remove for active debug sessions.
    // ------------------------------------------------------------------
    pinMode(PA13, INPUT_ANALOG);  // SWDIO
    pinMode(PA14, INPUT_ANALOG);  // SWCLK

    // ------------------------------------------------------------------
    // 9. Disable the debug block in Stop mode so it doesn't keep the
    //    debug supply domain awake (saves ~30–50 µA on some devices).
    // ------------------------------------------------------------------
    HAL_DBGMCU_DisableDBGStopMode();

    // ------------------------------------------------------------------
    // 10. Gate peripheral clocks.  Nothing should be accessing these
    //     buses now, but gating ensures their internal logic is fully
    //     quiesced before we hit WFI.
    // ------------------------------------------------------------------
    __HAL_RCC_SPI1_CLK_DISABLE();
    __HAL_RCC_I2C1_CLK_DISABLE();
    __HAL_RCC_I2C2_CLK_DISABLE();
    __HAL_RCC_USART1_CLK_DISABLE();
    __HAL_RCC_USART2_CLK_DISABLE();
    __HAL_RCC_ADC_CLK_DISABLE();

    // ------------------------------------------------------------------
    // 11. Suspend SysTick so it cannot fire a pending IRQ that would
    //     immediately wake the MCU after WFI.
    // ------------------------------------------------------------------
    HAL_SuspendTick();

    // ------------------------------------------------------------------
    // 12. Enter Stop 1 mode.
    //     PWR_LOWPOWERREGULATOR_ON → Stop 1 (LP regulator, ~2–5 µA MCU)
    //     PWR_STOPENTRY_WFI       → wait for any enabled EXTI interrupt
    //     Execution resumes on the line AFTER this call when PA0 fires.
    // ------------------------------------------------------------------
    HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);

    // ==================================================================
    //                      MCU IS ASLEEP HERE
    // ==================================================================

    // ==================================================================
    //                   MCU WAKES UP BELOW THIS LINE
    //            (triggered by PA0 rising edge from wake button)
    // ==================================================================

    // ------------------------------------------------------------------
    // 13. Re-initialise the 64 MHz PLL clock.
    //     Stop mode resets SYSCLK to HSI (16 MHz); we need PLL back.
    // ------------------------------------------------------------------
    SystemClock_Config();

    // ------------------------------------------------------------------
    // 14. Restore SysTick (needed for delay(), millis(), etc.).
    // ------------------------------------------------------------------
    HAL_ResumeTick();

    // ------------------------------------------------------------------
    // 15. Re-enable peripheral clocks.
    // ------------------------------------------------------------------
    __HAL_RCC_SPI1_CLK_ENABLE();
    __HAL_RCC_I2C1_CLK_ENABLE();
    __HAL_RCC_USART1_CLK_ENABLE();

    // ------------------------------------------------------------------
    // 16. Re-initialise UART first so we can log what happens next.
    // ------------------------------------------------------------------
    DebugSerial.begin(115200);
    delay(10);
    DebugSerial.println("\n[MCU] Woke up from Stop 1 mode!");

    // ------------------------------------------------------------------
    // 17. Re-initialise I2C.
    // ------------------------------------------------------------------
    Wire.begin();

    // ------------------------------------------------------------------
    // 18. Hardware-reset the SX1262 BEFORE calling radio.begin().
    //
    //     After a cold-start sleep the SX1262 needs an explicit RST
    //     pulse to leave its ultra-low-power state.  We do this
    //     manually rather than relying solely on RadioLib's internal
    //     reset so we can poll BUSY ourselves and guarantee the chip
    //     is fully ready before the first SPI transaction.
    // ------------------------------------------------------------------
    DebugSerial.print("[LoRa] Hardware reset of SX1262... ");
    pinMode(LORA_RST, OUTPUT);
    digitalWrite(LORA_RST, LOW);
    delay(20);                          // hold RST low ≥ 50 µs; 20 ms is safe
    digitalWrite(LORA_RST, HIGH);

    // Poll BUSY: it goes HIGH immediately after RST rises and returns
    // LOW once the chip has completed its internal boot (~3.5 ms typical).
    // We give it up to 1 s before declaring a timeout.
    {
        unsigned long deadline = millis() + 1000UL;
        while (digitalRead(LORA_BUSY) && millis() < deadline) { /* wait */ }
        if (millis() >= deadline) {
            DebugSerial.println("BUSY timeout! SX1262 may be damaged or missing.");
        } else {
            DebugSerial.println("done (BUSY LOW).");
        }
    }

    // ------------------------------------------------------------------
    // 19. Re-initialise the LoRa radio (full cold-start recovery).
    //     The chip is now awake and ready; radio.begin() will succeed.
    // ------------------------------------------------------------------
    DebugSerial.println("[LoRa] Re-initialising after cold-start sleep...");
    if (initLoRa()) {
        DebugSerial.println("[LoRa] Ready.");
    } else {
        DebugSerial.println("[LoRa] Re-init failed – check wiring.");
    }

    // ------------------------------------------------------------------
    // 20. Restore button pins and NeoPixel power.
    // ------------------------------------------------------------------
    for (uint8_t pin : buttonPins) {
        pinMode(pin, INPUT_PULLUP);
    }
    // Re-enable NeoPixel power (LOW = P-MOSFET on)
    digitalWrite(PA5, LOW);
}

// ---------------------------------------------------------------------------
// setup()
// ---------------------------------------------------------------------------
void setup() {
    // --- UART ---
    DebugSerial.begin(115200);
    delay(1000);

    // --- I2C ---
    Wire.begin();
    delay(10);

    DebugSerial.println("\n=== Actuation Module – Stop 1 Sleep/Wake Demo ===");
    DebugSerial.println("[MCU] Boot complete.");

    // --- LoRa ---
    initLoRa();

    // --- Wake pin: INPUT with internal pull-down ---
    //     The button pulls PA0 HIGH when pressed.
    //     At rest PA0 = LOW (pull-down keeps it quiet).
    //     We attach the interrupt BEFORE sleeping; it remains active
    //     in Stop mode because EXTI is clocked from the always-on domain.
    pinMode(WAKE_PIN, INPUT_PULLDOWN);
    attachInterrupt(digitalPinToInterrupt(WAKE_PIN), wakeUpISR, RISING);

    // --- 5-second programming window ---
    //     Upload a new sketch any time during this countdown by pressing
    //     Reset in PlatformIO; no need to hold buttons.
    DebugSerial.println("[MCU] Waiting 5 s before sleeping (upload window)...");
    for (int i = 5; i > 0; i--) {
        DebugSerial.print(i);
        DebugSerial.print("... ");
        delay(1000);
    }
    DebugSerial.println();

    // --- Enter Stop 1 ---
    enterStop1Mode();

    // setup() returns here after wake + full re-init.
    // loop() takes over for normal operation.
}

// ---------------------------------------------------------------------------
// loop() – runs after wake-up, normal operation resumes here
// ---------------------------------------------------------------------------
void loop() {
    DebugSerial.println("[MCU] Active – heartbeat.");
    delay(2000);
}
