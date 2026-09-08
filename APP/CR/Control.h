//
// Created by q3634 on 2026/8/12.
//

#ifndef CONTROLSYSTEM_STM32H7_CONTROL_H
#define CONTROLSYSTEM_STM32H7_CONTROL_H
#include "math.h"

typedef struct
{
	float Kp, Ki, Kd;
	float integral;
	float prev_error;
	float output_max; // 限幅（防止积分饱和）
	float output_min;
} PID_Handle;

void Control_Init(PID_Handle *pid, float kp, float ki, float kd, float min_out, float max_out);
float PID_Update(PID_Handle *pid, float setpoint, float measurement, float dt);

#endif //CONTROLSYSTEM_STM32H7_CONTROL_H