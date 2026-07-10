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
	float r_bias;
	float ref_phi;          // 目标旋转角（用户输入，度）
	float ref_theta[3];     // 目标弯曲角（用户输入，度）
	float model_phi;        // 模型输入旋转角（补偿后，弧度）
	float model_theta[3];   // 模型输入弯曲角（补偿后，弧度）
	float total_model_theta; // 总补偿角（上位机反馈，度）
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


	// 非线性补偿系数: cmd_deg = a * desired + b * desired^2
	float calib_a;  // 一次项系数
	float calib_b;  // 二次项系数
	float calib_max_ratio;  // 安全限幅上限
	float calib_min_ratio;  // 安全限幅下限
} ArmParams;

typedef struct ContinuumRobot
{
	JointSpace joint_space;
	OperationSpace operation_space;
	CR_Parameter parameter;
	ArmParams arm_params[3];
	bool state;
} ContinuumRobot;

void CR_init(void);
uint8_t armBend(int seg, char direction, float val);
uint8_t armBend_total(char direction, float total_val);
uint8_t segBend(int seg, char direction, float val);
void deltaL_update(void);
void auto_straight(void);
void armRotate(float theta_deg, float step_deg);
void action_group_demo(void);
void scale_squared(uint8_t direction, float val);

extern ContinuumRobot CR;
#endif
