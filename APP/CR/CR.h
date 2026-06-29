#ifndef __CR_H
#define __CR_H

#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>
/**********************************************************
***	编写作者：blinest

***	qq：1071378062
**********************************************************/

// 数学常数
#define PI 3.14159265358979323846

// 向量类型 (3D)
typedef struct { double x, y, z; } vec3_t;

typedef struct CR_Parameter
{
	float r[3];
} CR_Parameter;

typedef struct JointSpace
{
	float target_phi;
	float target_theta[3];
	float total_target_theta;
	float current_phi;
	float current_theta[3];
	float deltaL[9];
} JointSpace;

typedef struct OperationSpace
{
	float scale;
}OperationSpace;

typedef struct ArmParams
{
	float L;//每段长度
	float tendon_preload; // 预紧力
	float friction_coeff; // 摩擦系数

	float backbone_stiffness; //臂体弯曲刚度
	float material_damping; //材料阻尼系数

	float calibrate_offset[3]; // 肌腱零点偏移量
	float direction_gain[4]; //方向增益，对应(u,r,d,l)

	// 非线性补偿系数: commanded_deg = a * desired_deg + b * desired_deg^2
	float calib_a[4];  // 一次项系数，对应(u,r,d,l)
	float calib_b[4];  // 二次项系数，对应(u,r,d,l)
	float calib_max_ratio;  // 安全限幅上限
	float calib_min_ratio;  // 安全限幅下限
} ArmParams;

typedef struct ContinuumRobot
{
	JointSpace joint_space;
	OperationSpace operation_space;
	CR_Parameter parameter;
	ArmParams arm_params[2];
	bool state;
} ContinuumRobot;

void CR_init(void);
uint8_t armBend(int seg, char direction, float val);
uint8_t armBend_edit(int seg, char direction, float val, float g_u, float g_r, float g_d, float g_l, float seg_limit[3]);
void deltaL_update(void);
void auto_straight(void);
void armRotate(float theta_deg, float step_deg);
void action_group_demo(void);
void scale_squared(uint8_t direction, float val);

extern ContinuumRobot CR;
#endif
