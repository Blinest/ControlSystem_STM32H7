/**
* @file cmd_packer.h
 * @brief 指令打包库头文件
 *
 * 负责将全局结构体中的数据打包成数据包，发送给上位机
 *
 * @date 2026-04-23
 * @author blin
 */

#ifndef CMD_PACKER_H
#define CMD_PACKER_H

#include <stdint.h>
#include "Motor/Motor.h"
#include "Sensor/Sensor.h"
#include "CR/CR.h"

uint16_t cmd_packer_pack_status_frame(uint8_t *frame,
									  const MotorContext *motor_ctx,
									  const GlobalSensor *sensor,
									  const ContinuumRobot *CR,
									  uint8_t state);

void cmd_packer_send_frame_to_queue(uint8_t *frame, uint16_t frame_len);

void cmd_packer_send_status_frame(const MotorContext *motor_ctx,
								  const GlobalSensor *sensor,
								  const ContinuumRobot *CR,
								  uint8_t state);

#endif