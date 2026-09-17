#include "Arduino.h"
#include <iostream>

// 宿主机桩实例：display / dht 由 .ino 自己定义，这里只提供 Serial、Wire 与读数变量
unsigned long g_millis = 0;
int g_led = 0, g_buzzerTone = 0, g_fan = 0, g_buzzerFreq = 0;
std::string g_serialOut, g_serialIn, g_displayOut;
SerialStub Serial;
TwoWire Wire;
float g_dhtTemperature = NAN, g_dhtHumidity = NAN;

#include "../../UNO_DHT22_OLED/UNO_DHT22_OLED.ino"

static int g_pass = 0, g_fail = 0;
void section(const char *name) { std::cout << "\n== " << name << " ==\n"; }
void check(bool ok, const std::string &what) {
  if (ok) { ++g_pass; std::cout << "  PASS  " << what << "\n"; }
  else { ++g_fail; std::cout << "  FAIL  " << what << "\n"; }
}
void checkEq(long actual, long expected, const std::string &what) {
  const bool ok = actual == expected;
  if (!ok) std::cout << "        expected=" << expected << " actual=" << actual << "\n";
  check(ok, what);
}
std::string sendLine(const std::string &line) {
  g_serialIn = line + "\n";
  g_serialOut.clear();
  pollSerialCommands();
  return g_serialOut;
}
void feedReading(float temperature, float humidity) {
  g_dhtTemperature = temperature; g_dhtHumidity = humidity;
  g_millis += READ_INTERVAL_MS + 1;
  readSensorIfDue();
}
void setClockAt(int hour, int minute) {
  char buf[20];
  snprintf(buf, sizeof(buf), "TIME %02d:%02d", hour, minute);
  sendLine(buf);
}

int main() {
  std::cout << "主程序逻辑测试（宿主机，脱离硬件）\n";

  section("时段边界 06:00 / 12:00 / 18:00 / 00:00");
  setClockAt(0, 0);   checkEq(periodForSecond(currentSecondOfDay()), PERIOD_NIGHT, "00:00 -> NIGHT");
  setClockAt(5, 59);  checkEq(periodForSecond(currentSecondOfDay()), PERIOD_NIGHT, "05:59 -> NIGHT");
  setClockAt(6, 0);   checkEq(periodForSecond(currentSecondOfDay()), PERIOD_MORNING, "06:00 -> DAY");
  setClockAt(11, 59); checkEq(periodForSecond(currentSecondOfDay()), PERIOD_MORNING, "11:59 -> DAY");
  setClockAt(12, 0);  checkEq(periodForSecond(currentSecondOfDay()), PERIOD_AFTERNOON, "12:00 -> PM");
  setClockAt(17, 59); checkEq(periodForSecond(currentSecondOfDay()), PERIOD_AFTERNOON, "17:59 -> PM");
  setClockAt(18, 0);  checkEq(periodForSecond(currentSecondOfDay()), PERIOD_NIGHT, "18:00 -> NIGHT");
  setClockAt(23, 59); checkEq(periodForSecond(currentSecondOfDay()), PERIOD_NIGHT, "23:59 -> NIGHT");

  section("分时阈值加载");
  setClockAt(8, 0);
  checkEq((long)(activeTemperatureThreshold.warning * 10), 250, "DAY warning 25.0");
  checkEq((long)(activeTemperatureThreshold.critical * 10), 300, "DAY critical 30.0");
  setClockAt(14, 0);
  checkEq((long)(activeTemperatureThreshold.warning * 10), 230, "PM warning 23.0");
  checkEq((long)(activeTemperatureThreshold.critical * 10), 280, "PM critical 28.0");
  setClockAt(22, 0);
  checkEq((long)(activeTemperatureThreshold.warning * 10), 120, "NIGHT warning 12.0");
  checkEq((long)(activeTemperatureThreshold.critical * 10), 150, "NIGHT critical 15.0");
  checkEq((long)humidityThreshold.warning, 70, "humidity warning 70 全天");
  checkEq((long)humidityThreshold.critical, 80, "humidity critical 80 全天");

  section("未校时回退 28/30");
  clockSet = false; applyTimePeriod(PERIOD_UNSET);
  checkEq((long)activeTemperatureThreshold.warning, 28, "UNSET warning 28");
  checkEq((long)activeTemperatureThreshold.critical, 30, "UNSET critical 30");
  check(periodForSecond(0) == PERIOD_UNSET, "未校时 -> UNSET");

  section("阈值触发与回差 DAY 25/30，温度回差 1 C");
  setClockAt(8, 0);
  feedReading(24.5, 60); checkEq(alarmLevel, NORMAL, "24.5 C 正常");
  feedReading(25.0, 60); checkEq(alarmLevel, WARNING, "25.0 C 预警（等于阈值）");
  feedReading(24.5, 60); checkEq(alarmLevel, WARNING, "回差内 24.5 C 保持预警");
  feedReading(23.9, 60); checkEq(alarmLevel, NORMAL, "低于 24.0 C 退出预警");
  feedReading(30.0, 60); checkEq(alarmLevel, CRITICAL, "30.0 C 严重报警");
  feedReading(29.5, 60); checkEq(alarmLevel, CRITICAL, "回差内 29.5 C 保持严重");
  feedReading(28.9, 60); checkEq(alarmLevel, WARNING, "低于 29.0 C 降为预警");

  section("湿度阈值 全天 70/80，回差 5 %RH");
  feedReading(20, 64.0); checkEq(alarmLevel, NORMAL, "先复位为正常");
  feedReading(20, 69.9); checkEq(alarmLevel, NORMAL, "RH 69.9 正常");
  feedReading(20, 70.0); checkEq(alarmLevel, WARNING, "RH 70.0 预警");
  feedReading(20, 65.5); checkEq(alarmLevel, WARNING, "回差内 65.5 保持预警");
  feedReading(20, 64.9); checkEq(alarmLevel, NORMAL, "低于 65.0 退出预警");

  section("时段切换后按新阈值重判");
  feedReading(13.0, 60); checkEq(alarmLevel, NORMAL, "DAY 13.0 C 正常（白天阈值 25/30）");
  setClockAt(19, 0);
  checkEq(alarmLevel, WARNING, "切到夜间 13.0 C 仍预警");
  feedReading(16.0, 60); checkEq(alarmLevel, CRITICAL, "夜间 16.0 C 严重");
  setClockAt(8, 0);
  checkEq(alarmLevel, NORMAL, "切回白天 16.0 C 正常");

  section("低温保护 8 进入 / 10 解除");
  feedReading(9.0, 60); check(!coldProtection, "9.0 C 不触发");
  feedReading(8.0, 60); check(coldProtection, "8.0 C 触发");
  feedReading(9.0, 60); check(coldProtection, "9.0 C 保持（<10）");
  feedReading(10.0, 60); check(!coldProtection, "10.0 C 解除");
  fanMode = FAN_AUTO;
  feedReading(8.0, 60); checkEq(fanShouldRun(), 0, "低温且正常：AUTO 不通风");
  feedReading(8.0, 90); checkEq(fanShouldRun(), 1, "低温但严重报警：安全强制运行");
  feedReading(8.0, 60); fanMode = FAN_FORCE_ON; checkEq(fanShouldRun(), 1, "低温不覆盖 FAN ON");
  fanMode = FAN_AUTO;

  section("安全覆盖：FAN OFF 不得绕过");
  feedReading(31.0, 60); checkEq(alarmLevel, CRITICAL, "31 C 严重报警");
  const std::string offReply = sendLine("FAN OFF");
  checkEq(fanMode, FAN_FORCE_OFF, "FAN OFF 命令已记录");
  checkEq(fanShouldRun(), 1, "严重报警下风扇仍运行");
  check(offReply.find("safety override") != std::string::npos, "返回 safety override 提示");
  setClockAt(8, 0); applyTimePeriod(PERIOD_MORNING);
  feedReading(20.0, 60); checkEq(fanShouldRun(), 0, "正常状态下 FAN OFF 生效");

  section("传感器失效安全策略");
  fanMode = FAN_AUTO; hasValidReading = true; sensorValid = false;
  checkEq(fanShouldRun(), 1, "曾有效读数：失效后保持运行");
  hasValidReading = false;
  checkEq(fanShouldRun(), 0, "从未有效读数：失效后保持关闭");

  section("静音不关闭安全风扇");
  sensorValid = true; hasValidReading = true; fanMode = FAN_AUTO;
  applyTimePeriod(PERIOD_MORNING);
  feedReading(31.0, 60);
  sendLine("ALARM OFF");
  check(alarmSilenced, "ALARM OFF 已静音");
  checkEq(alarmLevel, CRITICAL, "静音后报警级别不变");
  checkEq(fanShouldRun(), 1, "静音后安全风扇仍运行");
  checkEq(g_fan, 1, "风扇引脚实际为高");
  sendLine("OK"); check(alarmSilenced, "OK 也可静音");

  section("命令解析 TIME");
  unsigned long sec = 0;
  check(parseTimeArgument("08:00", sec) && sec == 8 * 3600, "TIME 08:00 -> 28800 s");
  check(parseTimeArgument("23:59:59", sec) && sec == 23 * 3600 + 59 * 60 + 59, "TIME 23:59:59 合法");
  check(parseTimeArgument("0:00", sec), "TIME 0:00 合法");
  check(!parseTimeArgument("24:00", sec), "TIME 24:00 拒绝");
  check(!parseTimeArgument("12:60", sec), "TIME 12:60 拒绝");
  check(!parseTimeArgument("8", sec), "TIME 8 拒绝（缺分钟）");
  check(!parseTimeArgument("08:00:00:00", sec), "TIME 多余字段拒绝");
  check(!parseTimeArgument("ab:cd", sec), "TIME 非数字拒绝");
  check(!parseTimeArgument("", sec), "TIME 空参数拒绝");
  check(!parseTimeArgument("08-00", sec), "TIME 错误分隔符拒绝");

  section("命令解析 SET 阈值");
  std::string reply = sendLine("SET DAY 26 31");
  check(reply.find("OK SET DAY") != std::string::npos, "SET DAY 26 31 接受");
  checkEq((long)morningTemperatureThreshold.warning, 26, "DAY warning 已更新");
  checkEq((long)morningTemperatureThreshold.critical, 31, "DAY critical 已更新");
  reply = sendLine("SET PM 30 25");
  check(reply.find("ERR") != std::string::npos, "SET 预警>严重 拒绝");
  checkEq((long)afternoonTemperatureThreshold.warning, 23, "被拒绝后 PM 阈值不变");
  reply = sendLine("SET NIGHT -5 5");
  check(reply.find("OK SET NIGHT") != std::string::npos, "SET 负温度接受");
  checkEq((long)nightTemperatureThreshold.warning, -5, "NIGHT warning -5");
  reply = sendLine("SET HUM 75 90");
  check(reply.find("OK SET HUM") != std::string::npos, "SET HUM 75 90 接受");
  reply = sendLine("SET HUM 80 120");
  check(reply.find("ERR") != std::string::npos, "SET HUM 超量程拒绝");
  reply = sendLine("SET XXX 10 20");
  check(reply.find("ERR") != std::string::npos, "SET 未知目标拒绝");
  reply = sendLine("SET DAY 25");
  check(reply.find("ERR") != std::string::npos, "SET 缺参数拒绝");
  reply = sendLine("SET DAY 25 30 40");
  check(reply.find("ERR") != std::string::npos, "SET 多余参数拒绝");
  reply = sendLine("SET DAY 25.5 30.5");
  check(reply.find("OK SET DAY") != std::string::npos, "SET 小数接受");

  section("命令解析 长度上限与未知命令");
  reply = sendLine("STATUS");
  check(reply.find("ENVIRONMENT MONITOR") != std::string::npos, "STATUS 返回报告");
  check(reply.find("Period:") != std::string::npos, "STATUS 含时段");
  check(reply.find("Threshold:") != std::string::npos, "STATUS 含阈值");
  reply = sendLine("HELP");
  check(reply.find("TIME HH:MM") != std::string::npos, "HELP 含 TIME 用法");
  reply = sendLine("BLAH BLAH");
  check(reply.find("ERR unknown command") != std::string::npos, "未知命令报错");
  reply = sendLine(std::string(60, 'A'));
  check(reply.find("ERR command too long") != std::string::npos, "超长命令被拒绝");
  reply = sendLine("status");
  check(reply.find("ENVIRONMENT MONITOR") != std::string::npos, "小写命令可识别");
  reply = sendLine("  ");
  check(reply.find("ERR") != std::string::npos, "空白命令报错");

  section("OLED 模块页显示时段/时间/阈值");
  clockSet = false; applyTimePeriod(PERIOD_UNSET);
  setClockAt(9, 30);
  sendLine("SET DAY 25 30");
  feedReading(26.0, 65.0);
  displayPage = 1; refreshDisplay();
  check(g_displayOut.find("Prd: DAY") != std::string::npos, "显示时段");
  check(g_displayOut.find("Time: 09:30") != std::string::npos, "显示时间");
  check(g_displayOut.find("25/30") != std::string::npos, "显示当前温度阈值");

  std::cout << "\n通过 " << g_pass << " 项，失败 " << g_fail << " 项\n";
  return g_fail == 0 ? 0 : 1;
}
