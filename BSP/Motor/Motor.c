/**
* @file motor.c
 * @brief 电机指令处理模块
 *
 * 本模块提供电机指令处理功能：
 * - motor_init()：电机初始化，初始化流程包括电机参数设置、
 * - motor_run()：启动电机，并设置绝对目标位置
 * - motor_position_control_snf()：
 * - motor_emergency_stop_all()：紧急停止所有电机
 *
 */
#include "Motor.h"

#include "math.h"
#include <stdio.h>
#include "usart.h"
#include "cmsis_os2.h"
#include "fdcan.h"
// #include "Emm_V5.h"
#include "X_V2.h"

// 创建电机与电机反馈数据结构体
MotorFeedback motor_feedback[MOTOR_NUM];
GlobalMotor global_motor[MOTOR_NUM];

//电机初始化函数
void motor_init()
{
	// 初始化电机相关参数
	for (int i = 0; i < MOTOR_NUM; i++)
	{
		global_motor[i].id = MOTOR_ID + i;
		global_motor[i].stepper_motor.daocheng = 12; // 根据丝杠导程设置，12mm
		global_motor[i].stepper_motor.xifen = 256; // 平滑控制
		global_motor[i].stepper_motor.step_angle = 1.8; // 步距角
		global_motor[i].stepper_motor.target_vel = 10; // 如果要完成指标，至少是 10mm/s
		global_motor[i].stepper_motor.current_vel = 10;
		global_motor[i].vel_max = 100; // 满足指标要求
		global_motor[i].current_acc = 0; // 由于位移量较小，为提高响应速度，直接启动，不做加减速处理 (0-255)
	}
}

/**
 *
 * @param addr : 电机地址
 * @param enable : 电机是否使能
 */

void motor_enable(uint8_t addr,bool enable)
{
	// 更新电机状态，(电机使能状态 0x02)
	X_V2_En_Control(addr, enable, 0);
}

/**
  * @brief 启动步进电机，并达到指定位置（带限制条件）
  * @param idx: 电机索引
  * @param vel 速度值, mm/s
  * @param target: 目标位置(绝对位置), mm
  * @param snf: 同步标志位，true同步
  */
void motor_run(int idx, float vel, float target, uint8_t snf) {

	// ==================== 执行控制 ====================
	const float daocheng = global_motor[idx].stepper_motor.daocheng;
	// 方向确定
	const int dir = target > 0 ? 0 : 1;

	// 速度计算
	const float vel_rpm = vel * 60.0f / daocheng ;
	const uint16_t vel_rpm_abs = (uint16_t)(fabsf(vel_rpm) + 0.5f);

	// 位置计算
	const float angle = 360.0f  * target / daocheng;
	float angle_abs = fabsf(angle);

	// 更新电机状态
	global_motor[idx].target_vel = vel_rpm;
	global_motor[idx].stepper_motor.target_vel = vel;
	global_motor[idx].target_pos = angle;
	global_motor[idx].stepper_motor.target_pos = target;

	// 梯形加减速位置模式（用于正常弯曲，单次到位）
	uint16_t acc = 200;
	uint16_t dec = 200;
	X_V2_Traj_Pos_Control(global_motor[idx].id, dir, acc, dec, vel_rpm_abs, angle_abs, 1, snf);

}

/**
 * @brief 直通限速位置模式驱动（无加减速，用于画圆等连续步进场景）
 * @param idx     电机索引
 * @param vel     速度 mm/s
 * @param target 目标位置（绝对位置）mm
 * @param snf    同步标志
 */
void motor_run_bypass(int idx, float vel, float target, uint8_t snf) {

	const float daocheng = global_motor[idx].stepper_motor.daocheng;
	const int dir = target > 0 ? 0 : 1;
	const float vel_rpm = vel * 60.0f / daocheng;
	const uint16_t vel_rpm_abs = (uint16_t)(fabsf(vel_rpm) + 0.5f);
	const float angle = 360.0f * target / daocheng;
	float angle_abs = fabsf(angle);

	global_motor[idx].target_vel = vel_rpm;
	global_motor[idx].stepper_motor.target_vel = vel;
	global_motor[idx].target_pos = angle;
	global_motor[idx].stepper_motor.target_pos = target;

	// 直通限速位置模式（无加减速）
	X_V2_Bypass_Pos_LV_Control(global_motor[idx].id, dir, vel_rpm_abs, angle_abs, 1, snf);
}

/**
 * 连续速度模式驱动（画圆等连续旋转场景，无位置步进）
 * @param idx  电机索引
 * @param vel  速度 mm/s（正=收紧/推，负=放松/拉，0=停）
 * @param snf  同步标志
 *
 * 内部把 mm/s 换算成 RPM 下发 X_V2_Vel_Control（acc 用较高值，让速度矢量
 * 平滑连续变化，避免每次微调都触发驱动器内部启停造成顿挫）。
 */
void motor_run_velocity(int idx, float vel, uint8_t snf) {

	const float daocheng = global_motor[idx].stepper_motor.daocheng;
	const uint8_t dir = vel >= 0.0f ? 0 : 1;          // 0=CW, 1=CCW
	const float vel_rpm = fabsf(vel) * 60.0f / daocheng;
	const uint16_t vel_rpm_abs = (uint16_t)(vel_rpm + 0.5f);
	const uint16_t acc = 1000;                          // RPM/s，斜坡足够快

	global_motor[idx].target_vel = (vel >= 0.0f) ? vel_rpm : -vel_rpm;
	global_motor[idx].stepper_motor.target_vel = vel;

	X_V2_Vel_Control(global_motor[idx].id, dir, acc, (float)vel_rpm_abs, snf);
}

/**
 * 多电机停止函数
 *
 */
void motor_stop_all()
{
	for(int i = 0; i < MOTOR_NUM; i++) {
		X_V2_Stop_Now(global_motor[i].id, false);
		global_motor[i].state = 0;
	}
}

/**
 * @brief 单电机控制函数（绝对位置控制）
 * @param idx : 电机索引号
 * @param direction ：电机旋转方向
 * @param distance ：步进电机移动距离 mm
 * @param vel ：步进电机位移速度 mm/s
 */
void motor_single_control(uint8_t idx, uint8_t direction, float distance, float vel)
{
	// 位置换算，0正1负
	float displacement = (direction == 0) ? distance : -distance;

	// 1. 检查所有限制条件 (使用默认速度)：解除限制 4.9
	//float default_speed = global_motor[idx].vel_max * 0.5f; // 使用50%最大速度
	//if (vel > default_speed) vel = default_speed;
	// float val = vel;
	// int int_part = (int)val;
	// int frac_part = (int)((val - int_part) * 100 + 0.5);  // 保留两位小数，四舍五入
	// if (frac_part < 0) frac_part = -frac_part;  // 小数部分取绝对值
	// if (frac_part >= 100) {  // 处理进位，如 1.999 -> 2.00
	// 	int_part += 1;
	// 	frac_part -= 100;
	// }
	//
	// char test[32];
	// int len = snprintf(test, sizeof(test), "%d.%02d", int_part, frac_part);
	// if (len > 0 && len < sizeof(test)) {
	// 	Usart_SendString(&huart1, (uint8_t*)test, len);
	// } else {
	// 	Usart_SendString(&huart1, (uint8_t*)"ERR_FMT\r\n", 9);
	// }

	// 调用底层控制函数
	motor_run(idx, vel, displacement, 0);
}

// 多电机同步控制函数
/**
 *
 * @param count :电机数量
 * @param start_idx ：电机初始索引值
 * @param distance ：电机移动距离数组
 */
void motor_sync_control(uint8_t count, uint8_t start_idx, float distance[])
{
	float max_distance = 0;
	uint16_t speed[MOTOR_NUM];

	// 检查每个电机的位移限制
	for (int i = start_idx; i < count; i++)
	{
		float displacement = distance[i];
		// 计算绝对位移用于速度分配
		float abs_distance = fabsf(displacement);
		max_distance = fmax(max_distance, abs_distance);
	}

	// ==================== 速度分配 ====================
	// 动态速度调整：线性比例控制，考虑每个电机的最大速度限制
	for (int i = start_idx; i < start_idx + count; i++)
	{
		float abs_distance = fabsf(distance[i-start_idx]);
		float ratio = (max_distance > 0) ? (abs_distance / max_distance) : 0;

		// 步进电机速度分配计算
		float vel_max = global_motor[i].vel_max / 60.0f * global_motor[i].stepper_motor.daocheng;

		// 计算当前步进电机速度
		float calculated_speed = ratio * vel_max;
		// 速度下限：位移非零但占比极小（旋转过零点附近侧向肌腱）时，速度会被
		// uint16 截断为 0，造成"非零位置+零速度"到位门卡死；加最小速度保证到达。
		if (calculated_speed > 0.0f && calculated_speed < 1.0f) {
			calculated_speed = 1.0f;            // 最小速度 1 mm/s
		}
		speed[i] =  calculated_speed == 0? (uint16_t)vel_max: (uint16_t)calculated_speed;
		// 储存目标电机位移与速度
		global_motor[i].target_pos = distance[i-start_idx];
		global_motor[i].stepper_motor.target_vel = speed[i];
	}

	// ==================== 执行同步控制 ====================
	// 最后一个电机用于其他控制
	for (int i = start_idx; i < start_idx + count; i++)
	{
		motor_run(i, global_motor[i].stepper_motor.target_vel, global_motor[i].target_pos, true);

		// 等待当前电机的 3 帧全部发出，避免 burst 堵塞
		uint32_t wait = 50000; // ~1ms @480MHz
		while (HAL_FDCAN_GetTxFifoFreeLevel(&hfdcan1) < 3 && wait-- > 0) {
			for (volatile int d = 0; d < 48; d++); // ~100ns
		}
		if (wait == 0) {
			// CAN 堵塞 → 等待后重启恢复
			HAL_FDCAN_Stop(&hfdcan1);
			osDelay(20);  // 等待总线错误帧消散
			HAL_FDCAN_Start(&hfdcan1);
			// 重新激活中断通知
			HAL_FDCAN_ActivateNotification(&hfdcan1, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0);
			HAL_FDCAN_ActivateNotification(&hfdcan1, FDCAN_IT_TX_COMPLETE, 0);
		}

		// 帧组间延迟：给总线时间发送当前电机的帧，再发下一个电机
		osDelay(2);  // 2ms，500kbps 下足够发完 3 帧（~0.7ms）
	}

	// 触发同步控制
	X_V2_Synchronous_motion(0);
	osDelay(10);
}

// ==================== 电机状态定期检查 ====================

/**
 * @brief 电机状态定期检查函数
 *
 * 应定期调用（如每100ms），检查电机状态和限制条件
 */
void motor_status_check(void)
{

    for (int i = 0; i < MOTOR_NUM; i++) {
    	X_V2_Read_Sys_Params(global_motor[i].id, S_CPOS);
    	osDelay(1); // 延时等待响应
    	X_V2_Read_Sys_Params(global_motor[i].id, S_VEL);
    	osDelay(1); // 延时等待响应
    }
}

/**
 * @brief 多电机同步控制 - 直通限速模式（无加减速，用于画圆等连续步进）
 * @param count     电机数量
 * @param start_idx 电机起始索引
 * @param distance  电机位移数组 mm
 */
void motor_sync_bypass(uint8_t count, uint8_t start_idx, float distance[])
{
	float max_distance = 0;

	for (int i = start_idx; i < count; i++)
	{
		float abs_distance = fabsf(distance[i]);
		max_distance = fmax(max_distance, abs_distance);
	}

	for (int i = start_idx; i < start_idx + count; i++)
	{
		float abs_distance = fabsf(distance[i-start_idx]);
		float ratio = (max_distance > 0) ? (abs_distance / max_distance) : 0;
		float vel_max = global_motor[i].vel_max / 60.0f * global_motor[i].stepper_motor.daocheng;
		float calculated_speed = ratio * vel_max;
		// 速度下限：同上，防止位移极小被截断为 0 导致电机卡死
		if (calculated_speed > 0.0f && calculated_speed < 1.0f) {
			calculated_speed = 1.0f;            // 最小速度 1 mm/s
		}
		uint16_t speed = (calculated_speed == 0) ? (uint16_t)vel_max : (uint16_t)calculated_speed;

		global_motor[i].target_pos = distance[i-start_idx];
		global_motor[i].stepper_motor.target_vel = speed;
	}

	for (int i = start_idx; i < start_idx + count; i++)
	{
		motor_run_bypass(i, global_motor[i].stepper_motor.target_vel,
		                 global_motor[i].target_pos, true);

		uint32_t wait = 50000;
		while (HAL_FDCAN_GetTxFifoFreeLevel(&hfdcan1) < 3 && wait-- > 0) {
			for (volatile int d = 0; d < 48; d++);
		}
		if (wait == 0) {
			HAL_FDCAN_Stop(&hfdcan1);
			osDelay(20);
			HAL_FDCAN_Start(&hfdcan1);
			HAL_FDCAN_ActivateNotification(&hfdcan1, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0);
			HAL_FDCAN_ActivateNotification(&hfdcan1, FDCAN_IT_TX_COMPLETE, 0);
		}
		osDelay(2);
	}

	// 触发同步控制 —— 不设额外延时
	X_V2_Synchronous_motion(0);
}
