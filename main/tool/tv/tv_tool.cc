#include "tv_tool.h"

#include <esp_log.h>

#include "application.h"
#include "device_state_event.h"

namespace {

const char* TAG = "TvTool";
constexpr int kMinIntervalMs = 3000;
constexpr int kTimeoutMs = 30000;
constexpr int kRetryBusyMs = 1000;

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

}  // namespace

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

    auto& state_manager = DeviceStateEventManager::GetInstance();
    state_manager.RegisterStateChangeCallback([this](DeviceState previous_state, DeviceState current_state) {
        OnStateChanged(previous_state, current_state);
    });
}

std::string TvTool::StartWatching(const PropertyList& properties) {
    Options parsed;
    parsed.interval_ms = properties["interval_ms"].value<int>();
    parsed.utterance = properties["utterance"].value<std::string>();
    parsed.max_chars = properties["max_chars"].value<int>();

    if (parsed.interval_ms < kMinIntervalMs) {
        parsed.interval_ms = kMinIntervalMs;
    }
    if (parsed.max_chars <= 0) {
        parsed.max_chars = 10;
    }
    parsed.utterance = TruncateUtf8(parsed.utterance, parsed.max_chars);

    {
        std::lock_guard<std::mutex> lock(mutex_);
        options_ = parsed;
        running_.store(true, std::memory_order_release);
        in_flight_.store(false, std::memory_order_release);
    }

    StartNextTimerMs(0); // 立即尝试第一轮

    ESP_LOGI(TAG, "TV watch started: interval_ms=%d utterance=%s max_chars=%d",
        parsed.interval_ms, parsed.utterance.c_str(), parsed.max_chars);

    return std::string("started");
}

std::string TvTool::StopWatching() {
    bool was_running = running_.exchange(false, std::memory_order_acq_rel);
    in_flight_.store(false, std::memory_order_release);
    StopAndDeleteTimers();

    if (!was_running) {
        return std::string("not_running");
    }

    ESP_LOGI(TAG, "TV watch stopped");

    return std::string("stopped");
}

void TvTool::StartNextTimerMs(uint64_t delay_ms) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!running_.load(std::memory_order_acquire)) {
        return;
    }
    if (timer_next_ == nullptr) {
        esp_timer_create_args_t timer_args = {
            .callback = [](void* arg) {
                static_cast<TvTool*>(arg)->OnNextTimer();
            },
            .arg = this,
            .name = "tv_next",
        };
        if (esp_timer_create(&timer_args, &timer_next_) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to create next timer");
            return;
        }
    } else {
        esp_timer_stop(timer_next_);
    }
    esp_timer_start_once(timer_next_, delay_ms * 1000ULL);
}

void TvTool::StartTimeoutTimerMs(uint64_t delay_ms) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!running_.load(std::memory_order_acquire)) {
        return;
    }
    if (timer_timeout_ == nullptr) {
        esp_timer_create_args_t timer_args = {
            .callback = [](void* arg) {
                static_cast<TvTool*>(arg)->OnTimeoutTimer();
            },
            .arg = this,
            .name = "tv_timeout",
        };
        if (esp_timer_create(&timer_args, &timer_timeout_) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to create timeout timer");
            return;
        }
    } else {
        esp_timer_stop(timer_timeout_);
    }
    esp_timer_start_once(timer_timeout_, delay_ms * 1000ULL);
}

void TvTool::StopAndDeleteTimers() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (timer_next_ != nullptr) {
        esp_timer_stop(timer_next_);
        esp_timer_delete(timer_next_);
        timer_next_ = nullptr;
    }
    if (timer_timeout_ != nullptr) {
        esp_timer_stop(timer_timeout_);
        esp_timer_delete(timer_timeout_);
        timer_timeout_ = nullptr;
    }
}

void TvTool::OnNextTimer() {
    if (!running_.load(std::memory_order_acquire)) {
        return;
    }
    if (app_ == nullptr) {
        return;
    }
    app_->Schedule([this]() {
        if (!running_.load(std::memory_order_acquire)) {
            return;
        }
        if (in_flight_.load(std::memory_order_acquire)) {
            return;
        }

        auto state = app_->GetDeviceState();
        if (state != kDeviceStateIdle && state != kDeviceStateListening) {
            StartNextTimerMs(kRetryBusyMs);
            return;
        }

        Options snapshot;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            snapshot = options_;
        }

        in_flight_.store(true, std::memory_order_release);
        StartTimeoutTimerMs(kTimeoutMs);

        auto utterance = snapshot.utterance;
        app_->SendWakeWordDetectedText(utterance);
    });
}

void TvTool::OnTimeoutTimer() {
    if (!running_.load(std::memory_order_acquire)) {
        return;
    }

    in_flight_.store(false, std::memory_order_release);

    uint64_t interval_ms = kMinIntervalMs;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        interval_ms = options_.interval_ms;
    }
    ESP_LOGW(TAG, "TV watch timeout hit, scheduling next after %llu ms", (unsigned long long)interval_ms);
    StartNextTimerMs(interval_ms);
}

void TvTool::OnStateChanged(DeviceState previous_state, DeviceState current_state) {
    if (!running_.load(std::memory_order_acquire)) {
        return;
    }

    if (previous_state == kDeviceStateSpeaking &&
        (current_state == kDeviceStateIdle || current_state == kDeviceStateListening)) {
        in_flight_.store(false, std::memory_order_release);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (timer_timeout_ != nullptr) {
                esp_timer_stop(timer_timeout_);
            }
        }

        uint64_t interval_ms = kMinIntervalMs;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            interval_ms = options_.interval_ms;
        }
        StartNextTimerMs(interval_ms);
    }
}
