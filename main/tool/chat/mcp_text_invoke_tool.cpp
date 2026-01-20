#include "mcp_text_invoke_tool.h"

#include <cstdio>
#include <vector>

#include "application.h"
#include "device_state_event.h"
#include "mcp_server.h"
#include <esp_log.h>
#include <esp_timer.h>

namespace {

static const char* TAG = "TextInvokeTool";
static const int kMaxSendRetries = 1;

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

std::vector<std::string> SplitUtf8ByChars(const std::string& text, size_t max_chars) {
    std::vector<std::string> chunks;
    if (max_chars == 0 || text.empty()) {
        return chunks;
    }
    size_t offset = 0;
    while (offset < text.size()) {
        size_t count = 0;
        size_t start = offset;
        while (offset < text.size() && count < max_chars) {
            size_t len = Utf8CharLen(static_cast<unsigned char>(text[offset]));
            if (offset + len > text.size()) {
                break;
            }
            offset += len;
            count++;
        }
        if (offset == start) {
            break;
        }
        chunks.push_back(text.substr(start, offset - start));
    }
    return chunks;
}

std::string EscapeJsonString(const std::string& input) {
    std::string out;
    out.reserve(input.size());
    for (unsigned char ch : input) {
        switch (ch) {
            case '\\':
                out += "\\\\";
                break;
            case '\"':
                out += "\\\"";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                if (ch < 0x20) {
                    char buf[7];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", ch);
                    out += buf;
                } else {
                    out.push_back(static_cast<char>(ch));
                }
                break;
        }
    }
    return out;
}

} // namespace

#if CONFIG_TEXT_INVOKE_SELFTEST
void StartTextInvokeSelftest();
#endif

TextInvokeTool& TextInvokeTool::GetInstance() {
    static TextInvokeTool instance;
    return instance;
}

void TextInvokeTool::Initialize() {
    if (initialized_) {
        return;
    }
    initialized_ = true;

    esp_timer_create_args_t timer_args = {
        .callback = [](void* arg) {
            static_cast<TextInvokeTool*>(arg)->OnAckTimeout();
        },
        .arg = this,
        .name = "text_invoke_ack",
    };
    if (esp_timer_create(&timer_args, reinterpret_cast<esp_timer_handle_t*>(&ack_timer_)) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create ack timer");
        ack_timer_ = nullptr;
    }

    auto& state_manager = DeviceStateEventManager::GetInstance();
    state_manager.RegisterStateChangeCallback([this](DeviceState previous_state, DeviceState current_state) {
        if (current_state == kDeviceStateSpeaking) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (sending_ && awaiting_ack_) {
                awaiting_ack_ = false;
                if (ack_timer_ != nullptr) {
                    esp_timer_stop(reinterpret_cast<esp_timer_handle_t>(ack_timer_));
                }
                ESP_LOGI(TAG, "Ack received, speaking started");
            }
        }
        if (previous_state == kDeviceStateSpeaking &&
            (current_state == kDeviceStateIdle || current_state == kDeviceStateListening)) {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (sending_) {
                    sending_ = false;
                    awaiting_ack_ = false;
                    has_current_item_ = false;
                    retry_count_ = 0;
                    if (ack_timer_ != nullptr) {
                        esp_timer_stop(reinterpret_cast<esp_timer_handle_t>(ack_timer_));
                    }
                    ESP_LOGI(TAG, "Speaking ended, ready for next");
                }
            }
            ProcessQueue(current_state);
        }
    });

    auto& mcp_server = McpServer::GetInstance();
    mcp_server.AddTool(
        "self.text_invoke",
        "Invoke LLM->TTS via the wake word path. Use for text-only prompts.",
        PropertyList({
            Property("text", kPropertyTypeString),
            Property("priority", kPropertyTypeInteger, 0),
            Property("timestamp_ms", kPropertyTypeInteger, 0),
            Property("queue_limit", kPropertyTypeInteger, 0),
            Property("drop_oldest", kPropertyTypeBoolean, true),
        }),
        [this](const PropertyList& properties) -> ReturnValue {
            auto text = properties["text"].value<std::string>();
            Options options;
            options.priority = properties["priority"].value<int>();
            options.has_priority = options.priority != 0;
            options.timestamp_ms = static_cast<uint64_t>(properties["timestamp_ms"].value<int>());
            options.has_timestamp = options.timestamp_ms != 0;
            int queue_limit = properties["queue_limit"].value<int>();
            if (queue_limit < 0) {
                queue_limit = 0;
            }
            options.queue_limit = static_cast<size_t>(queue_limit);
            options.has_queue_limit = options.queue_limit > 0;
            options.drop_oldest = properties["drop_oldest"].value<bool>();
            options.has_drop_oldest = true;

            Submit(text, options);
            return std::string("queued");
        });

#if CONFIG_TEXT_INVOKE_SELFTEST
    StartTextInvokeSelftest();
#endif
}

void TextInvokeTool::Submit(const std::string& text) {
    Submit(text, Options{});
}

void TextInvokeTool::Submit(const std::string& text, const Options& options) {
    if (text.empty()) {
        ESP_LOGW(TAG, "Submit ignored: empty text");
        return;
    }

    if (options.has_queue_limit) {
        std::lock_guard<std::mutex> lock(mutex_);
        max_queue_size_ = options.queue_limit;
    }
    if (options.has_drop_oldest) {
        std::lock_guard<std::mutex> lock(mutex_);
        drop_oldest_ = options.drop_oldest;
    }

    ESP_LOGI(TAG, "Submit text len=%u", static_cast<unsigned>(text.size()));

    bool enqueued_any = false;
    QueueItem item{
        .text = text,
        .priority = options.priority,
        .timestamp_ms = options.timestamp_ms,
        .has_priority = options.has_priority,
        .has_timestamp = options.has_timestamp,
    };
    enqueued_any |= EnqueueItem(item);

    if (!enqueued_any) {
        ESP_LOGW(TAG, "Submit dropped: queue policy rejected all items");
        return;
    }

    auto& app = Application::GetInstance();
    ProcessQueue(app.GetDeviceState());
}

void TextInvokeTool::ProcessQueue(DeviceState current_state) {
    if (current_state == kDeviceStateSpeaking) {
        ESP_LOGI(TAG, "ProcessQueue deferred: speaking");
        return;
    }
    if (current_state != kDeviceStateIdle && current_state != kDeviceStateListening) {
        ESP_LOGI(TAG, "ProcessQueue deferred: state=%d", static_cast<int>(current_state));
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
        awaiting_ack_ = true;
        current_item_ = item;
        has_current_item_ = true;
        retry_count_ = 0;
        ESP_LOGI(TAG, "Dequeue and send: queue_size=%u", static_cast<unsigned>(queue_.size()));
    }

    if (!SendViaWakeWordPath(item.text)) {
        ESP_LOGE(TAG, "Send failed immediately");
        OnAckTimeout();
        return;
    }
    StartAckTimer();

    {
        std::lock_guard<std::mutex> lock(mutex_);
        // Hold sending_ until speaking ends or ack timeout triggers a drop.
    }
}

bool TextInvokeTool::SendViaWakeWordPath(const std::string& text) {
    if (text.empty()) {
        return false;
    }
    std::string escaped = EscapeJsonString(text);
    auto& app = Application::GetInstance();
    app.Schedule([escaped]() {
        Application::GetInstance().SendWakeWordDetectedText(escaped);
    });
    ESP_LOGI(TAG, "SendViaWakeWordPath scheduled, len=%u", static_cast<unsigned>(text.size()));
    return true;
}

void TextInvokeTool::OnAckTimeout() {
    QueueItem retry_item;
    bool should_retry = false;
    bool should_drop = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!sending_ || !awaiting_ack_ || !has_current_item_) {
            return;
        }
        if (retry_count_ < kMaxSendRetries) {
            retry_count_++;
            should_retry = true;
            retry_item = current_item_;
        } else {
            should_drop = true;
            sending_ = false;
            awaiting_ack_ = false;
            has_current_item_ = false;
        }
    }

    if (should_retry) {
        ESP_LOGW(TAG, "Ack timeout, retry %d", retry_count_);
        if (!SendViaWakeWordPath(retry_item.text)) {
            ESP_LOGE(TAG, "Retry send failed, dropping");
            std::lock_guard<std::mutex> lock(mutex_);
            sending_ = false;
            awaiting_ack_ = false;
            has_current_item_ = false;
        } else {
            StartAckTimer();
        }
        return;
    }

    if (should_drop) {
        ESP_LOGE(TAG, "Ack timeout, drop item");
        auto& app = Application::GetInstance();
        ProcessQueue(app.GetDeviceState());
    }
}

void TextInvokeTool::StartAckTimer() {
    if (ack_timer_ == nullptr) {
        return;
    }
    esp_timer_stop(reinterpret_cast<esp_timer_handle_t>(ack_timer_));
    esp_timer_start_once(reinterpret_cast<esp_timer_handle_t>(ack_timer_),
        static_cast<uint64_t>(ack_timeout_ms_) * 1000ULL);
}

bool TextInvokeTool::EnqueueItem(const QueueItem& item) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (max_queue_size_ > 0 && queue_.size() >= max_queue_size_) {
        if (drop_oldest_) {
            size_t dropped = 0;
            while (!queue_.empty() && queue_.size() >= max_queue_size_) {
                queue_.pop_front();
                dropped++;
            }
            ESP_LOGW(TAG, "Queue full, dropped %u oldest", static_cast<unsigned>(dropped));
        } else {
            ESP_LOGW(TAG, "Queue full, reject new item");
            return false;
        }
    }
    queue_.push_back(item);
    ESP_LOGI(TAG, "Enqueue item, queue_size=%u", static_cast<unsigned>(queue_.size()));
    return true;
}
