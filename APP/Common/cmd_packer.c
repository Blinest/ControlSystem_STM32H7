/**
 * @file cmd_packer.c
 * @brief 指令打包库
 *
 * 负责将全局结构体中的数据打包成数据包，发送给上位机
 *
 * @date 2026-03-30
 * @author blin
 */

#include "cmd_packer.h"
#include "cmsis_os2.h"

// 引用外部队列
extern osMessageQueueId_t SensorMessageQueueHandle;

/**
 * @brief 打包系统状态帧 (test_frame 格式)
 * @param frame 存储打包后的数据缓冲区
 * @param motor_ctx 电机上下文数组（包含 GlobalMotor 和 Parser）
 * @param sensor 传感器全局结构体
 * @param lqts 运动学控制状态
 * @param state 系统状态
 * @return 打包后的总长度
 */
uint16_t cmd_packer_pack_status_frame(uint8_t* frame,
									  const MotorContext* motor_ctx,
									  const GlobalSensor* sensor,
									  const ContinuumRobot* CR,
									  uint8_t state)
{
    // 打包帧格式：帧头 + 功能码 + 数据长度 + 电机数量 + 传感器数量
	uint16_t idx = 0;
    frame[idx++] = 0xBB; // 帧头
    frame[idx++] = 0x02; // 功能码 (多传感器批量读取)

	// 预留数据长度字节的位置，稍后计算
	uint16_t data_length_pos = idx;
	idx += 1; // 为数据长度预留位置

    frame[idx++] = MOTOR_NUM;
    frame[idx++] = SENSOR_NUM;

    // 电机数据
    for (int i = 0; i < MOTOR_NUM; i++) {
    	const GlobalMotor* motor = &motor_ctx[i].global_motor;
        const int16_t pos = (int16_t)(motor->current_pos * 100); // mm
        const int16_t vel = (int16_t)(motor->current_vel * 100); // mm/s
        const int16_t acc = (int16_t)(motor->current_acc * 100); // mm/s^2
    	const uint8_t motor_state = motor->state;

    	// 填入数据包
        frame[idx++] = (pos >> 8) & 0xFF; frame[idx++] = pos & 0xFF;
        frame[idx++] = (vel >> 8) & 0xFF; frame[idx++] = vel & 0xFF;
        frame[idx++] = (acc >> 8) & 0xFF; frame[idx++] = acc & 0xFF;
    	frame[idx++] = motor_state & 0xFF;
    }
    // 传感器数据
    for (int i = 0; i < SENSOR_NUM; i++) {
        int16_t x = (int16_t)(sensor[i].x * 100);
        int16_t y = (int16_t)(sensor[i].y * 100);
        int16_t z = (int16_t)(sensor[i].z * 100);
        frame[idx++] = (x >> 8) & 0xFF; frame[idx++] = x & 0xFF;
        frame[idx++] = (y >> 8) & 0xFF; frame[idx++] = y & 0xFF;
        frame[idx++] = (z >> 8) & 0xFF; frame[idx++] = z & 0xFF;
    }

    // 第一段弯曲
    int16_t s_val = (int16_t)(CR->joint_space.current_theta[0] * 100);
    frame[idx++] = (s_val >> 8) & 0xFF; frame[idx++] = s_val & 0xFF;
	// 第二段弯曲
	int16_t s_val2 = (int16_t)(CR->joint_space.current_theta[1] * 100);
	frame[idx++] = s_val2 >> 8; frame[idx++] = s_val2 & 0xFF;
    frame[idx++] = state; // 使用传入的 state

	// 计算数据长度（从电机数量字节开始到state字节的所有字节）
	// 数据长度 = 当前idx - 3 （因为0:帧头,1:功能码,2:数据长度）
	frame[data_length_pos] = (uint8_t)(idx-3);

    // 校验和
    uint16_t checksum = 0;
    for (int i = 0; i < idx; i++) {
        checksum += frame[i];
    }
    frame[idx++] = checksum & 0xFF;

    return idx;
}

/**
 * @brief 发送打包后的帧到队列
 * @param frame 打包后的数据帧
 * @param frame_len 帧长度
 */
void cmd_packer_send_frame_to_queue(uint8_t* frame, uint16_t frame_len)
{
    for (int i = 0; i < frame_len; i++) {
        uint8_t msg = frame[i];
        osMessageQueuePut(SensorMessageQueueHandle, &msg, 0, 0);
    }
}

/**
 * @brief 发送系统状态帧（高级封装）
 * @param motor_ctx 电机上下文数组
 * @param sensor 传感器数组指针
 * @param CR  系统状态结构体指针
 * @param state 要上报的状态字节
 */
void cmd_packer_send_status_frame(const MotorContext *motor_ctx,
								  const GlobalSensor *sensor,
								  const ContinuumRobot *CR,
								  uint8_t state)
{
	uint8_t packed_frame[128];
	uint16_t frame_len = cmd_packer_pack_status_frame(packed_frame, motor_ctx, sensor, CR, state);
	cmd_packer_send_frame_to_queue(packed_frame, frame_len);
}
