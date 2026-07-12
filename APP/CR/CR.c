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

#define pi 3.1415926535

// ===== 分段式角度分配参数 =====
// 每段独立分配 20° 输入到达肌腱限位，保证电机位移：
// 20°→M0≈24.2mm, 40°→M3≈51.3mm, 60°→M6≈100.8mm
// 0~20°: 段1独占, 段2=段3=0
// 20~40°: 段1=22°(clamp), 段2独占剩余, 段3=0
// 40~60°: 段1=22°, 段2=23°, 段3独占剩余
#define SEG_INPUT_LIMIT 20.0f  // 每段独立输入的触限角度

// 三段肌腱行程限（mm）：压缩侧M2/5/8的物理限，与r_bias解耦
#define SEG_LENGTH1 25.0f
#define SEG_LENGTH2 28.0f
#define SEG_LENGTH3 51.2f

// 三段实际弯曲角度机械限值（度）
#define ACTUAL_LIMIT1 20.0f
#define ACTUAL_LIMIT2 40.0f
#define ACTUAL_LIMIT3 60.0f

// 总角度安全限值
#define TOTAL_ANGLE_LIMIT 120.0f


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
    CR.joint_space.model_theta[0] = 0;
    CR.joint_space.model_theta[1] = 0;
    CR.joint_space.model_theta[2] = 0;

    CR.parameter.r[0] = 70;
    CR.parameter.r[1] = 75;
    CR.parameter.r[2] = 80;

    // 初始化每段独立补偿系数（分段式分配，每段独立20°输入触限）
    // A = limit_deg / SEG_INPUT_LIMIT, B=0纯线性
    CR.arm_params[0].calib_a = 22.0f / SEG_INPUT_LIMIT;  // 1.1
    CR.arm_params[0].calib_b = 0.0f;
    // 段2 (R=75)
    CR.arm_params[1].calib_a = 23.0f / SEG_INPUT_LIMIT;  // 1.15
    CR.arm_params[1].calib_b = 0.0f;
    // 段3 (R=80)
    CR.arm_params[2].calib_a = 39.4f / SEG_INPUT_LIMIT;  // 1.97
    CR.arm_params[2].calib_b = 0.0f;

    CR.joint_space.r_bias = 0.8f;

    for (int i = 0; i < 3; i++) {
        CR.arm_params[i].calib_max_ratio = 5.0f;
        CR.arm_params[i].calib_min_ratio = 0.3f;
    }

    motor_init();
    sensor_init();
}


// 用于控制喷管弯曲（独立段控制，直接驱动某个段）
uint8_t armBend(int seg, char direction, float val)
{
    return segBend(seg, direction, val);
}

// 总角度弯曲（分段式分配，每段独占20°输入后触限）
uint8_t armBend_total(char direction, float total_val)
{
    if (total_val < 0 || total_val > TOTAL_ANGLE_LIMIT) return 1;

    float rem = total_val;
    float seg_input[3] = {0};

    for (int i = 0; i < 3 && rem > 0; i++) {
        float take = (rem > SEG_INPUT_LIMIT) ? SEG_INPUT_LIMIT : rem;
        seg_input[i] = take;
        rem -= take;
    }

    CR.joint_space.ref_theta[0] = seg_input[0];
    CR.joint_space.ref_theta[1] = seg_input[1];
    CR.joint_space.ref_theta[2] = seg_input[2];

    // 一次性计算三段补偿+驱动（避免 segBend 分三次驱动的机械抖动）
    for (int i = 0; i < 3; i++) {
        if (seg_input[i] > 0) {
            CR.joint_space.model_theta[i] = tendonCompensation(i + 1, seg_input[i]);
        } else {
            CR.joint_space.model_theta[i] = 0;
        }
    }
    // 根据方向设 phi（仅段1方向有意义，段2/段3沿袭）
    float phi = 0;
    switch (direction) {
        case 'u': phi = 0; break;
        case 'r': phi = pi / 2; break;
        case 'd': phi = pi; break;
        case 'l': phi = 3 * pi / 2; break;
        default: return 1;
    }
    CR.joint_space.model_phi = phi;
    deltaL_update();
    motor_sync_control(MOTOR_NUM - 1, 0, CR.joint_space.deltaL);
    return 0;
}

void deltaL_update(void)
{
    // 存储当前位置
    float cur_pos[MOTOR_NUM + 1];

    for(int i = 1; i <= MOTOR_NUM; i++)
    {
        cur_pos[i] = CR.joint_space.deltaL[i];
    }

    calculate_L(CR.parameter.r, CR.joint_space.model_theta, CR.joint_space.model_phi, CR.joint_space.deltaL);
}

void auto_straight(void)
{
    for (int i = 0; i < 3; i++)
    {
        CR.joint_space.model_theta[i] = 0;
    }
    CR.joint_space.model_phi = 0;
	CR.operation_space.scale = 0;
    deltaL_update();
    motor_sync_control(MOTOR_NUM, 0, CR.joint_space.deltaL);
}

/**
 * @brief 臂体360度旋转（同步位置模式，利用总线自然节拍实现准连续运动）
 * @param theta_deg 弯曲角度（度），旋转过程中保持不变
 * @param step_deg  每步旋转角度（度），默认0.25°=1440步/圈
 *
 * 顺滑原理：
 *   motor_sync_control 内部有 ~28ms 的总线时序（9电机×2ms + 10ms同步）。
 *   每步不设额外 osDelay，步长 0.25°(~0.02mm/步)，
 *   电机在前一步减速到位前已收到新目标 → 准连续运动。
 *   一圈自然耗时 1440×28ms ≈ 40s。
 */
void armRotate(float theta_deg, float step_deg)
{
    if (theta_deg < 0) theta_deg = 0;
    if (theta_deg > 90) theta_deg = 90;
    if (step_deg <= 0) step_deg = 0.25f;

    // 重置 r_bias 基线值
    CR.joint_space.r_bias = 0.8f;

    // ===== 1. 分段式分配 + position mode 弯曲到位 =====
    float rem = theta_deg;
    float seg_input[3] = {0};
    for (int i = 0; i < 3 && rem > 0; i++) {
        float take = (rem > SEG_INPUT_LIMIT) ? SEG_INPUT_LIMIT : rem;
        seg_input[i] = take;
        rem -= take;
    }

    for (int i = 0; i < 3; i++) {
        if (seg_input[i] > 0) {
            CR.joint_space.model_theta[i] = tendonCompensation(i + 1, seg_input[i]);
        } else {
            CR.joint_space.model_theta[i] = 0;
        }
    }
    CR.joint_space.model_phi = 0;
    deltaL_update();
    motor_sync_control(9, 0, CR.joint_space.deltaL);
    osDelay(4000);

    // ===== 2. 直通限速模式连续步进（无加减速，无额外延时） =====
    // 画圆时各phi有效总位移不同（R_BIAS导致三组电机不对称），
    float th0 = CR.joint_space.model_theta[0];
    float th1 = CR.joint_space.model_theta[1];
    float th2 = CR.joint_space.model_theta[2];

    for (float phi_deg = step_deg; phi_deg <= 360.0f; phi_deg += step_deg)
    {
        // 增强补偿：超高斯平坦区（100°~260°恒定，平滑滚降）
        // 1. boost: 150%, 超高斯n=6, sigma=50 → 100°~260°平坦, 0°~80°渐降
        //    避免高斯峰形导致135°处boost=1.8但180°处仅0.19的落差
        // 2. asym: +40%, 超高斯n=6, sigma=50, 仅seg2/seg3 (段1的M2 25mm限)
        // 3. 动态r_bias: 135°/225°+180°双重降低至0.0
        // 4. seg_shift: 20%段1→seg2/seg3, 超高斯n=6, sigma=50
        float phi_rad = phi_deg * pi / 180.0f;
        float d135 = fabsf(phi_deg - 135.0f);
        float d225 = fabsf(phi_deg - 225.0f);
        float dmin = fminf(d135, d225);
        float d180 = fabsf(phi_deg - 180.0f);
        float d_wide = fminf(dmin, d180);
        // 超高斯: exp(-(d/sigma)^6) — 平坦峰顶
        float boost_g = expf(-powf(d_wide / 50.0f, 6.0f));
        float boost = 1.80f * boost_g;
        float asym = 0.40f * boost_g;
        float b1 = 1.0f * expf(-(dmin * dmin) / (40.0f * 40.0f));
        float b2 = 1.0f * expf(-(d180 * d180) / (30.0f * 30.0f));
        float b = fminf(b1 + b2, 1.0f);
        float seg_shift = 0.20f * expf(-powf(d_wide / 50.0f, 6.0f));
        CR.joint_space.r_bias = 0.8f - 0.8f * b;
        float th0_comp = th0 * (1.0f + boost);
        float th_add = th0_comp * seg_shift;
        CR.joint_space.model_theta[0] = th0_comp - th_add;
        CR.joint_space.model_theta[1] = th1 * (1.0f + boost + asym * 0.75f) + th_add * 0.75f;
        CR.joint_space.model_theta[2] = th2 * (1.0f + boost + asym * 0.25f) + th_add * 0.25f;
        CR.joint_space.model_phi = phi_deg * pi / 180.0f;
        deltaL_update();
        motor_sync_bypass(9, 0, CR.joint_space.deltaL);
    }

    // ===== 3. 归零 =====
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
    const float angle = 10.0f;  // 弯曲角度（度）

    // 1. 360度旋转（同步位置模式，0.25°步长，无额外延时）
    armRotate(10.0f, 0.25f);

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
    // armBend(1, 'l', angle);
    // osDelay(3000);

    // 7. 回零
    // auto_straight();
    // osDelay(2000);

    // 8. 向右弯曲
    // armBend(1, 'r', angle);
    // osDelay(3000);

    // 9. 回零
    // auto_straight();
    // osDelay(2000);
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

uint8_t segBend(int seg, char direction, float val)
{
    // 节段检查
    if(seg < 1 || seg > 3) return 1;
    if (val < 0) return 1;

    // 使用肌腱补偿器: ref_angle → model_angle
    float model_theta_rad = 0;
    if(tendon_comp) {
       model_theta_rad = tendonCompensation(seg, val);
    } else {
        model_theta_rad = val * pi / 180.0;
    }

    // 补偿后角度安全检查（不应触发，因补偿系数已按肌腱行程设计）
    float model_theta_deg = model_theta_rad * 180.0 / pi;
    // 限值：使用独立偏置系数0.07计算模型角度，和calculate_L的r_bias解耦
    float tendon_limit_rad;
    switch(seg) {
        case 1: tendon_limit_rad = SEG_LENGTH1 / ((1.0f - 0.07f) * CR.parameter.r[0]); break;
        case 2: tendon_limit_rad = SEG_LENGTH2 / ((1.0f - 0.07f) * CR.parameter.r[1]); break;
        case 3: tendon_limit_rad = SEG_LENGTH3 / ((1.0f - 0.07f) * CR.parameter.r[2]); break;
        default: tendon_limit_rad = SEG_LENGTH1 / ((1.0f - 0.07f) * CR.parameter.r[0]); break;
    }
    float tendon_limit_deg = tendon_limit_rad * 180.0 / pi;
    if (model_theta_deg > tendon_limit_deg) {
        model_theta_rad = tendon_limit_rad;
    }

    // 设置 phi 角度
    float phi = 0;
    switch (direction)
    {
        case 'u': phi = 0; break;
        case 'r': phi = pi / 2; break;
        case 'd': phi = pi; break;
        case 'l': phi = 3 * pi / 2; break;
        default: return 1;
    }
    // 补偿后的角度直接赋给对应段（独立段控制，无三段分配）
    CR.joint_space.model_theta[seg-1] = model_theta_rad;
    CR.joint_space.model_phi = phi;
    deltaL_update();

    // 校验 + 驱动步进电机
    for (int i = 0; i < 9; i++) {
        if (isnan(CR.joint_space.deltaL[i]) || isinf(CR.joint_space.deltaL[i]))
            CR.joint_space.deltaL[i] = 0.0f;
    }
    motor_sync_control(MOTOR_NUM - 1, 0, CR.joint_space.deltaL);
    return 0;
}