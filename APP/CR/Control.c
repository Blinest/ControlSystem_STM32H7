#include "Control.h"
#include <stddef.h>

void Control_Init(PID_Handle *pid, float kp, float ki, float kd, float min_out, float max_out) {
	pid->Kp = kp;
	pid->Ki = ki;
	pid->Kd = kd;
	pid->integral = 0.0f;
	pid->prev_error = 0.0f;
	pid->output_min = min_out;
	pid->output_max = max_out;
}

float PID_Update(PID_Handle *pid, float setpoint, float measurement, float dt)
{
	if (pid == NULL) {
		return 0.0f;
	}

	if (dt <= 0.0f) {
		dt = 1.0f;
	}

	float error = setpoint - measurement;
	float proportional = pid->Kp * error;

	pid->integral += error * dt;
	float integral_term = pid->Ki * pid->integral;

	float derivative = (error - pid->prev_error) / dt;
	float derivative_term = pid->Kd * derivative;
	pid->prev_error = error;

	float output = proportional + integral_term + derivative_term;
	if (output > pid->output_max) {
		output = pid->output_max;
	} else if (output < pid->output_min) {
		output = pid->output_min;
	}

	return output;
}
