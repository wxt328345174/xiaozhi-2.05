#include "anti_drop_controller.h"
#include <esp_log.h>

#define TAG "AntiDrop"

AntiDropController::AntiDropController(IrSensorManager* ir_sensors, MotorDriver* motor_driver)
    : ir_sensors_(ir_sensors), motor_driver_(motor_driver) {
    ESP_LOGI(TAG, "Anti-drop controller created (speed=%d%%, duration=%dms)", speed_, duration_ms_);
}

void AntiDropController::Start() {
    if (running_) {
        ESP_LOGW(TAG, "Already running");
        return;
    }
    
    running_ = true;
    xTaskCreate(TaskWrapper, "anti_drop", 4096, this, 5, &task_handle_);  // 优先级 5，高于普通任务
    ESP_LOGI(TAG, "Anti-drop monitoring started");
}

void AntiDropController::Stop() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    if (task_handle_) {
        vTaskDelete(task_handle_);
        task_handle_ = nullptr;
    }
    ESP_LOGI(TAG, "Anti-drop monitoring stopped");
}

void AntiDropController::SetEnabled(bool enabled) {
    enabled_ = enabled;
    ESP_LOGI(TAG, "Anti-drop %s", enabled ? "enabled" : "disabled");
}

void AntiDropController::TaskWrapper(void* arg) {
    static_cast<AntiDropController*>(arg)->MonitorTask();
}

void AntiDropController::MonitorTask() {
    ESP_LOGI(TAG, "Monitor task started (interval=%dms)", check_interval_ms_);
    
    while (running_) {
        if (!enabled_) {
            vTaskDelay(pdMS_TO_TICKS(check_interval_ms_));
            continue;
        }
        
        // 读取传感器状态
        uint8_t status = ir_sensors_->ReadAllSensors();
        
        // 【单传感器测试模式】仅使用边缘中间传感器 (EM, GPIO17)
        // ITR8307: 1=地面, 0=悬空
        bool is_ground = (status & (1 << IrSensorManager::IR_EDGE_MIDDLE));
        
        if (!avoiding_drop_) {
            if (is_ground) {
                // 检测到地面 -> 后退
                // 注意：这会导致在平地上持续触发后退动作
                ESP_LOGI(TAG, "Ground detected (EM)! Moving backward...");
                ExecuteAvoidAction(-1); 
            } else {
                // 未检测到地面 (悬空) -> 前进
                ESP_LOGW(TAG, "Edge detected (EM)! Moving forward...");
                ExecuteAvoidAction(1);
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(check_interval_ms_));
    }
    
    vTaskDelete(NULL);
}

void AntiDropController::ExecuteAvoidAction(int direction) {
    avoiding_drop_ = true;
    
    int left_speed = direction * speed_;
    int right_speed = direction * speed_;
    
    ESP_LOGI(TAG, "Executing avoid action: dir=%d, speed=%d, duration=%dms", 
             direction, speed_, duration_ms_);
    
    // 设置电机速度（直接调用 MotorDriver，绕过 MCP）
    motor_driver_->SetSpeed(left_speed, right_speed);
    
    // 等待动作完成
    vTaskDelay(pdMS_TO_TICKS(duration_ms_));
    
    // 停止电机
    motor_driver_->Stop();
    
    ESP_LOGI(TAG, "Avoid action completed");
    avoiding_drop_ = false;
}
