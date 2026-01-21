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
- 使用 `esp_timer` 周期定时器，回调仅做轻量操作：读取当前参数并调用 `Application::Schedule(...)`，把“发送文本”任务排队到主循环线程执行。
- 发送文本通过 `Application::SendWakeWordDetectedText(utterance)` 走与 `self.text_invoke` 相同的唤醒词通道，避免在 timer 线程做网络/音频操作，也避免长任务多次回包。

## 4. 重要限制
- `interval_ms` 最小 3000 ms；如果传入更小值会被强制提升到 3000 ms。
- `utterance` 会按 `max_chars` 以 UTF-8 字符数截断，避免截断半个多字节。
- 运行状态依赖 `running_` 原子变量；未额外 gate 设备状态（如 speaking），若云端返回的 TTS 与本地音频冲突，需要上层策略协调。

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
