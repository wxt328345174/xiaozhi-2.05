# TV 工具（看电视场景）

## 1. 功能简介
- 进入“看电视”场景后，按 **60s** 周期触发一次“拍照 + 中文解说”。
- 触发方式为设备侧调用 `TextInvokeTool::Submit()`，由其引导云端调用 `self.camera.take_photo` 并输出中文解说。

## 2. MCP 方法
### 2.1 `tv.start`
- 入参：无
- 返回 JSON 字段说明：
  - `ok`：是否成功启动
  - `running`：当前是否运行中
  - `message`：状态信息（如 `started` / `already_running` / `timer_create_failed` / `timer_start_failed`）
  - `interval_sec`：周期秒数（固定 60）
  - `last_trigger_ms`：最近一次触发时间（ms 时间戳）

### 2.2 `tv.stop`
- 入参：无
- 返回 JSON 字段说明：
  - `ok`：是否成功停止
  - `running`：当前是否运行中
  - `message`：状态信息（如 `stopped` / `already_stopped`）
  - `interval_sec`：周期秒数（固定 60）
  - `last_trigger_ms`：最近一次触发时间（ms 时间戳）

### 2.3 `tv.status`
- 入参：无
- 返回 JSON 字段说明：
  - `ok`：查询是否成功
  - `running`：当前是否运行中
  - `interval_sec`：周期秒数（固定 60）
  - `last_trigger_ms`：最近一次触发时间（ms 时间戳）

## 3. 工作原理
- `tv` 定时触发后调用 `TextInvokeTool::Submit(固定文本)`。
- `TextInvokeTool` 是客户端侧 MCP 扩展，复用 `Application::SendWakeWordDetectedText()` 走既有链路（见 [main/tool/chat/mcp_text_invoke_tool.cpp](../../tool/chat/mcp_text_invoke_tool.cpp) 与 [main/application.cc](../../application.cc)）。
- 固定文本内容会提示服务端调用 `self.camera.take_photo` 并使用中文解说（工具注册见 [main/mcp_server.cc](../../mcp_server.cc)）。

固定文本（必须逐字一致）：

> [电视画面播报助手] 立刻调用 MCP 工具self.camera.take_photo，并在 question 中写：请用中文解说当前电视画面；若不是电视画面请提示用户把镜头对准屏幕。

## 4. 如何触发
设备侧入口为服务端下发 `type == "mcp"` 的 JSON-RPC `tools/call`（解析逻辑见 [main/mcp_server.cc](../../mcp_server.cc)）。

示例（`tv.start`）：
```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "tools/call",
  "params": {
    "name": "tv.start",
    "arguments": {}
  }
}
```

示例（`tv.stop`）：
```json
{
  "jsonrpc": "2.0",
  "id": 2,
  "method": "tools/call",
  "params": {
    "name": "tv.stop",
    "arguments": {}
  }
}
```

示例（`tv.status`）：
```json
{
  "jsonrpc": "2.0",
  "id": 3,
  "method": "tools/call",
  "params": {
    "name": "tv.status",
    "arguments": {}
  }
}
```

## 5. 调试与排错
- 关键日志：
  - `TvTool` 相关日志在 [main/tool/tv/tv_tool.cpp](tv_tool.cpp) 中以 `TAG = "TvTool"` 输出。
- 常见问题：
  - 重复 `tv.start`：返回 `already_running`。
  - 重复 `tv.stop`：返回 `already_stopped`。
  - 设备状态对 `text_invoke` 的影响：在 `listening` 状态会走 `SendWakeWordDetected` 发送文本；其他状态走 `WakeWordInvoke`（见 [main/application.cc](../../application.cc)）。

## 6. 兼容性
- ESP-IDF v5.5.2。
- 适配 ESP32-S3-CAM（N16R8，16MB Flash + 8MB PSRAM）。
- 60s 周期会产生持续网络与摄像头资源消耗，需关注内存与网络稳定性。\
