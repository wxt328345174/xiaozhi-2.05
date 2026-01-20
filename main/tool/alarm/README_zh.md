# 闹钟 MCP 工具说明（中文）

## 1. 能力概述
`alarm.*` 提供本地闹钟与倒计时能力，触发时通过 `self.text_invoke` 走 LLM 回复的“伪提醒”链路。

- 获取当前本地时间与同步状态
- 设置闹钟、列出闹钟、删除闹钟（一次性闹钟）
- 支持每日重复闹钟
- 设置/取消/查询倒计时
- 触发时调用 `self.text_invoke(text=..., max_len=10)`，确保每片 <= 10 字节

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

返回示例：
```json
{
  "alarm_id": 1,
  "trigger_epoch_ms": 1737340860000,
  "trigger_time": "2025-01-20 12:01:00",
  "is_daily": false
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
  - “多久后” 将被视为无效（缺少具体时长）

- 绝对时间：
  - “3月15日 8点30分” / “3月15日 08:30”
  - “2025年03月15日 08:30” / “2025-03-15 08:30”

- 特殊规则：
  - “X天X点X分” 解析为 “X 天后 + 当天 X 点 X 分”
    - 若 X 天为 0 且该时间已过去，则自动顺延到下一天
- 每日闹钟：
  - “每天/每日 + 时间” 解析为每天重复
  - 支持 “每天上午10点半”“每日晚上9点”“每天 7:30”

## 4. 触发提醒与 10 字节限制
闹钟/倒计时触发后会调用：

```
self.text_invoke(text=..., max_len=10)
```

- `text` 内容尽量短，可包含 label 关键词（示例：`闹钟:喝水`）
- `max_len=10` 用于分片，保证单片不超过 10 字节

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
```

结构化：
```
alarm.set_alarm { "time_text": "2025-03-15 08:30", "label": "会议" }
```

倒计时：
```
alarm.set_countdown { "time_text": "5分钟后", "label": "泡面" }
```
