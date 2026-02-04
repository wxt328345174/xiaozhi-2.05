#ifndef IR_SENSOR_H
#define IR_SENSOR_H

#include <driver/gpio.h>
#include <stdint.h>

/**
 * @brief 红外传感器管理类 (ITR8307)
 * 
 * 用于管理多个红外传感器的状态检测。
 * 典型应用：机器人防跌落检测。
 * 
 * 传感器逻辑：
 * - 高电平 (1): 检测到地面（红外反射回来）
 * - 低电平 (0): 未检测到地面（悬空/边缘）
 */
class IrSensorManager {
public:
    // 传感器位置索引
    enum SensorPosition {
        IR_FRONT_RIGHT = 0,   // 前向右侧 (FR)
        IR_FRONT_MIDDLE = 1,  // 前向中间 (FM)
        IR_FRONT_LEFT = 2,    // 前向左侧 (FL)
        IR_EDGE_LEFT = 3,     // 边缘左侧 (EL)
        IR_EDGE_MIDDLE = 4,   // 边缘中间 (EM)
        IR_EDGE_RIGHT = 5,    // 边缘右侧 (ER)
        IR_SENSOR_COUNT = 6
    };

    /**
     * @brief 构造函数
     * @param pins 6 个传感器的 GPIO 引脚数组，顺序为 [FR, FM, FL, EL, EM, ER]
     */
    IrSensorManager(const gpio_num_t pins[IR_SENSOR_COUNT]);

    /**
     * @brief 读取单个传感器状态
     * @param position 传感器位置
     * @return true=检测到地面, false=悬空/边缘
     */
    bool ReadSensor(SensorPosition position);

    /**
     * @brief 读取所有传感器状态
     * @return 6 位状态值，bit0=FR, bit1=FM, ..., bit5=ER
     */
    uint8_t ReadAllSensors();

    /**
     * @brief 打印所有传感器状态到日志
     */
    void PrintStatus();

    /**
     * @brief 检测是否有边缘/悬空危险
     * @return true=存在危险（至少一个传感器检测到悬空）
     */
    bool IsEdgeDetected();

    /**
     * @brief 获取传感器名称
     * @param position 传感器位置
     * @return 传感器名称字符串
     */
    static const char* GetSensorName(SensorPosition position);

private:
    gpio_num_t pins_[IR_SENSOR_COUNT];
    
    void Initialize();
};

#endif // IR_SENSOR_H
