//
// Created by blin on 2026/3/7.
//

#ifndef CONTROLSYSTEM_MOTOR_H
#define CONTROLSYSTEM_MOTOR_H
#include "stdint.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "Common/XV2_cmd_parser.h"
/**********************************************************
***	编写作者：Lin

***	qq：1071378062
**********************************************************/

#define MOTOR_NUM 4 // 定义步进电机数量
#define MOTOR_ID 1 // 定义电机起始 ID

// 4字节大端转 int32（避免依赖编译器对 inline/C99 的支持）
#define EMMV5_POS_BE_TO_I32(pos_be) \
((int32_t)( \
((uint32_t)((pos_be)[0]) << 24) | \
((uint32_t)((pos_be)[1]) << 16) | \
((uint32_t)((pos_be)[2]) << 8)  | \
((uint32_t)((pos_be)[3]) << 0)  \
))

// ==================== 步进电机参数 ====================
typedef struct
{
	uint8_t daocheng;
	uint16_t xifen;
	float step_angle;
	float current_pos; // mm
	float target_pos; // mm
	float current_vel; // mm/s
	float target_vel; // mm/s
	float current_acc; //mm/s^2
} StepperMotor;
// ==================== 舵机参数 ====================
typedef struct ServoMotor
{
	float current_ang; // mm
	float target_ang; // mm
	float current_ang_vel; // mm/s
	float target_ang_vel; // mm/s
} ServoMotor;

typedef union Motor
{
	ServoMotor servo_motor;
	StepperMotor stepper_motor;
} Motor;

// ==================== 全局电机结构体 ====================
typedef struct GlobalMotor
{
	int id;
	bool state;
	Motor motor;
	float current_pos; // rad
	float current_vel; // rpm
	float current_acc; // rad/s^2
	float target_pos;
	float target_vel;
	float target_acc;
	float vel_max;
	uint8_t size;
} GlobalMotor;

typedef struct MotorContext
{
	GlobalMotor global_motor;
	X_V2_SerialParser parser;
} MotorContext;
extern MotorContext motor_ctx[MOTOR_NUM];
/**
 * 获取电机索引指针
 * @param addr
 * @return 电机索引指针
 */
static inline MotorContext* Motor_GetContextByAddr(uint8_t addr) {
	if (addr >= MOTOR_ID && addr < MOTOR_ID + MOTOR_NUM)
		return &motor_ctx[addr - MOTOR_ID];
	return NULL;
}

/**
 * 通过电机索引号获取电机指针
 * @param idx
 * @return 电机指针
 */
static inline MotorContext* Motor_GetContextByIdx(int idx) {
	if (idx >= 0 && idx < MOTOR_NUM)
		return &motor_ctx[idx];
	return NULL;
}
void motor_init();
void motor_run(int idx, float vel, float target, uint8_t snf);
void motor_enable(uint8_t addr, bool enable);
void motor_stop_all();
void motor_single_control(uint8_t idx, uint8_t direction, float distance, float vel);
void motor_sync_control(uint8_t count, uint8_t start_addr, float distance[]);

typedef void (*Kinematic)(uint8_t R, float theta, float phi, float deltaL[]);
void motor_kinematic_control(Kinematic kinematic, uint8_t R, float theta, float phi, float deltaL[]);
void motor_custom_control(uint8_t count, uint8_t *params);
void motor_bend_control(uint8_t direction, float angle);
void motor_scale_control(uint8_t dit, float scale);
void motor_full_control(uint8_t addr, uint8_t dir, float dist, float velocity, float acceleration);

void motor_auto_calibrate(uint8_t addr);
void motor_status_check(void);

float motor_angle_to_displacement(uint8_t motor_index, float angle);
float motor_displacement_to_angle(uint8_t motor_index, float displacement);



#endif //CONTROLSYSTEM_MOTOR_H