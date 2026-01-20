# TextInvoke MCP 工具说明（中文）

## 1. 解决的问题
`TextInvokeTool` 是一个纯客户端的 MCP 扩展，通过复用 `Application::WakeWordInvoke` + `Protocol::SendWakeWordDetected` 这一条既有链路，实现在不改服务器协议的前提下，从设备端主动“发文本 → 触发 LLM 语音播报”。

关键点：当设备处于 `listening` 状态时，会直接走 `SendWakeWordDetected` 发送文本；其他状态则走 `WakeWordInvoke`，避免 `listening` 下误触发“关闭音频通道”的分支。

## 2. 使用方式
1. 在需要的逻辑里面引用 `tool/chat/mcp_text_invoke_tool.h`，然后调用：

```cpp
TextInvokeTool::Options options;
options.max_len = 120;  // 可选，超过会按 max_len 分片
TextInvokeTool::GetInstance().Submit("想要播报的文本", options);
```

也可以直接调用无 Options 的重载：

```cpp
TextInvokeTool::GetInstance().Submit("想要播报的文本");
```

2. 提交后工具会自动在 `idle`/`listening` 状态下立即发送；如果当前正在 `speaking`，会先入队，等 `application` 收到 TTS `stop` 后再发出下一条（hook 在 `DeviceStateEventManager`）。

3. 对 aicodeing 的提示词示例（用于让其调用该工具）：

```
请调用 MCP 工具 `self.text_invoke`，参数 text 为需要播报的内容；
如需分片可传 max_len；如需控制队列可传 queue_limit 和 drop_oldest。
```

## 3. 可配置项
- `max_len`（可选）：单条最大字节长度，超过会自动按此值分片发送；如果不设或设为 `0`，不会强制分片。
- `queue_limit`（可选）：队列最大条数，超过后的策略由 `drop_oldest` 决定（默认为 `true`，会丢弃最旧项）。
- `drop_oldest`（可选）：`true` 时队列满了丢弃最旧项；`false` 时拒绝新增消息。
- `retry` 次数固定为 1 次（当前实现不可修改）；如果发送没有进入 `speaking` 状态会重试一次，再失败就记录日志并丢弃。
- `ack_timeout` 固定为 8 秒（当前实现不可修改）；超过即触发重试/丢弃逻辑。
- 自测选项：`CONFIG_TEXT_INVOKE_SELFTEST`（Kconfig），开启后系统启动约 10 秒会自动提交两条自检文本。

## 4. 典型场景
闹钟触发后调用：

```cpp
TextInvokeTool::Options options;
options.max_len = 160;
TextInvokeTool::GetInstance().Submit("现在我设定的闹钟时间到了，可以提醒我", options);
```

这样会在当前会话通过 WakeWord 通道把文本推送给云端，云端收到后直接返回 TTS 数据，设备接收后播放声音。

## 5. 已知限制
1. 工具仍然依赖 WakeWordDetect 消息，因此如果服务器端只针对真实唤醒词处理（而非当用户输入），可能会忽略这些“模拟唤醒”文本。此时可以在文本前加固定前缀（例如“唤醒词: …”）作为兼容，确保后端能识别。
2. 当前 retry 固定 1 次，超过后包括网络中断、服务器未进入 `speaking` 等情况都会被丢弃并打印日志。
3. 由于开始阶段会等待 `DeviceState` 进入 `speaking` 才认定发送成功，若 `WakeWordInvoke` 立刻被其他音频打断，后续队列才会继续处理。

## 6. 最小验收自测与验证结果
1. 启用：在 `menuconfig` 打开 `CONFIG_TEXT_INVOKE_SELFTEST`，编译并刷入设备。
2. 预期行为：启动约 10 秒后，日志应显示两条自检文本提交；第一条触发 TTS 播放，第二条在 `speaking` 期间排队并在播放结束后自动发送。
3. 验证要点：确认听到两段语音播报且顺序正确；日志中包含 `Selftest trigger` 以及入队/出队/发送相关日志。
