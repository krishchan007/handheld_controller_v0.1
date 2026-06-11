// =============================================================================
// main_lora_actuator.cpp
//
// PlatformIO port of lora_actuator.ino
// ────────────────────────────────────
// Converted from Arduino IDE .ino to PlatformIO .cpp:
//   • Added explicit #include <Arduino.h>
//   • Renamed "Serial" → "DebugSerial" to avoid colliding with the
//     default global Serial object that STM32duino declares.
//   • All original functionality, pinout, LoRa parameters, and protocol
//     logic are preserved exactly as-is.
//
// Actuator Pinout (STM32G070):
//   LORA_NSS  → PB8     LORA_BUSY → PA8
//   LORA_SCK  → PB3     LORA_MISO → PB4    LORA_MOSI → PB5
//   LORA_RST  → PB7     LORA_DIO1 → PA7
//   ACT_LED   → PC14
//   UART TX   → PA9     UART RX   → PA10
// =============================================================================

#include <Arduino.h>
#include <RadioLib.h>
#include <SPI.h>

#include "protocol.h"

// UART Debug Serial (same pins as the .ino)
HardwareSerial DebugSerial(PA10, PA9);

// ------------------------------------------------------------
// SX1262 wiring – Actuator board pinout
// ------------------------------------------------------------
static const uint8_t LORA_SCK  = PB3;
static const uint8_t LORA_MISO = PB4;
static const uint8_t LORA_MOSI = PB5;

static const uint8_t LORA_NSS  = PB8;
static const uint8_t LORA_BUSY = PA8;
static const uint8_t LORA_DIO1 = PA7;
static const uint8_t LORA_RST  = PB7;

// Create LoRa instance using RadioLib (CS, IRQ/DIO1, RST, BUSY)
SX1262 radio = new Module(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY);

// ------------------------------------------------------------
// Actuator Channels
// ------------------------------------------------------------
static const uint8_t ACC_CHANNEL = 1;

static const uint8_t ACT_LED = PC14;

// ------------------------------------------------------------
// Receive flag
// ------------------------------------------------------------
static bool fired = false;
volatile bool receivedFlag = false;

void setFlag(void)
{
    receivedFlag = true;
}

extern "C" void SystemClock_Config(void);

// ------------------------------------------------------------
// Setup
// ------------------------------------------------------------
void setup()
{
    // Configure system clock to 64MHz (from clock_config.cpp PLL setup)
    SystemClock_Config();

    pinMode(ACT_LED, OUTPUT);
    digitalWrite(ACT_LED, LOW);

    DebugSerial.begin(115200);
    delay(500); // Give UART clock & serial terminal time to stabilize

    DebugSerial.println();
    DebugSerial.println("Actuator starting");
    SPI.setMOSI(LORA_MOSI);
    SPI.setMISO(LORA_MISO);
    SPI.setSCLK(LORA_SCK);
    SPI.begin();

    int state = radio.begin(865.0, 125.0, 9, 7, RADIOLIB_SX126X_SYNC_WORD_PRIVATE, 10, 8, 0, false);

    if(state != RADIOLIB_ERR_NONE)
    {
        DebugSerial.print("Radio init failed: ");
        DebugSerial.println(state);

        while(1)
        {
            delay(1000);
        }
    }

    DebugSerial.println("Radio init OK");

    radio.setPacketReceivedAction(setFlag);

    state = radio.startReceive();

    if(state != RADIOLIB_ERR_NONE)
    {
        DebugSerial.print("startReceive failed: ");
        DebugSerial.println(state);

        while(1)
        {
            delay(1000);
        }
    }

    DebugSerial.println("Listening...");
}

// ------------------------------------------------------------
// Loop
// ------------------------------------------------------------
void loop()
{
    if(!receivedFlag)
    {
        return;
    }

    receivedFlag = false;

    uint8_t rxBuf[32];

    int packetLength =
        radio.getPacketLength();

    int state =
        radio.readData(
            rxBuf,
            packetLength);

    if(state != RADIOLIB_ERR_NONE)
    {
        DebugSerial.print("RX error: ");
        DebugSerial.println(state);

        radio.startReceive();
        return;
    }

    DebugSerial.println("Packet received");

    if(packetLength != sizeof(CmdFrame))
    {
        DebugSerial.print("Unexpected length: ");
        DebugSerial.println(packetLength);

        radio.startReceive();
        return;
    }

    CmdFrame* frame =
        (CmdFrame*)rxBuf;

    DebugSerial.printf("[NetID: 0x%02X] [TTL: %d] [Type: 0x%02X]\n", frame->header.netId, frame->header.ttl, frame->payload.type);

    if(frame->header.netId != PROTO_NET_ID)
    {
        DebugSerial.println("Bad NetID");

        radio.startReceive();
        return;
    }

    if(frame->payload.type != FRAME_CMD)
    {
        DebugSerial.println("Not CMD");

        radio.startReceive();
        return;
    }

    uint16_t crc =
        protocolCrc16(
            (uint8_t*)&frame->payload,
            sizeof(CmdPayload) - sizeof(uint16_t));

    if(crc != frame->payload.crc16)
    {
        DebugSerial.println("CRC mismatch");

        radio.startReceive();
        return;
    }
    
    if(frame->payload.channel != ACC_CHANNEL)
    {
        DebugSerial.println("Not my channel");

        radio.startReceive();
        return;
    }

    DebugSerial.print("Counter: ");
    DebugSerial.println(frame->payload.counter);

    DebugSerial.print("Channel: ");
    DebugSerial.println(frame->payload.channel);

    DebugSerial.print("Command: ");
    DebugSerial.println(frame->payload.command);

    if(frame->payload.command == CMD_TRIGGER)
    {
        uint8_t txBuf[16];
        size_t txLen;

        buildAckPacket(
            frame->payload.channel,
            frame->payload.counter,
            fired ?
            RES_ACCEPTED :
            RES_ALREADY_FIRED,
            txBuf,
            &txLen);

        if(!fired)
        {
            DebugSerial.println("TRIGGER RECEIVED");
            fired = true;

            digitalWrite(
                ACT_LED,
                HIGH);
        }
        else
        {
            DebugSerial.println("TRIGGER RECEIVED, BUT ALREADY FIRED");
        }

        state = radio.transmit(txBuf, txLen);
        if(state != RADIOLIB_ERR_NONE)
        {
            DebugSerial.print("TX error: ");
            DebugSerial.println(state);
        }

        radio.finishTransmit();

        radio.startReceive();
    }
    else if(frame->payload.command == CMD_STATUS)
    {
        uint8_t txBuf[16];
        size_t txLen;

        DebugSerial.println("STATUS REQUEST RECEIVED");
        buildStatusPacket(
            frame->payload.channel,
            frame->payload.counter,
            fired ?
                STATE_FIRED :
                STATE_READY,
            100,
            txBuf,
            &txLen);

        radio.transmit(txBuf, txLen);
        radio.finishTransmit();

        radio.startReceive();
    }

    DebugSerial.print("RSSI: ");
    DebugSerial.println(radio.getRSSI());

    DebugSerial.print("SNR: ");
    DebugSerial.println(radio.getSNR());

    radio.startReceive();
}
