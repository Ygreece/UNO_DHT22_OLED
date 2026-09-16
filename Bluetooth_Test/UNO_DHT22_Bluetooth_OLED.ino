#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>

const uint8_t SCREEN_WIDTH = 128;
const uint8_t SCREEN_HEIGHT = 64;
const uint8_t OLED_ADDRESS = 0x3C;
const uint8_t DHT_PIN = 2;
const unsigned long READ_INTERVAL_MS = 2000;
const long BLUETOOTH_BAUD = 9600;

#define DHT_TYPE DHT22

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
DHT dht(DHT_PIN, DHT_TYPE);

void showReadError() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(F("DHT22 SENSOR"));
  display.setCursor(0, 24);
  display.println(F("Read failed"));
  display.setCursor(0, 42);
  display.println(F("Check DATA -> D2"));
  display.display();
}

void showSensorData(float temperature, float humidity) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(F("DHT22 + BLUETOOTH"));
  display.setTextSize(2);
  display.setCursor(0, 18);
  display.print(temperature, 1);
  display.println(F(" C"));
  display.setCursor(0, 43);
  display.print(humidity, 1);
  display.println(F(" %"));
  display.display();
}

void setup() {
  Serial.begin(BLUETOOTH_BAUD);
  dht.begin();

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    while (true) {
      delay(1000);
    }
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(F("DHT22 + BLUETOOTH"));
  display.display();
  Serial.println(F("DHT22 Bluetooth monitor started"));
  delay(2000);
}

void loop() {
  const float humidity = dht.readHumidity();
  const float temperature = dht.readTemperature();

  if (isnan(humidity) || isnan(temperature)) {
    showReadError();
    Serial.println(F("DHT22 read failed"));
    delay(READ_INTERVAL_MS);
    return;
  }

  showSensorData(temperature, humidity);

  // VOFA+ FireWater format: channel 0 = temperature, channel 1 = humidity.
  Serial.print(temperature, 1);
  Serial.print(',');
  Serial.print(humidity, 1);
  Serial.print(';');

  delay(READ_INTERVAL_MS);
}
