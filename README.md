# Arduino 智能温湿度监测与风扇控制系统

> **课程设计项目**：智能传感网络与检测系统综合设计

这是一个面向课程设计的简单智能传感监测系统，基于 Arduino Uno、DHT22、SSD1306 OLED 和蓝牙串口，实现环境温湿度采集、显示、报警与风扇控制。

项目当前以“能运行、易展示、便于后续整理报告”为目标，后续可在此基础上补充系统框图、流程图、实验数据和 Word 汇报材料。

## 功能

- OLED 显示温度和湿度
- 温度预警与严重报警
- 蜂鸣器和板载 LED 状态提示
- 风扇自动、强制开启和强制关闭模式
- 按键切换 OLED 页面
- 通过蓝牙串口查询状态和控制风扇
- 提供 VOFA+ FireWater 串口输出测试程序

## 硬件连接

| 模块 | Arduino Uno |
| --- | --- |
| DHT22 DATA | D2 |
| 蜂鸣器 | D3 |
| 页面按键 | D4（使用内部上拉） |
| 风扇控制 | D9 |
| OLED SDA | A4 |
| OLED SCL | A5 |
| OLED 地址 | `0x3C` |

蓝牙模块使用 Arduino Uno 的硬件串口，波特率为 `9600`。风扇驱动应使用合适的晶体管或继电器模块，不要直接由 Arduino 引脚带动大功率负载。

## 软件依赖

在 Arduino IDE 的库管理器中安装：

- Adafruit GFX Library
- Adafruit SSD1306
- DHT sensor library

## 使用方式

1. 在 Arduino IDE 中打开 `UNO_DHT22_OLED/UNO_DHT22_OLED.ino`。
2. 安装所需库并选择 Arduino Uno 开发板。
3. 按照上表完成接线。
4. 编译并上传程序。
5. 通过按键切换 OLED 页面，或通过蓝牙发送以下命令：

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

`ALARM OFF` 仅停止蜂鸣器声音，OLED、LED 和风扇安全控制仍然有效；发送 `ALARM ON` 可重新打开声音。发送 `OK` 也可以静音蜂鸣器。Arduino 复位后报警声音默认打开；温度或湿度达到报警阈值时蜂鸣器才会响。

温度达到 `28 °C` 时进入预警，达到 `30 °C` 时进入严重报警；湿度达到 `70%RH` 时进入预警，达到 `80%RH` 时进入严重报警。温度回差为 `1 °C`，湿度回差为 `5%RH`。OLED 和 `STATUS` 会显示报警来源：`TEMP`、`HUM` 或 `TEMP+HUM`。

## 项目结构

```text
UNO_DHT22_OLED/
├── UNO_DHT22_OLED.ino       # 主程序：采集、显示、报警、风扇和蓝牙控制
├── Bluetooth_Test/          # 蓝牙与传感器显示测试程序
│   ├── Bluetooth_Test.ino
│   └── UNO_DHT22_Bluetooth_OLED.ino
├── Bluetooth_Test_old.txt   # 早期测试记录
├── README.md                # 项目说明与复现步骤
├── docs/                    # 课程设计报告图表、实验记录和文档素材
│   ├── system-block-diagram.svg # 系统总体框图
│   ├── system-flowchart.svg     # 系统运行流程图
│   └── README.md                # 报告资料组织说明
```

建议后续 Word 汇报按以下章节整理：

1. 设计任务与需求分析
2. 系统总体方案与系统框图
3. 硬件电路设计与元器件说明
4. 软件流程与程序模块设计
5. 蓝牙通信协议与人机交互
6. 系统调试、实验数据与结果分析
7. 总结与改进方向


- `Bluetooth_Test.ino`：基础蓝牙串口收发测试
- `UNO_DHT22_Bluetooth_OLED.ino`：DHT22 温湿度 OLED 显示及 VOFA+ FireWater 输出测试；不作为当前主程序依据

## 课程设计材料

`docs/submission/` 中保存已脱敏的答辩 PPT、设计报告和实测数据表。视频仅保留在本地答辩拷贝包，不随仓库上传。
