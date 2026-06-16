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
#include "mpu9250.h"
#include <stdio.h>
#include "IMU.h"


// 初始化全局传感器数组
GlobalSensor global_sensor[SENSOR_NUM];

// 模拟传感器数据（实际项目中应从硬件读取）
static int16_t simulated_sensor_data[SENSOR_NUM][3] = {
    {1000, 2000, 3000},  // 传感器1: X,Y,Z
};

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

    // 1. 初始化传感器硬件
    // 这里调用实际的MPU9250初始化函数
    // mpu9250_init();
	IMU_Init();
    // 2. 配置传感器参数
    // 设置量程、采样率、滤波器等

    // 3. 初始化传感器数据结构


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
	// TODO: 现在已经实现自动读取，因为只有一个传感器，所以这里为了方便直接调用了底层的 IMU.c 中的函数完成了传感器读取 (IMU_single_read)
	IMU_single_read(sensor_id);
	// 1、获取传感器的 ID

	// 2、针对不同的传感器通信方法实现不同的读取操作


}

/**
 * @brief 多传感器数据读取函数
 *
 * 批量读取所有传感器的数据
 */
void sensor_multi_read(void)
{
	// TODO:后续如果有更多的传感器，可以调用该函数
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

/**
 * @brief 获取传感器原始数据
 * @param sensor_id 传感器ID
 * @param x 指向X轴数据的指针
 * @param y 指向Y轴数据的指针
 * @param z 指向Z轴数据的指针
 */
void sensor_get_raw_data(uint8_t sensor_id, int16_t* x, int16_t* y, int16_t* z)
{
    if (sensor_id < 1 || sensor_id > SENSOR_NUM) {
        *x = *y = *z = 0;
        return;
    }

    int idx = sensor_id - 1;
    *x = (int16_t)global_sensor[idx].x;
    *y = (int16_t)global_sensor[idx].y;
    *z = (int16_t)global_sensor[idx].z;
}

/**
 * @brief 获取传感器角度数据
 * @param sensor_id 传感器ID
 * @param pitch 指向俯仰角的指针
 * @param roll 指向横滚角的指针
 * @param yaw 指向偏航角的指针
 *
 * 将原始数据转换为角度值（单位：度）
 */
void sensor_get_angle_data(uint8_t sensor_id, float* pitch, float* roll, float* yaw)
{
    if (sensor_id < 1 || sensor_id > SENSOR_NUM) {
        *pitch = *roll = *yaw = 0.0f;
        return;
    }

    int idx = sensor_id - 1;

    // 假设原始数据单位是0.01度
    *pitch = (float)global_sensor[idx].x / 100.0f;
    *roll = (float)global_sensor[idx].y / 100.0f;
    *yaw = (float)global_sensor[idx].z / 100.0f;
}