# TV 模式 MCP 工具说明

## 1. 功能简介
“看电视”模式会按固定间隔向云端发送一条短文本，复用唤醒词通道触发远端的拍照解说（云端负责拉起摄像头工具并返回语音播报）。设备端只做轻量定时触发，不做本地图像处理。

## 2. MCP 工具列表
- `self.tv.watch`  
  - 作用：进入电视模式并启动周期触发。  
  - 参数（带默认值）：  
    - `interval_ms` (integer, default 5000, 最小 3000)：拍照/解说触发间隔，毫秒。  
    - `utterance` (string, default "拍照解说")：发送给云端的提示文本。  
    - `max_chars` (integer, default 10)：按 UTF-8 字符数截断 utterance，避免超长提示。  
  - 返回值：`"started"`（成功启动/重启）或 `"not_running"`（创建定时器失败）。
- `self.tv.stop`  
  - 作用：退出电视模式，停止周期触发。  
  - 返回值：`"stopped"` 或 `"not_running"`（本就未运行）。

## 3. 运行机制
- 使用 `esp_timer` 单次定时器：发起一次解说后等待结束（speaking→idle/listening）再按 `interval_ms` 重新启动下一轮，避免固定 5s 周期叠加导致 AFE/MQTT/HTTP 拥塞。
- 定时回调仅做轻量操作，所有发送都通过 `Application::Schedule(...)` 排队到主循环线程；发送文本走 `Application::SendWakeWordDetectedText(utterance)`，与 `self.text_invoke` 通道相同。
- 设有 `in_flight_` 防重入；并有 30s failsafe 超时，超时后清理状态并按 `interval_ms` 重试。

## 4. 重要限制
- `interval_ms` 最小 3000 ms；如果传入更小值会被强制提升到 3000 ms。
- `utterance` 会按 `max_chars` 以 UTF-8 字符数截断，避免截断半个多字节。
- 发送只在设备状态为 idle/listening 时发起；若处于 speaking 会延后 1s 再试。
- 解说结束判定：设备状态从 speaking 切回 idle/listening；否则由 30s 超时兜底。stop 会停止并删除所有定时器，避免旧回调误触发。

## 5. 调试方法
- 列出工具：调用 MCP `tools/list`（JSON-RPC），`result.tools` 中可看到 `self.tv.watch` / `self.tv.stop`。示例请求：
```json
{"jsonrpc":"2.0","id":1,"method":"tools/list","params":{"withUserTools":false}}
```
- 触发 watch 示例（JSON-RPC `tools/call`）：
```json
{
  "jsonrpc":"2.0",
  "id":2,
  "method":"tools/call",
  "params":{
    "name":"self.tv.watch",
    "arguments":{
      "interval_ms":4000,
      "utterance":"拍照解说",
      "max_chars":10
    }
  }
}
```
- 停止示例：
```json
{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"self.tv.stop","arguments":{}}}
```

## 6. 目录结构与可移植性
- 代码集中在 `main/tool/tv/tv_tool.h` / `tv_tool.cc`，仅在 `main/CMakeLists.txt` 加入源文件、`main/application.cc` 调用 `Initialize`，和 `application.h` 前置声明成员，保持对其他模块零侵入，便于在不同板卡/项目中迁移。README 放在同目录便于随组件一起复制。
