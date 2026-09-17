#pragma once
// 宿主机测试桩：模拟 Arduino 核心 + Wire/OLED/DHT，用于脱离硬件验证 .ino 的纯逻辑。
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>

#define HIGH 1
#define LOW 0
#define OUTPUT 1
#define INPUT_PULLUP 2
#define LED_BUILTIN 13
#define SSD1306_WHITE 1
#define SSD1306_SWITCHCAPVCC 2
#define DHT22 22

// F() 仅用 flash 字符串；宿主机退化为普通字符串
using std::isnan;
#define F(x) (x)
#define __FlashStringHelper char

extern unsigned long g_millis;
inline unsigned long millis() { return g_millis; }
inline void delay(unsigned long) {}

extern int g_led, g_buzzerTone, g_fan, g_buzzerFreq;
inline void pinMode(int, int) {}
inline int digitalRead(int) { return HIGH; }
inline void digitalWrite(int pin, int value) {
  if (pin == LED_BUILTIN) g_led = value;
  if (pin == 9) g_fan = value;
}
inline void tone(int, unsigned int freq) { g_buzzerTone = 1; g_buzzerFreq = (int)freq; }
inline void noTone(int) { g_buzzerTone = 0; g_buzzerFreq = 0; }

extern std::string g_serialOut;          // 捕获串口输出
extern std::string g_serialIn;           // 模拟蓝牙输入
struct SerialStub {
  void begin(long) {}
  int available() { return (int)g_serialIn.size(); }
  int read() {
    if (g_serialIn.empty()) return -1;
    int c = (unsigned char)g_serialIn[0];
    g_serialIn.erase(0, 1);
    return c;
  }
  void print(const char *s) { g_serialOut += s; }
  void print(char c) { g_serialOut += c; }
  void print(unsigned char v) { g_serialOut += std::to_string((unsigned)v); }
  void print(int v) { g_serialOut += std::to_string(v); }
  void print(unsigned int v) { g_serialOut += std::to_string(v); }
  void print(long v) { g_serialOut += std::to_string(v); }
  void print(unsigned long v) { g_serialOut += std::to_string(v); }
  void print(double v, int digits = 2) {
    char buf[32]; snprintf(buf, sizeof(buf), "%.*f", digits, v); g_serialOut += buf;
  }
  void println() { g_serialOut += "\n"; }
  void println(const char *s) { print(s); println(); }
  void println(char c) { print(c); println(); }
  void println(unsigned char v) { print(v); println(); }
  void println(int v) { print(v); println(); }
  void println(unsigned int v) { print(v); println(); }
  void println(long v) { print(v); println(); }
  void println(unsigned long v) { print(v); println(); }
  void println(double v, int digits = 2) { print(v, digits); println(); }
};
extern SerialStub Serial;

struct TwoWire {};
extern TwoWire Wire;

extern std::string g_displayOut;
struct Adafruit_SSD1306 {
  Adafruit_SSD1306(int, int, TwoWire *, int) {}
  bool begin(int, int) { return true; }
  void clearDisplay() { g_displayOut.clear(); }
  void setTextColor(int) {}
  void setTextSize(int) {}
  void setCursor(int, int) {}
  void display() {}
  void print(const char *s) { g_displayOut += s; }
  void print(char c) { g_displayOut += c; }
  void print(int v) { g_displayOut += std::to_string(v); }
  void print(double v, int digits = 2) {
    char buf[32]; snprintf(buf, sizeof(buf), "%.*f", digits, v); g_displayOut += buf;
  }
  void println(const char *s) { print(s); g_displayOut += "\n"; }
  void println(char c) { print(c); g_displayOut += "\n"; }
  void println(int v) { print(v); g_displayOut += "\n"; }
};
extern Adafruit_SSD1306 display;

// 测试可写的 DHT22 读数
extern float g_dhtTemperature, g_dhtHumidity;
struct DHT {
  DHT(int, int) {}
  void begin() {}
  float readTemperature() { return g_dhtTemperature; }
  float readHumidity() { return g_dhtHumidity; }
};
