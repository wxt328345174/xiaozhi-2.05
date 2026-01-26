#ifndef TV_TOOL_H
#define TV_TOOL_H

#include <cstdint>
#include <mutex>
#include <string>

#include <esp_timer.h>

#include "device_state.h"

#include "mcp_server.h"

class TvTool {
public:
    static TvTool& GetInstance();

    void Initialize(McpServer* server);

private:
    TvTool() = default;
    ~TvTool() = default;
    TvTool(const TvTool&) = delete;
    TvTool& operator=(const TvTool&) = delete;

    ReturnValue HandleStart(const PropertyList& properties);
    ReturnValue HandleStop(const PropertyList& properties);
    ReturnValue HandleStatus(const PropertyList& properties);

    void OnTimer();
    void OnDeviceStateChanged(DeviceState previous_state, DeviceState current_state);
    void CreateTimerIfNeeded();
    void StopTimer();
    uint64_t GetNowMs() const;

    enum class TvState {
        kIdle,
        kArmed,
        kWaitSpeaking,
        kWaitListening,
    };

    std::mutex mutex_;
    esp_timer_handle_t timer_ = nullptr;
    bool running_ = false;
    bool initialized_ = false;
    uint64_t last_trigger_ms_ = 0;
    TvState state_ = TvState::kIdle;
};

#endif // TV_TOOL_H
