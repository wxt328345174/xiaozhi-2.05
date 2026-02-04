# 硬件控制驱动 (HW Drivers)

本目录包含机器人硬件控制驱动，包含红外传感器和防掉落控制系统。

## 文件结构

| 文件                        | 说明                          |
| --------------------------- | ----------------------------- |
| `ir_sensor.h/cc`            | 红外传感器管理类 (ITR8307 x6) |
| `anti_drop_controller.h/cc` | 防掉落控制器                  |
| `README.md`                 | 本文档                        |

---

## 1. 红外传感器 (IrSensorManager)

### 传感器布局

```
        前进方向 →
    ┌─────────────────┐
    │   FL   FM   FR  │  ← 前向传感器 (检测前方地面)
    │                 │
    │   EL   EM   ER  │  ← 边缘传感器 (检测底盘边缘)
    └─────────────────┘
```

### 引脚配置 (bread-compact-wifi-lcd)

| 传感器 | GPIO   | 说明     |
| ------ | ------ | -------- |
| IR_FR  | GPIO38 | 前向右侧 |
| IR_FM  | GPIO39 | 前向中间 |
| IR_FL  | GPIO40 | 前向左侧 |
| IR_EL  | GPIO8  | 边缘左侧 |
| IR_EM  | GPIO17 | 边缘中间 |
| IR_ER  | GPIO18 | 边缘右侧 |

### 传感器逻辑 (ITR8307)

- **高电平 (O)**: 检测到地面 → 安全
- **低电平 (X)**: 悬空/边缘 → 危险

---

## 2. 防掉落控制器 (AntiDropController)

### 控制逻辑

| 触发条件                     | 动作     |
| ---------------------------- | -------- |
| 前向传感器任意一个检测到边缘 | **后退** |
| 边缘传感器任意一个检测到边缘 | **前进** |

### 默认参数

| 参数           | 默认值 | 说明                   |
| -------------- | ------ | ---------------------- |
| speed          | 50%    | 避让速度（独立于 MCP） |
| duration       | 1000ms | 避让动作时间           |
| check_interval | 50ms   | 传感器检测间隔         |

### 特性

- **本地控制**: 直接操作 MotorDriver，绕过 MCP，保证时效性
- **参数独立**: 避让速度/时间不受 MCP 运动控制的数据记忆影响
- **高优先级**: 任务优先级为 5，高于普通任务

---

## 移植指南

### 1. 添加文件

复制 `main/tool/hw/` 目录到项目。

### 2. CMakeLists.txt

```cmake
set(SOURCES ...
    "tool/hw/ir_sensor.cc"
    "tool/hw/anti_drop_controller.cc"
)
set(INCLUDE_DIRS ... "tool/hw")
```

### 3. config.h 引脚定义

```c
// 红外传感器引脚
#define IR_SENSOR_FRONT_RIGHT_PIN   GPIO_NUM_38
#define IR_SENSOR_FRONT_MIDDLE_PIN  GPIO_NUM_39
#define IR_SENSOR_FRONT_LEFT_PIN    GPIO_NUM_40
#define IR_SENSOR_EDGE_LEFT_PIN     GPIO_NUM_8
#define IR_SENSOR_EDGE_MIDDLE_PIN   GPIO_NUM_17
#define IR_SENSOR_EDGE_RIGHT_PIN    GPIO_NUM_18
```

### 4. 初始化代码

```cpp
#include "ir_sensor.h"
#include "anti_drop_controller.h"

void InitializeTools() {
    // 电机初始化（需要先初始化）
    static MotionTool motion(...);

    // 红外传感器初始化
    static const gpio_num_t ir_pins[6] = {
        IR_SENSOR_FRONT_RIGHT_PIN, IR_SENSOR_FRONT_MIDDLE_PIN, IR_SENSOR_FRONT_LEFT_PIN,
        IR_SENSOR_EDGE_LEFT_PIN, IR_SENSOR_EDGE_MIDDLE_PIN, IR_SENSOR_EDGE_RIGHT_PIN
    };
    static IrSensorManager ir_sensors(ir_pins);

    // 防掉落控制器
    static AntiDropController anti_drop(&ir_sensors, motion.GetMotorDriver());
    anti_drop.Start();
}
```

---

## API 参考

### IrSensorManager

```cpp
bool ReadSensor(SensorPosition pos);   // 读取单个传感器
uint8_t ReadAllSensors();              // 读取所有传感器 (6位)
bool IsEdgeDetected();                 // 是否检测到边缘
void PrintStatus();                    // 打印状态日志
```

### AntiDropController

```cpp
void Start();                          // 启动监控
void Stop();                           // 停止监控
void SetEnabled(bool enabled);         // 启用/禁用
void SetSpeed(int speed);              // 设置避让速度 (0-100)
void SetDuration(int ms);              // 设置避让时间
bool IsAvoidingDrop();                 // 是否正在执行避让
```

---

## 日志输出

```
I (xxx) AntiDrop: Anti-drop monitoring started
I (xxx) AntiDrop: FRONT EDGE detected! Moving backward...
I (xxx) AntiDrop: Executing avoid action: dir=-1, speed=50, duration=1000ms
I (xxx) AntiDrop: Avoid action completed
```
