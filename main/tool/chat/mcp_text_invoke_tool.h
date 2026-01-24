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
        size_t queue_limit = 0;
        bool has_queue_limit = false;
        bool drop_oldest = true;
        bool has_drop_oldest = false;
    };

    void Initialize();
    void Submit(const std::string& text);
    void Submit(const std::string& text, const Options& options);
    void ProcessQueue(DeviceState current_state);

private:
    struct QueueItem {
        std::string text;
        int priority = 0;
        uint64_t timestamp_ms = 0;
        bool has_priority = false;
        bool has_timestamp = false;
    };

    bool SendViaWakeWordPath(const std::string& text);
    void OnAckTimeout();
    void StartAckTimer();
    bool EnqueueItem(const QueueItem& item);

    // Protects queue_ and sending_; use std::mutex since no project-specific lock is available here.
    std::mutex mutex_;
    std::deque<QueueItem> queue_;
    bool sending_ = false;
    bool awaiting_ack_ = false;
    bool initialized_ = false;
    QueueItem current_item_;
    bool has_current_item_ = false;
    int retry_count_ = 0;
    size_t max_queue_size_ = 20;
    bool drop_oldest_ = true;
    uint64_t ack_timeout_ms_ = 8000;
    void* ack_timer_ = nullptr;
};

#endif // MCP_TEXT_INVOKE_TOOL_H
