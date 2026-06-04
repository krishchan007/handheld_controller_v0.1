#include <Arduino.h>
#include <Wire.h>

HardwareSerial DebugSerial(PA10, PA9);

void setup() {
  // Force debug output onto USART1 TX/RX pins for the custom G070KB board.
  DebugSerial.begin(115200);
  delay(200);

  DebugSerial.println("\n--- STM32G070K I2C Scanner Ready ---");

  // Initialize I2C using the pins defined in platformio.ini
  // If you need to manually override them, use: Wire.setSDA(PB9); Wire.setSCL(PB8);
  Wire.begin();
}

void loop() {
  byte error, address;
  int nDevices;

  DebugSerial.println("Scanning for I2C devices...");

  nDevices = 0;
  for (address = 1; address < 127; address++) {
    // The i2c_scanner uses the return value of
    // the Write.endTransmisstion to see if
    // a device did acknowledge to the address.
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0) {
      DebugSerial.print("I2C device found at address 0x");
      if (address < 16) {
        DebugSerial.print("0");
      }
      DebugSerial.print(address, HEX);
      DebugSerial.println(" !");

      nDevices++;
    } 
    else if (error == 4) {
      DebugSerial.print("Unknown error at address 0x");
      if (address < 16) {
        DebugSerial.print("0");
      }
      DebugSerial.println(address, HEX);
    }
  }

  if (nDevices == 0) {
    DebugSerial.println("No I2C devices found\n");
  } else {
    DebugSerial.println("Scan complete.\n");
  }

  delay(5000); // Wait 5 seconds before scanning again
}
