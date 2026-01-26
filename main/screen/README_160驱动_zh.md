# GC9D01N 160x160 SPI LCD 驱动

该目录包含 GC9D01N 160x160 LCD 屏幕的通用驱动实现。

## 支持的板型

目前以下板型已适配该屏幕驱动：

1. **bread-compact-wifi-lcd** (面包板 + LCD)
2. **bread-compact-wifi-s3cam** (面包板 + S3 Camera)

## menuconfig 配置

在 `menuconfig` 中进行如下配置以启用该屏幕：

1. **Board Type**:
   - 选择 `Bread Compact WiFi + LCD` 或 `Bread Compact WiFi + S3 Camera`
2. **LCD Type**:
   - 选择 `GC9D01 160*160`

## 引脚定义

### 1. bread-compact-wifi-lcd

| 信号 | GPIO   |
| ---- | ------ |
| MOSI | GPIO47 |
| SCLK | GPIO21 |
| CS   | GPIO41 |
| DC   | GPIO40 |
| RST  | GPIO45 |
| BL   | GPIO42 |

### 2. bread-compact-wifi-s3cam

| 信号 | GPIO   |
| ---- | ------ |
| MOSI | GPIO20 |
| SCLK | GPIO19 |
| CS   | GPIO45 |
| DC   | GPIO47 |
| RST  | GPIO21 |
| BL   | GPIO38 |

## 常见问题

- **显示颜色异常**：请检查 `config.h` 中的 `DISPLAY_RGB_ORDER` 或 `DISPLAY_INVERT_COLOR` 配置。
- **屏幕方向错误**：调整 `DISPLAY_MIRROR_X`, `DISPLAY_MIRROR_Y`, `DISPLAY_SWAP_XY`。
- **花屏**：尝试降低 SPI 频率。
