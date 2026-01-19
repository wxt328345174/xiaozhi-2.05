#include "mcp_text_invoke_tool.h"

#include <cstdio>

#include "application.h"
#include "device_state_event.h"
#include "mcp_server.h"

namespace {

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

TextInvokeTool& TextInvokeTool::GetInstance() {
    static TextInvokeTool instance;
    return instance;
}

void TextInvokeTool::Initialize() {
    auto& state_manager = DeviceStateEventManager::GetInstance();
    state_manager.RegisterStateChangeCallback([this](DeviceState previous_state, DeviceState current_state) {
        if (previous_state == kDeviceStateSpeaking &&
            (current_state == kDeviceStateIdle || current_state == kDeviceStateListening)) {
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
            Property("max_len", kPropertyTypeInteger, 0),
        }),
        [this](const PropertyList& properties) -> ReturnValue {
            auto text = properties["text"].value<std::string>();
            Options options;
            options.priority = properties["priority"].value<int>();
            options.has_priority = options.priority != 0;
            options.timestamp_ms = static_cast<uint64_t>(properties["timestamp_ms"].value<int>());
            options.has_timestamp = options.timestamp_ms != 0;
            int max_len = properties["max_len"].value<int>();
            if (max_len < 0) {
                max_len = 0;
            }
            options.max_len = static_cast<size_t>(max_len);
            options.has_max_len = options.max_len > 0;

            Submit(text, options);
            return std::string("queued");
        });
}

void TextInvokeTool::Submit(const std::string& text, const Options& options) {
    if (text.empty()) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (options.has_max_len && options.max_len > 0 && text.size() > options.max_len) {
            for (size_t offset = 0; offset < text.size(); offset += options.max_len) {
                queue_.push_back(QueueItem{
                    .text = text.substr(offset, options.max_len),
                    .priority = options.priority,
                    .timestamp_ms = options.timestamp_ms,
                    .has_priority = options.has_priority,
                    .has_timestamp = options.has_timestamp,
                });
            }
        } else {
            queue_.push_back(QueueItem{
                .text = text,
                .priority = options.priority,
                .timestamp_ms = options.timestamp_ms,
                .has_priority = options.has_priority,
                .has_timestamp = options.has_timestamp,
            });
        }
    }

    auto& app = Application::GetInstance();
    ProcessQueue(app.GetDeviceState());
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
        // Clear immediately; we only block while speaking per requirement.
        sending_ = false;
    }
}

void TextInvokeTool::SendViaWakeWordPath(const std::string& text) {
    std::string escaped = EscapeJsonString(text);
    auto& app = Application::GetInstance();
    app.Schedule([escaped]() {
        Application::GetInstance().WakeWordInvoke(escaped);
    });
}
