#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>

const uint8_t SCREEN_WIDTH = 128, SCREEN_HEIGHT = 64, OLED_ADDRESS = 0x3C;
const uint8_t DHT_PIN = 2, BUZZER_PIN = 3, BUTTON_PIN = 4, FAN_PIN = 9;
const uint8_t STATUS_LED_PIN = LED_BUILTIN, COMMAND_BUFFER_SIZE = 32;
const float WARNING_TEMPERATURE_C = 28.0, CRITICAL_TEMPERATURE_C = 30.0;
const float WARNING_HUMIDITY_PCT = 70.0, CRITICAL_HUMIDITY_PCT = 80.0;
const float TEMPERATURE_HYSTERESIS_C = 1.0, HUMIDITY_HYSTERESIS_PCT = 5.0;
const unsigned long READ_INTERVAL_MS = 2000, BUTTON_DEBOUNCE_MS = 40;
const unsigned long WARNING_BEEP_INTERVAL_MS = 1500, WARNING_BEEP_DURATION_MS = 180;
const long BLUETOOTH_BAUD = 9600;
const bool FAN_ACTIVE_HIGH = true;
#define DHT_TYPE DHT22

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
DHT dht(DHT_PIN, DHT_TYPE);
enum AlarmLevel { NORMAL, WARNING, CRITICAL };
enum FanMode { FAN_AUTO, FAN_FORCE_ON, FAN_FORCE_OFF };
AlarmLevel alarmLevel = NORMAL;
FanMode fanMode = FAN_AUTO;
uint8_t displayPage = 0;
float lastTemperature = 0, lastHumidity = 0;
bool sensorValid = false, hasValidReading = false, alarmSilenced = false;
bool buttonStableState = HIGH, buttonLastReading = HIGH;
unsigned long buttonChangedAt = 0, lastReadAt = 0, lastWarningBeepAt = 0;
char commandBuffer[COMMAND_BUFFER_SIZE];
uint8_t commandLength = 0;
bool commandOverflow = false;

const __FlashStringHelper *alarmName() {
  if (alarmLevel == CRITICAL) return F("CRITICAL");
  if (alarmLevel == WARNING) return F("WARNING");
  return F("NORMAL");
}
const __FlashStringHelper *alarmCause() {
  const bool hot = lastTemperature >= WARNING_TEMPERATURE_C;
  const bool humid = lastHumidity >= WARNING_HUMIDITY_PCT;
  if (hot && humid) return F("TEMP+HUM");
  if (hot) return F("TEMP");
  if (humid) return F("HUM");
  return F("NONE");
}
const __FlashStringHelper *fanModeName() {
  if (fanMode == FAN_FORCE_ON) return F("ON");
  if (fanMode == FAN_FORCE_OFF) return F("OFF");
  return F("AUTO");
}
bool fanShouldRun() {
  if (!sensorValid) return hasValidReading;
  if (fanMode == FAN_FORCE_ON) return true;
  if (fanMode == FAN_FORCE_OFF) return alarmLevel == CRITICAL;
  return alarmLevel != NORMAL;
}
void setFan(bool enabled) {
  digitalWrite(FAN_PIN, (FAN_ACTIVE_HIGH ? enabled : !enabled) ? HIGH : LOW);
}
void updateAlarmLevel(float temperature, float humidity) {
  const bool critical = temperature >= CRITICAL_TEMPERATURE_C || humidity >= CRITICAL_HUMIDITY_PCT;
  const bool warning = temperature >= WARNING_TEMPERATURE_C || humidity >= WARNING_HUMIDITY_PCT;
  const bool belowWarning = temperature < WARNING_TEMPERATURE_C - TEMPERATURE_HYSTERESIS_C && humidity < WARNING_HUMIDITY_PCT - HUMIDITY_HYSTERESIS_PCT;
  const bool belowCritical = temperature < CRITICAL_TEMPERATURE_C - TEMPERATURE_HYSTERESIS_C && humidity < CRITICAL_HUMIDITY_PCT - HUMIDITY_HYSTERESIS_PCT;
  if (alarmLevel == CRITICAL) {
    if (belowCritical) alarmLevel = WARNING;
    return;
  }
  if (alarmLevel == WARNING) {
    if (critical) alarmLevel = CRITICAL;
    else if (belowWarning) { alarmLevel = NORMAL; alarmSilenced = false; }
    return;
  }
  alarmSilenced = false;
  if (critical) alarmLevel = CRITICAL;
  else if (warning) alarmLevel = WARNING;
}
void updateAlarmOutputs() {
  const unsigned long now = millis();
  if (!sensorValid) {
    digitalWrite(STATUS_LED_PIN, LOW); noTone(BUZZER_PIN); setFan(fanShouldRun()); return;
  }
  if (alarmLevel == CRITICAL) {
    digitalWrite(STATUS_LED_PIN, HIGH);
    if (alarmSilenced) noTone(BUZZER_PIN); else tone(BUZZER_PIN, 1000);
  } else if (alarmLevel == WARNING) {
    digitalWrite(STATUS_LED_PIN, (now / 500) % 2 ? HIGH : LOW);
    if (alarmSilenced) noTone(BUZZER_PIN);
    else {
      if (now - lastWarningBeepAt >= WARNING_BEEP_INTERVAL_MS) lastWarningBeepAt = now;
      if (now - lastWarningBeepAt < WARNING_BEEP_DURATION_MS) tone(BUZZER_PIN, 1000);
      else noTone(BUZZER_PIN);
    }
  } else {
    digitalWrite(STATUS_LED_PIN, LOW); noTone(BUZZER_PIN);
  }
  setFan(fanShouldRun());
}
void showReadError() {
  display.clearDisplay(); display.setTextColor(SSD1306_WHITE); display.setTextSize(1);
  display.setCursor(0, 0); display.println(F("DHT22 SENSOR"));
  display.setCursor(0, 24); display.println(F("Read failed"));
  display.setCursor(0, 42); display.println(F("Check DATA -> D2")); display.display();
}
void showSensorPage() {
  display.clearDisplay(); display.setTextColor(SSD1306_WHITE); display.setTextSize(1);
  display.setCursor(0, 0); display.print(F("P1 TEMP/HUM ")); display.println(alarmName());
  display.setTextSize(2); display.setCursor(0, 16); display.print(lastTemperature, 1); display.println(F(" C"));
  display.setCursor(0, 40); display.print(lastHumidity, 1); display.println(F(" %")); display.display();
}
void showControlPage() {
  display.clearDisplay(); display.setTextColor(SSD1306_WHITE); display.setTextSize(1);
  display.setCursor(0, 0); display.println(F("P2 MODULE STATUS"));
  display.print(F("Sensor: ")); display.println(sensorValid ? F("OK") : F("ERROR"));
  display.print(F("Alarm:  ")); display.print(alarmName()); display.print(F(" ")); display.println(alarmCause());
  display.print(F("Temp:   ")); display.print(lastTemperature, 1); display.println(F(" C"));
  display.print(F("Hum:    ")); display.print(lastHumidity, 1); display.println(F(" %"));
  display.print(F("Buzzer: ")); display.println(alarmLevel == NORMAL || alarmSilenced ? F("OFF") : F("ON"));
  display.print(F("Fan:    ")); display.print(fanModeName()); display.print(F(" ")); display.println(fanShouldRun() ? F("RUN") : F("STOP"));
  display.print(F("LED:    ")); display.println(alarmLevel == NORMAL ? F("OFF") : F("ON"));
  display.print(F("BT:     ")); display.println(F("9600")); display.display();
}
void showHelpPage() {
  display.clearDisplay(); display.setTextColor(SSD1306_WHITE); display.setTextSize(1); display.setCursor(0, 0);
  display.println(F("P3 COMMANDS"));
  display.println(F("STATUS   HELP"));
  display.println(F("PAGE 0/1/2"));
  display.println(F("FAN AUTO/ON/OFF"));
  display.println(F("ALARM ON/OFF"));
  display.println(F("OK = MUTE BUZZER"));
  display.println(F("TEMP 28/30C HUM 70/80%")); display.display();
}
void refreshDisplay() {
  if (displayPage == 0 && !sensorValid) showReadError();
  else if (displayPage == 0) showSensorPage();
  else if (displayPage == 1) showControlPage();
  else showHelpPage();
}
void sendPhoneReport() {
  if (!sensorValid) { Serial.println(F("DHT22 read failed")); return; }
  Serial.println(F("------ ENVIRONMENT MONITOR ------"));
  Serial.print(F("Temperature: ")); Serial.print(lastTemperature, 1); Serial.println(F(" C"));
  Serial.print(F("Humidity: ")); Serial.print(lastHumidity, 1); Serial.println(F(" %"));
  Serial.print(F("Status: ")); Serial.print(alarmName()); Serial.print(F(" Cause: ")); Serial.println(alarmCause());
  Serial.println(alarmLevel == NORMAL ? F("LED: OFF") : F("LED: ON"));
  Serial.print(F("Buzzer: ")); Serial.println(alarmLevel == NORMAL || alarmSilenced ? F("OFF") : F("ON"));
  Serial.print(F("Fan mode: ")); Serial.println(fanModeName());
  Serial.print(F("Fan: ")); Serial.println(fanShouldRun() ? F("ON") : F("OFF"));
  Serial.println(F("---------------------------------"));
}
void uppercaseCommand() {
  for (uint8_t i = 0; i < commandLength; ++i)
    if (commandBuffer[i] >= 'a' && commandBuffer[i] <= 'z') commandBuffer[i] -= 'a' - 'A';
  commandBuffer[commandLength] = '\0';
}
bool commandEquals(const char *expected) { return strcmp(commandBuffer, expected) == 0; }
void handleCommand() {
  if (commandOverflow) { Serial.println(F("ERR command too long")); return; }
  uppercaseCommand();
  if (commandEquals("STATUS")) sendPhoneReport();
  else if (commandEquals("HELP")) Serial.println(F("OK STATUS | PAGE 0/1/2 | FAN AUTO/ON/OFF | ALARM ON/OFF | HELP"));
  else if (commandEquals("PAGE 0") || commandEquals("PAGE 1") || commandEquals("PAGE 2")) {
    displayPage = commandBuffer[5] - '0'; refreshDisplay(); Serial.print(F("OK PAGE ")); Serial.println(displayPage);
  } else if (commandEquals("ALARM OFF")) {
    alarmSilenced = true; updateAlarmOutputs(); refreshDisplay(); Serial.println(F("OK ALARM OFF (sound muted)"));
  } else if (commandEquals("ALARM ON")) {
    alarmSilenced = false; updateAlarmOutputs(); refreshDisplay(); Serial.println(F("OK ALARM ON (sound enabled)"));
  } else if (commandEquals("OK")) {
    alarmSilenced = true; updateAlarmOutputs(); refreshDisplay(); Serial.println(F("OK ALARM OFF (sound muted)"));
  } else if (commandEquals("FAN AUTO")) {
    fanMode = FAN_AUTO; updateAlarmOutputs(); refreshDisplay(); Serial.println(F("OK FAN AUTO"));
  } else if (commandEquals("FAN ON")) {
    fanMode = FAN_FORCE_ON; updateAlarmOutputs(); refreshDisplay(); Serial.println(F("OK FAN ON"));
  } else if (commandEquals("FAN OFF")) {
    fanMode = FAN_FORCE_OFF; updateAlarmOutputs(); refreshDisplay();
    if (alarmLevel == CRITICAL || !sensorValid) Serial.println(F("ERR safety override: fan remains ON"));
    else Serial.println(F("OK FAN OFF"));
  } else Serial.println(F("ERR unknown command"));
}
void pollSerialCommands() {
  while (Serial.available() > 0) {
    const char received = static_cast<char>(Serial.read());
    if (received == '\r') continue;
    if (received == '\n') { handleCommand(); commandLength = 0; commandOverflow = false; }
    else if (!commandOverflow) {
      if (commandLength < COMMAND_BUFFER_SIZE - 1) commandBuffer[commandLength++] = received;
      else commandOverflow = true;
    }
  }
}
void pollButton() {
  const bool reading = digitalRead(BUTTON_PIN); const unsigned long now = millis();
  if (reading != buttonLastReading) buttonChangedAt = now;
  if (now - buttonChangedAt >= BUTTON_DEBOUNCE_MS && reading != buttonStableState) {
    buttonStableState = reading;
    if (buttonStableState == LOW) { displayPage = (displayPage + 1) % 3; refreshDisplay(); }
  }
  buttonLastReading = reading;
}
void readSensorIfDue() {
  const unsigned long now = millis(); if (now - lastReadAt < READ_INTERVAL_MS) return; lastReadAt = now;
  const float humidity = dht.readHumidity(), temperature = dht.readTemperature();
  sensorValid = !isnan(humidity) && !isnan(temperature) && humidity >= 0.0 && humidity <= 100.0 && temperature >= -40.0 && temperature <= 80.0;
  if (!sensorValid) { updateAlarmOutputs(); refreshDisplay(); sendPhoneReport(); return; }
  lastHumidity = humidity; lastTemperature = temperature;
  hasValidReading = true;
  updateAlarmLevel(temperature, humidity);
  updateAlarmOutputs();
  refreshDisplay();
  sendPhoneReport();
}
void setup() {
  Serial.begin(BLUETOOTH_BAUD); pinMode(BUZZER_PIN, OUTPUT); pinMode(STATUS_LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP); pinMode(FAN_PIN, OUTPUT); setFan(false); updateAlarmOutputs(); dht.begin();
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) while (true) delay(1000);
  display.clearDisplay(); display.setTextColor(SSD1306_WHITE); display.setTextSize(1); display.setCursor(0, 0);
  display.println(F("DHT22 + BLUETOOTH")); display.display(); Serial.println(F("DHT22 Bluetooth monitor started"));
  lastReadAt = millis() - READ_INTERVAL_MS;
}
void loop() { pollSerialCommands(); pollButton(); updateAlarmOutputs(); readSensorIfDue(); }
