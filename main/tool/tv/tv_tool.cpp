#include "tv_tool.h"

#include <cstdio>
#include <sys/time.h>

#include <esp_err.h>
#include <esp_log.h>

#include "tool/chat/mcp_text_invoke_tool.h"

namespace {

static const char* TAG = "TvTool";
static const uint64_t kIntervalUs = 30ULL * 1000000ULL;
static const char* kInvokeText = "[电视画面播报助手] 立刻调用 MCP 工具self.camera.take_photo，并在 question 中写：请用中文解说当前电视画面；若不是电视画面请提示用户把镜头对准屏幕。";

}

TvTool& TvTool::GetInstance() {
    static TvTool instance;
    return instance;
}

void TvTool::Initialize(McpServer* server) {
    if (initialized_) {
        return;
    }
    if (server == nullptr) {
        ESP_LOGE(TAG, "Initialize failed: server is null");
        return;
    }
    initialized_ = true;

    server->AddTool("tv.start",
        "Enter TV scene and start periodic photo narration prompts.",
        PropertyList(),
        [this](const PropertyList& properties) -> ReturnValue {
            return HandleStart(properties);
        });

    server->AddTool("tv.stop",
        "Exit TV scene and stop periodic prompts.",
        PropertyList(),
        [this](const PropertyList& properties) -> ReturnValue {
            return HandleStop(properties);
        });

    server->AddTool("tv.status",
        "Get TV scene status.",
        PropertyList(),
        [this](const PropertyList& properties) -> ReturnValue {
            return HandleStatus(properties);
        });
}

ReturnValue TvTool::HandleStart(const PropertyList& properties) {
    (void)properties;
    bool trigger_now = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (running_) {
            char buffer[128] = {0};
            std::snprintf(buffer, sizeof(buffer),
                "{\"ok\":false,\"running\":true,\"message\":\"already_running\",\"interval_sec\":%u,\"last_trigger_ms\":%llu}",
                static_cast<unsigned>(kIntervalUs / 1000000ULL),
                static_cast<unsigned long long>(last_trigger_ms_));
            return std::string(buffer);
        }

        CreateTimerIfNeeded();
        if (timer_ == nullptr) {
            return std::string("{\"ok\":false,\"running\":false,\"message\":\"timer_create_failed\"}");
        }

        esp_err_t err = esp_timer_start_periodic(timer_, kIntervalUs);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start tv timer: %s", esp_err_to_name(err));
            return std::string("{\"ok\":false,\"running\":false,\"message\":\"timer_start_failed\"}");
        }

        running_ = true;
        trigger_now = true;
    }

    if (trigger_now) {
        OnTimer();
    }

    char buffer[160] = {0};
    std::snprintf(buffer, sizeof(buffer),
        "{\"ok\":true,\"running\":true,\"message\":\"started\",\"interval_sec\":%u,\"last_trigger_ms\":%llu}",
        static_cast<unsigned>(kIntervalUs / 1000000ULL),
        static_cast<unsigned long long>(last_trigger_ms_));
    return std::string(buffer);
}

ReturnValue TvTool::HandleStop(const PropertyList& properties) {
    (void)properties;
    std::lock_guard<std::mutex> lock(mutex_);
    if (!running_) {
        char buffer[128] = {0};
        std::snprintf(buffer, sizeof(buffer),
            "{\"ok\":false,\"running\":false,\"message\":\"already_stopped\",\"interval_sec\":%u,\"last_trigger_ms\":%llu}",
            static_cast<unsigned>(kIntervalUs / 1000000ULL),
            static_cast<unsigned long long>(last_trigger_ms_));
        return std::string(buffer);
    }

    running_ = false;
    StopTimer();

    char buffer[160] = {0};
    std::snprintf(buffer, sizeof(buffer),
        "{\"ok\":true,\"running\":false,\"message\":\"stopped\",\"interval_sec\":%u,\"last_trigger_ms\":%llu}",
        static_cast<unsigned>(kIntervalUs / 1000000ULL),
        static_cast<unsigned long long>(last_trigger_ms_));
    return std::string(buffer);
}

ReturnValue TvTool::HandleStatus(const PropertyList& properties) {
    (void)properties;
    std::lock_guard<std::mutex> lock(mutex_);
    char buffer[160] = {0};
    std::snprintf(buffer, sizeof(buffer),
        "{\"ok\":true,\"running\":%s,\"interval_sec\":%u,\"last_trigger_ms\":%llu}",
        running_ ? "true" : "false",
        static_cast<unsigned>(kIntervalUs / 1000000ULL),
        static_cast<unsigned long long>(last_trigger_ms_));
    return std::string(buffer);
}

void TvTool::OnTimer() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!running_) {
        return;
    }
    last_trigger_ms_ = GetNowMs();
    TextInvokeTool::GetInstance().Submit(kInvokeText);
}

void TvTool::CreateTimerIfNeeded() {
    if (timer_ != nullptr) {
        return;
    }

    esp_timer_create_args_t timer_args = {
        .callback = [](void* arg) {
            static_cast<TvTool*>(arg)->OnTimer();
        },
        .arg = this,
        .name = "tv_timer",
    };

    if (esp_timer_create(&timer_args, &timer_) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create tv timer");
        timer_ = nullptr;
    }
}

void TvTool::StopTimer() {
    if (timer_ == nullptr) {
        return;
    }
    esp_timer_stop(timer_);
}

uint64_t TvTool::GetNowMs() const {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return static_cast<uint64_t>(tv.tv_sec) * 1000ULL + static_cast<uint64_t>(tv.tv_usec) / 1000ULL;
}
