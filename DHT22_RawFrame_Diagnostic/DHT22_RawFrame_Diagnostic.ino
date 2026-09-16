// Diagnostic only: expose DHTlib's received 5-byte frame without changing the library.
#define private public
#include <dht.h>
#undef private

const uint8_t DHT_PIN = 2;
dht sensor;

void printHexByte(uint8_t value) {
  if (value < 16) Serial.print('0');
  Serial.print(value, HEX);
}

void setup() {
  Serial.begin(9600);
  Serial.println(F("DHT22 raw-frame diagnostic started"));
}

void loop() {
  const int8_t status = sensor.read22(DHT_PIN);
  Serial.print(F("status="));
  Serial.print(status);

  if (status == DHTLIB_OK) {
    Serial.print(F(" raw="));
    uint8_t calculatedChecksum = 0;
    for (uint8_t i = 0; i < 5; ++i) {
      printHexByte(sensor.bits[i]);
      if (i < 4) {
        calculatedChecksum += sensor.bits[i];
        Serial.print(' ');
      }
    }
    Serial.print(F(" checksum="));
    printHexByte(calculatedChecksum);
    Serial.print(F(" decoded: T="));
    Serial.print(sensor.temperature, 1);
    Serial.print(F(" C H="));
    Serial.print(sensor.humidity, 1);
    Serial.println(F(" %RH"));
  } else {
    Serial.println();
  }

  delay(2000);
}
