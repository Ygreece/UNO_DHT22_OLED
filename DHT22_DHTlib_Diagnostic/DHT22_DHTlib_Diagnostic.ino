#include <dht.h>

const uint8_t DHT_PIN = 2;
dht sensor;

void setup() {
  Serial.begin(9600);
  Serial.println(F("DHT22 DHTlib diagnostic started"));
  Serial.println(F("Status: 0=OK, -1=checksum, -2=timeout, -3=connect, -4/-5=ack"));
}

void loop() {
  const int8_t status = sensor.read22(DHT_PIN);
  Serial.print(F("status="));
  Serial.print(status);

  if (status == DHTLIB_OK) {
    Serial.print(F(", T="));
    Serial.print(sensor.temperature, 1);
    Serial.print(F(" C, H="));
    Serial.print(sensor.humidity, 1);
    Serial.println(F(" %RH"));
  } else {
    Serial.println();
  }

  delay(2000);
}
