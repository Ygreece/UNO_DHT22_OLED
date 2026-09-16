#include <DHT.h>

const uint8_t DHT_PIN = 2;
#define DHT_TYPE DHT22

DHT dht(DHT_PIN, DHT_TYPE);

void setup() {
  Serial.begin(9600);
  dht.begin();
  Serial.println(F("DHT22 diagnostic started"));
  Serial.println(F("Read interval: 2 seconds"));
}

void loop() {
  const float humidity = dht.readHumidity();
  const float temperature = dht.readTemperature();

  if (isnan(humidity) || isnan(temperature)) {
    Serial.println(F("READ FAILED"));
  } else {
    Serial.print(F("T="));
    Serial.print(temperature, 1);
    Serial.print(F(" C, H="));
    Serial.print(humidity, 1);
    Serial.println(F(" %RH"));
  }

  delay(2000);
}
