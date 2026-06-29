/**
 *上层控制实现，用于处理上层指令解析和执行，提供电机控制和传感器数据读取等功能
 *功能包括：
 *1. 电机控制：基于运动学的电机控制
 *2. 传感器数据读取
 *3. 指令解析：解析上层指令，执行相应的操作，如控制电机、读取传感器数据等
 *4. 样机控制：根据指令控制样机的运动，如弯曲等
 *5. 错误处理：处理指令解析错误、通信错误等情况，确保系统稳定运行
 */

#include "CR.h"
#include "usart.h"
#include "kinematic.h"
#include "Motor/Motor.h"
#include <stdio.h>
#include <stdlib.h>

#include "math.h"
#include "Sensor/Sensor.h"


#define CR_THETA1_MAX 60
#define CR_THETA1_MIN -40
#define CR_THETA2_MAX 60
#define CR_THETA2_MIN -40
#define CR_ANGLE_RANGE 30
#define pi 3.1415926535

/*
 臂体补偿器
*/
bool tendon_comp = true;

/**********************************************************
***	编写作者：blinest

***	qq：1071378062
**********************************************************/

ContinuumRobot CR;
void CR_init(void)
{
    CR.operation_space.scale = 20;

    // 直接对数组元素逐个赋值，不影响结构体其他成员
    CR.joint_space.target_theta[0] = 0;
    CR.joint_space.target_theta[1] = 0;
    CR.joint_space.target_theta[2] = 0;

    CR.parameter.r[0] = 70;
    CR.parameter.r[1] = 75;
    CR.parameter.r[2] = 80;

    // 初始化方向增益（默认1.0，无校准）
    CR.arm_params[0].direction_gain[0] = 1.0f;
    CR.arm_params[0].direction_gain[1] = 1.0f;
    CR.arm_params[0].direction_gain[2] = 1.0f;
    CR.arm_params[0].direction_gain[3] = 1.0f;
    CR.arm_params[1].direction_gain[0] = 1.0f;
    CR.arm_params[1].direction_gain[1] = 1.0f;
    CR.arm_params[1].direction_gain[2] = 1.0f;
    CR.arm_params[1].direction_gain[3] = 1.0f;

    // 初始化非线性补偿系数（默认线性1:1）
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 4; j++) {
            CR.arm_params[i].calib_a[j] = 1.0f;
            CR.arm_params[i].calib_b[j] = 0.0f;
        }
        CR.arm_params[i].calib_max_ratio = 5.0f;
        CR.arm_params[i].calib_min_ratio = 0.3f;
    }

    motor_init();
    sensor_init();
}

// 用于控制喷管弯曲
uint8_t armBend(int seg, char direction, float val)
{
	CR.joint_space.target_theta[0] = direction == 1 ? val : -val;
    return armBend_edit(seg, direction, val, 0, 0, 0, 0, (float[3]){20, 20, 20});
}

void deltaL_update(void)
{
    // 存储当前位置
    float cur_pos[MOTOR_NUM + 1];

    for(int i = 1; i <= MOTOR_NUM; i++)
    {
        cur_pos[i] = CR.joint_space.deltaL[i];
    }

    calculate_L(CR.parameter.r, CR.joint_space.target_theta,CR.joint_space.target_phi,CR.joint_space.deltaL);
}

void auto_straight(void)
{
    for (int i = 0; i < 3; i++)
    {
        CR.joint_space.target_theta[i] = 0;
    }
    CR.joint_space.target_phi = 0;
	CR.operation_space.scale = 0;
    deltaL_update();
    motor_sync_control(MOTOR_NUM, 0, CR.joint_space.deltaL);
}

/**
 * @brief 臂体360度旋转（保持弯曲角度不变，phi从0旋转到2π）
 * @param theta_deg 弯曲角度（度），旋转过程中保持不变
 * @param step_deg  每步旋转角度（度），默认30度=12步完成一圈
 */
void armRotate(float theta_deg, float step_deg)
{
    if (theta_deg < 0) theta_deg = 0;
    if (theta_deg > 90) theta_deg = 90;
    if (step_deg <= 0) step_deg = 30.0f;

    float theta_rad = theta_deg * pi / 180.0f;

    // 1. 先弯曲到指定角度（phi=0），等待到位
    CR.joint_space.target_theta[0] = theta_rad / 3;
    CR.joint_space.target_theta[1] = theta_rad / 2;
    CR.joint_space.target_theta[2] = theta_rad;
    CR.joint_space.target_phi = 0;
    deltaL_update();
    motor_sync_control(9, 0, CR.joint_space.deltaL);
    osDelay(4000);  // 等待弯曲到位（位移大，需要较长时间）

    // 2. 弯曲到位后，逐步旋转 phi（每步只改变旋转角，位移变化小，延时可以短）
    for (float phi_deg = step_deg; phi_deg <= 360.0f; phi_deg += step_deg)
    {
        CR.joint_space.target_phi = phi_deg * pi / 180.0f;
        deltaL_update();
        motor_sync_control(9, 0, CR.joint_space.deltaL);
        osDelay(1000);  // 旋转步进，位移变化小
    }

    // 3. 最后归零（从弯曲状态回到零位，位移大，需要较长时间）
    auto_straight();
    osDelay(5000);
}

/**
 * @brief 动作组演示函数
 *
 * 执行顺序：360°旋转 → 上弯 → 回零 → 下弯 → 回零 → 左弯 → 回零 → 右弯 → 回零
 * 每个动作之间留有延时，确保运动完整执行
 * 所有动作参数在此函数内部定义
 */
void action_group_demo(void)
{
    const float angle = 30.0f;  // 弯曲角度（度）

    // 1. 360度旋转（保持30度弯曲）
    armRotate(30.0f, 30.0f);

    // 2. 向上弯曲
    armBend(1, 'u', angle);
    osDelay(3000);

    // 3. 回零
    auto_straight();
    osDelay(2000);

    // 4. 向下弯曲
    armBend(1, 'd', angle);
    osDelay(3000);

    // 5. 回零
    auto_straight();
    osDelay(2000);

    // 6. 向左弯曲
    armBend(1, 'l', angle);
    osDelay(3000);

    // 7. 回零
    auto_straight();
    osDelay(2000);

    // 8. 向右弯曲
    armBend(1, 'r', angle);
    osDelay(3000);

    // 9. 回零
    auto_straight();
    osDelay(2000);
}


/**
 * @brief 用于控制截面面积收缩
 * @param direction 1正
 * @param val 目前的取值范围为 50.0f-100.0f，百分数
 */
void scale_squared(uint8_t direction, float val)
{
    // 限制条件
    if (val < 75) return;

    // 储存到结构体中
    CR.operation_space.scale = direction == 1? val : -val;
    // 运动学推导
    float R = 50;
    float target;
    float val_sqrt = sqrtf(val) / 10.0f;
    target = 2.0f * pi * (R -  val_sqrt * R);

    motor_run(9, 10, target ,false);
}

uint8_t armBend_edit(int seg, char direction, float val, float g_u, float g_r, float g_d, float g_l, float seg_limit[3])
{
    // 节段、角度限制检查
    if(seg != 1 && seg != 2) return 1;
    if (seg == 1 && (val > seg_limit[0] || val < 0)) return 1;
    if (seg == 2 && (val > seg_limit[1] || val < 0)) return 1;
    if (seg == 2 && (val > seg_limit[2] || val < 0)) return 1;
    float val_rad = val * pi / 180.0;

    // 使用肌腱补偿器
    float compensated_angle_rad = 0;
    if(tendon_comp) {
       compensated_angle_rad = tendonCompensation(seg, direction, val);
    } else {
        compensated_angle_rad = val * pi / 180.0;
    }

    // 检查补偿后的角度是否超出安全范围
    float compensated_deg = compensated_angle_rad * 180.0 / pi;
    float max_angle = (seg == 1) ? 120.0 : 60.0;  // 允许一定的超调，目前第一段臂体可以超调到120°左右
    if (compensated_deg > max_angle) {
        compensated_angle_rad = max_angle * pi / 180.0;
    }

    // 设置 phi 角度，并进行简单的扭转补偿
    float phi = 0;
    switch (direction)
    {
        case 'u': phi = 0; break;
        case 'r': phi = pi / 2 - val_rad * g_r; break;
        case 'd': phi = pi;; break;
        case 'l': phi = 3 * pi / 2 + val_rad * g_l; break;
        default: return 1;
    }
    float compensated_deg_abs = fabs(compensated_deg);
    // 更新补偿后的关节角度
    if (compensated_deg_abs <= 20)
    {
        CR.joint_space.target_theta[0] = compensated_angle_rad;
    } else if (compensated_deg_abs <= 40 && compensated_deg_abs > 20)
    {
        CR.joint_space.target_theta[0] = compensated_angle_rad < 0 ? -20: 20;
        CR.joint_space.target_theta[1] = compensated_angle_rad - CR.joint_space.target_theta[0];
    } else
    {
        CR.joint_space.target_theta[0] = compensated_angle_rad < 0 ? -20: 20;
        CR.joint_space.target_theta[1] = CR.joint_space.target_theta[0];
        CR.joint_space.target_theta[2] = compensated_angle_rad - CR.joint_space.target_theta[0] - CR.joint_space.target_theta[1];
    }
    CR.joint_space.target_phi = phi;
    deltaL_update();

    // 校验 + 驱动步进电机
    for (int i = 0; i < 9; i++) {
        if (isnan(CR.joint_space.deltaL[i]) || isinf(CR.joint_space.deltaL[i]))
            CR.joint_space.deltaL[i] = 0.0f;
    }
    motor_sync_control(MOTOR_NUM - 1, 0, CR.joint_space.deltaL);
    return 0;
}