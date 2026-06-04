/**
* @file sensor_cmd_parser.h
 * @brief 下位机指令解析库头文件
 *
 * 负责解析外设（电机/传感器）反馈的指令，更新全局结构体
 *
 * @date 2026-04-23
 * @author blin
 */

#ifndef SENSOR_CMD_PARSER_H
#define SENSOR_CMD_PARSER_H

#include <stdint.h>
#include "Sensor/Sensor.h"          // GlobalSensor, SENSOR_NUM
#include "cmd_packer.h"      // cmd_packer_send_status_frame 的新接口

/* 帧头定义 */
#define SENSOR_ID           0x01    // 传感器总线地址（所有传感器帧的帧头）

/* 功能码定义 */
#define FUNC_SENSOR_FEEDBACK  0x03  // 传感器反馈功能码

/* 帧解析状态机 */
typedef enum {
	SENSOR_STATE_HEAD = 0,
	SENSOR_STATE_FUNC,
	SENSOR_STATE_LEN,
	SENSOR_STATE_DATA,
	SENSOR_STATE_CHECK
} SensorParseState_t;

/* 传感器解析器上下文（每个串口一个实例） */
typedef struct {
	SensorParseState_t state;
	uint8_t buf[32];        // 接收缓冲区
	uint8_t data_len;       // 协议中的 L 字段
	uint8_t idx;            // 当前写入位置
} SensorParser;

/* 初始化解析器 */
void SensorParser_Init(SensorParser *parser);

/**
 * @brief 喂入一个字节，状态机自动解析
 * @param parser 解析器实例
 * @param byte 接收到的字节
 * @param sensors 传感器数组指针（长度至少 SENSOR_NUM）
 * @param motors  电机数组指针（用于状态打包）
 * @param lqts    系统状态指针（用于状态打包）
 * @note  若一帧有效且校验通过，会更新 sensors 并自动调用打包发送函数
 */
void SensorParser_Feed(SensorParser* parser, uint8_t byte,
					   GlobalSensor* sensors,
					   const GlobalMotor* motors,
					   const ContinuumRobot* CR);

#endif