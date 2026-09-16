# 温湿度大棚项目背景

## 项目定位

本项目是“智能传感网络与检测系统综合设计”课程设计。目标是在 Arduino Uno 上实现一个适合演示和写课程报告的智能温湿度监测与风扇控制系统：采集环境温湿度、OLED 本地显示、声光报警、蓝牙串口查询/控制和风扇联动。

当前目标是“能运行、易展示、便于整理报告”，不是工业级温室控制器。

## 已确定的系统方案

- 控制器：Arduino Uno。
- 传感器：DHT22；数据线接 D2。
- 显示：128×64 SSD1306 I2C OLED，地址 `0x3C`；SDA 接 A4，SCL 接 A5。
- 执行/提示：蜂鸣器 D3、板载 LED、页面按键 D4（`INPUT_PULLUP`）、风扇控制 D9。
- 通信：蓝牙模块接 Uno 硬件串口，波特率 `9600`。
- 软件库：Adafruit GFX、Adafruit SSD1306、DHT sensor library。

风扇必须通过合适的三极管/MOSFET 或继电器驱动，使用与负载匹配的独立供电；Arduino 与驱动电源需要共地。不得用 Arduino 引脚直接驱动风扇。

## 软件状态与安全策略

主草图为 `../UNO_DHT22_OLED.ino`。

- 温度 `≥28 °C` 或湿度 `≥70%RH` 为预警；温度 `≥30 °C` 或湿度 `≥80%RH` 为严重报警。
- 退出报警使用回差：温度 1 °C、湿度 5 %RH，避免阈值附近频繁跳变。
- `WARNING`：LED 闪烁、蜂鸣器间歇响、风扇自动运行。
- `CRITICAL`：LED 常亮、蜂鸣器持续响、风扇运行。
- `FAN AUTO` 跟随报警；`FAN ON` 强制运行；`FAN OFF` 在严重报警或传感器失效时不能关掉风扇。
- DHT22 读数无效时，显示错误、停止蜂鸣器、保留最后一次有效读数相关的安全风扇策略。
- `ALARM OFF` / `OK` 只静音蜂鸣器，不关闭风扇、LED 或 OLED 状态显示。

## 人机交互

OLED 共有三页：

1. 温湿度和总体报警等级。
2. 模块状态、报警来源、风扇模式与实际状态。
3. 蓝牙命令帮助。

蓝牙命令：

```text
STATUS
HELP
PAGE 0
PAGE 1
PAGE 2
FAN AUTO
FAN ON
FAN OFF
ALARM OFF
ALARM ON
OK
```

`STATUS` 会返回温湿度、报警级别与来源、LED、蜂鸣器、风扇模式和实际状态。

## 辅助文件

- `../Bluetooth_Test/Bluetooth_Test.ino`：基础蓝牙串口收发测试。
- `../Bluetooth_Test/UNO_DHT22_Bluetooth_OLED.ino`：DHT22 + OLED + 蓝牙显示和 VOFA+ FireWater 输出测试，不作为当前主程序依据。
- `system-block-diagram.svg`：系统总体框图。
- `system-flowchart.svg`：系统运行流程图。

课程汇报建议涵盖：任务/需求分析、总体方案、硬件设计、软件流程、蓝牙协议与交互、调试实验数据、总结与改进。

## 后续工作（按需）

- 添加实物照片、接线图或电路图至 `docs/hardware/`。
- 记录温湿度、报警、蓝牙命令测试数据至 `docs/experiments/`。
- 整理 Word 汇报章节草稿和图注至 `docs/report/`。
- 当前已使用 DHT22；如课程要求更高精度或更稳定读数，再评估 SHT 系列，并同步调整程序与报告。
