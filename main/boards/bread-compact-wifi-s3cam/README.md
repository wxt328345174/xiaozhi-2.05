硬件基于基于ESP32S3CAM开发板，代码基于bread-compact-wifi-lcd修改
使用的摄像头是OV2640
注意因为摄像头占用IO较多，所以占用了ESP32S3的USB 19 20两个引脚
连线方式参考config.h文件中对引脚的定义

# 编译配置命令

**配置编译目标为 ESP32S3：**

```bash
idf.py set-target esp32s3
```

**打开 menuconfig：**

```bash
idf.py menuconfig
```

**选择板子：**

```bash
Xiaozhi Assistant -> Board Type ->面包板新版接线（WiFi）+ LCD + Camera
```

**配置摄像头传感器：**

> **注意：** 确认摄像头传感器型号，确定型号在 esp_cam_sensor 支持的范围内。当前板子用的是 OV2640，是符合支持范围。

在 menuconfig 中按以下步骤启用对应型号的支持：

1. **导航到传感器配置：**

   ```
   (Top) → Component config → Espressif Camera Sensors Configurations → Camera Sensor Configuration → Select and Set Camera Sensor
   ```

2. **选择传感器型号：**
   - 选中所需的传感器型号（OV2640）

3. **配置传感器参数：**
   - 按 → 进入传感器详细设置
   - 启用 **Auto detect**
   - 推荐将 **default output format** 调整为 **YUV422** 及合适的分辨率大小
   - （目前支持 YUV422、RGB565，YUV422 更节省内存空间）

**MCP 工具支持：**

本板级支持 MCP (Model Context Protocol) 工具，包括：

- **LampController**: 用于控制灯光（默认 GPIO 14）。
- **MotionTool**: 用于控制双路直流电机（如 DRV8833 驱动）。

**关于 Motion Tool 引脚配置的特别说明：**
由于摄像头模组占用了大量 GPIO（包括原 LCD 板用于电机的 GPIO 10-13），Motion Tool 的默认电机控制引脚在 `config.h` 中被设置为 `GPIO_NUM_NC`（未连接），以防止硬件冲突。

如果您需要使用电机控制功能，请执行以下操作：

1. 打开 `main/boards/bread-compact-wifi-s3cam/config.h`。
2. 找到 `Motion Tool Pin Definitions` 部分。
3. 将 `MOTION_MOTOR_LEFT_IN1_PIN` 等宏定义修改为您实际连接的、未被摄像头占用的空闲 GPIO 引脚。
4. 重新编译并烧录。

**编译烧入：**

```bash
idf.py build flash
```
