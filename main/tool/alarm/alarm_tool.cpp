#include "alarm_tool.h"

#include <algorithm>
#include <cinttypes>
#include <stdexcept>
#include <vector>

#include <esp_err.h>
#include <esp_log.h>
#include <sys/time.h>

namespace {

static const char* TAG = "AlarmTool";
static const size_t kMaxAlarms = 16;
static const size_t kTextMaxChars = 10;

size_t Utf8CharLen(unsigned char ch) {
    if ((ch & 0x80) == 0x00) {
        return 1;
    }
    if ((ch & 0xE0) == 0xC0) {
        return 2;
    }
    if ((ch & 0xF0) == 0xE0) {
        return 3;
    }
    if ((ch & 0xF8) == 0xF0) {
        return 4;
    }
    return 1;
}

std::string TruncateUtf8(const std::string& text, size_t max_chars) {
    if (max_chars == 0 || text.empty()) {
        return "";
    }
    size_t count = 0;
    size_t offset = 0;
    while (offset < text.size() && count < max_chars) {
        size_t len = Utf8CharLen(static_cast<unsigned char>(text[offset]));
        if (offset + len > text.size()) {
            break;
        }
        offset += len;
        count++;
    }
    return text.substr(0, offset);
}

std::string BuildReminderText(const std::string& prefix, const std::string& label) {
    std::string text = prefix;
    if (!label.empty()) {
        text += ":";
        text += label;
    }
    return TruncateUtf8(text, kTextMaxChars);
}

std::string RepeatTypeString(bool is_daily) {
    return is_daily ? "DAILY" : "NONE";
}

std::string FormatHourMinute(int hour, int minute) {
    char buffer[8] = {0};
    std::snprintf(buffer, sizeof(buffer), "%02d:%02d", hour, minute);
    return std::string(buffer);
}

void ToLowerAscii(std::string& text) {
    for (auto& ch : text) {
        if (ch >= 'A' && ch <= 'Z') {
            ch = static_cast<char>(ch - 'A' + 'a');
        }
    }
}

bool ContainsAnyToken(const std::string& text, const std::vector<std::string>& tokens) {
    for (const auto& token : tokens) {
        if (text.find(token) != std::string::npos) {
            return true;
        }
    }
    return false;
}

bool HasPeriodToken(const std::string& text) {
    std::string lowered = text;
    ToLowerAscii(lowered);
    return ContainsAnyToken(lowered, {"早上", "上午", "中午", "下午", "晚上", "am", "pm"});
}

std::string NormalizePeriodParam(const std::string& period, const std::string& meridiem) {
    std::string value = period;
    if (value.empty()) {
        value = meridiem;
    }
    std::string lowered = value;
    ToLowerAscii(lowered);
    if (lowered == "am" || lowered == "上午" || lowered == "早上") {
        return "上午";
    }
    if (lowered == "pm" || lowered == "下午" || lowered == "晚上" || lowered == "夜晚" || lowered == "傍晚") {
        return "晚上";
    }
    if (lowered == "中午") {
        return "中午";
    }
    return "";
}

bool IsTimeOnlyInput(const std::string& text) {
    return !ContainsAnyToken(text, {"年", "月", "日", "天", "今天", "明天", "后天"}) &&
        ContainsAnyToken(text, {":", "点", "时", "半"});
}

int64_t ComputeNextDailyTrigger(int64_t now_ms, int hour, int minute) {
    time_t now_sec = static_cast<time_t>(now_ms / 1000);
    std::tm target_tm = {};
    if (localtime_r(&now_sec, &target_tm) == nullptr) {
        return now_ms;
    }
    target_tm.tm_hour = hour;
    target_tm.tm_min = minute;
    target_tm.tm_sec = 0;
    target_tm.tm_isdst = -1;
    time_t target_epoch = mktime(&target_tm);
    if (target_epoch < 0) {
        return now_ms;
    }
    int64_t epoch_ms = static_cast<int64_t>(target_epoch) * 1000;
    if (epoch_ms <= now_ms) {
        target_tm.tm_mday += 1;
        target_epoch = mktime(&target_tm);
        if (target_epoch < 0) {
            return now_ms;
        }
        epoch_ms = static_cast<int64_t>(target_epoch) * 1000;
    }
    return epoch_ms;
}

}

AlarmTool& AlarmTool::GetInstance() {
    static AlarmTool instance;
    return instance;
}

void AlarmTool::Initialize() {
    if (initialized_) {
        return;
    }
    initialized_ = true;

    CreateTimerIfNeeded();

    if (!store_.Load(alarms_, next_alarm_id_)) {
        ESP_LOGW(TAG, "Failed to load alarms from NVS, starting empty");
        alarms_.clear();
        next_alarm_id_ = 1;
    }
    ESP_LOGI(TAG, "Alarm store loaded: count=%u next_id=%u",
        static_cast<unsigned>(alarms_.size()),
        static_cast<unsigned>(next_alarm_id_));

    const int64_t now_ms = GetNowMs();
    bool removed_expired = false;
    bool updated_daily = false;
    for (auto it = alarms_.begin(); it != alarms_.end(); ) {
        if (it->second.is_daily) {
            int64_t updated_trigger = ComputeNextDailyTrigger(now_ms, it->second.hour, it->second.minute);
            if (updated_trigger != it->second.trigger_ms) {
                it->second.trigger_ms = updated_trigger;
                updated_daily = true;
            }
            ++it;
        } else if (it->second.trigger_ms <= now_ms) {
            it = alarms_.erase(it);
            removed_expired = true;
        } else {
            ++it;
        }
    }
    if (removed_expired || updated_daily) {
        SaveAlarms(alarms_);
    }

    auto& mcp_server = McpServer::GetInstance();
    mcp_server.AddTool("alarm.get_time",
        "Get current local time and sync status",
        PropertyList(),
        [this](const PropertyList& properties) -> ReturnValue {
            return HandleGetTime(properties);
        });

    mcp_server.AddTool("alarm.set_alarm",
        "Set an alarm with time_text and optional label. repeat=DAILY only when user explicitly says 每天/每日 (not for plain time-only). "
        "Ambiguity: 1-12 without period defaults to AM; 13-23 is 24h. "
        "Optional period/meridiem only when user says 上午/下午/晚上 or AM/PM.",
        PropertyList({
            Property("time_text", kPropertyTypeString),
            Property("label", kPropertyTypeString, ""),
            Property("repeat", kPropertyTypeString, "NONE"),
            Property("period", kPropertyTypeString, ""),
            Property("meridiem", kPropertyTypeString, ""),
            Property("raw_time_text", kPropertyTypeString, "")
        }),
        [this](const PropertyList& properties) -> ReturnValue {
            return HandleSetAlarm(properties);
        });

    mcp_server.AddTool("alarm.list_alarms",
        "List all alarms",
        PropertyList(),
        [this](const PropertyList& properties) -> ReturnValue {
            return HandleListAlarms(properties);
        });

    mcp_server.AddTool("alarm.delete_alarm",
        "Delete an alarm by id",
        PropertyList({
            Property("id", kPropertyTypeInteger)
        }),
        [this](const PropertyList& properties) -> ReturnValue {
            return HandleDeleteAlarm(properties);
        });

    mcp_server.AddTool("alarm.set_countdown",
        "Set a countdown timer",
        PropertyList({
            Property("time_text", kPropertyTypeString),
            Property("label", kPropertyTypeString, "")
        }),
        [this](const PropertyList& properties) -> ReturnValue {
            return HandleSetCountdown(properties);
        });

    mcp_server.AddTool("alarm.cancel_countdown",
        "Cancel the current countdown",
        PropertyList({
            Property("id", kPropertyTypeInteger, 0)
        }),
        [this](const PropertyList& properties) -> ReturnValue {
            return HandleCancelCountdown(properties);
        });

    mcp_server.AddTool("alarm.get_countdown",
        "Get current countdown status",
        PropertyList(),
        [this](const PropertyList& properties) -> ReturnValue {
            return HandleGetCountdown(properties);
        });

    ScheduleNext();
}

ReturnValue AlarmTool::HandleGetTime(const PropertyList& properties) {
    (void)properties;
    int64_t now_ms = GetNowMs();
    bool synced = TimeParser::IsTimeSynced(now_ms);

    cJSON* root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "epoch_ms", static_cast<double>(now_ms));
    cJSON_AddNumberToObject(root, "epoch_sec", static_cast<double>(now_ms / 1000));
    cJSON_AddStringToObject(root, "local_time", TimeParser::FormatLocalTime(now_ms).c_str());
    cJSON_AddBoolToObject(root, "synced", synced);
    return root;
}

ReturnValue AlarmTool::HandleSetAlarm(const PropertyList& properties) {
    auto time_text = properties["time_text"].value<std::string>();
    auto label = properties["label"].value<std::string>();
    auto repeat = properties["repeat"].value<std::string>();
    auto period = properties["period"].value<std::string>();
    auto meridiem = properties["meridiem"].value<std::string>();
    auto raw_time_text = properties["raw_time_text"].value<std::string>();

    int64_t now_ms = GetNowMs();
    ESP_LOGI(TAG, "Alarm set request: time_text='%s' label='%s' repeat='%s' period='%s' meridiem='%s' raw_time_text='%s'",
        time_text.c_str(),
        label.c_str(),
        repeat.c_str(),
        period.c_str(),
        meridiem.c_str(),
        raw_time_text.c_str());
    std::string parse_text = time_text;
    std::string period_prefix = NormalizePeriodParam(period, meridiem);
    if (!period_prefix.empty() && !HasPeriodToken(parse_text)) {
        parse_text = period_prefix + parse_text;
    }
    auto parsed = TimeParser::Parse(parse_text, now_ms);
    if (!parsed.ok) {
        throw std::runtime_error("E_TIME_PARSE(" + std::to_string(parsed.error_code) + "): " + parsed.message);
    }

    std::string repeat_upper = repeat;
    for (auto& ch : repeat_upper) {
        if (ch >= 'a' && ch <= 'z') {
            ch = static_cast<char>(ch - 'a' + 'A');
        }
    }

    bool corrected = false;
    if (IsTimeOnlyInput(time_text) && !raw_time_text.empty()) {
        auto raw_parsed = TimeParser::Parse(raw_time_text, now_ms);
        if (raw_parsed.ok && raw_parsed.default_am && !raw_parsed.used_period && raw_parsed.hour >= 1 && raw_parsed.hour <= 12) {
            if (parsed.is_24h && parsed.hour >= 13) {
                parsed.hour = raw_parsed.hour;
                parsed.minute = raw_parsed.minute;
                parsed.trigger_ms = ComputeNextDailyTrigger(now_ms, parsed.hour, parsed.minute);
                corrected = true;
            }
        }
    }
    if (corrected) {
        ESP_LOGI(TAG, "Alarm time corrected: raw_time_text='%s' corrected_time=%s",
            raw_time_text.c_str(),
            FormatHourMinute(parsed.hour, parsed.minute).c_str());
    }
    ESP_LOGI(TAG, "Alarm time meta: is_24h=%d default_am=%d period_adj=%d corrected=%d",
        parsed.is_24h ? 1 : 0,
        parsed.default_am ? 1 : 0,
        parsed.period_adjusted ? 1 : 0,
        corrected ? 1 : 0);

    AlarmRecord record;
    record.label = label;
    bool is_daily_final = false;
    const bool repeat_param_daily = (repeat_upper == "DAILY");
    const bool has_daily_hint = parsed.has_daily_keyword || parsed.is_daily;
    std::string repeat_reason = "none";
    if (repeat_param_daily) {
        is_daily_final = true;
        repeat_reason = "user_param";
    } else if (has_daily_hint) {
        is_daily_final = true;
        repeat_reason = "daily_keyword";
    }
    record.is_daily = is_daily_final;
    if (is_daily_final && parsed.kind == TimeParser::ParseKind::kRelative) {
        throw std::runtime_error("daily alarm requires time-of-day, not duration");
    }
    if (is_daily_final) {
        record.hour = parsed.hour;
        record.minute = parsed.minute;
        record.trigger_ms = ComputeNextDailyTrigger(now_ms, record.hour, record.minute);
    } else {
        record.trigger_ms = parsed.trigger_ms;
        if (record.trigger_ms <= now_ms) {
            throw std::runtime_error("trigger time is in the past");
        }
    }

    std::map<uint32_t, AlarmRecord> snapshot;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (alarms_.size() >= kMaxAlarms) {
            throw std::runtime_error("too many alarms");
        }
        record.id = next_alarm_id_++;
        alarms_[record.id] = record;
        snapshot = alarms_;
    }

    SaveAlarms(snapshot);
    ScheduleNext();

    std::string preview_text;
    if (record.is_daily) {
        preview_text = BuildReminderText("每日闹钟", record.label);
    } else if (record.label.empty()) {
        preview_text = BuildReminderText("闹钟到时", "");
    } else {
        preview_text = BuildReminderText("闹钟", record.label);
    }
    ESP_LOGI(TAG, "Alarm preview text: %s", preview_text.c_str());

    cJSON* root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "alarm_id", static_cast<double>(record.id));
    cJSON_AddBoolToObject(root, "is_daily", record.is_daily);
    cJSON_AddStringToObject(root, "repeat_type", RepeatTypeString(record.is_daily).c_str());
    ESP_LOGI(TAG, "Alarm saved: id=%u repeat=%s reason=%s",
        static_cast<unsigned>(record.id),
        RepeatTypeString(record.is_daily).c_str(),
        repeat_reason.c_str());
    return root;
}

ReturnValue AlarmTool::HandleListAlarms(const PropertyList& properties) {
    (void)properties;
    cJSON* root = cJSON_CreateArray();
    int64_t now_ms = GetNowMs();

    store_.DumpRaw();

    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<const AlarmRecord*> daily_records;
    std::vector<const AlarmRecord*> one_time_records;
    daily_records.reserve(alarms_.size());
    one_time_records.reserve(alarms_.size());
    for (const auto& entry : alarms_) {
        const AlarmRecord& record = entry.second;
        if (record.is_daily) {
            daily_records.push_back(&record);
        } else {
            one_time_records.push_back(&record);
        }
    }

    auto append_item = [&](const AlarmRecord& record) {
        cJSON* item = cJSON_CreateObject();
        cJSON_AddNumberToObject(item, "id", static_cast<double>(record.id));
        if (!record.label.empty()) {
            cJSON_AddStringToObject(item, "label", record.label.c_str());
        }
        if (record.is_daily) {
            cJSON_AddNumberToObject(item, "hour", record.hour);
            cJSON_AddNumberToObject(item, "minute", record.minute);
            cJSON_AddStringToObject(item, "time_of_day", FormatHourMinute(record.hour, record.minute).c_str());
        }
        cJSON_AddBoolToObject(item, "is_daily", record.is_daily);
        cJSON_AddStringToObject(item, "repeat_type", RepeatTypeString(record.is_daily).c_str());
        int64_t remaining = (record.trigger_ms - now_ms) / 1000;
        if (remaining < 0) {
            remaining = 0;
        }
        cJSON_AddNumberToObject(item, "remaining_sec", static_cast<double>(remaining));
        std::string display;
        if (record.is_daily) {
            display = "每天 " + FormatHourMinute(record.hour, record.minute);
        } else {
            display = "一次性 " + TimeParser::FormatLocalTime(record.trigger_ms);
        }
        if (!record.label.empty()) {
            display += "「";
            display += record.label;
            display += "」";
        }
        display += "（下次：";
        display += TimeParser::FormatLocalTime(record.trigger_ms);
        display += "）";
        cJSON_AddStringToObject(item, "display", display.c_str());
        cJSON_AddStringToObject(item, "group", record.is_daily ? "DAILY" : "NONE");
        cJSON_AddItemToArray(root, item);
        ESP_LOGI(TAG, "Alarm list item: id=%u repeat=%s",
            static_cast<unsigned>(record.id),
            RepeatTypeString(record.is_daily).c_str());
    };

    for (const auto* record : daily_records) {
        append_item(*record);
    }
    for (const auto* record : one_time_records) {
        append_item(*record);
    }

    return root;
}

ReturnValue AlarmTool::HandleDeleteAlarm(const PropertyList& properties) {
    uint32_t id = static_cast<uint32_t>(properties["id"].value<int>());
    bool deleted = false;
    std::map<uint32_t, AlarmRecord> snapshot;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = alarms_.find(id);
        if (it != alarms_.end()) {
            alarms_.erase(it);
            deleted = true;
            snapshot = alarms_;
        }
    }

    if (deleted) {
        SaveAlarms(snapshot);
        ScheduleNext();
    }

    cJSON* root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "deleted", deleted);
    return root;
}

ReturnValue AlarmTool::HandleSetCountdown(const PropertyList& properties) {
    auto time_text = properties["time_text"].value<std::string>();
    auto label = properties["label"].value<std::string>();

    int64_t now_ms = GetNowMs();
    auto parsed = TimeParser::Parse(time_text, now_ms);
    if (!parsed.ok) {
        throw std::runtime_error("E_TIME_PARSE(" + std::to_string(parsed.error_code) + "): " + parsed.message);
    }
    if (parsed.kind != TimeParser::ParseKind::kRelative) {
        throw std::runtime_error("countdown requires relative time");
    }
    if (parsed.trigger_ms <= now_ms) {
        throw std::runtime_error("countdown duration too short");
    }

    CountdownItem item;
    item.deadline_ms = parsed.trigger_ms;
    item.label = label;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        item.id = next_countdown_id_++;
        countdown_ = item;
    }

    ScheduleNext();

    std::string preview_text;
    if (item.label.empty()) {
        preview_text = BuildReminderText("倒计时到时", "");
    } else {
        preview_text = BuildReminderText("倒计时", item.label);
    }
    ESP_LOGI(TAG, "Countdown preview text: %s", preview_text.c_str());

    cJSON* root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "countdown_id", static_cast<double>(item.id));
    cJSON_AddNumberToObject(root, "deadline_epoch_ms", static_cast<double>(item.deadline_ms));
    cJSON_AddStringToObject(root, "deadline_time", TimeParser::FormatLocalTime(item.deadline_ms).c_str());
    return root;
}

ReturnValue AlarmTool::HandleCancelCountdown(const PropertyList& properties) {
    int id = properties["id"].value<int>();
    bool canceled = false;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (countdown_.has_value()) {
            if (id == 0 || static_cast<uint32_t>(id) == countdown_->id) {
                countdown_.reset();
                canceled = true;
            }
        }
    }

    if (canceled) {
        ScheduleNext();
    }

    cJSON* root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "canceled", canceled);
    return root;
}

ReturnValue AlarmTool::HandleGetCountdown(const PropertyList& properties) {
    (void)properties;
    cJSON* root = cJSON_CreateObject();

    std::lock_guard<std::mutex> lock(mutex_);
    if (!countdown_.has_value()) {
        cJSON_AddBoolToObject(root, "active", false);
        return root;
    }

    int64_t now_ms = GetNowMs();
    int64_t remaining = (countdown_->deadline_ms - now_ms) / 1000;
    if (remaining < 0) {
        remaining = 0;
    }

    cJSON_AddBoolToObject(root, "active", true);
    cJSON_AddNumberToObject(root, "countdown_id", static_cast<double>(countdown_->id));
    cJSON_AddNumberToObject(root, "deadline_epoch_ms", static_cast<double>(countdown_->deadline_ms));
    cJSON_AddStringToObject(root, "deadline_time", TimeParser::FormatLocalTime(countdown_->deadline_ms).c_str());
    cJSON_AddNumberToObject(root, "remaining_sec", static_cast<double>(remaining));
    if (!countdown_->label.empty()) {
        cJSON_AddStringToObject(root, "label", countdown_->label.c_str());
    }
    return root;
}

void AlarmTool::OnTimer() {
    std::vector<AlarmRecord> due_alarms;
    std::optional<CountdownItem> due_countdown;
    std::map<uint32_t, AlarmRecord> snapshot;
    bool save_needed = false;

    int64_t now_ms = GetNowMs();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto it = alarms_.begin(); it != alarms_.end(); ) {
            if (it->second.trigger_ms <= now_ms) {
                if (it->second.is_daily) {
                    due_alarms.push_back(it->second);
                    it->second.trigger_ms = ComputeNextDailyTrigger(now_ms, it->second.hour, it->second.minute);
                    ESP_LOGI(TAG, "Alarm rescheduled: id=%u repeat=%s",
                        static_cast<unsigned>(it->second.id),
                        RepeatTypeString(it->second.is_daily).c_str());
                    save_needed = true;
                    ++it;
                } else {
                    due_alarms.push_back(it->second);
                    it = alarms_.erase(it);
                    save_needed = true;
                }
            } else {
                ++it;
            }
        }
        if (countdown_.has_value() && countdown_->deadline_ms <= now_ms) {
            due_countdown = countdown_;
            countdown_.reset();
        }
        if (save_needed) {
            snapshot = alarms_;
        }
    }

    if (save_needed) {
        SaveAlarms(snapshot);
    }

    for (const auto& alarm : due_alarms) {
        TriggerAlarm(alarm);
    }

    if (due_countdown.has_value()) {
        TriggerCountdown(due_countdown.value());
    }

    ScheduleNext();
}

void AlarmTool::ScheduleNext() {
    int64_t now_ms = GetNowMs();
    int64_t next_ms = 0;
    bool has_next = false;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& entry : alarms_) {
            int64_t trigger_ms = entry.second.trigger_ms;
            if (!has_next || trigger_ms < next_ms) {
                next_ms = trigger_ms;
                has_next = true;
            }
        }
        if (countdown_.has_value()) {
            int64_t trigger_ms = countdown_->deadline_ms;
            if (!has_next || trigger_ms < next_ms) {
                next_ms = trigger_ms;
                has_next = true;
            }
        }
    }

    if (!has_next) {
        StopTimer();
        return;
    }

    int64_t delay_ms = next_ms - now_ms;
    if (delay_ms < 0) {
        delay_ms = 0;
    }

    CreateTimerIfNeeded();
    if (timer_ == nullptr) {
        return;
    }

    esp_timer_stop(timer_);
    esp_err_t err = esp_timer_start_once(timer_, static_cast<uint64_t>(delay_ms) * 1000ULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start alarm timer: %s", esp_err_to_name(err));
        timer_active_ = false;
    } else {
        timer_active_ = true;
    }
}

void AlarmTool::TriggerAlarm(const AlarmRecord& alarm) {
    std::string text;
    if (alarm.label.empty()) {
        text = BuildReminderText("闹钟到时", "");
    } else {
        text = BuildReminderText("闹钟", alarm.label);
    }
    TextInvokeTool::GetInstance().Submit(text);
    ESP_LOGI(TAG, "Alarm triggered: id=%u repeat=%s",
        static_cast<unsigned>(alarm.id),
        RepeatTypeString(alarm.is_daily).c_str());
}

void AlarmTool::TriggerCountdown(const CountdownItem& countdown) {
    std::string text;
    if (countdown.label.empty()) {
        text = BuildReminderText("倒计时到时", "");
    } else {
        text = BuildReminderText("倒计时", countdown.label);
    }
    TextInvokeTool::GetInstance().Submit(text);
    ESP_LOGI(TAG, "Countdown triggered: id=%u", static_cast<unsigned>(countdown.id));
}

void AlarmTool::SaveAlarms(const std::map<uint32_t, AlarmRecord>& snapshot) {
    if (!store_.Save(snapshot)) {
        ESP_LOGW(TAG, "Failed to save alarms");
    }
}

int64_t AlarmTool::GetNowMs() const {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return static_cast<int64_t>(tv.tv_sec) * 1000 + tv.tv_usec / 1000;
}

void AlarmTool::CreateTimerIfNeeded() {
    if (timer_ != nullptr) {
        return;
    }

    esp_timer_create_args_t timer_args = {
        .callback = [](void* arg) {
            static_cast<AlarmTool*>(arg)->OnTimer();
        },
        .arg = this,
        .name = "alarm_timer",
    };

    if (esp_timer_create(&timer_args, &timer_) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create alarm timer");
        timer_ = nullptr;
    }
}

void AlarmTool::StopTimer() {
    if (timer_ == nullptr || !timer_active_) {
        return;
    }
    esp_timer_stop(timer_);
    timer_active_ = false;
}
