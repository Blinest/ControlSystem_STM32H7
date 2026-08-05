//
// Created by blin on 2026/3/7.
//
// 用于IMU数据读取与处理，通过串口1实现读取
#include "Sensor/IMU.h"
#include "Sensor/WT_IMU.h"
#include <string.h>

#include "usart.h"
#include "cmsis_os2.h"

static volatile char s_cDataUpdate = 0, s_cCmd = 0xff;

// 空回调：上层采用同步读取 sReg[] 的方式获取数据，
// 无需回调做额外处理，但必须注册，否则 WitSerialDataIn 会因回调为空而直接丢弃数据。
static void IMU_RegUpdateCb(uint32_t uiReg, uint32_t uiRegNum)
{
	(void)uiReg;
	(void)uiRegNum;
}

void IMU_Init(void)
{
	WitInit(WIT_PROTOCOL_MODBUS, 0x50);
	WitSerialWriteRegister(SensorUartSend);
	WitRegisterCallBack(IMU_RegUpdateCb);
}

void IMU_single_read(uint8_t sensor_id)
{
	// 使用串口1完成数据读取
	uint16_t reg_start = 0x3D;   // Roll 寄存器地址（需根据实际模块调整）
	uint16_t reg_count = 3;      // 连续读取 3 个寄存器

	WitReadReg(reg_start, reg_count);
}

static void SensorUartSend(uint8_t *p_data, uint32_t uiSize)
{
	Usart_SendString(&huart1, p_data, uiSize);
}


void IMU_Cal(uint8_t sensor_id)
{
	// WT 传感器校准，由于目前只有一个IMU传感器，故直接校准即可
	// 加速度计+陀螺仪校准
	WitStartAccCali();
	// 延时
	osDelay(2000);
	WitStopAccCali();
}
