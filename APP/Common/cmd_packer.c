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

/**
 * @brief 打包系统状态帧 (test_frame 格式)
 * @param frame 存储打包后的数据缓冲区
 * @param motor 电机全局结构体
 * @param sensor 传感器全局结构体
 * @param scale 缩放比例
 * @param state 系统状态
 * @return 打包后的总长度
 */
uint16_t cmd_packer_pack_status_frame(uint8_t* frame, GlobalMotor motor[MOTOR_NUM], GlobalSensor sensor[SENSOR_NUM], const ContinuumRobot *lqts, uint8_t state) {
	uint16_t idx = 0;
    frame[idx++] = 0xAA; // 帧头，用于设备标识
    frame[idx++] = 0x02; // 设备标识

	// 预留数据长度字节，稍后回填
	uint16_t data_length_pos = idx;
	idx += 1;

    frame[idx++] = MOTOR_NUM;
    frame[idx++] = SENSOR_NUM;

    for (int i = 0; i < MOTOR_NUM; i++) {
        int16_t pos = (int16_t)(motor[i].stepper_motor.current_pos * 100); // mm
        int16_t vel = (int16_t)(motor[i].stepper_motor.current_vel * 100); // mm/s
        int16_t acc = (int16_t)(motor[i].stepper_motor.current_acc * 100); // mm/s^2
    	// 调试用 4.12
    	// int16_t pos = (int16_t)(motor[i].stepper_motor.target_pos * 100); // mm
    	// int16_t vel = (int16_t)(motor[i].stepper_motor.target_vel * 100); // mm/s
    	// int16_t acc = (int16_t)(motor[i].stepper_motor.current_acc * 100); // mm/s^2
    	int8_t motor_state = (int8_t)(motor[i].state);
        frame[idx++] = (pos >> 8) & 0xFF; frame[idx++] = pos & 0xFF;
        frame[idx++] = (vel >> 8) & 0xFF; frame[idx++] = vel & 0xFF;
        frame[idx++] = (acc >> 8) & 0xFF; frame[idx++] = acc & 0xFF;
    	frame[idx++] = motor_state & 0xFF;
    }
    for (int i = 0; i < SENSOR_NUM; i++) {
        int16_t x = (int16_t)(sensor[i].x * 100) ;
        int16_t y = 0;
        int16_t z = 0;
        frame[idx++] = (x >> 8) & 0xFF; frame[idx++] = x & 0xFF;
        frame[idx++] = (y >> 8) & 0xFF; frame[idx++] = y & 0xFF;
        frame[idx++] = (z >> 8) & 0xFF; frame[idx++] = z & 0xFF;
    }
    // scale
    int16_t s_val = (int16_t)(lqts->operation_space.scale * 100);
    frame[idx++] = (s_val >> 8) & 0xFF; frame[idx++] = s_val & 0xFF;
	// armbend
	int16_t s_val2 = (int16_t)(lqts->joint_space.total_model_theta * 100);
	frame[idx++] = s_val2 >> 8; frame[idx++] = s_val2 & 0xFF;
    frame[idx++] = state;

	frame[data_length_pos] = (uint8_t)(idx-3);

    // 校验和
    uint16_t checksum = 0;
    for (int i = 0; i < idx; i++) {
        checksum += frame[i];
    }
    frame[idx++] = checksum & 0xFF;

    return idx;
}
