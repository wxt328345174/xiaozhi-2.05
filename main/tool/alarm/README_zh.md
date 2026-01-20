# 闹钟 MCP 工具说明（中文）

## 1. 能力概述
`alarm.*` 提供本地闹钟与倒计时能力，触发时通过 `self.text_invoke` 走 LLM 回复的“伪提醒”链路。

- 获取当前本地时间与同步状态
- 设置闹钟、列出闹钟、删除闹钟（一次性闹钟）
- 支持每日重复闹钟
- 设置/取消/查询倒计时
- 触发时调用 `self.text_invoke(text=...)`，提醒文本会裁剪为 <=10 个字符（按 UTF-8 字符数）

## 2. 工具列表与参数

### 2.1 alarm.get_time
无参数。

返回示例：
```json
{
  "epoch_ms": 1737340800000,
  "epoch_sec": 1737340800,
  "local_time": "2025-01-20 12:00:00",
  "synced": true
}
```

### 2.2 alarm.set_alarm
参数：
- `time_text` (string, 必填)
- `label` (string, 可选，提醒内容)
- `repeat` (string, 可选，默认 `NONE`，可选 `DAILY`)

返回示例：
```json
{
  "alarm_id": 1,
  "trigger_epoch_ms": 1737340860000,
  "trigger_time": "2025-01-20 12:01:00",
  "is_daily": false,
  "repeat_type": "NONE"
}
```

### 2.3 alarm.list_alarms
无参数。

返回示例：
```json
[
  {
    "id": 1,
    "label": "喝水",
    "hour": 7,
    "minute": 30,
    "trigger_epoch_ms": 1737340860000,
    "trigger_time": "2025-01-20 12:01:00",
    "is_daily": true,
    "repeat_type": "DAILY",
    "display": "每天 07:30 喝水（下次：2025-01-20 07:30）",
    "remaining_sec": 42
  }
]
```

### 2.4 alarm.delete_alarm
参数：
- `id` (int, 必填)

返回示例：
```json
{
  "deleted": true
}
```

### 2.5 alarm.set_countdown
参数：
- `time_text` (string, 必填，仅支持相对时长)
- `label` (string, 可选)

返回示例：
```json
{
  "countdown_id": 1,
  "deadline_epoch_ms": 1737340860000,
  "deadline_time": "2025-01-20 12:01:00"
}
```

### 2.6 alarm.cancel_countdown
参数：
- `id` (int, 可选；0 表示取消当前倒计时)

返回示例：
```json
{
  "canceled": true
}
```

### 2.7 alarm.get_countdown
无参数。

返回示例：
```json
{
  "active": true,
  "countdown_id": 1,
  "deadline_epoch_ms": 1737340860000,
  "deadline_time": "2025-01-20 12:01:00",
  "remaining_sec": 42,
  "label": "泡面"
}
```

## 3. time_text 解析规则
支持以下时间表达：

- 相对时间：
  - “10分钟后” / “2小时后” / “1天后”
  - 组合写法如 “1小时30分钟后”
  - 英文/缩写：`10s`/`5m`/`2h15m`/`after 1 day`/`in 5 minutes`
  - “多久后” 将被视为无效（缺少具体时长）

- 绝对时间：
  - “3月15日 8点30分” / “3月15日 08:30”
  - “2025年03月15日 08:30” / “2025-03-15 08:30”
  - “今天 7:30” / “明天14:50” / “后天 9点半”

- 特殊规则：
  - “X天X点X分” 解析为 “X 天后 + 当天 X 点 X 分”
    - 若 X 天为 0 且该时间已过去，则自动顺延到下一天
- 每日闹钟：
  - “每天/每日 + 时间” 解析为每天重复
  - 支持 “每天上午10点半”“每日晚上9点”“每天 7:30”
  - 若 LLM 把 `time_text` 规范为 `14:43`，请在工具参数里传 `repeat="DAILY"`

## 4. 触发提醒与 10 字节限制
闹钟/倒计时触发后会调用：

```
self.text_invoke(text=...)
```

- `text` 内容尽量短，可包含 label 关键词（示例：`闹钟:喝水`）
- 提醒文本会截断为 <=10 个字符（按 UTF-8 字符数），避免分片干扰对话

## 5. 持久化说明
- 闹钟列表使用 NVS 存储（namespace: `alarm`, key: `alarms`），设备重启后可恢复
- 倒计时不持久化，重启后清空

## 6. 例子

自然语言：
```
alarm.set_alarm { "time_text": "10分钟后", "label": "喝水" }
alarm.set_alarm { "time_text": "3月15日 8点30分", "label": "起床" }
alarm.set_alarm { "time_text": "2天8点0分", "label": "复诊" }
alarm.set_alarm { "time_text": "每天上午10点半", "label": "点外卖" }
alarm.set_alarm { "time_text": "tomorrow 14:50", "label": "吃饭", "repeat": "NONE" }
alarm.set_alarm { "time_text": "14:43", "label": "点外卖", "repeat": "DAILY" }
```

结构化：
```
alarm.set_alarm { "time_text": "2025-03-15 08:30", "label": "会议" }
```

倒计时：
```
alarm.set_countdown { "time_text": "5分钟后", "label": "泡面" }
alarm.set_countdown { "time_text": "10s", "label": "喝水" }
```

## 7. 返回字段说明
- `alarm_id`/`countdown_id`：唯一标识
- `repeat_type`：`NONE` 或 `DAILY`
- `trigger_epoch_ms`/`trigger_time`：下一次触发时间
- `display`：便于 UI 直接展示的摘要文本

## 8. 已知限制与排错
- `E_TIME_PARSE(*)`：输入时间无法解析，检查是否符合格式
- daily 闹钟需要“时间点”，不支持纯时长（如 `10分钟后`）
- 查看 INFO 日志可确认：入参、解析分支、保存结果、触发/重排、持久化加载
