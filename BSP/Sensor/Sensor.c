/**
 * @file Sensor.c
 * @brief 传感器驱动函数实现
 * 
 * 实现传感器初始化、数据读取、自检等功能
 * 假设使用MPU9250作为主要传感器
 * 
 * @date 2026-04-02
 * @author Psyduck
 */

#include "Sensor.h"
#include "mpu9250.h"
#include <stdio.h>

// 初始化全局传感器数组
GlobalSensor global_sensor[SENSOR_NUM];

// 模拟传感器数据（实际项目中应从硬件读取）
static int16_t simulated_sensor_data[SENSOR_NUM][3] = {
    {1000, 2000, 3000},  // 传感器1: X,Y,Z
    {1100, 2100, 3100},  // 传感器2
    {1200, 2200, 3200},  // 传感器3
    {1300, 2300, 3300}   // 传感器4
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
    printf("[SENSOR] 传感器初始化开始\n");
    
    // 1. 初始化传感器硬件
    // 这里调用实际的MPU9250初始化函数
    // mpu9250_init();
    
    // 2. 配置传感器参数
    // 设置量程、采样率、滤波器等
    
    // 3. 初始化传感器数据结构
    for (int i = 0; i < SENSOR_NUM; i++) {
        global_sensor[i].x = 1;
        global_sensor[i].y = 2;
    	global_sensor[i].z = 3;
    }
    
    printf("[SENSOR] 传感器初始化完成\n");
}

/**
 * @brief 单传感器数据读取函数
 * @param sensor_id 传感器ID (1-4)
 * 
 * 读取指定传感器的数据并更新全局结构体
 */
void sensor_single_read(uint8_t sensor_id)
{
    if (sensor_id < 1 || sensor_id > SENSOR_NUM) {
        printf("[SENSOR] 错误: 无效的传感器ID %d\n", sensor_id);
        return;
    }
    
    printf("[SENSOR] 读取传感器%d数据\n", sensor_id);
    
    // 实际项目中应从硬件读取数据
    // 这里使用模拟数据
    int idx = sensor_id - 1;
    
    // 模拟从硬件读取数据
    // uint8_t data[6];
    // mpu9250_read_accel(data);
    // int16_t raw_x = (data[0] << 8) | data[1];
    // int16_t raw_y = (data[2] << 8) | data[3];
    // int16_t raw_z = (data[4] << 8) | data[5];
    
    // 使用模拟数据
    int16_t raw_x = simulated_sensor_data[idx][0];
    int16_t raw_y = simulated_sensor_data[idx][1];
    int16_t raw_z = simulated_sensor_data[idx][2];
    
    // 添加一些随机变化，模拟真实传感器
    static int counter = 0;
    raw_x += (counter % 10) - 5;
    raw_y += ((counter + 3) % 10) - 5;
    raw_z += ((counter + 7) % 10) - 5;
    counter++;
    
    // 更新传感器结构体
    global_sensor[idx].x = (uint16_t)raw_x;
    global_sensor[idx].y = (uint16_t)raw_y;
    global_sensor[idx].z = (uint16_t)raw_z;
    
    printf("[SENSOR] 传感器%d数据: X=%d, Y=%d, Z=%d\n", 
           sensor_id, raw_x, raw_y, raw_z);
}

/**
 * @brief 多传感器数据读取函数
 * 
 * 批量读取所有传感器的数据
 */
void sensor_multi_read(void)
{
    printf("[SENSOR] 批量读取所有传感器数据\n");
    
    for (int i = 0; i < SENSOR_NUM; i++) {
        // 调用单传感器读取函数
        sensor_single_read(i + 1);
    }
    
    printf("[SENSOR] 批量读取完成\n");
}

/**
 * @brief 传感器自检函数
 * 
 * 执行传感器自检，检查传感器状态
 */
void sensor_self_test(uint8_t sensor_id)
{
    printf("[SENSOR] 开始传感器自检\n");
    
    // 1. 检查传感器连接
    printf("  1. 检查传感器连接... ");
    
    // 实际项目中应发送WHO_AM_I命令检查传感器响应
    // uint8_t who_am_i = mpu9250_read_register(MPU9250_WHO_AM_I);
    // if (who_am_i == MPU9250_WHO_AM_I_VALUE) {
    //     printf("通过\n");
    // } else {
    //     printf("失败 (收到: 0x%02X, 期望: 0x%02X)\n", who_am_i, MPU9250_WHO_AM_I_VALUE);
    // }
    
    printf("通过 (模拟)\n");
    
    // 2. 检查传感器数据范围
    printf("  2. 检查数据范围... ");
    
    // 读取一些样本数据检查是否在合理范围内
    sensor_single_read(sensor_id);
    
    // 检查数据是否在合理范围内
    if (global_sensor[0].x > 0 && global_sensor[0].x < 0xFFFF &&
        global_sensor[0].y > 0 && global_sensor[0].y < 0xFFFF &&
        global_sensor[0].z > 0 && global_sensor[0].z < 0xFFFF) {
        printf("通过\n");
    } else {
        printf("失败\n");
    }
    
    // 3. 检查所有传感器
    printf("  3. 检查所有传感器... ");
    int passed = 1;
    
    for (int i = 0; i < SENSOR_NUM; i++) {
        // 简单检查传感器数据是否非零
        if (global_sensor[i].x == 0 && global_sensor[i].y == 0 && global_sensor[i].z == 0) {
            printf("传感器%d异常 ", i+1);
            passed = 0;
        }
    }
    
    if (passed) {
        printf("全部通过\n");
    } else {
        printf("\n");
    }
    
    printf("[SENSOR] 自检完成\n");
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