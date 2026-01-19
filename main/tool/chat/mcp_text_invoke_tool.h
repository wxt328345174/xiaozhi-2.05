#ifndef MCP_TEXT_INVOKE_TOOL_H
#define MCP_TEXT_INVOKE_TOOL_H

#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>

#include "device_state.h"

class TextInvokeTool {
public:
    static TextInvokeTool& GetInstance();

    struct Options {
        int priority = 0;
        bool has_priority = false;
        uint64_t timestamp_ms = 0;
        bool has_timestamp = false;
        size_t max_len = 0;
        bool has_max_len = false;
    };

    void Initialize();
    void Submit(const std::string& text, const Options& options = Options());
    void ProcessQueue(DeviceState current_state);

private:
    struct QueueItem {
        std::string text;
        int priority = 0;
        uint64_t timestamp_ms = 0;
        bool has_priority = false;
        bool has_timestamp = false;
    };

    void SendViaWakeWordPath(const std::string& text);

    // Protects queue_ and sending_; use std::mutex since no project-specific lock is available here.
    std::mutex mutex_;
    std::deque<QueueItem> queue_;
    bool sending_ = false;
};

#endif // MCP_TEXT_INVOKE_TOOL_H
