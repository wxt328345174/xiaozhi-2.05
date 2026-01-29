# Motion Tool (运动控制工具)

Motion Tool 是一个基于 MCP 协议的运动控制工具，专为 ESP32-S3 和 DRV8833 电机驱动设计。它允许用户或 AI Agent 通过自然语言命令控制机器人运动。

## 功能特性

- **基本运动**: 前进 (forward)、后退 (backward)、左转 (left)、右转 (right)、停止 (stop)。
- **速度控制**: 支持 0-100% 速度调节。
  - **速度记忆**: 如果指令未指定速度，自动使用上一次的速度（默认初始为 80）。
- **时间控制**: 支持指定运动时长（毫秒）。
  - **转动时间记忆**: 左转/右转如果未指定时间，自动使用上一次的转动时间（默认初始为 1000ms）。
  - **持续运动**: 前进/后退默认时间为 0（无限），直到收到停止指令或新的运动指令。

## 移植指南

### 1. 文件集成

将 `Motion` 文件夹整体复制到你的项目的 `main/tool/` 目录下。

修改 `main/CMakeLists.txt`，添加源文件和头文件路径：

```cmake
set(SOURCES ...
    "tool/Motion/motor_driver.cc"
    "tool/Motion/motion_tool.cc"
)

set(INCLUDE_DIRS ... "tool/Motion")
```

### 2. 引脚配置 (关键)

在板级配置文件（如 `main/boards/<board_name>/config.h`）中定义电机驱动引脚。

> [!WARNING]
> **ESP32-S3 N16R8 注意事项**:
> N16R8 模组使用 Octal PSRAM 和 Quad/Octal Flash，**GPIO 10-14 和 33-37 被内部占用**。连接到这些引脚会导致电机在上电时乱转或系统无法启动。
> **推荐使用 Safe GPIO**: 如 GPIO 1, 2, 8, 9, 38-48 (避开 LCD/Camera 占用)。

```c
// DRV8833 Motor Driver Configuration
// 请根据实际连线修改，确保不冲突
#define MOTION_MOTOR_LEFT_IN1_PIN  GPIO_NUM_10
#define MOTION_MOTOR_LEFT_IN2_PIN  GPIO_NUM_11
#define MOTION_MOTOR_RIGHT_IN1_PIN GPIO_NUM_12
#define MOTION_MOTOR_RIGHT_IN2_PIN GPIO_NUM_13
```

### 3. 工具注册

在板级实现文件（如 `compact_wifi_board_lcd.cc`）的 `InitializeTools` 方法中注册：

```cpp
#include "motion_tool.h"

// ...

void InitializeTools() {
    // ... 其他工具
    static MotionTool motion(MOTION_MOTOR_LEFT_IN1_PIN, MOTION_MOTOR_LEFT_IN2_PIN,
                             MOTION_MOTOR_RIGHT_IN1_PIN, MOTION_MOTOR_RIGHT_IN2_PIN);
}
```

## 可修改参数与调试

如果遇到电机不转、抖动或冲突，可调整 `main/tool/Motion/` 下的源码参数：

### PWM 频率 (`motor_driver.h`)

```cpp
// 默认 1000Hz (1kHz)。
// 较高的频率 (如 20kHz) 会导致 DRV8833 在低占空比下力矩不足（电机发出高频啸叫但不转）。
// 如果低速启动困难，保持 1kHz 或更低。
const int kLedcFreq = 1000;
```

### LEDC 定时器与通道 (`motor_driver.cc`)

```cpp
// 默认使用 LEDC_TIMER_1 和 Channel 2-5。
// 原因：系统的背光 (Backlight) 通常占用 LEDC_TIMER_0 和 Channel 0。
// 如果发现电机受背光亮度影响，与背光冲突，请确保不要使用 Timer 0 或 Channel 0。
ledc_timer_config_t ledc_timer = {
    // ...
    .timer_num = LEDC_TIMER_1,
    // ...
};
```

### 默认速度与转动时间 (`motion_tool.h`)

```cpp
// 默认初始速度 (0-100)
int current_speed_ = 80;
// 默认初始转动时间 (ms)
int current_turn_duration_ = 1000;
```

## 使用示例 (MCP 协议)

AI Agent 调用 `self.motion.move` 工具：

- **前进**: `{"direction": "forward", "speed": 80, "duration": 2000}`
- **左转 (默认时间)**: `{"direction": "left"}` (使用记忆时间，默认 1000ms)
- **左转 (指定时间)**: `{"direction": "left", "duration": 500}` (同时更新记忆时间为 500ms)
- **停止**: `{"direction": "stop"}`
