#ifndef MOTION_MOTOR_DRIVER_H
#define MOTION_MOTOR_DRIVER_H

#include <stdint.h>
#include <driver/gpio.h>
#include <driver/ledc.h>

class MotorDriver {
public:
    // DRV8837 驱动: 每颗芯片控制一个电机，需要独立的 nSLEEP 引脚
    MotorDriver(gpio_num_t left_nsleep, gpio_num_t left_in1, gpio_num_t left_in2, 
                gpio_num_t right_nsleep, gpio_num_t right_in1, gpio_num_t right_in2);
    
    // speed range: -100 to 100
    void SetSpeed(int left_speed, int right_speed);
    void Stop();
    
    // 休眠/唤醒控制
    void Sleep();   // 进入低功耗模式
    void Wakeup();  // 唤醒

private:
    gpio_num_t left_nsleep_;
    gpio_num_t left_in1_;
    gpio_num_t left_in2_;
    gpio_num_t right_nsleep_;
    gpio_num_t right_in1_;
    gpio_num_t right_in2_;

    // LEDC configuration
    const int kLedcFreq = 1000; // 1kHz for better torque
    const ledc_timer_bit_t kLedcResolution = LEDC_TIMER_10_BIT; // 0-1023
    const int kMaxDuty = 1023;
    
    // DRV8837 控制逻辑 (与 DRV8833 相同):
    // Forward: IN1 = PWM, IN2 = 0
    // Backward: IN1 = 0, IN2 = PWM
    // Stop (Coast): IN1 = 0, IN2 = 0
    // Brake: IN1 = 1, IN2 = 1
    // nSLEEP: 低电平休眠，高电平使能
    
    // We will assign a unique channel to each pin to allow full control.
    ledc_channel_t left_in1_channel_ = LEDC_CHANNEL_2;
    ledc_channel_t left_in2_channel_ = LEDC_CHANNEL_3;
    ledc_channel_t right_in1_channel_ = LEDC_CHANNEL_4;
    ledc_channel_t right_in2_channel_ = LEDC_CHANNEL_5;

    void Initialize();
    void SetMotor(ledc_channel_t ch1, ledc_channel_t ch2, int speed);
};

#endif // MOTION_MOTOR_DRIVER_H
