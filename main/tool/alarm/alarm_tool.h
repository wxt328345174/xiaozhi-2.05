#ifndef ALARM_TOOL_H
#define ALARM_TOOL_H

#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include <esp_timer.h>

#include "alarm_store.h"
#include "mcp_server.h"
#include "time_parser.h"
#include "tool/chat/mcp_text_invoke_tool.h"

class AlarmTool {
public:
    static AlarmTool& GetInstance();
    void Initialize();

private:
    struct CountdownItem {
        uint32_t id = 0;
        int64_t deadline_ms = 0;
        std::string label;
    };

    ReturnValue HandleGetTime(const PropertyList& properties);
    ReturnValue HandleSetAlarm(const PropertyList& properties);
    ReturnValue HandleListAlarms(const PropertyList& properties);
    ReturnValue HandleDeleteAlarm(const PropertyList& properties);
    ReturnValue HandleSetCountdown(const PropertyList& properties);
    ReturnValue HandleCancelCountdown(const PropertyList& properties);
    ReturnValue HandleGetCountdown(const PropertyList& properties);

    void OnTimer();
    void ScheduleNext();
    void TriggerAlarm(const AlarmRecord& alarm);
    void TriggerCountdown(const CountdownItem& countdown);
    void SaveAlarms(const std::map<uint32_t, AlarmRecord>& snapshot);
    int64_t GetNowMs() const;

    void CreateTimerIfNeeded();
    void StopTimer();

    std::mutex mutex_;
    std::map<uint32_t, AlarmRecord> alarms_;
    std::optional<CountdownItem> countdown_;
    AlarmStore store_;
    uint32_t next_alarm_id_ = 1;
    uint32_t next_countdown_id_ = 1;
    esp_timer_handle_t timer_ = nullptr;
    bool timer_active_ = false;
    bool initialized_ = false;
};

#endif // ALARM_TOOL_H
