# Motion Tool (运动控制工具)

Motion Tool 是一个基于 MCP 协议的运动控制工具，专为 ESP32-S3 设计。当前版本使用 **2 颗 DRV8837DSGR 芯片** 驱动双电机。

## 硬件方案

### DRV8837 vs DRV8833 对比

| 特性     | DRV8833               | DRV8837                      |
| -------- | --------------------- | ---------------------------- |
| 芯片数量 | 1 颗控制 2 个电机     | 1 颗控制 1 个电机（需 2 颗） |
| 控制引脚 | 4 个 (每电机 IN1/IN2) | 6 个 (每芯片 nSLEEP/IN1/IN2) |
| 休眠控制 | 共用或无              | 每颗芯片独立 nSLEEP          |

### 当前接线方案 (DRV8837 x2)

| 芯片             | 功能   | GPIO   | ESP32-S3 引脚 |
| ---------------- | ------ | ------ | ------------- |
| 芯片1 (电机A/左) | nSLEEP | GPIO1  | Pin 39        |
| 芯片1 (电机A/左) | IN1    | GPIO42 | Pin 35        |
| 芯片1 (电机A/左) | IN2    | GPIO41 | Pin 34        |
| 芯片2 (电机B/右) | nSLEEP | GPIO3  | Pin 8         |
| 芯片2 (电机B/右) | IN1    | GPIO48 | Pin 25        |
| 芯片2 (电机B/右) | IN2    | GPIO47 | Pin 24        |

> [!NOTE]
> nSLEEP 为**低电平休眠，高电平使能**。驱动初始化时会自动将两颗芯片唤醒。

## 功能特性

- **基本运动**: 前进 (forward)、后退 (backward)、左转 (left)、右转 (right)、停止 (stop)
- **速度控制**: 支持 0-100% 速度调节，具有速度记忆功能
- **时间控制**: 支持指定运动时长（毫秒），转动/移动时间分别记忆
- **休眠控制**: 提供 `Sleep()` / `Wakeup()` 方法，可手动进入低功耗模式

## 移植指南

### 1. 文件集成

将 `Motion` 文件夹复制到项目的 `main/tool/` 目录下。

修改 `main/CMakeLists.txt`：

```cmake
set(SOURCES ...
    "tool/Motion/motor_driver.cc"
    "tool/Motion/motion_tool.cc"
)

set(INCLUDE_DIRS ... "tool/Motion")
```

### 2. 引脚配置

在板级配置文件 `main/boards/<board_name>/config.h` 中定义引脚：

```c
// Motion Tool Pin Definitions
// DRV8837 Motor Driver x2 (每颗芯片控制一个电机)
// 电机A (左): nSLEEP=GPIO1, IN1=GPIO42, IN2=GPIO41
// 电机B (右): nSLEEP=GPIO3, IN1=GPIO48, IN2=GPIO47
#define MOTION_MOTOR_LEFT_NSLEEP_PIN  GPIO_NUM_1
#define MOTION_MOTOR_LEFT_IN1_PIN     GPIO_NUM_42
#define MOTION_MOTOR_LEFT_IN2_PIN     GPIO_NUM_41
#define MOTION_MOTOR_RIGHT_NSLEEP_PIN GPIO_NUM_3
#define MOTION_MOTOR_RIGHT_IN1_PIN    GPIO_NUM_48
#define MOTION_MOTOR_RIGHT_IN2_PIN    GPIO_NUM_47
```

> [!WARNING]
> **ESP32-S3 N16R8 注意事项**:
> N16R8 模组使用 Octal PSRAM 和 Quad/Octal Flash，**GPIO 10-14 和 33-37 被内部占用**。
> **推荐使用 Safe GPIO**: 如 GPIO 1, 2, 3, 8, 9, 38-48（需避开 LCD/Camera 占用）。

### 3. 工具注册

在板级实现文件（如 `compact_wifi_board_lcd.cc`）的 `InitializeTools` 方法中注册：

```cpp
#include "motion_tool.h"

void InitializeTools() {
    // 参数顺序: 左nSLEEP, 左IN1, 左IN2, 右nSLEEP, 右IN1, 右IN2
    static MotionTool motion(MOTION_MOTOR_LEFT_NSLEEP_PIN,
                             MOTION_MOTOR_LEFT_IN1_PIN, MOTION_MOTOR_LEFT_IN2_PIN,
                             MOTION_MOTOR_RIGHT_NSLEEP_PIN,
                             MOTION_MOTOR_RIGHT_IN1_PIN, MOTION_MOTOR_RIGHT_IN2_PIN);
}
```

## 代码结构

```
main/tool/Motion/
├── motor_driver.h    # 电机驱动类头文件（nSLEEP + PWM 控制）
├── motor_driver.cc   # 电机驱动实现（LEDC PWM + GPIO nSLEEP）
├── motion_tool.h     # MCP 工具类头文件
├── motion_tool.cc    # MCP 工具实现（方向/速度/时长解析）
└── README.md         # 本文档
```

### 关键修改位置

| 文件                        | 修改内容                                                                        | 作用                      |
| --------------------------- | ------------------------------------------------------------------------------- | ------------------------- |
| `motor_driver.h`            | 构造函数增加 `left_nsleep`, `right_nsleep` 参数；添加 `Sleep()`/`Wakeup()` 方法 | 支持 DRV8837 独立休眠控制 |
| `motor_driver.cc`           | 初始化时配置 nSLEEP 为 GPIO 输出并设为高电平；实现 `Sleep()`/`Wakeup()`         | nSLEEP 引脚控制逻辑       |
| `motion_tool.h`             | 构造函数参数从 4 个改为 6 个                                                    | 传递 nSLEEP 引脚          |
| `motion_tool.cc`            | 更新构造函数调用                                                                | 适配新 MotorDriver 签名   |
| `config.h`                  | 新增 `MOTION_MOTOR_LEFT_NSLEEP_PIN` 和 `MOTION_MOTOR_RIGHT_NSLEEP_PIN` 宏       | 配置 nSLEEP 引脚          |
| `compact_wifi_board_lcd.cc` | MotionTool 初始化改为 6 参数调用                                                | 传递完整引脚配置          |

## 可调参数

### PWM 频率 (`motor_driver.h`)

```cpp
const int kLedcFreq = 1000; // 1kHz，低速启动力矩好
```

### LEDC 定时器与通道 (`motor_driver.cc`)

```cpp
// 使用 LEDC_TIMER_1 和 Channel 2-5，避免与背光冲突
.timer_num = LEDC_TIMER_1
```

### 默认速度 (`motion_tool.h`)

```cpp
int current_speed_ = 80;         // 默认速度 80%
int current_turn_duration_ = 1000; // 默认转动 1000ms
```

## 使用示例 (MCP 协议)

AI Agent 调用 `self.motion.move` 工具：

- **前进**: `{"direction": "forward", "speed": 80, "duration": 2000}`
- **左转**: `{"direction": "left"}` (使用记忆时间)
- **停止**: `{"direction": "stop"}`

## 从 DRV8833 迁移到 DRV8837

如果你的项目原先使用 DRV8833，迁移步骤如下：

1. **config.h**: 新增 2 个 `NSLEEP_PIN` 宏定义
2. **board.cc**: MotionTool 初始化改为 6 参数
3. **无需修改**: `motor_driver.*` 和 `motion_tool.*` 已兼容（nSLEEP 设为 `GPIO_NUM_NC` 时自动跳过）

若仍使用 DRV8833（无独立 nSLEEP），可将 `NSLEEP_PIN` 设为 `GPIO_NUM_NC`：

```c
#define MOTION_MOTOR_LEFT_NSLEEP_PIN  GPIO_NUM_NC  // 无 nSLEEP
#define MOTION_MOTOR_RIGHT_NSLEEP_PIN GPIO_NUM_NC  // 无 nSLEEP
```
