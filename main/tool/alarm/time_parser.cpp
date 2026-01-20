#include "time_parser.h"

#include <cctype>
#include <cstdio>
#include <ctime>
#include <cstring>
#include <vector>

#include <esp_log.h>

namespace {

static const int64_t kMinValidEpochSec = 1609459200; // 2021-01-01
static const char* TAG = "TimeParser";
static const int kErrEmpty = 1;
static const int kErrInvalid = 2;
static const int kErrUnsupported = 3;

std::string RemoveSpaces(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (unsigned char ch : text) {
        if (std::isspace(ch)) {
            continue;
        }
        out.push_back(static_cast<char>(ch));
    }
    return out;
}

void ReplaceAll(std::string& text, const std::string& from, const std::string& to) {
    if (from.empty()) {
        return;
    }
    size_t start = 0;
    while ((start = text.find(from, start)) != std::string::npos) {
        text.replace(start, from.size(), to);
        start += to.size();
    }
}

bool StartsWith(const std::string& text, const std::string& prefix) {
    if (prefix.empty() || text.size() < prefix.size()) {
        return false;
    }
    return text.compare(0, prefix.size(), prefix) == 0;
}

void ToLowerAscii(std::string& text) {
    for (auto& ch : text) {
        if (ch >= 'A' && ch <= 'Z') {
            ch = static_cast<char>(ch - 'A' + 'a');
        }
    }
}

std::string NormalizeEnglishUnits(const std::string& text) {
    std::string out = text;
    ToLowerAscii(out);

    if (StartsWith(out, "after")) {
        out.erase(0, std::strlen("after"));
    } else if (StartsWith(out, "in")) {
        out.erase(0, std::strlen("in"));
    }

    ReplaceAll(out, "seconds", "秒");
    ReplaceAll(out, "second", "秒");
    ReplaceAll(out, "secs", "秒");
    ReplaceAll(out, "sec", "秒");

    ReplaceAll(out, "minutes", "分钟");
    ReplaceAll(out, "minute", "分钟");
    ReplaceAll(out, "mins", "分钟");
    ReplaceAll(out, "min", "分钟");

    ReplaceAll(out, "hours", "小时");
    ReplaceAll(out, "hour", "小时");
    ReplaceAll(out, "hrs", "小时");
    ReplaceAll(out, "hr", "小时");

    ReplaceAll(out, "days", "天");
    ReplaceAll(out, "day", "天");

    return out;
}

std::string NormalizeShortUnits(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    size_t i = 0;
    while (i < text.size()) {
        if (!std::isdigit(static_cast<unsigned char>(text[i]))) {
            out.push_back(text[i]);
            ++i;
            continue;
        }
        size_t start = i;
        while (i < text.size() && std::isdigit(static_cast<unsigned char>(text[i]))) {
            ++i;
        }
        std::string number = text.substr(start, i - start);
        size_t unit_start = i;
        while (unit_start < text.size() && std::isspace(static_cast<unsigned char>(text[unit_start]))) {
            ++unit_start;
        }
        std::string unit;
        size_t unit_len = 0;
        if (unit_start < text.size()) {
            if (text.compare(unit_start, 4, "secs") == 0) {
                unit = "秒";
                unit_len = 4;
            } else if (text.compare(unit_start, 3, "sec") == 0) {
                unit = "秒";
                unit_len = 3;
            } else if (text.compare(unit_start, 1, "s") == 0) {
                unit = "秒";
                unit_len = 1;
            } else if (text.compare(unit_start, 4, "mins") == 0) {
                unit = "分钟";
                unit_len = 4;
            } else if (text.compare(unit_start, 3, "min") == 0) {
                unit = "分钟";
                unit_len = 3;
            } else if (text.compare(unit_start, 1, "m") == 0) {
                unit = "分钟";
                unit_len = 1;
            } else if (text.compare(unit_start, 3, "hrs") == 0) {
                unit = "小时";
                unit_len = 3;
            } else if (text.compare(unit_start, 2, "hr") == 0) {
                unit = "小时";
                unit_len = 2;
            } else if (text.compare(unit_start, 1, "h") == 0) {
                unit = "小时";
                unit_len = 1;
            } else if (text.compare(unit_start, 1, "d") == 0) {
                unit = "天";
                unit_len = 1;
            }
        }

        if (!unit.empty()) {
            out += number;
            out += unit;
            i = unit_start + unit_len;
        } else {
            out += number;
            i = unit_start;
        }
    }
    return out;
}

void LogEnglishNormalizeSamples() {
    const std::vector<std::string> samples = {
        "10 seconds",
        "in 5 minutes",
        "2 hours 15 minutes",
        "after 1 day",
        "10s",
        "in 5m",
        "2h15m",
        "after 1d",
    };
    for (const auto& sample : samples) {
        std::string normalized = NormalizeShortUnits(NormalizeEnglishUnits(RemoveSpaces(sample)));
        ESP_LOGD(TAG, "English normalize sample: '%s' -> '%s'", sample.c_str(), normalized.c_str());
    }
}

bool ContainsAny(const std::string& text, const std::vector<std::string>& tokens) {
    for (const auto& token : tokens) {
        if (text.find(token) != std::string::npos) {
            return true;
        }
    }
    return false;
}

bool ValidateTimeParts(int hour, int minute) {
    return hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59;
}

bool ValidateDateParts(int year, int month, int day) {
    return year >= 1970 && month >= 1 && month <= 12 && day >= 1 && day <= 31;
}

bool ToEpochMs(int year, int month, int day, int hour, int minute, int second, int64_t* epoch_ms) {
    if (!ValidateDateParts(year, month, day) || !ValidateTimeParts(hour, minute)) {
        return false;
    }
    std::tm tm_value = {};
    tm_value.tm_year = year - 1900;
    tm_value.tm_mon = month - 1;
    tm_value.tm_mday = day;
    tm_value.tm_hour = hour;
    tm_value.tm_min = minute;
    tm_value.tm_sec = second;
    tm_value.tm_isdst = -1;
    time_t epoch = mktime(&tm_value);
    if (epoch < 0) {
        return false;
    }
    std::tm verify = {};
    if (localtime_r(&epoch, &verify) == nullptr) {
        return false;
    }
    if (verify.tm_year != tm_value.tm_year || verify.tm_mon != tm_value.tm_mon || verify.tm_mday != tm_value.tm_mday) {
        return false;
    }
    *epoch_ms = static_cast<int64_t>(epoch) * 1000;
    return true;
}

enum Period {
    kPeriodNone,
    kPeriodMorning,
    kPeriodNoon,
    kPeriodAfternoon,
    kPeriodEvening
};

Period ExtractPeriod(std::string& text) {
    if (text.find("早上") != std::string::npos || text.find("上午") != std::string::npos) {
        ReplaceAll(text, "早上", "");
        ReplaceAll(text, "上午", "");
        return kPeriodMorning;
    }
    if (text.find("中午") != std::string::npos) {
        ReplaceAll(text, "中午", "");
        return kPeriodNoon;
    }
    if (text.find("下午") != std::string::npos) {
        ReplaceAll(text, "下午", "");
        return kPeriodAfternoon;
    }
    if (text.find("晚上") != std::string::npos) {
        ReplaceAll(text, "晚上", "");
        return kPeriodEvening;
    }
    return kPeriodNone;
}

bool ParseTimePart(std::string text, int& hour, int& minute, Period* period) {
    if (period != nullptr) {
        *period = ExtractPeriod(text);
    } else {
        ExtractPeriod(text);
    }

    if (std::sscanf(text.c_str(), "%d:%d", &hour, &minute) == 2) {
        return ValidateTimeParts(hour, minute);
    }

    size_t dot_pos = text.find("点");
    if (dot_pos == std::string::npos) {
        dot_pos = text.find("时");
    }
    if (dot_pos == std::string::npos) {
        return false;
    }

    try {
        hour = std::stoi(text.substr(0, dot_pos));
    } catch (...) {
        return false;
    }

    std::string rest = text.substr(dot_pos + 1);
    if (rest.empty()) {
        minute = 0;
        return ValidateTimeParts(hour, minute);
    }
    if (rest.find("半") != std::string::npos) {
        minute = 30;
        return ValidateTimeParts(hour, minute);
    }
    size_t minute_pos = rest.find("分");
    std::string minute_text = rest;
    if (minute_pos != std::string::npos) {
        minute_text = rest.substr(0, minute_pos);
    }
    if (minute_text.empty()) {
        minute = 0;
        return ValidateTimeParts(hour, minute);
    }
    try {
        minute = std::stoi(minute_text);
    } catch (...) {
        return false;
    }
    return ValidateTimeParts(hour, minute);
}

bool ParseDateFromString(const std::string& text, int& year, int& month, int& day, size_t& end_pos, bool& has_year) {
    size_t i = 0;
    auto read_number = [&text, &i](int& out) -> bool {
        if (i >= text.size() || !std::isdigit(static_cast<unsigned char>(text[i]))) {
            return false;
        }
        size_t start = i;
        while (i < text.size() && std::isdigit(static_cast<unsigned char>(text[i]))) {
            ++i;
        }
        try {
            out = std::stoi(text.substr(start, i - start));
        } catch (...) {
            return false;
        }
        return true;
    };

    int first = 0;
    if (!read_number(first)) {
        return false;
    }
    if (i >= text.size()) {
        return false;
    }

    if (text.compare(i, std::strlen("年"), "年") == 0 || text[i] == '-') {
        has_year = true;
        year = first;
        if (text[i] == '-') {
            ++i;
        } else {
            i += std::strlen("年");
        }
        if (!read_number(month)) {
            return false;
        }
        if (i >= text.size()) {
            return false;
        }
        if (text.compare(i, std::strlen("月"), "月") == 0) {
            i += std::strlen("月");
        } else if (text[i] == '-') {
            ++i;
        } else {
            return false;
        }
        if (!read_number(day)) {
            return false;
        }
        if (i < text.size() && text.compare(i, std::strlen("日"), "日") == 0) {
            i += std::strlen("日");
        }
        end_pos = i;
        return true;
    }

    if (text.compare(i, std::strlen("月"), "月") == 0) {
        has_year = false;
        month = first;
        i += std::strlen("月");
        if (!read_number(day)) {
            return false;
        }
        if (i < text.size() && text.compare(i, std::strlen("日"), "日") == 0) {
            i += std::strlen("日");
        }
        end_pos = i;
        return true;
    }

    return false;
}

bool ParseDayTimeOffset(const std::string& text, int& days, int& hour, int& minute, Period* period) {
    size_t day_pos = text.find("天");
    if (day_pos == std::string::npos) {
        return false;
    }
    if (day_pos == 0) {
        return false;
    }
    try {
        days = std::stoi(text.substr(0, day_pos));
    } catch (...) {
        return false;
    }
    std::string time_part = text.substr(day_pos + std::strlen("天"));
    if (time_part.empty()) {
        return false;
    }
    return ParseTimePart(time_part, hour, minute, period);
}

bool ParseDurationSeconds(const std::string& text, int64_t& seconds) {
    struct UnitSpec {
        const char* unit;
        int64_t multiplier;
    };
    static const UnitSpec kUnits[] = {
        {"天", 86400},
        {"小时", 3600},
        {"分钟", 60},
        {"分", 60},
        {"秒", 1},
    };

    bool matched = false;
    seconds = 0;
    size_t i = 0;
    while (i < text.size()) {
        if (!std::isdigit(static_cast<unsigned char>(text[i]))) {
            ++i;
            continue;
        }
        size_t start = i;
        while (i < text.size() && std::isdigit(static_cast<unsigned char>(text[i]))) {
            ++i;
        }
        int64_t value = 0;
        try {
            value = std::stoll(text.substr(start, i - start));
        } catch (...) {
            continue;
        }

        bool unit_found = false;
        for (const auto& unit : kUnits) {
            size_t unit_len = std::strlen(unit.unit);
            if (text.compare(i, unit_len, unit.unit) == 0) {
                seconds += value * unit.multiplier;
                i += unit_len;
                matched = true;
                unit_found = true;
                break;
            }
        }
        if (!unit_found) {
            ++i;
        }
    }

    return matched;
}

bool EndsWith(const std::string& text, const std::string& suffix) {
    if (suffix.empty() || text.size() < suffix.size()) {
        return false;
    }
    return text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

void ApplyPeriodAdjust(Period period, int& hour) {
    if (period == kPeriodAfternoon || period == kPeriodEvening) {
        if (hour < 12) {
            hour += 12;
        }
    } else if (period == kPeriodNoon) {
        if (hour == 0) {
            hour = 12;
        } else if (hour < 11) {
            hour += 12;
        }
    }
}

std::string BuildSupportMessage() {
    return "支持格式: 10分钟后/2小时后/1天2小时15分钟后; "
           "3月2日7点30分/2026年03月02日07:30/3月2日早上7点半; "
           "7:30/7点半/7点30; 2天9点30分; 每天7:30/每日晚上9点";
}

} // namespace

TimeParser::ParseResult TimeParser::Parse(const std::string& text, int64_t now_ms) {
    ParseResult result;
    if (text.empty()) {
        result.error_code = kErrEmpty;
        result.error = "time_text is empty";
        result.message = "time_text 为空. " + BuildSupportMessage();
        return result;
    }

    static bool g_samples_logged = false;
    if (!g_samples_logged) {
        g_samples_logged = true;
        LogEnglishNormalizeSamples();
    }

    std::string normalized = RemoveSpaces(text);
    ReplaceAll(normalized, "：", ":");
    ReplaceAll(normalized, "号", "日");
    ReplaceAll(normalized, "之后", "后");
    ReplaceAll(normalized, "以后", "后");
    normalized = NormalizeEnglishUnits(normalized);
    normalized = NormalizeShortUnits(normalized);

    ESP_LOGI(TAG, "Parse time_text raw='%s'", text.c_str());
    ESP_LOGI(TAG, "Parse time_text normalized='%s'", normalized.c_str());

    bool is_daily = false;
    if (normalized.find("每天") != std::string::npos || normalized.find("每日") != std::string::npos) {
        is_daily = true;
        ReplaceAll(normalized, "每天", "");
        ReplaceAll(normalized, "每日", "");
        ESP_LOGI(TAG, "Parse branch: daily keyword");
    }

    if (normalized.find("多久后") != std::string::npos) {
        result.error_code = kErrInvalid;
        result.error = "missing duration";
        result.message = "缺少具体时长. " + BuildSupportMessage();
        return result;
    }

    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    Period period = kPeriodNone;

    if (!is_daily && normalized.find("天") != std::string::npos && ContainsAny(normalized, {"点", "时", ":"})) {
        int days = 0;
        if (ParseDayTimeOffset(normalized, days, hour, minute, &period)) {
            ApplyPeriodAdjust(period, hour);
            if (days < 0 || !ValidateTimeParts(hour, minute)) {
                result.error_code = kErrInvalid;
                result.error = "invalid day/time";
                result.message = "日期/时间无效. " + BuildSupportMessage();
                return result;
            }
            time_t now_sec = static_cast<time_t>(now_ms / 1000);
            std::tm target_tm = {};
            if (localtime_r(&now_sec, &target_tm) == nullptr) {
                result.error_code = kErrInvalid;
                result.error = "failed to read local time";
                result.message = "读取本地时间失败. " + BuildSupportMessage();
                return result;
            }
            target_tm.tm_mday += days;
            target_tm.tm_hour = hour;
            target_tm.tm_min = minute;
            target_tm.tm_sec = 0;
            target_tm.tm_isdst = -1;
            time_t target_epoch = mktime(&target_tm);
            if (target_epoch < 0) {
                result.error_code = kErrInvalid;
                result.error = "invalid date/time";
                result.message = "日期/时间无效. " + BuildSupportMessage();
                return result;
            }
            int64_t epoch_ms = static_cast<int64_t>(target_epoch) * 1000;
            if (days == 0 && epoch_ms <= now_ms) {
                target_tm.tm_mday += 1;
                target_epoch = mktime(&target_tm);
                if (target_epoch < 0) {
                    result.error_code = kErrInvalid;
                    result.error = "invalid date/time";
                    result.message = "日期/时间无效. " + BuildSupportMessage();
                    return result;
                }
                epoch_ms = static_cast<int64_t>(target_epoch) * 1000;
            }
            result.ok = true;
            result.kind = ParseKind::kDayTimeRelative;
            result.trigger_ms = epoch_ms;
            ESP_LOGI(TAG, "Parse branch: day_offset_time");
            return result;
        }
    }

    size_t date_end = 0;
    bool has_year = false;
    if (!is_daily && ParseDateFromString(normalized, year, month, day, date_end, has_year)) {
        std::string time_part = normalized.substr(date_end);
        if (time_part.empty()) {
            result.error_code = kErrInvalid;
            result.error = "missing time part";
            result.message = "缺少时间部分. " + BuildSupportMessage();
            return result;
        }
        if (!ParseTimePart(time_part, hour, minute, &period)) {
            result.error_code = kErrInvalid;
            result.error = "invalid time part";
            result.message = "时间部分解析失败. " + BuildSupportMessage();
            return result;
        }
        ApplyPeriodAdjust(period, hour);
        if (!ValidateTimeParts(hour, minute)) {
            result.error_code = kErrInvalid;
            result.error = "invalid time";
            result.message = "时间无效. " + BuildSupportMessage();
            return result;
        }
        int64_t epoch_ms = 0;
        if (!ToEpochMs(year, month, day, hour, minute, 0, &epoch_ms)) {
            result.error_code = kErrInvalid;
            result.error = "invalid date/time";
            result.message = "日期/时间无效. " + BuildSupportMessage();
            return result;
        }
        if (!has_year && epoch_ms <= now_ms) {
            if (!ToEpochMs(year + 1, month, day, hour, minute, 0, &epoch_ms)) {
                result.error_code = kErrInvalid;
                result.error = "invalid date/time";
                result.message = "日期/时间无效. " + BuildSupportMessage();
                return result;
            }
        }
        result.ok = true;
        result.kind = ParseKind::kAbsolute;
        result.trigger_ms = epoch_ms;
        ESP_LOGI(TAG, "Parse branch: absolute(date + time)");
        return result;
    }

    if (!ContainsAny(normalized, {"年", "月", "日"}) && ContainsAny(normalized, {"点", "时", ":", "半"})) {
        std::string time_part = normalized;
        if (!ParseTimePart(time_part, hour, minute, &period)) {
            result.error_code = kErrInvalid;
            result.error = "invalid time part";
            result.message = "时间部分解析失败. " + BuildSupportMessage();
            return result;
        }
        ApplyPeriodAdjust(period, hour);
        result.hour = hour;
        result.minute = minute;
        if (is_daily) {
            result.ok = true;
            result.kind = ParseKind::kAbsolute;
            result.is_daily = true;
            ESP_LOGI(TAG, "Parse branch: daily time_only");
            return result;
        }
        time_t now_sec = static_cast<time_t>(now_ms / 1000);
        std::tm target_tm = {};
        if (localtime_r(&now_sec, &target_tm) == nullptr) {
            result.error_code = kErrInvalid;
            result.error = "failed to read local time";
            result.message = "读取本地时间失败. " + BuildSupportMessage();
            return result;
        }
        target_tm.tm_hour = hour;
        target_tm.tm_min = minute;
        target_tm.tm_sec = 0;
        target_tm.tm_isdst = -1;
        time_t target_epoch = mktime(&target_tm);
        if (target_epoch < 0) {
            result.error_code = kErrInvalid;
            result.error = "invalid time";
            result.message = "时间无效. " + BuildSupportMessage();
            return result;
        }
        int64_t epoch_ms = static_cast<int64_t>(target_epoch) * 1000;
        if (epoch_ms <= now_ms) {
            target_tm.tm_mday += 1;
            target_epoch = mktime(&target_tm);
            if (target_epoch < 0) {
                result.error_code = kErrInvalid;
                result.error = "invalid time";
                result.message = "时间无效. " + BuildSupportMessage();
                return result;
            }
            epoch_ms = static_cast<int64_t>(target_epoch) * 1000;
        }
        result.ok = true;
        result.kind = ParseKind::kAbsolute;
        result.trigger_ms = epoch_ms;
        ESP_LOGI(TAG, "Parse branch: time_only");
        return result;
    }

    std::string duration_text = normalized;
    if (EndsWith(duration_text, "后")) {
        duration_text.erase(duration_text.size() - std::strlen("后"));
    }
    if (!is_daily && !ContainsAny(duration_text, {"年", "月", "日", "点", "时", ":"})) {
        int64_t duration_sec = 0;
        if (ParseDurationSeconds(duration_text, duration_sec) && duration_sec > 0) {
            result.ok = true;
            result.kind = ParseKind::kRelative;
            result.trigger_ms = now_ms + duration_sec * 1000;
            ESP_LOGI(TAG, "Parse branch: relative(duration)");
            return result;
        }
    }

    if (is_daily) {
        result.error_code = kErrInvalid;
        result.error = "invalid daily time";
        result.message = "每日闹钟需要时间格式，如 每天7:30/每日晚上9点. " + BuildSupportMessage();
        return result;
    }

    result.error_code = kErrUnsupported;
    result.error = "unsupported time format";
    result.message = "时间格式不支持. " + BuildSupportMessage();
    return result;
}

std::string TimeParser::FormatLocalTime(int64_t epoch_ms) {
    time_t seconds = static_cast<time_t>(epoch_ms / 1000);
    std::tm tm_value = {};
    if (localtime_r(&seconds, &tm_value) == nullptr) {
        return "";
    }
    char buffer[32] = {0};
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm_value);
    return std::string(buffer);
}

bool TimeParser::IsTimeSynced(int64_t now_ms) {
    return (now_ms / 1000) >= kMinValidEpochSec;
}
