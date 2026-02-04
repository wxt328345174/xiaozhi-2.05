#ifndef MOTION_TOOL_H
#define MOTION_TOOL_H

#include "motor_driver.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

class MotionTool {
public:
    // DRV8837 x2: 每颗芯片需要 nSLEEP + IN1 + IN2
    MotionTool(gpio_num_t left_nsleep, gpio_num_t left_in1, gpio_num_t left_in2, 
               gpio_num_t right_nsleep, gpio_num_t right_in1, gpio_num_t right_in2);

private:
    MotorDriver motor_driver_;
    
    // Helper to stop motor after duration
    static void StopTask(void* arg);
    
    int current_speed_ = 80;
    int current_turn_duration_ = 1000;
    int current_move_duration_ = 0;
};

#endif // MOTION_TOOL_H
