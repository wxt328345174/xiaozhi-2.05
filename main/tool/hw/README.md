# 红外传感器驱动 (IR Sensor Driver)

本驱动用于管理 **ITR8307** 红外反射式传感器阵列，典型应用是机器人**防跌落检测**。

## 硬件配置

### 传感器布局

```
        前进方向 →
    ┌─────────────────┐
    │   FL   FM   FR  │  ← 前向传感器 (检测前方地面)
    │                 │
    │   EL   EM   ER  │  ← 边缘传感器 (检测底盘边缘)
    └─────────────────┘
```

### 默认接线 (bread-compact-wifi-lcd)

| 传感器 | 位置     | GPIO   | 引脚标签 |
| ------ | -------- | ------ | -------- |
| IR_FR  | 前向右侧 | GPIO38 | G38      |
| IR_FM  | 前向中间 | GPIO39 | G39      |
| IR_FL  | 前向左侧 | GPIO40 | G40      |
| IR_EL  | 边缘左侧 | GPIO8  | G8       |
| IR_EM  | 边缘中间 | GPIO17 | G17      |
| IR_ER  | 边缘右侧 | GPIO18 | G18      |

### ITR8307 传感器逻辑

- **高电平 (1)**: 红外线反射回来 → 检测到地面 → 安全
- **低电平 (0)**: 红外线未反射 → 悬空/边缘 → 危险！

## 移植指南

### 1. 文件集成

将以下文件复制到项目 `main/tool/hw/` 目录：

```
main/tool/hw/
├── ir_sensor.h      # 红外传感器管理类头文件
├── ir_sensor.cc     # 红外传感器管理类实现
└── README.md        # 本文档
```

### 2. CMakeLists.txt 配置

修改 `main/CMakeLists.txt`：

```cmake
# 添加源文件
set(SOURCES ...
    "tool/hw/ir_sensor.cc"
)

# 添加头文件路径
set(INCLUDE_DIRS ... "tool/hw")
```

### 3. 引脚配置

在板级配置文件 `main/boards/<board_name>/config.h` 中定义引脚：

```c
// IR Sensor (ITR8307 x6) Pin Definitions
// 前向传感器 (Front)
#define IR_SENSOR_FRONT_RIGHT_PIN   GPIO_NUM_38
#define IR_SENSOR_FRONT_MIDDLE_PIN  GPIO_NUM_39
#define IR_SENSOR_FRONT_LEFT_PIN    GPIO_NUM_40
// 边缘传感器 (Edge)
#define IR_SENSOR_EDGE_LEFT_PIN     GPIO_NUM_8
#define IR_SENSOR_EDGE_MIDDLE_PIN   GPIO_NUM_17
#define IR_SENSOR_EDGE_RIGHT_PIN    GPIO_NUM_18
```

### 4. 初始化使用

在板级实现文件中初始化：

```cpp
#include "ir_sensor.h"

void InitializeSensors() {
    // 引脚顺序: [FR, FM, FL, EL, EM, ER]
    static const gpio_num_t ir_pins[6] = {
        IR_SENSOR_FRONT_RIGHT_PIN,
        IR_SENSOR_FRONT_MIDDLE_PIN,
        IR_SENSOR_FRONT_LEFT_PIN,
        IR_SENSOR_EDGE_LEFT_PIN,
        IR_SENSOR_EDGE_MIDDLE_PIN,
        IR_SENSOR_EDGE_RIGHT_PIN
    };
    static IrSensorManager ir_sensors(ir_pins);

    // 启动状态监控任务（可选）
    xTaskCreate([](void* arg) {
        IrSensorManager* sensors = (IrSensorManager*)arg;
        while (true) {
            sensors->PrintStatus();
            vTaskDelay(pdMS_TO_TICKS(500));  // 每 500ms 打印一次
        }
    }, "ir_monitor", 2048, &ir_sensors, 1, NULL);
}
```

## API 参考

### 构造函数

```cpp
IrSensorManager(const gpio_num_t pins[6]);
```

参数 `pins` 顺序：`[FR, FM, FL, EL, EM, ER]`

### 读取方法

| 方法                   | 说明                 | 返回值                           |
| ---------------------- | -------------------- | -------------------------------- |
| `ReadSensor(position)` | 读取单个传感器       | `true`=地面, `false`=悬空        |
| `ReadAllSensors()`     | 读取所有传感器       | 6 位状态值 (bit0=FR ... bit5=ER) |
| `IsEdgeDetected()`     | 检测是否存在边缘危险 | `true`=有危险                    |
| `PrintStatus()`        | 打印状态到日志       | -                                |

### 传感器位置枚举

```cpp
enum SensorPosition {
    IR_FRONT_RIGHT = 0,   // 前向右侧
    IR_FRONT_MIDDLE = 1,  // 前向中间
    IR_FRONT_LEFT = 2,    // 前向左侧
    IR_EDGE_LEFT = 3,     // 边缘左侧
    IR_EDGE_MIDDLE = 4,   // 边缘中间
    IR_EDGE_RIGHT = 5,    // 边缘右侧
};
```

## 日志输出示例

```
I (15000) IrSensor: IR Sensor Status: [███|███] (0x3F)   // 全部检测到地面
I (15500) IrSensor: IR Sensor Status: [██░|███] (0x3B)   // FR 悬空
I (16000) IrSensor: IR Sensor Status: [░░░|███] (0x38)   // 前方全部悬空
```

日志格式：`[FL FM FR | EL EM ER]`

- `█` = 检测到地面 (安全)
- `░` = 悬空 (危险)

## 未来扩展

本驱动设计为可与 Motion 工具结合，实现自动防跌落：

```cpp
// 示例：与运动控制结合（未来实现）
if (ir_sensors.IsEdgeDetected()) {
    motion_tool.EmergencyStop();
    motion_tool.Move("backward", 50, 500);  // 后退
}
```
