# bread-compact-wifi-lcd - GC9D01 160x160 SPI LCD

## 新增内容
- 在 bread-compact-wifi-lcd 板级新增 GC9D01 160x160 SPI LCD 类型选项。
- 复用 GC9D01N 面板驱动实现（板级目录内）以避免新增全局组件依赖。

## menuconfig 选择路径
1. Board 选择：
   - Xiaozhi Assistant -> Board Type -> Bread Compact WiFi + LCD (面包板)
2. LCD 类型选择：
   - Xiaozhi Assistant -> LCD Type -> GC9D01 160*160

## 引脚说明（8pin SPI）
| 信号 | GPIO |
|---|---|
| MOSI | GPIO47 |
| SCLK | GPIO21 |
| CS | GPIO41 |
| DC | GPIO40 |
| RST | GPIO45 |
| BL(背光PWM) | GPIO42 |
| MISO | NC |
| SPI Host | SPI3_HOST |

## 常见问题
- 颜色不对：检查 RGB/BGR 配置与屏幕实际色序。
- 偏移/裁剪：检查 X/Y offset 与镜像/旋转设置。
- 反色异常：检查 Invert Color 设置。
- 显示方向不对：检查 swap_xy/mirror_x/mirror_y 设置。
- 黑屏/花屏：建议降低 SPI 时钟（例如 20MHz 或更低）排障。

## 验证步骤
1. menuconfig 选择 bread-compact-wifi-lcd + GC9D01 160x160。
2. 编译并烧录固件。
3. 上电后观察是否能正常显示 UI。
4. 检查状态栏、表情、消息等基础显示逻辑。

## 回归检查点
- 其他 LCD 类型（ST7789/ILI9341/GC9A01）仍可编译通过。
- 其他板型不受影响（仅 bread-compact-wifi-lcd 可见新 LCD 选项）。
- 语音/网络/按键等原有功能不受影响。
