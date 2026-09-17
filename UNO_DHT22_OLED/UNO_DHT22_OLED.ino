#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>

const uint8_t SCREEN_WIDTH = 128, SCREEN_HEIGHT = 64, OLED_ADDRESS = 0x3C;
const uint8_t DHT_PIN = 2, BUZZER_PIN = 3, BUTTON_PIN = 4, FAN_PIN = 9;
const uint8_t STATUS_LED_PIN = LED_BUILTIN, COMMAND_BUFFER_SIZE = 32;
const float TEMPERATURE_HYSTERESIS_C = 1.0, HUMIDITY_HYSTERESIS_PCT = 5.0;
const float COLD_TEMPERATURE_C = 8.0, COLD_RELEASE_TEMPERATURE_C = 10.0;
const unsigned long READ_INTERVAL_MS = 2000, BUTTON_DEBOUNCE_MS = 40;
const unsigned long WARNING_BEEP_INTERVAL_MS = 1500, WARNING_BEEP_DURATION_MS = 180;
const long BLUETOOTH_BAUD = 9600;
const bool FAN_ACTIVE_HIGH = true;
#define DHT_TYPE DHT22

// 分时时段边界（一天内的秒数）
const unsigned long MORNING_START_S = 6UL * 3600UL;
const unsigned long AFTERNOON_START_S = 12UL * 3600UL;
const unsigned long NIGHT_START_S = 18UL * 3600UL;
const unsigned long SECONDS_PER_DAY = 24UL * 3600UL;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
DHT dht(DHT_PIN, DHT_TYPE);
enum AlarmLevel { NORMAL, WARNING, CRITICAL };
enum FanMode { FAN_AUTO, FAN_FORCE_ON, FAN_FORCE_OFF };
enum TimePeriod { PERIOD_UNSET, PERIOD_MORNING, PERIOD_AFTERNOON, PERIOD_NIGHT };

struct ThresholdPair { float warning; float critical; };

// 未校时按 28/30 ℃；分时温度阈值 上午 25/30、下午 23/28、夜间 12/15 ℃；湿度全天 70/80 %RH
const ThresholdPair UNSET_TEMPERATURE_THRESHOLD = {28.0, 30.0};
ThresholdPair morningTemperatureThreshold = {25.0, 30.0};
ThresholdPair afternoonTemperatureThreshold = {23.0, 28.0};
ThresholdPair nightTemperatureThreshold = {12.0, 15.0};
ThresholdPair humidityThreshold = {70.0, 80.0};

AlarmLevel alarmLevel = NORMAL;
FanMode fanMode = FAN_AUTO;
TimePeriod currentPeriod = PERIOD_UNSET;
ThresholdPair activeTemperatureThreshold = UNSET_TEMPERATURE_THRESHOLD;
uint8_t displayPage = 0;
float lastTemperature = 0, lastHumidity = 0;
bool sensorValid = false, hasValidReading = false, alarmSilenced = false;
bool coldProtection = false;
bool clockSet = false;
uint32_t clockSecondOfDay = 0, clockTickMillis = 0;
bool buttonStableState = HIGH, buttonLastReading = HIGH;
unsigned long buttonChangedAt = 0, lastReadAt = 0, lastWarningBeepAt = 0;
char commandBuffer[COMMAND_BUFFER_SIZE];
uint8_t commandLength = 0;
bool commandOverflow = false;

void updateAlarmOutputs();
void refreshDisplay();

const __FlashStringHelper *alarmName() {
  if (alarmLevel == CRITICAL) return F("CRITICAL");
  if (alarmLevel == WARNING) return F("WARNING");
  return F("NORMAL");
}
const __FlashStringHelper *alarmCause() {
  const bool hot = lastTemperature >= activeTemperatureThreshold.warning;
  const bool humid = lastHumidity >= humidityThreshold.warning;
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
const __FlashStringHelper *periodName() {
  if (currentPeriod == PERIOD_MORNING) return F("DAY");
  if (currentPeriod == PERIOD_AFTERNOON) return F("PM");
  if (currentPeriod == PERIOD_NIGHT) return F("NIGHT");
  return F("UNSET");
}
bool fanShouldRun() {
  if (!sensorValid) return hasValidReading;
  if (fanMode == FAN_FORCE_ON) return true;
  if (fanMode == FAN_FORCE_OFF) return alarmLevel == CRITICAL;
  if (alarmLevel == CRITICAL) return true;
  if (alarmLevel == WARNING) return !coldProtection;
  return false;
}
void setFan(bool enabled) {
  digitalWrite(FAN_PIN, (FAN_ACTIVE_HIGH ? enabled : !enabled) ? HIGH : LOW);
}
// 时钟仅由 millis() 的无符号时间差推进，断电或复位后需重新发送 TIME。
// 按增量更新时间可跨越约 49.7 天的 millis() 回绕；它仍不是带后备电池的 RTC。
unsigned long currentSecondOfDay() {
  return clockSet ? clockSecondOfDay : 0;
}
TimePeriod periodForSecond(unsigned long secondOfDay) {
  if (!clockSet) return PERIOD_UNSET;
  if (secondOfDay < MORNING_START_S) return PERIOD_NIGHT;
  if (secondOfDay < AFTERNOON_START_S) return PERIOD_MORNING;
  if (secondOfDay < NIGHT_START_S) return PERIOD_AFTERNOON;
  return PERIOD_NIGHT;
}
void formatClock(char *buffer, uint8_t size) {
  if (!clockSet) { strncpy(buffer, "NOT SET", size); buffer[size - 1] = '\0'; return; }
  const unsigned long second = currentSecondOfDay();
  snprintf(buffer, size, "%02u:%02u:%02u", (unsigned)(second / 3600UL), (unsigned)((second / 60UL) % 60UL), (unsigned)(second % 60UL));
}
void updateAlarmLevel(float temperature, float humidity) {
  const bool critical = temperature >= activeTemperatureThreshold.critical || humidity >= humidityThreshold.critical;
  const bool warning = temperature >= activeTemperatureThreshold.warning || humidity >= humidityThreshold.warning;
  const bool belowWarning = temperature < activeTemperatureThreshold.warning - TEMPERATURE_HYSTERESIS_C && humidity < humidityThreshold.warning - HUMIDITY_HYSTERESIS_PCT;
  const bool belowCritical = temperature < activeTemperatureThreshold.critical - TEMPERATURE_HYSTERESIS_C && humidity < humidityThreshold.critical - HUMIDITY_HYSTERESIS_PCT;
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
void updateColdProtection(float temperature) {
  if (temperature <= COLD_TEMPERATURE_C) coldProtection = true;
  else if (temperature >= COLD_RELEASE_TEMPERATURE_C) coldProtection = false;
}
// 时段切换后按新阈值重新判定，旧的报警状态和回差不再沿用
void applyTimePeriod(TimePeriod period) {
  currentPeriod = period;
  if (period == PERIOD_MORNING) activeTemperatureThreshold = morningTemperatureThreshold;
  else if (period == PERIOD_AFTERNOON) activeTemperatureThreshold = afternoonTemperatureThreshold;
  else if (period == PERIOD_NIGHT) activeTemperatureThreshold = nightTemperatureThreshold;
  else activeTemperatureThreshold = UNSET_TEMPERATURE_THRESHOLD;
  alarmLevel = NORMAL;
  if (hasValidReading) updateAlarmLevel(lastTemperature, lastHumidity);
  updateAlarmOutputs();
  refreshDisplay();
}
void updateTimePeriod() {
  if (!clockSet) return;
  const TimePeriod period = periodForSecond(currentSecondOfDay());
  if (period != currentPeriod) applyTimePeriod(period);
}
void tickClock() {
  if (!clockSet) return;
  const uint32_t now = (uint32_t)millis();
  const uint32_t elapsedSeconds = (uint32_t)(now - clockTickMillis) / 1000UL;
  if (elapsedSeconds == 0) return;
  clockTickMillis += elapsedSeconds * 1000UL;
  clockSecondOfDay = (clockSecondOfDay + elapsedSeconds) % SECONDS_PER_DAY;
  updateTimePeriod();
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
  display.setCursor(0, 0); display.print(F("P1 ")); display.print(periodName()); display.print(' '); display.println(alarmName());
  display.setTextSize(2); display.setCursor(0, 16); display.print(lastTemperature, 1); display.println(F(" C"));
  display.setCursor(0, 40); display.print(lastHumidity, 1); display.println(F(" %")); display.display();
}
void showControlPage() {
  char clockText[9];
  formatClock(clockText, sizeof(clockText));
  display.clearDisplay(); display.setTextColor(SSD1306_WHITE); display.setTextSize(1);
  display.setCursor(0, 0); display.println(F("P2 MODULE STATUS"));
  display.print(F("Time: ")); display.println(clockText);
  display.print(F("Prd: ")); display.print(periodName()); display.print(' ');
  display.print(activeTemperatureThreshold.warning, 0); display.print('/'); display.print(activeTemperatureThreshold.critical, 0); display.println(F(" C"));
  display.print(F("HumTH: ")); display.print(humidityThreshold.warning, 0); display.print('/'); display.print(humidityThreshold.critical, 0); display.println(F(" %RH"));
  display.print(F("Sensor: ")); display.print(sensorValid ? F("OK") : F("ERROR")); display.print(F(" Cold:")); display.println(coldProtection ? F("YES") : F("NO"));
  display.print(F("Alarm: ")); display.print(alarmName()); display.print(' '); display.println(alarmCause());
  display.print(F("Buzz: ")); display.print(alarmLevel == NORMAL || alarmSilenced ? F("OFF") : F("ON")); display.print(F("  LED: ")); display.println(alarmLevel == NORMAL ? F("OFF") : F("ON"));
  display.print(F("Fan: ")); display.print(fanModeName()); display.print(' '); display.println(fanShouldRun() ? F("RUN") : F("STOP")); display.display();
}
void showHelpPage() {
  display.clearDisplay(); display.setTextColor(SSD1306_WHITE); display.setTextSize(1); display.setCursor(0, 0);
  display.println(F("P3 COMMANDS"));
  display.println(F("STATUS   HELP"));
  display.println(F("PAGE 0/1/2"));
  display.println(F("FAN AUTO/ON/OFF"));
  display.println(F("ALARM ON/OFF OK=MUTE"));
  display.println(F("TIME HH:MM[:SS]"));
  display.println(F("SET DAY/PM/NIGHT W C"));
  display.println(F("SET HUM W C")); display.display();
}
void refreshDisplay() {
  if (displayPage == 0 && !sensorValid) showReadError();
  else if (displayPage == 0) showSensorPage();
  else if (displayPage == 1) showControlPage();
  else showHelpPage();
}
void sendPhoneReport() {
  if (!sensorValid) { Serial.println(F("DHT22 read failed")); return; }
  char clockText[9];
  formatClock(clockText, sizeof(clockText));
  Serial.println(F("------ ENVIRONMENT MONITOR ------"));
  Serial.print(F("Time: ")); Serial.print(clockText); Serial.print(F(" Period: ")); Serial.println(periodName());
  Serial.print(F("Temperature: ")); Serial.print(lastTemperature, 1); Serial.println(F(" C"));
  Serial.print(F("Humidity: ")); Serial.print(lastHumidity, 1); Serial.println(F(" %"));
  Serial.print(F("Threshold: ")); Serial.print(activeTemperatureThreshold.warning, 0); Serial.print('/'); Serial.print(activeTemperatureThreshold.critical, 0);
  Serial.print(F(" C  ")); Serial.print(humidityThreshold.warning, 0); Serial.print('/'); Serial.print(humidityThreshold.critical, 0); Serial.println(F(" %RH"));
  Serial.print(F("Status: ")); Serial.print(alarmName()); Serial.print(F(" Cause: ")); Serial.println(alarmCause());
  Serial.println(alarmLevel == NORMAL ? F("LED: OFF") : F("LED: ON"));
  Serial.print(F("Buzzer: ")); Serial.println(alarmLevel == NORMAL || alarmSilenced ? F("OFF") : F("ON"));
  Serial.print(F("Fan mode: ")); Serial.println(fanModeName());
  Serial.print(F("Fan: ")); Serial.println(fanShouldRun() ? F("ON") : F("OFF"));
  Serial.print(F("Cold protection: ")); Serial.println(coldProtection ? F("ON") : F("OFF"));
  Serial.println(F("---------------------------------"));
}
void uppercaseCommand() {
  for (uint8_t i = 0; i < commandLength; ++i)
    if (commandBuffer[i] >= 'a' && commandBuffer[i] <= 'z') commandBuffer[i] -= 'a' - 'A';
  commandBuffer[commandLength] = '\0';
}
bool commandEquals(const char *expected) { return strcmp(commandBuffer, expected) == 0; }
// 解析 HH:MM 或 HH:MM:SS，拒绝越界和多余字符
bool parseTimeArgument(const char *argument, unsigned long &secondOfDay) {
  int part[3] = {0, 0, 0};
  uint8_t index = 0;
  while (*argument == ' ') ++argument;
  while (index < 3) {
    uint8_t digits = 0;
    int value = 0;
    while (*argument >= '0' && *argument <= '9' && digits < 2) { value = value * 10 + (*argument - '0'); ++argument; ++digits; }
    if (digits == 0) break;
    part[index++] = value;
    if (*argument == ':') { ++argument; continue; }
    break;
  }
  if (*argument != '\0' || index < 2 || index > 3) return false;
  if (part[0] > 23 || part[1] > 59 || part[2] > 59) return false;
  secondOfDay = (unsigned long)part[0] * 3600UL + (unsigned long)part[1] * 60UL + (unsigned long)part[2];
  return true;
}
// 解析无符号/小数阈值，拒绝非法字符
bool parseFloatArgument(const char *&argument, float &value) {
  while (*argument == ' ') ++argument;
  bool negative = false;
  if (*argument == '-') { negative = true; ++argument; }
  if (*argument < '0' || *argument > '9') return false;
  float result = 0.0;
  while (*argument >= '0' && *argument <= '9') { result = result * 10.0 + (*argument - '0'); ++argument; }
  if (*argument == '.') {
    ++argument;
    float scale = 0.1;
    uint8_t digits = 0;
    while (*argument >= '0' && *argument <= '9' && digits < 2) { result += (*argument - '0') * scale; scale *= 0.1; ++argument; ++digits; }
  }
  value = negative ? -result : result;
  return true;
}
bool validThresholdPair(const ThresholdPair &pair, float minimumWarning, float maximumCritical, float minimumGap) {
  return pair.warning >= minimumWarning && pair.critical <= maximumCritical &&
         pair.critical >= pair.warning + minimumGap;
}
void handleTimeCommand() {
  unsigned long secondOfDay = 0;
  if (!parseTimeArgument(commandBuffer + 4, secondOfDay)) {
    Serial.println(F("ERR TIME format: TIME HH:MM[:SS]")); return;
  }
  clockSet = true;
  clockSecondOfDay = secondOfDay;
  clockTickMillis = millis();
  applyTimePeriod(periodForSecond(secondOfDay));
  char clockText[9];
  formatClock(clockText, sizeof(clockText));
  Serial.print(F("OK TIME ")); Serial.println(clockText);
}
void handleSetCommand() {
  const char *argument = commandBuffer + 3;
  while (*argument == ' ') ++argument;
  char target[6];
  uint8_t length = 0;
  while (*argument != '\0' && *argument != ' ' && length < sizeof(target) - 1) target[length++] = *argument++;
  target[length] = '\0';
  ThresholdPair pair;
  if (!parseFloatArgument(argument, pair.warning) || !parseFloatArgument(argument, pair.critical)) {
    Serial.println(F("ERR SET format: SET DAY/PM/NIGHT W C | SET HUM W C")); return;
  }
  while (*argument == ' ') ++argument;
  if (*argument != '\0') { Serial.println(F("ERR SET too many values")); return; }
  if (strcmp(target, "DAY") == 0 || strcmp(target, "PM") == 0 || strcmp(target, "NIGHT") == 0) {
    if (!validThresholdPair(pair, 5.0, 45.0, 1.0)) { Serial.println(F("ERR SET temperature: warning >=5 C, critical <=45 C, gap >=1 C")); return; }
    if (strcmp(target, "DAY") == 0) morningTemperatureThreshold = pair;
    else if (strcmp(target, "PM") == 0) afternoonTemperatureThreshold = pair;
    else nightTemperatureThreshold = pair;
    if ((strcmp(target, "DAY") == 0 && currentPeriod == PERIOD_MORNING) ||
        (strcmp(target, "PM") == 0 && currentPeriod == PERIOD_AFTERNOON) ||
        (strcmp(target, "NIGHT") == 0 && currentPeriod == PERIOD_NIGHT)) {
      activeTemperatureThreshold = pair;
      alarmLevel = NORMAL;
      if (hasValidReading) updateAlarmLevel(lastTemperature, lastHumidity);
      updateAlarmOutputs(); refreshDisplay();
    }
  } else if (strcmp(target, "HUM") == 0) {
    if (!validThresholdPair(pair, 30.0, 100.0, 5.0)) { Serial.println(F("ERR SET humidity: warning >=30 %RH, critical <=100 %RH, gap >=5 %RH")); return; }
    humidityThreshold = pair;
    alarmLevel = NORMAL;
    if (hasValidReading) updateAlarmLevel(lastTemperature, lastHumidity);
    updateAlarmOutputs(); refreshDisplay();
  } else {
    Serial.println(F("ERR SET target: DAY/PM/NIGHT/HUM")); return;
  }
  Serial.print(F("OK SET ")); Serial.print(target); Serial.print(' ');
  Serial.print(pair.warning, 1); Serial.print('/'); Serial.println(pair.critical, 1);
}
void handleCommand() {
  if (commandOverflow) { Serial.println(F("ERR command too long")); return; }
  uppercaseCommand();
  if (commandEquals("STATUS")) sendPhoneReport();
  else if (commandEquals("HELP")) Serial.println(F("OK STATUS | PAGE 0/1/2 | FAN AUTO/ON/OFF | ALARM ON/OFF | TIME HH:MM[:SS] | SET DAY|PM|NIGHT W C | SET HUM W C"));
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
    if (fanShouldRun()) Serial.println(F("ERR safety override: fan remains ON"));
    else Serial.println(F("OK FAN OFF"));
  } else if (strncmp(commandBuffer, "TIME ", 5) == 0) handleTimeCommand();
  else if (strncmp(commandBuffer, "SET ", 4) == 0) handleSetCommand();
  else Serial.println(F("ERR unknown command"));
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
  updateColdProtection(temperature);
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
  display.println(F("DHT22 + BLUETOOTH")); display.println(F("TIME HH:MM TO SET")); display.display();
  Serial.println(F("DHT22 Bluetooth monitor started"));
  Serial.println(F("Send TIME HH:MM[:SS] to set clock; SET DAY|PM|NIGHT W C and SET HUM W C to adjust thresholds"));
  lastReadAt = millis() - READ_INTERVAL_MS;
}
void loop() { pollSerialCommands(); tickClock(); pollButton(); updateAlarmOutputs(); readSensorIfDue(); }
