#include <Arduino.h>
#include <Wire.h>

void setup() {
  // Force debug output onto USART1 TX/RX pins for the custom G070KB board.
  Serial1.setTx(PA9);
  Serial1.setRx(PA10);
  Serial1.begin(115200);
  delay(200);

  Serial1.println("\n--- STM32G070K I2C Scanner Ready ---");

  // Initialize I2C using the pins defined in platformio.ini
  // If you need to manually override them, use: Wire.setSDA(PB9); Wire.setSCL(PB8);
  Wire.begin();
}

void loop() {
  byte error, address;
  int nDevices;

  Serial1.println("Scanning for I2C devices...");

  nDevices = 0;
  for (address = 1; address < 127; address++) {
    // The i2c_scanner uses the return value of
    // the Write.endTransmisstion to see if
    // a device did acknowledge to the address.
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0) {
      Serial1.print("I2C device found at address 0x");
      if (address < 16) {
        Serial1.print("0");
      }
      Serial1.print(address, HEX);
      Serial1.println(" !");

      nDevices++;
    } 
    else if (error == 4) {
      Serial1.print("Unknown error at address 0x");
      if (address < 16) {
        Serial1.print("0");
      }
      Serial1.println(address, HEX);
    }
  }

  if (nDevices == 0) {
    Serial1.println("No I2C devices found\n");
  } else {
    Serial1.println("Scan complete.\n");
  }

  delay(5000); // Wait 5 seconds before scanning again
}
