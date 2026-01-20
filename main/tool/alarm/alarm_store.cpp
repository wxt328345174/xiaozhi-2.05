#include "alarm_store.h"

#include <algorithm>

#include <cJSON.h>
#include <esp_log.h>

#include "settings.h"

namespace {

static const char* TAG = "AlarmStore";
static const char* kNamespace = "alarm";
static const char* kKeyAlarms = "alarms";

}

bool AlarmStore::Load(std::map<uint32_t, AlarmRecord>& alarms, uint32_t& next_id) {
    Settings settings(kNamespace, false);
    std::string payload = settings.GetString(kKeyAlarms, "");
    if (payload.empty()) {
        next_id = 1;
        return true;
    }

    cJSON* root = cJSON_Parse(payload.c_str());
    if (root == nullptr || !cJSON_IsArray(root)) {
        ESP_LOGE(TAG, "Invalid alarm payload in NVS");
        if (root != nullptr) {
            cJSON_Delete(root);
        }
        return false;
    }

    uint32_t max_id = 0;
    alarms.clear();
    cJSON* item = nullptr;
    cJSON_ArrayForEach(item, root) {
        if (!cJSON_IsObject(item)) {
            continue;
        }
        cJSON* id = cJSON_GetObjectItem(item, "id");
        cJSON* trigger_ms = cJSON_GetObjectItem(item, "trigger_ms");
        cJSON* label = cJSON_GetObjectItem(item, "label");
        cJSON* is_daily = cJSON_GetObjectItem(item, "is_daily");
        cJSON* hour = cJSON_GetObjectItem(item, "hour");
        cJSON* minute = cJSON_GetObjectItem(item, "minute");
        if (!cJSON_IsNumber(id) || !cJSON_IsNumber(trigger_ms)) {
            continue;
        }
        AlarmRecord record;
        record.id = static_cast<uint32_t>(id->valueint);
        record.trigger_ms = static_cast<int64_t>(trigger_ms->valuedouble);
        if (cJSON_IsString(label)) {
            record.label = label->valuestring;
        }
        if (cJSON_IsBool(is_daily)) {
            record.is_daily = is_daily->valueint == 1;
        }
        if (cJSON_IsNumber(hour)) {
            record.hour = hour->valueint;
        }
        if (cJSON_IsNumber(minute)) {
            record.minute = minute->valueint;
        }
        if (record.id == 0) {
            continue;
        }
        alarms[record.id] = record;
        max_id = std::max(max_id, record.id);
    }

    cJSON_Delete(root);
    next_id = max_id + 1;
    return true;
}

bool AlarmStore::Save(const std::map<uint32_t, AlarmRecord>& alarms) {
    Settings settings(kNamespace, true);
    if (alarms.empty()) {
        settings.EraseKey(kKeyAlarms);
        return true;
    }

    cJSON* root = cJSON_CreateArray();
    for (const auto& entry : alarms) {
        const AlarmRecord& record = entry.second;
        cJSON* item = cJSON_CreateObject();
        cJSON_AddNumberToObject(item, "id", static_cast<double>(record.id));
        cJSON_AddNumberToObject(item, "trigger_ms", static_cast<double>(record.trigger_ms));
        if (!record.label.empty()) {
            cJSON_AddStringToObject(item, "label", record.label.c_str());
        }
        if (record.is_daily) {
            cJSON_AddBoolToObject(item, "is_daily", true);
            cJSON_AddNumberToObject(item, "hour", record.hour);
            cJSON_AddNumberToObject(item, "minute", record.minute);
        }
        cJSON_AddItemToArray(root, item);
    }

    char* json_str = cJSON_PrintUnformatted(root);
    if (json_str != nullptr) {
        settings.SetString(kKeyAlarms, json_str);
        cJSON_free(json_str);
    }
    cJSON_Delete(root);
    return true;
}
