#include "ir_sensor.h"
#include <esp_log.h>

#define TAG "IrSensor"

static const char* SENSOR_NAMES[IrSensorManager::IR_SENSOR_COUNT] = {
    "FR(前右)", "FM(前中)", "FL(前左)",
    "EL(边左)", "EM(边中)", "ER(边右)"
};

IrSensorManager::IrSensorManager(const gpio_num_t pins[IR_SENSOR_COUNT]) {
    for (int i = 0; i < IR_SENSOR_COUNT; i++) {
        pins_[i] = pins[i];
    }
    Initialize();
}

void IrSensorManager::Initialize() {
    ESP_LOGI(TAG, "Initializing IR Sensor Manager (6x ITR8307)");
    
    for (int i = 0; i < IR_SENSOR_COUNT; i++) {
        if (pins_[i] != GPIO_NUM_NC && pins_[i] >= 0) {
            gpio_config_t io_conf = {
                .pin_bit_mask = (1ULL << pins_[i]),
                .mode = GPIO_MODE_INPUT,
                .pull_up_en = GPIO_PULLUP_DISABLE,
                .pull_down_en = GPIO_PULLDOWN_ENABLE,  // 启用下拉，未连接时默认为 0
                .intr_type = GPIO_INTR_DISABLE
            };
            ESP_ERROR_CHECK(gpio_config(&io_conf));
            ESP_LOGI(TAG, "  %s -> GPIO%d initialized (pull-down)", SENSOR_NAMES[i], pins_[i]);
        } else {
            ESP_LOGW(TAG, "  %s -> GPIO_NUM_NC (disabled)", SENSOR_NAMES[i]);
        }
    }
    
    ESP_LOGI(TAG, "IR Sensor Manager initialized successfully");
}

bool IrSensorManager::ReadSensor(SensorPosition position) {
    if (position >= IR_SENSOR_COUNT) {
        return false;
    }
    if (pins_[position] == GPIO_NUM_NC || pins_[position] < 0) {
        return false;
    }
    // ITR8307: 高电平=检测到地面, 低电平=悬空
    return gpio_get_level(pins_[position]) == 1;
}

uint8_t IrSensorManager::ReadAllSensors() {
    uint8_t status = 0;
    for (int i = 0; i < IR_SENSOR_COUNT; i++) {
        if (ReadSensor(static_cast<SensorPosition>(i))) {
            status |= (1 << i);
        }
    }
    return status;
}

void IrSensorManager::PrintStatus() {
    uint8_t status = ReadAllSensors();
    
    // 使用 ASCII 字符避免终端编码问题: O=地面, X=悬空
    ESP_LOGI(TAG, "IR Status: [%c%c%c|%c%c%c] (0x%02X) %s",
        (status & (1 << IR_FRONT_LEFT)) ? 'O' : 'X',
        (status & (1 << IR_FRONT_MIDDLE)) ? 'O' : 'X',
        (status & (1 << IR_FRONT_RIGHT)) ? 'O' : 'X',
        (status & (1 << IR_EDGE_LEFT)) ? 'O' : 'X',
        (status & (1 << IR_EDGE_MIDDLE)) ? 'O' : 'X',
        (status & (1 << IR_EDGE_RIGHT)) ? 'O' : 'X',
        status,
        IsEdgeDetected() ? "EDGE!" : "OK");
    
    // 详细状态
    for (int i = 0; i < IR_SENSOR_COUNT; i++) {
        bool detected = (status & (1 << i)) != 0;
        ESP_LOGD(TAG, "  %s: %s", SENSOR_NAMES[i], detected ? "Ground" : "EDGE!");
    }
}

bool IrSensorManager::IsEdgeDetected() {
    uint8_t status = ReadAllSensors();
    // 如果任意传感器为 0（未检测到地面），则存在边缘危险
    return status != 0x3F;  // 0x3F = 0b00111111 = 所有 6 个传感器都检测到地面
}

const char* IrSensorManager::GetSensorName(SensorPosition position) {
    if (position >= IR_SENSOR_COUNT) {
        return "Unknown";
    }
    return SENSOR_NAMES[position];
}
