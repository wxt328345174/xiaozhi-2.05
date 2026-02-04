# bread-compact-wifi-lcd 显示单张图片说明

## 1) 背景与目标
- 板级：bread-compact-wifi-lcd，LCD 为 GC9D01 160x160 SPI（本工程配置）
- 显示链路：沿用现有 LVGL 显示链路（esp_lvgl_port 驱动）
- 目标：启动后仅显示由 `眼睛.jpg` 转换得到的 LVGL 图像资源，放弃原默认 UI/原图形

## 2) 本次新增/修改内容（列路径）
- 新增资源文件：
  - `main/boards/bread-compact-wifi-lcd/assets/eyes_img.c`
  - `main/boards/bread-compact-wifi-lcd/assets/eyes_img.h`
- 新增转换脚本：
  - `main/boards/bread-compact-wifi-lcd/tools/convert_eyes_to_lvgl.py`
- 修改 UI 文件：
  - `main/display/lcd_display.cc`
- 资源符号名：
  - `eyes_img`

## 3) 关键实现说明
- 采用离线转换为 `lv_image_dsc_t`：最稳定、无运行时 JPEG 解码依赖，避免额外内存占用与解码失败风险。
- 颜色格式：RGB565；刷屏链路已启用 `swap_bytes=1`，因此资源数据 **不要** 再做 byte swap。
- 在 `main/display/lcd_display.cc` 的 `LcdDisplay::SetupUI()` 中：
  - `#if CONFIG_BOARD_TYPE_BREAD_COMPACT_WIFI_LCD` 分支直接调用 `SetupEyesOnlyUI()` 并 `return`。
  - `SetupEyesOnlyUI()` 仅创建 `lv_img` 并居中显示 `eyes_img`。
- 对 `SetEmotion` / `SetChatMessage` / `SetPreviewImage` / `SetTheme` / `SetHideSubtitle` 做板级短路，
  以免访问已移除的 UI 指针（本板只显示静态 eyes 图）。

## 4) 移植方法（给其他板复用）
### 前置条件
- ESP-IDF：v5.5.2（项目当前版本）
- LVGL：v9.3.0（项目当前版本）
- 屏幕分辨率/色深：确保目标屏幕分辨率与 RGB565 色深明确

### 准备图片（变成 160x160）
- 将原图裁剪/缩放为 160x160（本板做法：中心裁剪成正方形后缩放到 160x160）。

### 使用脚本转换为 RGB565 lv_image_dsc_t
```bash
python main/boards/bread-compact-wifi-lcd/tools/convert_eyes_to_lvgl.py --strategy crop
```

### 将生成的 .c/.h 加入构建（示例片段）
在 `main/CMakeLists.txt` 里对板级分支追加：
```cmake
list(APPEND SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/boards/bread-compact-wifi-lcd/assets/eyes_img.c")
list(APPEND INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/boards/bread-compact-wifi-lcd/assets")
```

### LVGL 显示最小代码片段
```c
lv_obj_t* img = lv_image_create(lv_screen_active());
lv_image_set_src(img, &eyes_img);
lv_obj_center(img);
```

## 5) 常见问题排查
- 颜色不对：
  - RGB/BGR 配置是否一致（面板与资源）
  - `swap_bytes` 是否已在刷屏链路启用（本工程为 `swap_bytes=1`）
  - RGB565 字节序是否为小端（资源生成脚本输出为小端）
- 显示反色/偏色：
  - 确认 `LV_COLOR_FORMAT_RGB565` 与转换格式一致
- 图像方向问题：
  - 面板旋转/MADCTL 设置
  - 若工程存在 `lv_display_set_rotation`，确认其方向与资源一致
- 刷新慢/撕裂：
  - `buffer_size`、SPI 时钟、DMA、`double_buffer` 配置是否合理
