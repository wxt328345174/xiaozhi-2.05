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
        
        // 检查前向传感器 (bit0=FR, bit1=FM, bit2=FL)
        bool front_edge = false;
        if (!(status & (1 << IrSensorManager::IR_FRONT_RIGHT)) ||
            !(status & (1 << IrSensorManager::IR_FRONT_MIDDLE)) ||
            !(status & (1 << IrSensorManager::IR_FRONT_LEFT))) {
            front_edge = true;
        }
        
        // 检查边缘传感器 (bit3=EL, bit4=EM, bit5=ER)
        bool edge_edge = false;
        if (!(status & (1 << IrSensorManager::IR_EDGE_LEFT)) ||
            !(status & (1 << IrSensorManager::IR_EDGE_MIDDLE)) ||
            !(status & (1 << IrSensorManager::IR_EDGE_RIGHT))) {
            edge_edge = true;
        }
        
        // 执行避让动作（前向优先）
        if (front_edge && !avoiding_drop_) {
            ESP_LOGW(TAG, "FRONT EDGE detected! Moving backward...");
            ExecuteAvoidAction(-1);  // 后退
        } else if (edge_edge && !avoiding_drop_) {
            ESP_LOGW(TAG, "EDGE detected! Moving forward...");
            ExecuteAvoidAction(1);   // 前进
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
