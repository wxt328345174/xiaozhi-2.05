# 闹钟 MCP 工具说明（中文）

## 1. 能力概述
`alarm.*` 提供本地闹钟与倒计时能力，触发时通过 `self.text_invoke` 走 LLM 回复的“伪提醒”链路。

- 一次性闹钟 / 每日闹钟 / 倒计时
- 列出/删除闹钟
- 提醒文本自动裁剪为 <=10 个字符（UTF-8 字符数）

## 2. MCP 工具
- `alarm.get_time`：获取本地时间与同步状态
- `alarm.set_alarm(time_text, label?, repeat?)`
  - `repeat`: `NONE`（默认）或 `DAILY`（仅当用户明确“每天/每日”时使用）
- `alarm.list_alarms`：返回结构化列表（含 `repeat_type` 与 `display`）
- `alarm.delete_alarm(id)`
- `alarm.set_countdown(time_text, label?)`：仅支持相对时长
- `alarm.get_countdown` / `alarm.cancel_countdown`

## 3. time_text 支持格式
相对时间：
- `10分钟后` / `2小时后` / `1天后` / `1小时30分钟后`
- 英文/缩写：`10s` / `5m` / `2h15m` / `after 1 day` / `in 5 minutes`

绝对时间：
- `3月15日 8点30分` / `2025-03-15 08:30`
- `今天 7:30` / `明天14:50` / `后天 9点半`

每日闹钟：
- `每天/每日 + 时间`
- 如 `每天上午10点半`、`每日晚上9点`、`每天 7:30`
- 若 LLM 把 `time_text` 规范为 `14:43`，需传 `repeat="DAILY"`

特殊规则：
- `X天X点X分` 解析为 “X 天后 + 当天 X 点 X 分”（当天已过则顺延）

## 4. 提醒文本与限制
触发时调用：
```
self.text_invoke(text=...)
```
提醒文本会裁剪为 <=10 个字符（UTF-8 字符数），避免分片干扰对话。

## 5. 返回字段
- `alarm_id` / `countdown_id`：唯一标识
- `repeat_type`：`NONE` 或 `DAILY`
- `display`：面向 UI/LLM 的摘要文本（如 “每天 07:30 喝水（下次：...）”）

## 6. 例子
```
alarm.set_alarm { "time_text": "10分钟后", "label": "喝水" }
alarm.set_alarm { "time_text": "明天14:50", "label": "吃饭" }
alarm.set_alarm { "time_text": "每天上午10点半", "label": "点外卖" }
alarm.set_alarm { "time_text": "14:43", "label": "点外卖", "repeat": "DAILY" }
alarm.set_countdown { "time_text": "10s", "label": "喝水" }
```

## 7. 持久化
- 闹钟列表存 NVS（namespace: `alarm`, key: `alarms`）
- 倒计时不持久化，重启后清空

## 8. 已知限制与排错
- `E_TIME_PARSE(*)`：输入时间无法解析
- DAILY 闹钟需要“时间点”，不支持纯时长（如 `10分钟后`）
- INFO 日志覆盖：入参、解析分支、保存结果、触发/重排、持久化加载

## 9. 移植策略
新增文件（`main/tool/alarm/`）：
- `alarm_tool.h/.cpp`、`alarm_store.h/.cpp`、`time_parser.h/.cpp`、`README_zh.md`

修改原文件：
- `main/application.cc`（初始化注册）
- `main/CMakeLists.txt`（加入源码）
