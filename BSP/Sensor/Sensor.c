/**
 * @file Sensor.c
 * @brief 传感器驱动函数实现
 *
 * 实现传感器初始化、数据读取、自检等功能
 * 假设使用MPU9250作为主要传感器
 *
 * @date 2026-04-02
 * @author blin
 */

#include "Sensor.h"
#include <stdio.h>
#include "IMU.h"


// 初始化全局传感器数组
GlobalSensor global_sensor[SENSOR_NUM];

/**
 * @brief 传感器初始化函数
 *
 * 初始化所有传感器，包括：
 * 1. 配置I2C接口
 * 2. 配置传感器参数
 * 3. 执行自检
 */
void sensor_init(void)
{
    IMU_Init();
}

/**
 * @brief 单传感器数据读取函数
 * @param sensor_id 传感器ID
 *
 * 读取指定传感器的数据并更新全局结构体
 */
void sensor_single_read(uint8_t sensor_id)
{
	// IMU数据读取
	IMU_single_read(sensor_id);
}

/**
 * @brief 传感器校准函数
 *
 * 执行传感器校准指令
 */
void sensor_cal(uint8_t sensor_id)
{
	// IMU 校准
	IMU_Cal(sensor_id);
}
