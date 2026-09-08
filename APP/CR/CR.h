#ifndef __CR_H
#define __CR_H

#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>
#include "Control.h"

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
} ArmParams;


typedef struct ContinuumRobot
{
	JointSpace joint_space;
	OperationSpace operation_space;
	CR_Parameter parameter;
	ArmParams arm_params[3];
	PID_Handle pid;
	bool state;
} ContinuumRobot;

void CR_init(void);
uint8_t armBend(int seg, char direction, float val);
uint8_t armBend_closed_loop(char direction, float target_total_val, float dt, float tolerance, uint8_t max_iterations);
uint8_t armBend_total(char direction, float total_val);
// 旋转（画圆）驱动（阻塞式）：固定弯曲角 total_val，φ 从 0° 按 step_deg 步进，
// 每步等到位后继续，转满一圈返回。
// 旋转（画圆）速度模式：连续速度 + 每圈重锚定 + 行程软限位
uint8_t rotate_vel_tick(void);         // 每 10ms 调用（累加 φ_cmd → 下发速度 → 重锚定），返回满圈标志
void rotate_vel_start(float theta, float omega);   // 启动（theta 弯曲角°，omega 角速度°/s，<=0 用默认）
void rotate_vel_stop(void);            // 停止并停转
uint8_t armRotate(float total_val, float step_deg);
uint8_t segBend(int seg, char direction, float val);
void deltaL_update(void);
void auto_straight(void);
void scale_squared(uint8_t direction, float val);
void auto_calibrate(char direction);
void calibrate_auto_tick(void);

// ===== 循环运动控制（状态机，非阻塞，由 PeriphCtrlTask 每 10ms 驱动） =====
// 状态：空闲 → 上弯 → 保持 → 回零 → 下弯 → 保持 → 回零 → 循环，直到次数耗尽或收到停止。
// 旋转序列：空闲 → 上弯 → 保持 → 旋转一圈 → 回正 → 下一轮上弯 → ... 循环。
typedef enum {
    AG_IDLE = 0,     // 未运行
    AG_BEND_UP,      // 向上弯曲到位
    AG_HOLD_UP,      // 上弯到位后保持
    AG_RESET,        // 回零到位
    AG_BEND_DOWN,    // 向下弯曲到位
    AG_HOLD_DOWN,    // 下弯到位后保持
    AG_ROTATE_STEP,  // 旋转画圆：当前 φ 步进到位，连续旋转
    AG_ROTATE_RESET, // 旋转一圈后回正到位
} ActionGroupState_t;

// 启动动作循环：angle 每轮弯曲角（度），cycles 循环次数（0=无限，受安全上限约束）。
// step_deg<=0 → 弯曲模式（上弯→回零→下弯→回零循环，原行为）；
// step_deg>0  → 旋转序列模式（上弯 angle → 旋转一圈 → 回正 → 循环，每圈由 step_deg 步进）。
void action_group_start(float angle, uint8_t cycles, float step_deg);
// 非阻塞推进（PeriphCtrlTask 每 10ms 调用一次），安全上限内自动停止
void action_group_tick(void);
// 立即停止并回零
void action_group_stop(void);
// 当前状态
ActionGroupState_t action_group_get_state(void);

extern ContinuumRobot CR;
#endif
