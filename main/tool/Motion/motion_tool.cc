#include "motion_tool.h"
#include "mcp_server.h"
#include <esp_log.h>
#include <cstring>

#define TAG "MotionTool"

struct MotionCommand {
    MotorDriver* driver;
    int duration_ms;
};

MotionTool::MotionTool(gpio_num_t left_nsleep, gpio_num_t left_in1, gpio_num_t left_in2, 
                       gpio_num_t right_nsleep, gpio_num_t right_in1, gpio_num_t right_in2)
    : motor_driver_(left_nsleep, left_in1, left_in2, right_nsleep, right_in1, right_in2) {
    
    auto& mcp_server = McpServer::GetInstance();

    mcp_server.AddTool("self.motion.move", 
        "Control the robot movement. Supports moving forward, backward, turning left, turning right, and stopping.\n"
        "Args:\n"
        "  `direction`: 'forward', 'backward', 'left', 'right', 'stop'.\n"
        "  `speed`: Speed percentage (0-100), default is -1 (use last speed).\n"
        "  `duration`: Duration in milliseconds (0 for infinite, -1 for default/last), default -1.",
        PropertyList({
            Property("direction", kPropertyTypeString),
            Property("speed", kPropertyTypeInteger, -1, -1, 100),
            Property("duration", kPropertyTypeInteger, -1, -1, 60000) // Max 60 seconds safety limit
        }),
        [this](const PropertyList& properties) -> ReturnValue {
            std::string direction = properties["direction"].value<std::string>();
            int speed = properties["speed"].value<int>();
            int duration = properties["duration"].value<int>();

            // Handle speed persistence
            if (speed == -1) {
                speed = current_speed_;
            } else {
                current_speed_ = speed;
            }

            // Duration persistence logic will be handled per-type (move vs turn) below
            // because they have different defaults (0 vs 1000)

            ESP_LOGI(TAG, "Motion command: dir=%s, speed=%d, duration=%d", direction.c_str(), speed, duration);

            if (direction == "stop") {
                motor_driver_.Stop();
                return true;
            }

            int left_speed = 0;
            int right_speed = 0;
            int final_duration = 0;

            if (direction == "forward" || direction == "backward") {
                if (duration == -1) {
                    final_duration = current_move_duration_;
                } else {
                    current_move_duration_ = duration;
                    final_duration = duration;
                }
                
                if (direction == "forward") {
                    left_speed = speed;
                    right_speed = speed;
                } else {
                    left_speed = -speed;
                    right_speed = -speed;
                }
            } else if (direction == "left" || direction == "right") {
                if (duration == -1) {
                    final_duration = current_turn_duration_;
                } else {
                    current_turn_duration_ = duration;
                    final_duration = duration;
                }

                if (direction == "left") {
                    left_speed = -speed;
                    right_speed = speed;
                } else {
                    left_speed = speed;
                    right_speed = -speed;
                }
            } else {
                throw std::invalid_argument("Invalid direction: " + direction);
            }
            
            // Use final_duration for execution
            duration = final_duration;
            ESP_LOGI(TAG, "Executing: left=%d, right=%d, duration=%d", left_speed, right_speed, duration);

            motor_driver_.SetSpeed(left_speed, right_speed);

            if (duration > 0) {
                // Create a task or timer to stop the motor
                // For simplicity, we launch a detached thread (or FreeRTOS task) that waits and stops
                // Note: This simple implementation might have race conditions if multiple commands are sent quickly.
                // A better approach is to use a dedicated task loop or timer handle.
                // Given the requirement "Design as simple as possible", we will use a dedicated task that deletes itself.
                
                MotionCommand* cmd = new MotionCommand{&motor_driver_, duration};
                xTaskCreate([](void* arg) {
                    MotionCommand* cmd = (MotionCommand*)arg;
                    vTaskDelay(pdMS_TO_TICKS(cmd->duration_ms));
                    cmd->driver->Stop();
                    delete cmd;
                    vTaskDelete(NULL);
                }, "motion_stop", 2048, cmd, 5, NULL);
            }

            return true;
        });
}
