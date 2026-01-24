# 闹钟 MCP 工具说明（中文）

## 1. 能力
- 一次性闹钟 / 每日闹钟 / 倒计时
- 列出 / 删除闹钟
- 触发走 `self.text_invoke` 播报（不分片、不截断）

## 2. 工具与参数
- `alarm.get_time`
- `alarm.set_alarm(time_text, label?, repeat?, period?, meridiem?, raw_time_text?)`
  - `repeat`: `NONE`（默认）或 `DAILY`（仅当用户明确“每天/每日”）
  - `period`/`meridiem`: 仅在用户说上午/下午/晚上或 AM/PM 时填写
  - `raw_time_text`: 用户原始时间短语，用于歧义纠正
- `alarm.list_alarms`（含 `repeat_type`、`display`）
- `alarm.delete_alarm(id)`
- `alarm.set_countdown(time_text, label?)`（仅相对时长）
- `alarm.get_countdown` / `alarm.cancel_countdown`

## 3. time_text 规则（摘要）
- 相对：`10分钟后`/`2小时后`/`1天后`/`1小时30分钟后`，`10s`/`5m`/`2h15m`/`after 1 day`
- 绝对：`3月15日 8点30分`、`2025-03-15 08:30`、`今天 7:30`、`明天14:50`、`后天 9点半`
- 每日：`每天/每日 + 时间`，如 `每天上午10点半`、`每日晚上9点`、`每天 7:30`；若被规范成 `14:43` 需传 `repeat="DAILY"`
- 歧义规则：1–12 点且无时段词默认 AM；13–23 点按 24h；只有明确下午/晚上/PM 才 +12

## 4. 提醒文本（WakeWordPath 注入）
- 固定模板：`【闹钟助手】该` + label + `了`（label 为空则 `【闹钟助手】到点了`）
- 示例：`【闹钟助手】该吃饭了` / `【闹钟助手】该喝水了` / `【闹钟助手】该开会了` / `【闹钟助手】到点了`
- 触发仅发送一条 `text`，不分片、不截断

## 5. 队列参数
`self.text_invoke` 支持 `queue_limit` / `drop_oldest` 控制队列；alarm 侧默认不传，沿用 TextInvokeTool 的默认队列策略。

## 6. 例子
```
alarm.set_alarm { "time_text": "10分钟后", "label": "喝水" }
alarm.set_alarm { "time_text": "明天14:50", "label": "吃饭" }
alarm.set_alarm { "time_text": "每天上午10点半", "label": "点外卖" }
alarm.set_alarm { "time_text": "14:43", "label": "点外卖", "repeat": "DAILY" }
alarm.set_countdown { "time_text": "10s", "label": "喝水" }
```

## 7. 持久化
- 闹钟列表存 NVS（namespace `alarm`, key `alarms`）；倒计时不持久化

## 8. 已知限制 / 排错
- `E_TIME_PARSE(*)`：输入时间不符合格式
- DAILY 需时间点，不支持纯时长
- INFO 日志可查：入参、解析分支、保存结果、触发/重排、NVS 加载

## 9. 移植提示
- 新增：`main/tool/alarm/` 内的 alarm_tool / alarm_store / time_parser / README
- 修改：`main/application.cc`（注册）与 `main/CMakeLists.txt`（编译源）
