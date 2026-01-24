#include "mcp_text_invoke_tool.h"

#include <string>

#include <esp_err.h>
#include <esp_log.h>
#include <esp_timer.h>

namespace {

static const char* TAG = "TextInvokeSelftest";
static esp_timer_handle_t g_timer = nullptr;
static bool g_started = false;

void OnSelftestTimer(void* arg) {
    (void)arg;
    ESP_LOGI(TAG, "Selftest trigger: submit two messages");
    TextInvokeTool::Options options;
    TextInvokeTool::GetInstance().Submit("自检：TTS 短消息验证立即播报", options);

    std::string long_text =
        "自检：长消息验证无长度限制和排队。"
        "这是用于测试的长文本，没有实际业务含义，只是为了占用大约 600+ 字符的空间，"
        "如果你听到这一段说明长消息已经正常通过 wake word 通道并触发了 TTS。";

    TextInvokeTool::GetInstance().Submit(long_text, options);
}

} // namespace

void StartTextInvokeSelftest() {
#if CONFIG_TEXT_INVOKE_SELFTEST
    if (g_started) {
        return;
    }
    g_started = true;

    esp_timer_create_args_t timer_args = {
        .callback = &OnSelftestTimer,
        .arg = nullptr,
        .name = "text_invoke_selftest",
    };

    esp_err_t err = esp_timer_create(&timer_args, &g_timer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create selftest timer: %s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "Selftest scheduled in 10 seconds");
    esp_timer_start_once(g_timer, 10 * 1000000LL);
#endif
}
