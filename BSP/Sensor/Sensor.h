/**
* @file Sensor.h
 * @brief 传感器驱动头文件
 *
 * 定义传感器数据结构和函数接口
 *
 * @date 2026-04-02
 * @author Psyduck
 */

#ifndef CONTROLSYSTEM_SENSOR_H
#define CONTROLSYSTEM_SENSOR_H

#include <stdint.h>

#define SENSOR_NUM 1

/**
 * @brief 全局传感器数据结构
 */
typedef struct
{
	float x;  /**< X轴数据 */
	float y;  /**< Y轴数据 */
	float z;  /**< Z轴数据 */
} GlobalSensor;

/**
 * @brief 传感器初始化函数
 */
void sensor_init(void);

/**
 * @brief 单传感器数据读取函数
 * @param sensor_id 传感器ID (1-4)
 */
void sensor_single_read(uint8_t sensor_id);

/**
 * @brief 传感器自检函数
 */
void sensor_self_test(uint8_t sensor_id);
/**
 * @brief 传感器校准
 * @param sensor_id 传感器ID
 */
void sensor_cal(uint8_t sensor_id);
// 全局传感器数组声明
extern GlobalSensor global_sensor[SENSOR_NUM];

#endif //CONTROLSYSTEM_SENSOR_H