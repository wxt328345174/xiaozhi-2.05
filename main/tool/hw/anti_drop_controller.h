#ifndef ANTI_DROP_CONTROLLER_H
#define ANTI_DROP_CONTROLLER_H

#include "ir_sensor.h"
#include "motor_driver.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

/**
 * @brief 防掉落控制器
 * 
 * 将红外传感器与电机驱动结合，实现本地防掉落机制。
 * 独立于 MCP 控制，优先级更高，保证时效性。
 * 
 * 控制逻辑：
 * - 前向传感器检测到边缘 → 后退
 * - 边缘传感器检测到边缘 → 前进
 */
class AntiDropController {
public:
    /**
     * @brief 构造函数
     * @param ir_sensors 红外传感器管理器指针
     * @param motor_driver 电机驱动器指针
     */
    AntiDropController(IrSensorManager* ir_sensors, MotorDriver* motor_driver);
    
    /**
     * @brief 启动防掉落监控任务
     */
    void Start();
    
    /**
     * @brief 停止防掉落监控任务
     */
    void Stop();
    
    /**
     * @brief 启用/禁用防掉落功能
     * @param enabled true=启用, false=禁用
     */
    void SetEnabled(bool enabled);
    
    /**
     * @brief 检查防掉落功能是否启用
     */
    bool IsEnabled() const { return enabled_; }
    
    /**
     * @brief 检查是否正在执行防掉落动作
     */
    bool IsAvoidingDrop() const { return avoiding_drop_; }
    
    /**
     * @brief 设置防掉落速度 (0-100)
     */
    void SetSpeed(int speed) { speed_ = speed; }
    
    /**
     * @brief 设置防掉落动作持续时间 (ms)
     */
    void SetDuration(int duration_ms) { duration_ms_ = duration_ms; }
    
    /**
     * @brief 设置检测间隔 (ms)
     */
    void SetCheckInterval(int interval_ms) { check_interval_ms_ = interval_ms; }

private:
    IrSensorManager* ir_sensors_;
    MotorDriver* motor_driver_;
    TaskHandle_t task_handle_ = nullptr;
    
    bool enabled_ = true;
    bool avoiding_drop_ = false;
    bool running_ = false;
    
    // 防掉落参数（独立于 MCP 运动控制）
    int speed_ = 50;           // 默认速度 50%
    int duration_ms_ = 1000;   // 默认动作时间 1000ms
    int check_interval_ms_ = 50;  // 检测间隔 50ms
    
    void MonitorTask();
    static void TaskWrapper(void* arg);
    
    /**
     * @brief 执行防掉落动作
     * @param direction 1=前进, -1=后退
     */
    void ExecuteAvoidAction(int direction);
};

#endif // ANTI_DROP_CONTROLLER_H
