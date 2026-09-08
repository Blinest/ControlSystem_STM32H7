/**
 * 上层控制实现：上层指令解析执行、电机控制、传感器读取、样机运动控制
 */

#include "CR.h"
#include "usart.h"
#include "kinematic.h"
#include "Motor/Motor.h"
#include "Control/ClosedLoop.h"
#include <stdio.h>
#include <stdlib.h>

#include "math.h"
#include "Sensor/Sensor.h"
#include "cmsis_os2.h"

#define pi 3.1415926535

// ===== 分段式角度分配参数 =====
// 级联式分段分配（斜率级联）：各段斜率自分段界起叠加生效，整段分段线性、节点连续。
//   段1: min(rem,20°)×(1+0.4)  → 0~20° 升到 28°，之后保持 28°
//   段2: 0~20° 0.2·θ；20~40° 0.2·θ+0.4·(θ-20)；≥40° 保持 0.2×40+0.4×20 = 16°
//   段3: 0~20° 0.1·θ；20~40° 0.1·θ+0.15·(θ-20)；40~60° 再 +0.8809·(θ-40)；≥60° 保持 ≈29.6°

// 每段独立输入的触限角度（°），后续分段界由它派生
#define SEG_INPUT_LIMIT    20.0f
#define SEG_INPUT_LIMIT_2  (2.0f * SEG_INPUT_LIMIT)   // 40°
#define SEG_INPUT_LIMIT_3  (3.0f * SEG_INPUT_LIMIT)   // 60°

#define SEG1_SLOPE1        0.4f                         // seg1 = min(rem,20°)×(1+SEG1_SLOPE1)
#define SEG2_SLOPE1        0.2f                         // 0~20°: seg2 = SEG2_SLOPE1·θ
#define SEG2_SLOPE2        0.4f                         // 20~40°: + SEG2_SLOPE2·(θ-20)
#define SEG3_SLOPE1        0.1f                         // 0~20°: seg3 = SEG3_SLOPE1·θ
#define SEG3_SLOPE2        0.15f                        // 20~40°: + SEG3_SLOPE2·(θ-20)
#define SEG3_SLOPE3        0.8809f                      // 40~60°: + SEG3_SLOPE3·(θ-40)

// 三段肌腱累计行程限（mm）：压缩侧 M2/M5/M8 以此累计限位（安全限，防堵转）
#define SEG_LENGTH1 40.0f
#define SEG_LENGTH2 70.0f
#define SEG_LENGTH3 90.0f

// 总角度安全限值
#define TOTAL_ANGLE_LIMIT 120.0f

// ===== 端到端逆补偿（查表：命令角→实测角，分段线性逆映射） =====
// 实测标定（IMU 上弯）：
//   命令角 10/20/30/40/50/60° → 实测 11.529/23.62/30.707/37.848/48.247/58.744°
//   小角度过冲(+15~18%)、30°附近准、大角度欠冲(-2~5%)。
// 正常弯曲 bend_calib_gain 按该表查分段线性逆映射 → 命令角×gain 使实测逼近目标。
// 防超调折减：查表增益统一乘以该系数，实测偏保守（略欠冲）以换取不超调。
#define BEND_CALIB_CMD_PER_DEG  1.0f
#define BEND_CALIB_GAIN_SAFETY  0.95f               // 防超调折减系数（<1 保守，>1 激进）

// ===== 自动标定模块前置声明 / 状态 =====
// 标定档位：命令角 0/10/20/30/40/50/60°（增益1.0直通 → IMU 实测）。静态标定数据在 CR_init 填充。
#define CALIB_POINTS_NUM  7u                    // 标定点数（0°+6 档）
#define CALIB_STEP_DEG    10.0f                 // 每档命令角步进（度）
#define CALIB_MAX_CMD_DEG (CALIB_STEP_DEG * (CALIB_POINTS_NUM - 1))   // 最大标定命令角 60°
#define CALIB_SETTLE_MS   1200u                 // 每档等待稳定时长（ms）
// 标定表：命令角→实测角，升序。正常弯曲按此表分段线性插值做逆映射。
static float s_calib_cmd[CALIB_POINTS_NUM];    // 标定点命令角（度）
static float s_calib_meas[CALIB_POINTS_NUM];   // 对应稳定实测角（IMU 读数，度）
static uint8_t s_calib_count = 0;              // 已采标定点数（0=未标定）
// 标定运行状态
static char s_cal_dir = 'u';                   // 标定方向
static uint8_t s_cal_idx = 0;                  // 当前档索引（0=回零档）
static float s_cal_last_cmd = -1.0f;           // 已下发的命令角（防重发）
static uint32_t s_cal_t0 = 0;                  // 当前档开始时刻（ms）
static bool s_calibrating = false;             // 标定进行中

static float bend_calib_gain(float total_val);
static uint8_t armBend_total_core_phi(float phi_rad, float total_val, float gain);
// 方向字符 → φ（弧度）：'u'=0, 'r'=π/2, 'd'=π, 'l'=3π/2
static float dir_to_phi(char direction);


ContinuumRobot CR;

// ===== 闭环控制模块回调（解耦：反馈换算/到位/驱动 均由本层注入） =====
static float closedloop_get_measured_deg(void)
{
    // 读当前实测弯曲角（度）：实机下 global_sensor[0].x 即 IMU 真实角
    return global_sensor[0].x;
}

static bool closedloop_is_busy(void)
{
    // 到位门控：电机速度归零（连续满足若干拍）即判到位，否则重置稳定计数
    static const float SPEED_ZERO_TH  = 0.5f;   // mm/s
    static const uint8_t SETTLE_STEPS = 3u;     // 连续满足次数
    static uint8_t s_settle_count = 0;

    for (int i = 0; i < 9; i++) {
        if (fabsf(global_motor[i].stepper_motor.current_vel) > SPEED_ZERO_TH) {
            s_settle_count = 0;
            return true;
        }
    }

    if (++s_settle_count >= SETTLE_STEPS) {
        s_settle_count = 0;
        return false;
    }
    return true;
}

static void closedloop_apply(char direction, float cmd_deg)
{
    // 闭环 apply 走查表逆补偿（armBend_total 内部乘 bend_calib_gain），
    // 使"命令角"坐标 = 目标实测角：开环第一拍即命中目标附近，消除首拍过冲。
    // PID 仅处理标定残余误差；命令角始终在实测角坐标，两套补偿不冲突。
    armBend_total(direction, cmd_deg);
}

void CR_init(void)
{
    // 闭环控制模块初始化
    static const ClosedLoopCallbacks_t cl_cbs = {
        .get_measured_deg = closedloop_get_measured_deg,
        .is_busy          = closedloop_is_busy,
        .apply            = closedloop_apply,
    };
    ClosedLoop_Init(&cl_cbs);

    CR.operation_space.scale = 20;

    // 直接对数组元素逐个赋值，不影响结构体其他成员
    CR.joint_space.model_theta[0] = 0;
    CR.joint_space.model_theta[1] = 0;
    CR.joint_space.model_theta[2] = 0;

    CR.parameter.r[0] = 70;
    CR.parameter.r[1] = 75;
    CR.parameter.r[2] = 80;

    CR.joint_space.r_bias = 0.8f;

    // 静态标定表（IMU 实测）：命令角→实测角，升序。bend_calib_gain 据此做逆映射。
    static const float calib_cmd_tbl[CALIB_POINTS_NUM] = {0.0f, 10.0f, 20.0f, 30.0f, 40.0f, 50.0f, 60.0f};
    static const float calib_meas_tbl[CALIB_POINTS_NUM] = {0.0f, 11.529f, 23.62f, 30.707f, 37.848f, 48.247f, 58.744f};
    for (uint8_t i = 0; i < CALIB_POINTS_NUM; i++) {
        s_calib_cmd[i] = calib_cmd_tbl[i];
        s_calib_meas[i] = calib_meas_tbl[i];
    }
    s_calib_count = CALIB_POINTS_NUM;

    // 控制器初始化
    Control_Init(&CR.pid, 1, 0, 0, -62, 62);
    // 外设初始化
    motor_init();
    sensor_init();
}

// 用于控制喷管弯曲（独立段控制，直接驱动某个段）
uint8_t armBend(int seg, char direction, float val)
{
    return segBend(seg, direction, val);
}

// 闭环总角度弯曲：自动修正理论角度，底层仍走 armBend_total 运动学分配
uint8_t armBend_closed_loop(char direction, float target_total_val, float dt, float tolerance, uint8_t max_iterations)
{
    if (target_total_val < 0.0f || target_total_val > TOTAL_ANGLE_LIMIT || max_iterations == 0) {
        return 1;
    }

    if (tolerance <= 0.0f) {
        tolerance = 1.0f;
    }

    float theory_total_val = target_total_val;
    uint8_t ret = armBend_total(direction, theory_total_val);
    if (ret != 0) {
        return ret;
    }

    for (uint8_t i = 0; i < max_iterations; i++) {
        if (dt > 0.0f) {
            osDelay((uint32_t)(dt * 1000.0f));
        }

        float measured_total_val = global_sensor[0].x;
        float error = target_total_val - measured_total_val;
        CR.joint_space.current_theta[0] = measured_total_val;

        if (fabsf(error) <= tolerance) {
            return 0;
        }

        float correction = PID_Update(&CR.pid, target_total_val, measured_total_val, dt);
        theory_total_val += correction;

        if (theory_total_val < 0.0f) {
            theory_total_val = 0.0f;
        } else if (theory_total_val > TOTAL_ANGLE_LIMIT) {
            theory_total_val = TOTAL_ANGLE_LIMIT;
        }

        ret = armBend_total(direction, theory_total_val);
        if (ret != 0) {
            return ret;
        }
    }

    return 2;
}

// 方向字符 → φ（弧度）
static float dir_to_phi(char direction)
{
    switch (direction) {
        case 'u': return 0;
        case 'r': return pi / 2;
        case 'd': return pi;
        case 'l': return 3 * pi / 2;
        default:  return 0;
    }
}

// 端到端逆补偿核心：期望实际角 → 命令角。phi_rad 为任意旋转角（弧度），支持连续旋转。
// gain：正常模式查表增益（命令角=期望实际角×gain）；标定模式 gain=1.0 直通，测真实链路
static uint8_t armBend_total_core_phi(float phi_rad, float total_val, float gain)
{
    if (total_val < 0 || total_val > TOTAL_ANGLE_LIMIT) return 1;

    total_val *= gain;

    float rem = total_val;
    float seg_input[3] = {0};

    // 级联式分段分配（斜率级联）：各段斜率自分段界起叠加，整段分段线性、节点连续

    // 段1
    seg_input[0] = (rem > SEG_INPUT_LIMIT ? SEG_INPUT_LIMIT : rem) * (1.0f + SEG1_SLOPE1);

    // 段2
    if (rem <= SEG_INPUT_LIMIT) {
        seg_input[1] = SEG2_SLOPE1 * rem;
    } else if (rem < SEG_INPUT_LIMIT_2) {
        seg_input[1] = SEG2_SLOPE1 * rem + SEG2_SLOPE2 * (rem - SEG_INPUT_LIMIT);
    } else {
        seg_input[1] = SEG2_SLOPE1 * SEG_INPUT_LIMIT_2 + SEG2_SLOPE2 * SEG_INPUT_LIMIT;
    }

    // 段3
    if (rem <= SEG_INPUT_LIMIT) {
        seg_input[2] = SEG3_SLOPE1 * rem;
    } else if (rem < SEG_INPUT_LIMIT_2) {
        seg_input[2] = SEG3_SLOPE1 * rem + SEG3_SLOPE2 * (rem - SEG_INPUT_LIMIT);
    } else if (rem < SEG_INPUT_LIMIT_3) {
        seg_input[2] = SEG3_SLOPE1 * rem + SEG3_SLOPE2 * (rem - SEG_INPUT_LIMIT) + SEG3_SLOPE3 * (rem - SEG_INPUT_LIMIT_2);
    } else {
        seg_input[2] = SEG3_SLOPE1 * SEG_INPUT_LIMIT_3 + SEG3_SLOPE2 * SEG_INPUT_LIMIT_2 + SEG3_SLOPE3 * SEG_INPUT_LIMIT;
    }

    CR.joint_space.ref_theta[0] = seg_input[0];
    CR.joint_space.ref_theta[1] = seg_input[1];
    CR.joint_space.ref_theta[2] = seg_input[2];

    // model_theta 直接取段输入角（端到端逆补偿已在 armBend_total 入口按总角折算，段内不再放大）
    for (int i = 0; i < 3; i++) {
        CR.joint_space.model_theta[i] = seg_input[i] * pi / 180;
    }
    CR.joint_space.model_phi = phi_rad;
    deltaL_update();

    static const uint8_t motor_seg[9] = {0,0,0, 1,1,1, 2,2,2};
    static const float seg_limit[3] = {SEG_LENGTH1, SEG_LENGTH2, SEG_LENGTH3};
    for (int i = 0; i < 9; i++) {
        float lim = seg_limit[motor_seg[i]];
        if (CR.joint_space.deltaL[i] > lim) CR.joint_space.deltaL[i] = lim;
        else if (CR.joint_space.deltaL[i] < -lim) CR.joint_space.deltaL[i] = -lim;
    }
    motor_sync_control(MOTOR_NUM - 1, 0, CR.joint_space.deltaL);
    return 0;
}

// 总角度弯曲（分段式分配，每段独占20°输入后触限）
uint8_t armBend_total(char direction, float total_val)
{
    return armBend_total_core_phi(dir_to_phi(direction), total_val, bend_calib_gain(total_val));
}

// ==================== 旋转（画圆）速度模式（方案B：连续速度 + 每圈重锚定 + 行程软限位） ====================
// 不用位置步进：每 tick 按肌腱行程对 φ 的导数 dL/dφ 下发速度命令，电机连续转不停。
// 误差累积的是"行程"，用三重防护兜住：行程软限位(圈内降速) + 每圈编码器重锚定(锁跨圈漂移) + 圈间回零。
#define ROT_OMEGA_DEFAULT     10.0f     // 默认旋转角速度（°/s）
#define ROT_DT_MS             10u       // rotate_vel_tick 调用周期（ms）
#define ROT_ANCHOR_TH_MM      2.0f      // 重锚定用电机的最小行程阈值（mm，信噪比）
#define ROT_SOFT_LIMIT_MARGIN 3.0f      // 行程软限位裕量（mm）
#define ROT_DRIFT_LOCK_DEG    10.0f     // 每圈重锚定允许校正的最大角度（°，防跳变）
#define ROT_VEL_MAX_MMPS      60.0f     // 单电机速度上限（mm/s，安全钳制）
#define ROT_ACCEL_DEGPSS      60.0f     // ω 平滑斜坡（°/s²）

static float    s_rot_phi_cmd = 0.0f;   // 命令 φ（度，0~360）
static float    s_rot_omega   = ROT_OMEGA_DEFAULT; // 当前 ω（°/s，可能被软限位缩放）
static float    s_rot_theta   = 10.0f;  // 旋转时保持的弯曲角（度）
static uint32_t s_rot_last_ms = 0;      // 上次 tick 时刻（ms）
static bool     s_rot_running = false;  // 速度模式激活标志
static float    s_rot_anchor_phi = 0.0f;// 上一圈重锚定得到的实测 φ（度）

// 电机行程对 φ 的导数 dL/dφ（与 kinematic.c calculate_L 的公式逐项求导一致）。
// 输入 model_theta_rad（弧度）、phi_rad（弧度）、r_bias，输出 dLdphi[9]（mm/rad）。
// 公式：M0 =  (cos(φ+60°)+r·|cos(φ+60°)|)·K
//       M1 =  (cos(-φ+60°)+r·|cos(-φ+60°)|)·K
//       M2 = -(cosφ - r·|cosφ|)·K
//   其中 K = 各段累计 S_cum（=Σ R·θ，旋转时固定）。导数用链式求导 + sign 项。
static void rotate_dLdphi(const float model_theta_rad[3], float phi_rad, float r_bias, float dLdphi[9])
{
    // 各段累计弧长 S_cum[s] = Σ_{i≤s} R[i]·θ[i]（与 calculate_L 一致）
    float S_cum[3] = {0};
    S_cum[0] = CR.parameter.r[0] * model_theta_rad[0];
    S_cum[1] = S_cum[0] + CR.parameter.r[1] * model_theta_rad[1];
    S_cum[2] = S_cum[1] + CR.parameter.r[2] * model_theta_rad[2];

    const float p60 = pi / 3.0f;
    for (int s = 0; s < 3; s++) {
        float K = S_cum[s];
        // M0:  cos(φ+60°) + r·|cos(φ+60°)|
        float a0 = phi_rad + p60;
        float c0 = cosf(a0);
        float d0 = -sinf(a0) * (1.0f + r_bias * (c0 >= 0 ? 1.0f : -1.0f));
        // M1:  cos(-φ+60°) + r·|cos(-φ+60°)|
        float a1 = -phi_rad + p60;
        float c1 = cosf(a1);
        float d1 =  sinf(a1) * (1.0f + r_bias * (c1 >= 0 ? 1.0f : -1.0f));
        // M2: -(cosφ - r·|cosφ|)
        float c2 = cosf(phi_rad);
        float d2 =  sinf(phi_rad) * (1.0f - r_bias * (c2 >= 0 ? 1.0f : -1.0f));

        dLdphi[3*s + 0] = d0 * K;
        dLdphi[3*s + 1] = d1 * K;
        dLdphi[3*s + 2] = d2 * K;
    }
}

// 每圈重锚定：读电机编码器实测位置，反解 φ，校正 φ_cmd 漂移。
// 只取 |L| 大于阈值（信噪比好）的电机参与平均，并将校正量限制在 ROT_DRIFT_LOCK_DEG 内。
static void rotate_anchor_resync(void)
{
    float phi_sum = 0.0f;
    uint8_t n = 0;
    static const uint8_t motor_seg[9] = {0,0,0, 1,1,1, 2,2,2};
    const float r_bias = CR.joint_space.r_bias;

    for (int i = 0; i < 9; i++) {
        // 实测行程（mm）：电机绝对位置 = 距直立位的行程（raf=1 绝对语义）
        float L_meas = global_motor[i].stepper_motor.current_pos;
        if (fabsf(L_meas) < ROT_ANCHOR_TH_MM) continue;          // 信噪比不足，跳过

        int s = motor_seg[i];
        float R = CR.parameter.r[s];
        float theta_rad = CR.joint_space.model_theta[s];
        float K = R * theta_rad;   // 该电机本段行程系数（旋转时 θ 固定）
        // 只用段1的 M2（主收紧电机）反解 φ：M2 的 L = -(cosφ - r|cosφ|)·K0
        // 段2/3 的 M2 是级联累计项，反解复杂，这里仅用段1，够做粗锚定。
        if (s != 0) continue;

        // 反解 cosφ：cosφ - r|cosφ| = -L/K
        float u = -L_meas / K;
        float c;
        if (u >= 0) c = u / (1.0f - r_bias);       // cosφ≥0
        else        c = u / (1.0f + r_bias);       // cosφ<0
        if (c > 1.0f) c = 1.0f; else if (c < -1.0f) c = -1.0f;
        float phi_est = acosf(c) * 180.0f / pi;

        // 象限判定：段1 M0 在 φ∈(0,π) 正、φ∈(π,2π) 负（cos(φ+60°) 主项）
        float phi_full = (global_motor[0].stepper_motor.current_pos >= 0)
                         ? phi_est : (360.0f - phi_est);
        if (phi_full >= 360.0f) phi_full -= 360.0f;

        phi_sum += phi_full;
        n++;
    }
    if (n == 0) return;

    float phi_meas = phi_sum / n;
    // 校正量限制在 ROT_DRIFT_LOCK_DEG 内，防异常
    float delta = phi_meas - s_rot_phi_cmd;
    while (delta > 180.0f) delta -= 360.0f;
    while (delta < -180.0f) delta += 360.0f;
    if (delta > ROT_DRIFT_LOCK_DEG) delta = ROT_DRIFT_LOCK_DEG;
    else if (delta < -ROT_DRIFT_LOCK_DEG) delta = -ROT_DRIFT_LOCK_DEG;

    s_rot_phi_cmd = s_rot_phi_cmd + delta;
    if (s_rot_phi_cmd < 0.0f) s_rot_phi_cmd += 360.0f;
    else if (s_rot_phi_cmd >= 360.0f) s_rot_phi_cmd -= 360.0f;
    s_rot_anchor_phi = phi_meas;
}

// 行程软限位：预读未来 ROT_LOOKAHEAD 内的最大行程，若某电机逼近 ±SEG_LENGTH 则平滑降 ω。
// 返回缩放系数 (0,1]。预测用 L(φ_cmd+lookahead) 与限位比较。
static float rotate_soft_limit_scale(void)
{
    // 预读角度（固定 10°，近似一圈的 1/36，够提前减速）
    const float lookahead_deg = 10.0f;
    float phi_look = s_rot_phi_cmd + lookahead_deg;
    if (phi_look >= 360.0f) phi_look -= 360.0f;

    // 算当前 φ 的 model_theta（旋转时固定，用 s_rot_theta 走级联分配）
    float rem = s_rot_theta;
    float seg[3];
    seg[0] = (rem > SEG_INPUT_LIMIT ? SEG_INPUT_LIMIT : rem) * (1.0f + SEG1_SLOPE1);
    if (rem <= SEG_INPUT_LIMIT)      seg[1] = SEG2_SLOPE1 * rem;
    else if (rem < SEG_INPUT_LIMIT_2) seg[1] = SEG2_SLOPE1 * rem + SEG2_SLOPE2 * (rem - SEG_INPUT_LIMIT);
    else                              seg[1] = SEG2_SLOPE1 * SEG_INPUT_LIMIT_2 + SEG2_SLOPE2 * SEG_INPUT_LIMIT;
    if (rem <= SEG_INPUT_LIMIT)      seg[2] = SEG3_SLOPE1 * rem;
    else if (rem < SEG_INPUT_LIMIT_2) seg[2] = SEG3_SLOPE1 * rem + SEG3_SLOPE2 * (rem - SEG_INPUT_LIMIT);
    else if (rem < SEG_INPUT_LIMIT_3) seg[2] = SEG3_SLOPE1 * rem + SEG3_SLOPE2 * (rem - SEG_INPUT_LIMIT) + SEG3_SLOPE3 * (rem - SEG_INPUT_LIMIT_2);
    else                              seg[2] = SEG3_SLOPE1 * SEG_INPUT_LIMIT_3 + SEG3_SLOPE2 * SEG_INPUT_LIMIT_2 + SEG3_SLOPE3 * SEG_INPUT_LIMIT;

    float theta_rad[3] = { seg[0]*pi/180.0f, seg[1]*pi/180.0f, seg[2]*pi/180.0f };
    float S_cum[3];
    S_cum[0] = CR.parameter.r[0] * theta_rad[0];
    S_cum[1] = S_cum[0] + CR.parameter.r[1] * theta_rad[1];
    S_cum[2] = S_cum[1] + CR.parameter.r[2] * theta_rad[2];

    static const uint8_t motor_seg[9] = {0,0,0, 1,1,1, 2,2,2};
    static const float seg_limit[3] = {SEG_LENGTH1, SEG_LENGTH2, SEG_LENGTH3};
    const float r = CR.joint_space.r_bias;
    const float p60 = pi/3.0f;
    const float phi_lr = phi_look * pi / 180.0f;

    float min_scale = 1.0f;
    for (int i = 0; i < 9; i++) {
        int s = motor_seg[i];
        float K = S_cum[s];
        float lim = seg_limit[s];
        float L;
        if (i % 3 == 0)      L =  (cosf(phi_lr + p60) + r*fabsf(cosf(phi_lr + p60))) * K;
        else if (i % 3 == 1) L =  (cosf(-phi_lr + p60) + r*fabsf(cosf(-phi_lr + p60))) * K;
        else                 L = -(cosf(phi_lr) - r*fabsf(cosf(phi_lr))) * K;
        float absL = fabsf(L);
        if (absL > lim - ROT_SOFT_LIMIT_MARGIN) {
            // 剩余裕量不足 → 按比例降速（平滑，不硬停）
            float room = (lim - absL);
            float scale = (room > ROT_SOFT_LIMIT_MARGIN) ? 1.0f : (room / ROT_SOFT_LIMIT_MARGIN);
            if (scale < 0.0f) scale = 0.0f;
            if (scale < min_scale) min_scale = scale;
        }
    }
    return min_scale;
}

// 旋转速度模式 tick：每 ROT_DT_MS 调用，累加 φ_cmd、算 dL/dφ、下发速度、重锚定。
// 返回：满圈标志（φ_cmd 回绕过 360° 返回 1）。需在旋转激活期间持续调用。
uint8_t rotate_vel_tick(void)
{
    if (!s_rot_running) return 0;

    uint32_t now = osKernelGetTickCount();
    float dt_s = (now - s_rot_last_ms) / 1000.0f;
    if (dt_s <= 0.0f) dt_s = (float)ROT_DT_MS / 1000.0f;
    if (dt_s > 0.5f) dt_s = 0.5f;               // 防 tick 间隙过大

    // 1. 累加 φ_cmd（带 ω 平滑斜坡，避免启动/软限位恢复时突变）
    float target_omega = ROT_OMEGA_DEFAULT;
    float scale = rotate_soft_limit_scale();     // 行程软限位（预测降速）
    target_omega *= scale;
    // ω 平滑：向目标线性逼近
    float omega_diff = target_omega - s_rot_omega;
    float max_dw = ROT_ACCEL_DEGPSS * dt_s;
    if (omega_diff > max_dw) omega_diff = max_dw;
    else if (omega_diff < -max_dw) omega_diff = -max_dw;
    s_rot_omega += omega_diff;
    if (s_rot_omega < 0.0f) s_rot_omega = 0.0f;

    s_rot_phi_cmd += s_rot_omega * dt_s;
    uint8_t full_round = 0;
    if (s_rot_phi_cmd >= 360.0f) {
        s_rot_phi_cmd -= 360.0f;                 // 回绕
        full_round = 1;
        rotate_anchor_resync();                  // 每圈重锚定（锁跨圈漂移）
    }

    // 2. 算 dL/dφ 与速度，下发 9 个电机（连续速度模式）
    float phi_rad = s_rot_phi_cmd * pi / 180.0f;
    // model_theta（旋转时固定，用 s_rot_theta 走级联分配）
    float rem = s_rot_theta;
    float seg[3];
    seg[0] = (rem > SEG_INPUT_LIMIT ? SEG_INPUT_LIMIT : rem) * (1.0f + SEG1_SLOPE1);
    if (rem <= SEG_INPUT_LIMIT)      seg[1] = SEG2_SLOPE1 * rem;
    else if (rem < SEG_INPUT_LIMIT_2) seg[1] = SEG2_SLOPE1 * rem + SEG2_SLOPE2 * (rem - SEG_INPUT_LIMIT);
    else                              seg[1] = SEG2_SLOPE1 * SEG_INPUT_LIMIT_2 + SEG2_SLOPE2 * SEG_INPUT_LIMIT;
    if (rem <= SEG_INPUT_LIMIT)      seg[2] = SEG3_SLOPE1 * rem;
    else if (rem < SEG_INPUT_LIMIT_2) seg[2] = SEG3_SLOPE1 * rem + SEG3_SLOPE2 * (rem - SEG_INPUT_LIMIT);
    else if (rem < SEG_INPUT_LIMIT_3) seg[2] = SEG3_SLOPE1 * rem + SEG3_SLOPE2 * (rem - SEG_INPUT_LIMIT) + SEG3_SLOPE3 * (rem - SEG_INPUT_LIMIT_2);
    else                              seg[2] = SEG3_SLOPE1 * SEG_INPUT_LIMIT_3 + SEG3_SLOPE2 * SEG_INPUT_LIMIT_2 + SEG3_SLOPE3 * SEG_INPUT_LIMIT;

    float theta_rad[3] = { seg[0]*pi/180.0f, seg[1]*pi/180.0f, seg[2]*pi/180.0f };
    float dLdphi[9];
    rotate_dLdphi(theta_rad, phi_rad, CR.joint_space.r_bias, dLdphi);

    // 速度 = dL/dφ · ω（°/s → rad/s），并做单电机速度上限钳制
    float omega_rad = s_rot_omega * pi / 180.0f;
    for (int i = 0; i < 9; i++) {
        float vel = dLdphi[i] * omega_rad;       // mm/s
        if (vel > ROT_VEL_MAX_MMPS) vel = ROT_VEL_MAX_MMPS;
        else if (vel < -ROT_VEL_MAX_MMPS) vel = -ROT_VEL_MAX_MMPS;
        motor_run_velocity(i, vel, 0);           // 连续速度下发（snf=0，各自独立）
    }

    s_rot_last_ms = now;
    return full_round;
}

// 启动速度模式旋转：弯曲角 theta（度）、角速度 omega（°/s，<=0 用默认）。
void rotate_vel_start(float theta, float omega)
{
    if (theta < 0.0f || theta > TOTAL_ANGLE_LIMIT) theta = 10.0f;
    s_rot_theta = theta;
    s_rot_omega = 0.0f;                        // 从 0 平滑加速
    s_rot_phi_cmd = 0.0f;
    s_rot_running = true;
    s_rot_last_ms = osKernelGetTickCount();
    s_rot_anchor_phi = 0.0f;
}

// 停止速度模式旋转（回到位置/回零控制）
void rotate_vel_stop(void)
{
    s_rot_running = false;
    for (int i = 0; i < 9; i++) {
        motor_run_velocity(i, 0.0f, 0);        // 停转
    }
}

// ==================== 自动标定（查表逆补偿） ====================
// 流程：逐点下发原始命令角（不乘增益，直通真实链路），每点等稳定后读 IMU 实测角，
//       存成 (命令角, 实测角) 标定表。正常弯曲按该表分段线性插值做逆映射，
//       自动处理非线性 / 触限 / 分段效应。
//
// 触发：上位机 addr=0xFB 指令（pc_cmd_parser.c）。标定过程中上位机应停止其他运动指令。
// 推进：PeriphCtrlTask 每 10ms 调用 calibrate_auto_tick() 驱动状态机。
// 表项被校准后立即生效（查表增益），可通过上位机查看标定点验证。

// 分段线性插值：给定输入 x，在 points[i] 升序表中取区间，越界取端点
static float calib_interp(const float *points, const float *vals, uint8_t n, float x)
{
    if (x <= points[0]) return vals[0];
    if (x >= points[n - 1]) return vals[n - 1];
    for (uint8_t i = 1; i < n; i++) {
        if (x <= points[i]) {
            float t = (x - points[i - 1]) / (points[i] - points[i - 1]);
            return vals[i - 1] + t * (vals[i] - vals[i - 1]);
        }
    }
    return vals[n - 1];
}

// 命令角 → 期望实测角（插值查表）
static float calib_cmd_to_meas(float cmd_deg)
{
    if (s_calib_count == 0) {
        // 未标定：退回线性比例，命令角×增益倒数即期望实测
        return cmd_deg / BEND_CALIB_CMD_PER_DEG;
    }
    return calib_interp(s_calib_cmd, s_calib_meas, s_calib_count, cmd_deg);
}

// 标定表完整 → 正常弯曲增益 = 命令角 / 期望实测角（命令角输入总角，除以查表实测角）
static float bend_calib_gain(float total_val)
{
    if (s_calib_count == 0) {
        return BEND_CALIB_CMD_PER_DEG;              // 未标定：回退到出厂系数
    }
    float want = calib_cmd_to_meas(total_val);       // 期望实测角（查表）
    if (want <= 0.0f) want = 0.001f;                 // 防御除零
    return (total_val / want) * BEND_CALIB_GAIN_SAFETY;  // 逆映射增益 × 防超调折减
}

// 标定状态机：逐档下发原始命令 → 等待稳定 → 采样 IMU → 存表
void auto_calibrate(char direction)
{
    auto_straight();
    osDelay(1000);
    s_cal_dir = direction;
    s_cal_idx = 0;                                   // 从第 0 档（命令角 0°）开始
    s_cal_t0 = osKernelGetTickCount();
    s_calib_cmd[0] = 0.0f;
    s_calib_count = 0;
    s_calibrating = true;
}

void calibrate_auto_tick(void)
{
    if (!s_calibrating) return;

    if (s_cal_idx == 0) {
        // 第 0 档：已 auto_straight 回零，直接采样零点
        if (osKernelGetTickCount() - s_cal_t0 < CALIB_SETTLE_MS) return;
        s_calib_cmd[0] = 0.0f;
        s_calib_meas[0] = global_sensor[0].x;
        s_calib_count = 1;
        s_cal_idx = 1;
        s_cal_t0 = osKernelGetTickCount();
        return;
    }

    if (s_cal_idx >= CALIB_POINTS_NUM) {
        // 全部标定点采完，标定结束
        s_calibrating = false;
        return;
    }

    // 下发当前档原始命令角（直通，不乘增益）
    float cmd = s_cal_idx * CALIB_STEP_DEG;
    if (cmd != s_cal_last_cmd) {
        armBend_total_core_phi(dir_to_phi(s_cal_dir), cmd, 1.0f);
        s_cal_last_cmd = cmd;
        s_cal_t0 = osKernelGetTickCount();
        return;
    }

    // 等待稳定后采样
    if (osKernelGetTickCount() - s_cal_t0 < CALIB_SETTLE_MS) return;
    s_calib_cmd[s_cal_idx] = cmd;
    s_calib_meas[s_cal_idx] = global_sensor[0].x;
    s_calib_count = s_cal_idx + 1;
    s_cal_idx++;
    s_cal_last_cmd = -1.0f;   // 强制下一步重发新档命令
    s_cal_t0 = osKernelGetTickCount();
}

void deltaL_update(void)
{
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
 * @brief 动作组演示函数
 *
 * 执行顺序：上弯 → 回零 → 下弯 → 回零 → 左弯 → 回零 → 右弯 → 回零
 * 每个动作之间留有延时，确保运动完整执行
 * 所有动作参数在此函数内部定义
 */
void action_group_demo(void)
{
    const float angle = 10.0f;  // 弯曲角度（度）

    // 1. 向上弯曲
    armBend_total('u', angle);
    osDelay(3000);

    // 2. 回零
    auto_straight();
    osDelay(2000);

    // 3. 向下弯曲
    armBend_total('d', angle);
    osDelay(3000);

    // 4. 回零
    auto_straight();
    osDelay(2000);

    // 5. 向左弯曲
    // armBend(1, 'l', angle);
    // osDelay(3000);

    // 6. 回零
    // auto_straight();
    // osDelay(2000);

    // 7. 向右弯曲
    // armBend(1, 'r', angle);
    // osDelay(3000);

    // 8. 回零
    // auto_straight();
    // osDelay(2000);
}

// ==================== 循环运动控制（状态机，非阻塞） ====================
// 由 PeriphCtrlTask 每 10ms 调用 action_group_tick() 推进。
// 每步以"到位判定"驱动（closedloop_is_busy：电机归零 + IMU 角稳定），不用固定延时，
// 避免柔性臂蠕变导致采样过早、闭环命令累加超调。每轮强制回零防累积漂移。
// 安全保护：循环次数上限、单步超时、收到停止/失能立即回零。
#define AG_ANGLE_DEFAULT     10.0f   // 默认弯曲角（度）
#define AG_CYCLES_DEFAULT    3u      // 默认循环次数
#define AG_CYCLES_MAX        100u    // 循环次数安全上限（防无限循环）
#define AG_STEP_TIMEOUT_MS   8000u   // 单步到位超时（ms）：到位失败强制跳步，防卡死
#define AG_HOLD_MS           1000u   // 到位后保持时长（ms）
#define AG_BEND_SETTLE_MS    1500u   // 上/下弯到位预算（ms）：不靠 200ms 反馈轮询，固定等物理走完

static ActionGroupState_t s_ag_state = AG_IDLE;
static float    s_ag_angle   = AG_ANGLE_DEFAULT;
static uint8_t  s_ag_cycles  = AG_CYCLES_DEFAULT;
static uint8_t  s_ag_cycle   = 0;         // 已完成循环数（一个循环=上弯+下弯）
static bool     s_ag_next_up = true;      // 下一次弯曲方向（true=上弯，false=下弯）
static uint32_t s_ag_t0      = 0;         // 当前状态起始 tick（ms）
static uint32_t s_ag_hold_t0 = 0;         // 保持阶段起始 tick（ms）
// 旋转画圆专用状态
static uint8_t  s_ag_rotate_cycles = 0;   // 旋转圈数（每圈 360°）
static bool     s_ag_rotating   = true;  // 旋转序列模式标志（true=旋转，false=弯曲）
static float    s_ag_step       = 5.0f;  // 旋转步长（度），armRotate 阻塞式整圈使用

// 动作循环启动：step_deg<=0 走弯曲模式（上弯→回零→下弯→回零循环）；
// step_deg>0 走旋转序列模式（上弯 angle → 旋转一圈 → 回正 → 循环）。
// 统一参数：angle 弯曲角（度），cycles 循环次数（0=无限，受安全上限约束）。
void action_group_start(float angle, uint8_t cycles, float step_deg)
{
    if (angle < 0.0f || angle > TOTAL_ANGLE_LIMIT) angle = AG_ANGLE_DEFAULT;
    s_ag_angle  = angle;
    s_ag_cycles = (cycles == 0) ? AG_CYCLES_DEFAULT : (cycles > AG_CYCLES_MAX ? AG_CYCLES_MAX : cycles);
    s_ag_cycle  = 0;
    s_ag_t0     = osKernelGetTickCount();

    if (step_deg > 0.0f) {
        // ===== 旋转序列模式：上弯 → 旋转一圈 → 回正 → 循环 =====
        s_ag_rotate_cycles = 0;
        s_ag_rotating  = true;
        s_ag_step      = step_deg;            // 记录旋转步长（armRotate 阻塞式整圈使用）
        s_ag_state  = AG_BEND_UP;            // 先上弯到 angle
        armBend_total('u', s_ag_angle);
    } else {
        // ===== 弯曲模式（默认） =====
        s_ag_rotating  = false;
        s_ag_next_up = true;                     // 从向上弯开始
        s_ag_state  = AG_BEND_UP;
        armBend_total('u', s_ag_angle);          // 首拍：向上弯曲（走查表逆补偿，命令角=目标实测角）
    }
}

void action_group_stop(void)
{
    rotate_vel_stop();               // 若正在速度模式旋转，先停转
    s_ag_state = AG_IDLE;
    auto_straight();
}

ActionGroupState_t action_group_get_state(void)
{
    return s_ag_state;
}

void action_group_tick(void)
{
    if (s_ag_state == AG_IDLE) return;

    uint32_t now = osKernelGetTickCount();

    // 单步超时保护：仅对"位置到位"的离散步（弯曲/回零）生效。
    // 旋转（AG_ROTATE_STEP）是长时连续运动（一圈 360°/ω 可达 12s+），
    // 由 s_ag_cycles 圈数上限兜底；若按 8s 超时强跳会误杀正常旋转。
    if (s_ag_state != AG_ROTATE_STEP && now - s_ag_t0 > AG_STEP_TIMEOUT_MS) {
        rotate_vel_stop();               // 防御：若仍在速度模式，先停转
        auto_straight();
        s_ag_state = AG_IDLE;           // 超时视为异常，直接结束（安全态）
        return;
    }

    switch (s_ag_state) {
    case AG_BEND_UP:
        // 等上弯物理完成 → 保持。
        // 用固定时间预算，不靠 200ms 反馈轮询：current_vel 每 200ms 才更新，
        // 若用 is_busy() 判"电机停"，陈旧值会在上弯刚启动时就误判到位 → 旋转过早叠加。
        // 上弯位移≈15mm @ ~10mm/s → 预算 1500ms，含稳定裕量。
        if (now - s_ag_t0 >= AG_BEND_SETTLE_MS) {
            s_ag_state = AG_HOLD_UP;
            s_ag_hold_t0 = now;
            s_ag_t0 = now;
        }
        break;

    case AG_HOLD_UP:
        // 上弯到位后：旋转序列 → 启动速度模式连续旋转（不再等 AG_HOLD_MS）；
        // 弯曲模式 → 保持 AG_HOLD_MS 后回零
        if (s_ag_rotating) {
            // 启动速度模式：从 φ=0 连续旋转（无位置步进，顺滑且快）
            rotate_vel_start(s_ag_angle, ROT_OMEGA_DEFAULT);
            s_ag_state = AG_ROTATE_STEP;    // 连续旋转，由 rotate_vel_tick 推进
            s_ag_t0 = now;
        } else if (now - s_ag_hold_t0 >= AG_HOLD_MS) {
            auto_straight();
            s_ag_state = AG_RESET;
            s_ag_t0 = now;
        }
        break;

    case AG_RESET:
        // 回零到位 → 按方向标志发下一弯（上弯后→下弯；下弯后→结束或下轮上弯）
        if (!closedloop_is_busy()) {
            if (s_ag_next_up) {
                // 上弯完成：本次方向已用，转下弯（同循环内）
                s_ag_next_up = false;
                s_ag_state = AG_BEND_DOWN;
                armBend_total('d', s_ag_angle);
            } else {
                // 下弯完成：一个循环结束
                s_ag_cycle++;
                if (s_ag_cycle >= s_ag_cycles) {
                    s_ag_state = AG_IDLE;   // 循环次数耗尽 → 结束
                } else {
                    s_ag_next_up = true;    // 进入下一循环：再上弯
                    s_ag_state = AG_BEND_UP;
                    armBend_total('u', s_ag_angle);
                }
            }
            s_ag_t0 = now;
        }
        break;

    case AG_BEND_DOWN:
        // 等下弯物理完成 → 保持（同 AG_BEND_UP：固定时间预算）
        if (now - s_ag_t0 >= AG_BEND_SETTLE_MS) {
            s_ag_state = AG_HOLD_DOWN;
            s_ag_hold_t0 = now;
            s_ag_t0 = now;
        }
        break;

    case AG_HOLD_DOWN:
        // 下弯保持后 → 回零
        if (now - s_ag_hold_t0 >= AG_HOLD_MS) {
            auto_straight();
            s_ag_state = AG_RESET;
            s_ag_t0 = now;
        }
        break;

    case AG_ROTATE_STEP:
        // 速度模式连续旋转：由 rotate_vel_tick 推进（累加 φ_cmd → 下发速度 → 重锚定）
        if (rotate_vel_tick()) {
            // 转满一圈 → 停止速度模式并回正
            rotate_vel_stop();
            auto_straight();
            s_ag_state = AG_ROTATE_RESET;
            s_ag_t0 = now;
        }
        break;

    case AG_ROTATE_RESET:
        // 回正到位 → 一循环结束：下一圈重新上弯，或全部圈数耗尽结束（回正不保持）
        if (!closedloop_is_busy()) {
            s_ag_rotate_cycles++;
            if (s_ag_rotate_cycles >= s_ag_cycles) {
                s_ag_state = AG_IDLE;       // 圈数耗尽 → 结束（回正态）
            } else {
                s_ag_state = AG_BEND_UP;    // 下一圈：重新上弯
                armBend_total('u', s_ag_angle);
            }
            s_ag_t0 = now;
        }
        break;

    default:
        s_ag_state = AG_IDLE;
        break;
    }
}


/**
 * @brief 用于控制截面面积收缩
 * @param direction 1正
 * @param val 目前的取值范围为 50.0f-100.0f，百分数
 */
void scale_squared(uint8_t direction, float val)
{
    if (val < 75) return;

    CR.operation_space.scale = direction == 1? val : -val;
    // 运动学推导
    float R = 50;
    float target;
    float val_sqrt = sqrtf(val) / 10.0f;
    target = 2.0f * pi * (R -  val_sqrt * R);

    motor_run(9, 10, target ,false);
}