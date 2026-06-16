#ifndef __KINEMATIC_H
#define __KINEMATIC_H
#include <stdint.h>
#include "Motor/Motor.h"
#include "CR.h"
/**********************************************************
***	编写作者：blinest

***	qq：1071378062
**********************************************************/

void calculate_L(float R[], float theta[], float phi, float deltaL[]);
void forward_kinematics(const robot_params_t *param, const robot_state_t *state,
						vec3_t *pos, double R[3][3]);
void body_point_fk(const robot_params_t *param, const robot_state_t *state,
				   int seg_idx, double eta, vec3_t *point, double R[3][3]);
void jacobian_numerical(const robot_params_t *param, const robot_state_t *state,
						int dof, double J[][dof]);
#endif
