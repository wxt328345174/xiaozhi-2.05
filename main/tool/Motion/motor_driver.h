#ifndef MOTION_MOTOR_DRIVER_H
#define MOTION_MOTOR_DRIVER_H

#include <stdint.h>
#include <driver/gpio.h>
#include <driver/ledc.h>

class MotorDriver {
public:
    MotorDriver(gpio_num_t left_in1, gpio_num_t left_in2, 
                gpio_num_t right_in1, gpio_num_t right_in2);
    
    // speed range: -100 to 100
    void SetSpeed(int left_speed, int right_speed);
    void Stop();

private:
    gpio_num_t left_in1_;
    gpio_num_t left_in2_;
    gpio_num_t right_in1_;
    gpio_num_t right_in2_;

    // LEDC configuration
    const int kLedcFreq = 1000; // 1kHz for better torque
    const ledc_timer_bit_t kLedcResolution = LEDC_TIMER_10_BIT; // 0-1023
    const int kMaxDuty = 1023;
    
    // Mapping pins to LEDC channels
    // Since we have 4 pins, we need 4 channels if we want full PWM control on all pins for brake/coast modes,
    // or we can just toggle directions and PWM the enable pin if the hardware supported it,
    // but DRV8833 uses IN1/IN2.
    // To support variable speed and direction:
    // Forward: IN1 = PWM, IN2 = 0
    // Backward: IN1 = 0, IN2 = PWM
    // Stop (Coast): IN1 = 0, IN2 = 0
    // Brake: IN1 = 1, IN2 = 1 (We'll use Coast for simplicity or Brake for immediate stop)
    
    // We will assign a unique channel to each pin to allow full control.
    ledc_channel_t left_in1_channel_ = LEDC_CHANNEL_2;
    ledc_channel_t left_in2_channel_ = LEDC_CHANNEL_3;
    ledc_channel_t right_in1_channel_ = LEDC_CHANNEL_4;
    ledc_channel_t right_in2_channel_ = LEDC_CHANNEL_5;

    void Initialize();
    void SetMotor(ledc_channel_t ch1, ledc_channel_t ch2, int speed);
};

#endif // MOTION_MOTOR_DRIVER_H
