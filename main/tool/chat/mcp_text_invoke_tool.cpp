#include "mcp_text_invoke_tool.h"

void TextInvokeTool::Submit(const std::string& text, const Options& options) {
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.push_back(QueueItem{
        .text = text,
        .priority = options.priority,
        .timestamp_ms = options.timestamp_ms,
        .has_priority = options.has_priority,
        .has_timestamp = options.has_timestamp,
    });
}

void TextInvokeTool::ProcessQueue(DeviceState current_state) {
    if (current_state == kDeviceStateSpeaking) {
        return;
    }
    if (current_state != kDeviceStateIdle && current_state != kDeviceStateListening) {
        return;
    }

    QueueItem item;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (sending_ || queue_.empty()) {
            return;
        }
        item = std::move(queue_.front());
        queue_.pop_front();
        sending_ = true;
    }

    SendViaWakeWordPath(item.text);

    {
        std::lock_guard<std::mutex> lock(mutex_);
        // Skeleton: clear sending_ immediately; real integration should clear on send completion.
        sending_ = false;
    }
}

void TextInvokeTool::SendViaWakeWordPath(const std::string& text) {
    (void)text;
}
