/**
 * @file motor.c
 * @brief 电机指令处理模块（基于 MotorContext 架构，支持多电机驱动切换）
 *
 * ...（原注释保留）
 *
 * 通过定义不同的 MOTOR_DRIVER 宏来选择底层驱动：
 *   MOTOR_DRIVER_EMM_V5   -> 使用 Emm_V5 驱动
 *   MOTOR_DRIVER_SCSCL    -> 使用飞特 SCSCL 舵机驱动
 */

// ==================== 电机驱动选择（在此处定义）====================
// #define MOTOR_DRIVER_EMM_V5      // 当前使用 Emm_V5 驱动
#define MOTOR_DRIVER_SCSCL           // 切换到 SCSCL 舵机

#include "Motor.h"
#include "math.h"
#include <stdio.h>

#include "SCS.h"
#include "usart.h"

#if defined(MOTOR_DRIVER_EMM_V5)
    #include "Emm_V5.h"
#elif defined(MOTOR_DRIVER_SCSCL)
    #include "SCSCL.h"
#endif

#include "Common/XV2_cmd_parser.h"
#include "CR/CR.h"
#include "CR/kinematic.h"
#include "string.h"

// 创建电机与电机反馈数据结构体
MotorContext motor_ctx[MOTOR_NUM];

/* ---------- SCSCL 舵机相关参数宏（根据实际舵机规格调整） ---------- */
#define SCSCL_ANGLE_RANGE       300.0f   // 舵机总行程（度），例如 0~300°
#define SCSCL_POS_MAX           1023     // 位置值最大值
#define SCSCL_POS_MID           512      // 中位位置值（对应角度 0°）
#define SCSCL_MAX_RPM           60.0f    // 舵机额定最大转速（RPM），用于速度转换

// 角度单位变成 0.01 度，比如 90.00 度写成 9000
// 支持多圈：输入任意角度（百分度），输出单圈编码值 0~4095，其中 0°→0，180°→2048，360°→4096（实际0）
static inline uint16_t angle_to_scscl_pos_int(int32_t angle_cdeg)
{
	// 取模 36000（即 360°），得到单圈内的百分度
	int32_t mod = angle_cdeg % 36000;
	if (mod < 0) mod += 36000;   // 处理负角度

	// 线性映射：0~36000 → 0~4096，四舍五入
	// 使用 (mod * 4096 + 18000) / 36000 实现四舍五入
	int32_t pos = (mod * 4096 + 18000) / 36000;

	// 当 mod=36000 时，pos=4096，但编码值应回绕到 0
	if (pos >= 4096) pos = 0;

	return (uint16_t)pos;
}

// rpm_crpm 单位是 0.01 转/分钟，比如 50.00 rpm 写成 5000
static inline uint16_t rpm_to_scscl_speed_int(int32_t rpm_crpm)
{
	int32_t abs_rpm = (rpm_crpm > 0) ? rpm_crpm : -rpm_crpm;
	if (abs_rpm > (int32_t)(SCSCL_MAX_RPM * 100)) abs_rpm = (int32_t)(SCSCL_MAX_RPM * 100);
	return (uint16_t)(abs_rpm * 1023 / (SCSCL_MAX_RPM * 100));
}

/* ==================== 初始化 ==================== */
void motor_init()
{
    for (int i = 0; i < MOTOR_NUM; i++) {
        GlobalMotor *gm = &motor_ctx[i].global_motor;
        gm->id = MOTOR_ID + i;
        gm->motor.stepper_motor.daocheng = 2;
        gm->motor.stepper_motor.xifen = 128;
        gm->motor.stepper_motor.step_angle = 1.8f;
        gm->motor.stepper_motor.target_vel = 10.0f;
        gm->motor.stepper_motor.current_vel = 10.0f;
        gm->vel_max = 120.0f;
        gm->current_acc = 0.0f;

        // 初始化每个电机的串口解析器
        X_V2_SerialParser_Init(&motor_ctx[i].parser);

        #if defined(MOTOR_DRIVER_SCSCL)
            // 可选：上电时使能所有舵机扭矩（根据需求启用）
            // EnableTorque(gm->id, 1);
        #endif
    }
}

/* ==================== 使能控制 ==================== */
void motor_enable(uint8_t addr, bool enable)
{
#if defined(MOTOR_DRIVER_EMM_V5)
    Emm_V5_En_Control(addr, enable, 0);
#elif defined(MOTOR_DRIVER_SCSCL)
    // 使用 EnableTorque 控制舵机扭矩（enable=1 使能，0 失能）
    // EnableTorque(addr, enable ? 1 : 0);
	if (1) // 后续修改为ping的方式
	{
		for (uint8_t i = 0; i < MOTOR_NUM; i++)
		{
			if (motor_ctx[i].global_motor.id == addr) motor_ctx[i].global_motor.state = 1;
		}
	}
#endif
}

/* ==================== 自动校准（占位） ==================== */
void motor_auto_calibrate(uint8_t idx)
{
#if defined(MOTOR_DRIVER_EMM_V5)
    Emm_V5_Calibrate(motor_ctx[idx].global_motor.id);
#elif defined(MOTOR_DRIVER_SCSCL)
    // 舵机一般无自动校准，可留空
#endif
    HAL_Delay(40000);
}

/* ==================== 单电机绝对位置控制 ==================== */
void motor_run(int idx, float vel_rpm, float target, uint8_t snf)
{
    if (idx < 0 || idx >= MOTOR_NUM) return;
    GlobalMotor *gm = &motor_ctx[idx].global_motor;
    gm->target_vel = vel_rpm;
    gm->target_pos = target;
#if defined(MOTOR_DRIVER_EMM_V5)
	StepperMotor *sm = &gm->motor.stepper_motor;
	sm->target_vel = vel_rpm;
	sm->target_pos = target;
    float step_angle = sm->step_angle;
    float xifen = sm->xifen;
    uint8_t dir = (target >= 0) ? 0 : 1;
    float angle_abs = fabsf(target);
    uint32_t clk = (uint32_t)(angle_abs / step_angle * xifen);
    uint16_t acc = 10;
    Emm_V5_Pos_Control(gm->id, dir, (uint16_t)vel_rpm, acc, clk, 1, snf);

#elif defined(MOTOR_DRIVER_SCSCL)
    // 将目标角度转换为舵机位置值，速度转换为舵机速度字
	uint16_t pos = angle_to_scscl_pos_int((uint32_t)(target * 100));
	uint16_t speed = rpm_to_scscl_speed_int((uint32_t)(vel_rpm * 100));
    WritePos(gm->id, pos, 0, speed);
#endif
}

/* ==================== 速度模式控制 ==================== */
void motor_run_velocity_mode(uint8_t idx, float vel_rpm, uint16_t acc_rpm_s) {
    if (idx < 0 || idx >= MOTOR_NUM) return;

    GlobalMotor *gm = &motor_ctx[idx].global_motor;

#if defined(MOTOR_DRIVER_EMM_V5)
    uint8_t dir = (vel_rpm >= 0) ? 0 : 1;
    float abs_vel = fabsf(vel_rpm);
    Emm_V5_Vel_Control(gm->id, dir, abs_vel, 10, false);
    gm->target_vel = vel_rpm;

#elif defined(MOTOR_DRIVER_SCSCL)
    /**
     * 注意：飞特 SCSCL 舵机通常没有直接的速度环模式。
     * 若需实现连续转动，可考虑：
     *  1. 使用 PWMMode() + WritePWM() 切换到 PWM 模式控制（需事先切换）
     *  2. 不断更新目标位置（很消耗资源）
     * 此处暂以空实现保留接口，避免编译错误。
     * 实际使用时可根据需求重新设计。
     */
    (void)acc_rpm_s;   // 未使用参数
    gm->target_vel = vel_rpm;   // 记录速度，但不执行动作
#endif
}

/* ==================== 紧急停止所有电机 ==================== */
void motor_stop_all()
{
    for (int i = 0; i < MOTOR_NUM; i++) {
        GlobalMotor *gm = &motor_ctx[i].global_motor;
#if defined(MOTOR_DRIVER_EMM_V5)
        Emm_V5_Stop_Now(gm->id, false);

#elif defined(MOTOR_DRIVER_SCSCL)
        // 快速停止：读取当前位置，并立即以最大速度设为当前目标位置
        int cur_pos = ReadPos(gm->id);
        if (cur_pos >= 0) {
            WritePos(gm->id, (uint16_t)cur_pos, 0, 1023);  // 最大速度维持当前位置
        } else {
            // 读取失败时，失能扭矩使电机无力（作为后备）
            EnableTorque(gm->id, 0);
        }
#endif
        gm->state = 0;
    }
}

/* ==================== 单电机相对位移控制 ==================== */
void motor_single_control(uint8_t idx, uint8_t direction, float angle_abs, float angle_vel)
{
    if (idx >= MOTOR_NUM) return;
    float angle = (direction == 0) ? angle_abs : -angle_abs;
    motor_run(idx, angle_vel, angle, 0);
}

/* ==================== 多电机同步控制 ==================== */
void motor_sync_control(uint8_t count, uint8_t start_idx, float distance[])
{
    if (start_idx + count > MOTOR_NUM) return;

    float max_abs_dis = 0.0f;
    for (uint8_t i = 0; i < count; i++) {
        float abs_dis = fabsf(distance[i]);
        if (abs_dis > max_abs_dis) max_abs_dis = abs_dis;
    }

#if defined(MOTOR_DRIVER_EMM_V5)
    // 原步进电机流程：逐轴预置命令，最后同步触发
    for (uint8_t i = 0; i < count; i++) {
        uint8_t idx = start_idx + i;
        GlobalMotor *gm = &motor_ctx[idx].global_motor;
        float ratio = (max_abs_dis > 0) ? fabsf(distance[i]) / max_abs_dis : 0.0f;
        float vel_rpm = ratio * gm->vel_max;
        float angle = distance[i] * 360.0f / gm->motor.stepper_motor.daocheng;
        motor_run(idx, vel_rpm, angle, 1);
        HAL_Delay(8);
    }
    Emm_V5_Synchronous_motion(0);
    HAL_Delay(10);

#elif defined(MOTOR_DRIVER_SCSCL)
    // 使用飞特 SyncWritePos 实现多舵机同步
    // ✅ 优化：减小栈分配，避免溢出
    // 使用动态大小数组，而不是固定分配 MOTOR_NUM 大小
    uint8_t  id_arr[count];    // 只分配需要的大小
    uint16_t pos_arr[count];
    uint16_t time_arr[count];
    uint16_t speed_arr[count];

    for (uint8_t i = 0; i < count; i++) {
        uint8_t idx = start_idx + i;
        GlobalMotor *gm = &motor_ctx[idx].global_motor;
        float ratio = (max_abs_dis > 0) ? fabsf(distance[i]) / max_abs_dis : 0.0f;
        float vel_rpm = ratio * gm->vel_max;
        float angle = distance[i] / (float) (2 * M_PI * 5);

        id_arr[i] = gm->id;
        pos_arr[i] = angle_to_scscl_pos_int(angle*100);
        speed_arr[i] = rpm_to_scscl_speed_int(vel_rpm*100);
        time_arr[i] = 0;      // 不指定时间
    }

    // 同步写入所有舵机
    SyncWritePos(id_arr, count, pos_arr, time_arr, speed_arr);
    HAL_Delay(10);
#endif
}

/* ==================== 运动学控制接口 ==================== */
// void motor_kinematic_control(Kinematic kinematic, uint8_t R, float theta[], int dir[], float deltaL[])
// {
//     float phi[2];
//     motor_sync_control(MOTOR_NUM, 0, deltaL);
// }

/* ==================== 角度/位移换算 ==================== */
// 以下两个函数为纯数学运算，与底层驱动无关，无需修改
float motor_angle_to_displacement(uint8_t motor_index, float angle)
{
    if (motor_index >= MOTOR_NUM) return 0.0f;
    GlobalMotor *gm = &motor_ctx[motor_index].global_motor;
    StepperMotor *sm = &gm->motor.stepper_motor;

    float steps_per_rev = 360.0f / sm->step_angle * sm->xifen;
    float steps = angle / 360.0f * steps_per_rev;
    float displacement = steps * sm->daocheng / steps_per_rev;

    sm->current_pos = displacement;
    gm->current_pos = angle * (float)(3.1415926f / 180.0f);
    return displacement;
}

float motor_displacement_to_angle(uint8_t motor_index, float displacement)
{
    if (motor_index >= MOTOR_NUM) return 0.0f;
    GlobalMotor *gm = &motor_ctx[motor_index].global_motor;
    StepperMotor *sm = &gm->motor.stepper_motor;

    float steps_per_rev = 360.0f / sm->step_angle * sm->xifen;
    float steps = displacement * steps_per_rev / sm->daocheng;
    float angle = steps / steps_per_rev * 360.0f;

    sm->current_pos = displacement;
    gm->current_pos = angle * (float)(3.1415926f / 180.0f);
    return angle;
}
/* ==================== 状态检查 ==================== */
void motor_status_check(void)
{
    for (int i = 0; i < MOTOR_NUM; i++) {
        GlobalMotor *gm = &motor_ctx[i].global_motor;
#if defined(MOTOR_DRIVER_EMM_V5)
        Emm_V5_Read_Sys_Params(gm->id, S_CPOS);
        HAL_Delay(10);
        Emm_V5_Read_Sys_Params(gm->id, S_VEL);
#elif defined(MOTOR_DRIVER_SCSCL)
        // 读取舵机当前位置和速度（速度可能为负载值，需根据需求选择）
        int pos = ReadPos(gm->id);
        int speed = ReadSpeed(gm->id);
        gm->current_pos = (float)(pos) / 4096.0f * 360.0f;  // 12位编码器 → 角度
    	gm->current_vel = (float)(speed & 0x7FFF) * 0.1f;    // 15位速度值 → °/s
    	//  uint8_t data[2];
    	//  data[0] = pos & 0xFF;
    	//  data[1] = (pos >> 8) & 0xFF;

#endif
        HAL_Delay(10);
    }
}

/* ==================== 完整控制接口 ==================== */
void motor_full_control(uint8_t idx, uint8_t dir, float dist, float velocity, float acceleration)
{
    if (idx >= MOTOR_NUM) return;
    GlobalMotor *gm = &motor_ctx[idx].global_motor;
    float displacement = (dir == 0) ? -dist : dist;

    gm->motor.stepper_motor.target_pos = gm->motor.stepper_motor.current_pos + displacement;
    gm->motor.stepper_motor.target_vel = velocity;
    gm->motor.stepper_motor.current_acc = acceleration;

    motor_run(idx, velocity / 100.0f, gm->motor.stepper_motor.current_pos + displacement, 0);
}