// 用于IMU数据读取与处理，通过串口1实现读取
#include "Sensor/IMU.h"
#include "Sensor/WT_IMU.h"
#include "Sensor/Sensor.h"
#include <string.h>

#include "usart.h"
#include "cmsis_os2.h"

static volatile char s_cDataUpdate = 0, s_cCmd = 0xff;

static void SensorUartSend(uint8_t *p_data, uint32_t uiSize);

// 寄存器更新回调：WIT 解析到新数据后，同步到 global_sensor（缩放同 DataTask）
static void IMU_RegUpdate(uint32_t uiReg, uint32_t uiRegNum)
{
    if (uiReg <= Roll && (uiReg + uiRegNum) > Roll) {
        // Roll/Pitch/Yaw 原始值 → 角度（°/32768*180）
        s_cDataUpdate = 1;
    }
}

void IMU_Init(void)
{
    WitInit(WIT_PROTOCOL_MODBUS, 0x50);
    WitSerialWriteRegister(SensorUartSend);
    // 必须注册更新回调，否则 WitSerialDataIn 首行判空直接丢弃所有 RX 数据
    WitRegisterCallBack(IMU_RegUpdate);
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
