#ifndef TIME_PARSER_H
#define TIME_PARSER_H

#include <cstdint>
#include <string>

class TimeParser {
public:
    enum class ParseKind {
        kAbsolute,
        kRelative,
        kDayTimeRelative
    };

    struct ParseResult {
        bool ok = false;
        ParseKind kind = ParseKind::kRelative;
        int64_t trigger_ms = 0;
        int error_code = 0;
        std::string error;
        std::string message;
    };

    static ParseResult Parse(const std::string& text, int64_t now_ms);
    static std::string FormatLocalTime(int64_t epoch_ms);
    static bool IsTimeSynced(int64_t now_ms);
};

#endif // TIME_PARSER_H
