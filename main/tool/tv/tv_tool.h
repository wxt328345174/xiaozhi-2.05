#ifndef TV_TOOL_H
#define TV_TOOL_H

#include <atomic>
#include <mutex>
#include <string>

#include "esp_timer.h"
#include "device_state.h"
#include "mcp_server.h"

class Application;

class TvTool {
public:
    static TvTool& GetInstance();

    void Initialize(McpServer& mcp_server, Application& application);

private:
    TvTool() = default;
    ~TvTool() = default;
    TvTool(const TvTool&) = delete;
    TvTool& operator=(const TvTool&) = delete;

    struct Options {
        int interval_ms = 5000;
        std::string utterance = "拍照解说";
        int max_chars = 10;
    };

    std::string StartWatching(const PropertyList& properties);
    std::string StopWatching();

    void StartNextTimerMs(uint64_t delay_ms);
    void StartTimeoutTimerMs(uint64_t delay_ms);
    void StopAndDeleteTimers();

    void OnNextTimer();
    void OnTimeoutTimer();
    void OnStateChanged(DeviceState previous_state, DeviceState current_state);

    std::mutex mutex_;
    std::atomic<bool> running_{false};
    std::atomic<bool> in_flight_{false};
    Options options_;
    esp_timer_handle_t timer_next_ = nullptr;
    esp_timer_handle_t timer_timeout_ = nullptr;
    Application* app_ = nullptr;
};

#endif // TV_TOOL_H
