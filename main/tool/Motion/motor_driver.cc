#include "motor_driver.h"
#include <algorithm>
#include <cmath>
#include <esp_log.h>

#define TAG "MotorDriver"

MotorDriver::MotorDriver(gpio_num_t left_nsleep, gpio_num_t left_in1, gpio_num_t left_in2, 
                         gpio_num_t right_nsleep, gpio_num_t right_in1, gpio_num_t right_in2)
    : left_nsleep_(left_nsleep), left_in1_(left_in1), left_in2_(left_in2),
      right_nsleep_(right_nsleep), right_in1_(right_in1), right_in2_(right_in2) {
    Initialize();
}

void MotorDriver::Initialize() {
    ESP_LOGI(TAG, "Initializing Motor Driver (DRV8837 x2)");

    // 初始化 nSLEEP 引脚为 GPIO 输出模式，并唤醒芯片
    gpio_config_t io_conf = {
        .pin_bit_mask = 0,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    
    if (left_nsleep_ != GPIO_NUM_NC && left_nsleep_ >= 0) {
        io_conf.pin_bit_mask = (1ULL << left_nsleep_);
        ESP_ERROR_CHECK(gpio_config(&io_conf));
        gpio_set_level(left_nsleep_, 1);  // 唤醒左电机驱动
        ESP_LOGI(TAG, "Left motor nSLEEP (GPIO%d) initialized and enabled", left_nsleep_);
    }
    
    if (right_nsleep_ != GPIO_NUM_NC && right_nsleep_ >= 0) {
        io_conf.pin_bit_mask = (1ULL << right_nsleep_);
        ESP_ERROR_CHECK(gpio_config(&io_conf));
        gpio_set_level(right_nsleep_, 1);  // 唤醒右电机驱动
        ESP_LOGI(TAG, "Right motor nSLEEP (GPIO%d) initialized and enabled", right_nsleep_);
    }

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
        if (ledc_channel[i].gpio_num != GPIO_NUM_NC && ledc_channel[i].gpio_num >= 0) {
            ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel[i]));
        } else {
             ESP_LOGW(TAG, "Skipping LEDC channel %d config because GPIO is invalid (%d)", i, ledc_channel[i].gpio_num);
        }
    }
}

void MotorDriver::Sleep() {
    ESP_LOGI(TAG, "Entering sleep mode");
    if (left_nsleep_ != GPIO_NUM_NC && left_nsleep_ >= 0) {
        gpio_set_level(left_nsleep_, 0);
    }
    if (right_nsleep_ != GPIO_NUM_NC && right_nsleep_ >= 0) {
        gpio_set_level(right_nsleep_, 0);
    }
}

void MotorDriver::Wakeup() {
    ESP_LOGI(TAG, "Waking up from sleep mode");
    if (left_nsleep_ != GPIO_NUM_NC && left_nsleep_ >= 0) {
        gpio_set_level(left_nsleep_, 1);
    }
    if (right_nsleep_ != GPIO_NUM_NC && right_nsleep_ >= 0) {
        gpio_set_level(right_nsleep_, 1);
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
