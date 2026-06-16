/**
 * @file DataTask.c
 * @brief 数据任务：负责 USART1 RX (外设反馈) 和 USART2 TX (发送至 PC)
 *
 * 任务流程：
 * 1. 监听 CmdDataQueue (USART1 RX)，解析外设反馈并更新全局状态。
 * 2. 将系统状态打包并通过 SensorMessageQueue 缓冲。
 * 3. 轮询 SensorMessageQueue，将反馈字节发送至 USART2 TX (上位机)。
 * 
 * 简化版本：将复杂逻辑移到 DataTaskUtils 库中
 * 
 * @date 2026-03-30
 * @author Psyduck
 */
#include <stdio.h>
#include "cmsis_os2.h"
#include "FreeRTOS.h"
#include "task.h"
#include "usart.h"
#include "Motor/Motor.h"
#include "Sensor/Sensor.h"
#include "Common/sensor_cmd_parser.h"
#include "Common/cmd_packer.h"
#include "Common/can_driver.h"
#include "CR/CR.h"
#include "Sensor/WT_IMU.h"


#define RX_BUF_SIZE 256



void StartDataHandleTask(void *argument)
{
    uint8_t rx_byte = 0;
    uint8_t tx_byte = 0;

    // 数据采集时间戳
    static uint32_t last_motor_check_time = 0;
    static uint32_t last_sensor_read_time = 0;
    static uint32_t last_send_time = 0;

    for(;;)
    {
        uint32_t current_time = osKernelGetTickCount();

        // ====================================
        // 1. 数据采集: 电机状态和传感器数据
        // ====================================

        // 每200ms读取一次电机状态（位置、速度）
        if ((current_time - last_motor_check_time) >= 200)
        {
            motor_status_check();
            last_motor_check_time = current_time;
        }

        // 每500ms读取一次传感器数据
        if ((current_time - last_sensor_read_time) >= 500)
        {
            sensor_single_read(0x50);
            last_sensor_read_time = current_time;
        }

        // ====================================
        // 2. 数据解析: 从队列接收并解析
        // ====================================
        while (osMessageQueueGet(MotorDataParseQueueHandle, &rx_byte, NULL, 0) == osOK)
        {
            WitSerialDataIn(rx_byte);
        }
        global_sensor[0].x = sReg[Roll+0] / 32768.0f * 180.0f;
        global_sensor[0].y = sReg[Roll+1] / 32768.0f * 180.0f;
        global_sensor[0].z = sReg[Roll+2] / 32768.0f * 180.0f;

        // ====================================
        // 3. 数据发送: 打包并发送给上位机
        // ====================================

        // 每200ms发送一次数据到队列 SensorMessageQueue
        if ((current_time - last_send_time) >= 200)
        {
            // 打包系统状态数据 (使用 static 以节省堆栈空间)
            static uint8_t packed_frame[256];
            const float scale = CR.operation_space.scale;
            const uint8_t state = CR.state;

            const uint16_t frame_len = cmd_packer_pack_status_frame(packed_frame, global_motor, global_sensor, &CR, state);

            // 发送到队列
            for (int i = 0; i < frame_len; i++)
            {
                osMessageQueuePut(IMUDataParseQueueHandle, &packed_frame[i], 0, 0);
            }
            last_send_time = current_time;
        }

        // 批量发送数据
        static uint8_t tx_buffer[256];
        static uint16_t tx_buffer_len = 0;

        // 提取并发送
        tx_buffer_len = 0;
        while (osMessageQueueGet(IMUDataParseQueueHandle, &tx_byte, NULL, 0) == osOK && tx_buffer_len < 256)
        {
            tx_buffer[tx_buffer_len++] = tx_byte;
        }
        if (tx_buffer_len > 0) {
            Usart_SendString(&huart2, tx_buffer, tx_buffer_len);
        }

        osDelay(10); // 降低 CPU 占用
    }
}

