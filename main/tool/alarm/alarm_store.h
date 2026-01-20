#ifndef ALARM_STORE_H
#define ALARM_STORE_H

#include <cstdint>
#include <map>
#include <string>

struct AlarmRecord {
    uint32_t id = 0;
    int64_t trigger_ms = 0;
    std::string label;
    bool is_daily = false;
    int hour = 0;
    int minute = 0;
};

class AlarmStore {
public:
    bool Load(std::map<uint32_t, AlarmRecord>& alarms, uint32_t& next_id);
    bool Save(const std::map<uint32_t, AlarmRecord>& alarms);
};

#endif // ALARM_STORE_H
