#include "motor_driver.h"
#include <algorithm>
#include <cmath>
#include <esp_log.h>

#define TAG "MotorDriver"

MotorDriver::MotorDriver(gpio_num_t left_in1, gpio_num_t left_in2, 
                         gpio_num_t right_in1, gpio_num_t right_in2)
    : left_in1_(left_in1), left_in2_(left_in2),
      right_in1_(right_in1), right_in2_(right_in2) {
    Initialize();
}

void MotorDriver::Initialize() {
    ESP_LOGI(TAG, "Initializing Motor Driver");

    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_LOW_SPEED_MODE,
        .duty_resolution  = kLedcResolution,
        .timer_num        = LEDC_TIMER_1,
        .freq_hz          = static_cast<uint32_t>(kLedcFreq),
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    ledc_channel_config_t ledc_channel[4] = {
        {
            .gpio_num       = left_in1_,
            .speed_mode     = LEDC_LOW_SPEED_MODE,
            .channel        = left_in1_channel_,
            .intr_type      = LEDC_INTR_DISABLE,
            .timer_sel      = LEDC_TIMER_1,
            .duty           = 0,
            .hpoint         = 0,
            .flags          = { .output_invert = 0 }
        },
        {
            .gpio_num       = left_in2_,
            .speed_mode     = LEDC_LOW_SPEED_MODE,
            .channel        = left_in2_channel_,
            .intr_type      = LEDC_INTR_DISABLE,
            .timer_sel      = LEDC_TIMER_1,
            .duty           = 0,
            .hpoint         = 0,
            .flags          = { .output_invert = 0 }
        },
        {
            .gpio_num       = right_in1_,
            .speed_mode     = LEDC_LOW_SPEED_MODE,
            .channel        = right_in1_channel_,
            .intr_type      = LEDC_INTR_DISABLE,
            .timer_sel      = LEDC_TIMER_1,
            .duty           = 0,
            .hpoint         = 0,
            .flags          = { .output_invert = 0 }
        },
        {
            .gpio_num       = right_in2_,
            .speed_mode     = LEDC_LOW_SPEED_MODE,
            .channel        = right_in2_channel_,
            .intr_type      = LEDC_INTR_DISABLE,
            .timer_sel      = LEDC_TIMER_1,
            .duty           = 0,
            .hpoint         = 0,
            .flags          = { .output_invert = 0 }
        },
    };

    for (int i = 0; i < 4; i++) {
        ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel[i]));
    }
}

void MotorDriver::SetSpeed(int left_speed, int right_speed) {
    // Clamp speed to -100 to 100
    left_speed = std::max(-100, std::min(100, left_speed));
    right_speed = std::max(-100, std::min(100, right_speed));

    SetMotor(left_in1_channel_, left_in2_channel_, left_speed);
    SetMotor(right_in1_channel_, right_in2_channel_, right_speed);
}

void MotorDriver::Stop() {
    SetSpeed(0, 0);
}

void MotorDriver::SetMotor(ledc_channel_t ch1, ledc_channel_t ch2, int speed) {
    uint32_t duty = static_cast<uint32_t>(std::abs(speed) * kMaxDuty / 100);

    if (speed > 0) {
        // Forward
        ledc_set_duty(LEDC_LOW_SPEED_MODE, ch1, duty);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, ch1);
        ledc_set_duty(LEDC_LOW_SPEED_MODE, ch2, 0);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, ch2);
    } else if (speed < 0) {
        // Backward
        ledc_set_duty(LEDC_LOW_SPEED_MODE, ch1, 0);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, ch1);
        ledc_set_duty(LEDC_LOW_SPEED_MODE, ch2, duty);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, ch2);
    } else {
        // Stop (Coast)
        ledc_set_duty(LEDC_LOW_SPEED_MODE, ch1, 0);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, ch1);
        ledc_set_duty(LEDC_LOW_SPEED_MODE, ch2, 0);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, ch2);
    }
}
