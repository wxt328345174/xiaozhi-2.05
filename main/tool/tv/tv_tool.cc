#include "tv_tool.h"

#include <esp_log.h>
#include <vector>

#include "application.h"

namespace {

const char* TAG = "TvTool";

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

std::string TruncateUtf8(const std::string& text, int max_chars) {
    if (max_chars <= 0 || text.empty()) {
        return "";
    }
    size_t offset = 0;
    int count = 0;
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

}

TvTool& TvTool::GetInstance() {
    static TvTool instance;
    return instance;
}

void TvTool::Initialize(McpServer& mcp_server, Application& application) {
    app_ = &application;

    mcp_server.AddTool(
        "self.tv.watch",
        "进入电视模式，定期拍照并进行解说。参数：interval_ms 拍照间隔（毫秒），utterance 提示词，max_chars 问题截断长度。",
        PropertyList({
            Property("interval_ms", kPropertyTypeInteger, 5000),
            Property("utterance", kPropertyTypeString, std::string("拍照解说")),
            Property("max_chars", kPropertyTypeInteger, 10),
        }),
        [this](const PropertyList& properties) -> ReturnValue {
            return StartWatching(properties);
        });

    mcp_server.AddTool(
        "self.tv.stop",
        "退出电视模式，停止定时拍照。",
        PropertyList(),
        [this](const PropertyList& /*properties*/) -> ReturnValue {
            return StopWatching();
        });
}

std::string TvTool::StartWatching(const PropertyList& properties) {
    Options parsed;
    parsed.interval_ms = properties["interval_ms"].value<int>();
    parsed.utterance = properties["utterance"].value<std::string>();
    parsed.max_chars = properties["max_chars"].value<int>();

    if (parsed.interval_ms < 3000) {
        parsed.interval_ms = 3000;
    }
    if (parsed.max_chars <= 0) {
        parsed.max_chars = 10;
    }
    parsed.utterance = TruncateUtf8(parsed.utterance, parsed.max_chars);

    {
        std::lock_guard<std::mutex> lock(mutex_);
        options_ = parsed;

        if (timer_ == nullptr) {
            esp_timer_create_args_t timer_args = {
                .callback = [](void* arg) {
                    auto self = static_cast<TvTool*>(arg);
                    if (!self->running_.load(std::memory_order_acquire)) {
                        return;
                    }
                    Options snapshot;
                    {
                        std::lock_guard<std::mutex> lock(self->mutex_);
                        snapshot = self->options_;
                    }
                    if (self->app_ == nullptr) {
                        return;
                    }
                    auto utterance = snapshot.utterance; // copy for lambda capture
                    self->app_->Schedule([utterance]() {
                        Application::GetInstance().SendWakeWordDetectedText(utterance);
                    });
                },
                .arg = this,
                .name = "tv_watch_timer",
            };

            if (esp_timer_create(&timer_args, &timer_) != ESP_OK) {
                ESP_LOGE(TAG, "Failed to create timer");
                timer_ = nullptr;
                return std::string("not_running");
            }
        } else {
            esp_timer_stop(timer_);
        }

        if (esp_timer_start_periodic(timer_, static_cast<uint64_t>(options_.interval_ms) * 1000ULL) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start timer");
            running_.store(false, std::memory_order_release);
            return std::string("not_running");
        }

        running_.store(true, std::memory_order_release);
    }

    ESP_LOGI(TAG, "TV watch started: interval_ms=%d utterance=%s max_chars=%d",
        parsed.interval_ms, parsed.utterance.c_str(), parsed.max_chars);

    return std::string("started");
}

std::string TvTool::StopWatching() {
    bool was_running = running_.exchange(false, std::memory_order_acq_rel);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (timer_ != nullptr) {
            esp_timer_stop(timer_);
            esp_timer_delete(timer_);
            timer_ = nullptr;
        }
    }

    if (!was_running) {
        return std::string("not_running");
    }

    ESP_LOGI(TAG, "TV watch stopped");

    return std::string("stopped");
}
