/**
 * @file ClosedLoop.c
 * @brief 喷管弯曲闭环控制模块（多算法控制器）实现
 *
 * 状态机：IDLE → RUNNING(首拍下发 → 等到位 → 采样 → 算法计算命令角 → 下发 → 等到位…)
 *   → DONE（达容差）/ EXHAUSTED（迭代耗尽）/ STOPPED（显式停止）
 *
 * 支持算法（运行时切换）：
 *   - CLOSED_LOOP_ALGO_BANG     经典 bang-bang：固定步长增减
 *   - CLOSED_LOOP_ALGO_VAR_BANG 变步长 bang-bang：误差大时大步、小时细步
 *   - CLOSED_LOOP_ALGO_PID      PID：抗积分饱和（输出限幅）
 *   - CLOSED_LOOP_ALGO_PID_FF   PID + 前馈：理论命令角直接下发 + PID 修正项
 * 反馈可先经一阶低通滤波再送入控制器，抑制 IMU 抖动。
 *
 * @date 2026-08-25
 */
#include "ClosedLoop.h"

#include <math.h>
#include <string.h>

#include "cmsis_os2.h"

// ==================== 模块状态 ====================
static ClosedLoop_t          s_cl;             // 创建闭环实例
static ClosedLoopCallbacks_t s_cbs;            // 注入回调
static bool                  s_filter_enable;  // 反馈低通滤波开关
// 等电机真正到位再采样：记录上次命令下发的系统 tick（ms），在运动窗口内不采样
static uint32_t              s_last_cmd_ms = 0;

void ClosedLoop_Init(const ClosedLoopCallbacks_t *cbs)
{
    if (cbs) {
        s_cbs = *cbs;
    }
    memset(&s_cl, 0, sizeof(s_cl));
    s_cl.state = CLOSED_LOOP_IDLE;
    s_cl.algo  = CLOSED_LOOP_DEFAULT_ALGO;   // 编译期固定默认控制器
    s_filter_enable = false;   // 默认关滤波：到位后采样 raw 已干净（扰动衰减为0），滤波只引入滞后造成残差
    s_last_cmd_ms = 0;         // 无历史命令，到位门控立即放行
    // 初始化 PID（默认参数）
    Control_Init(&s_cl.pid,
                 CLOSED_LOOP_PID_KP, CLOSED_LOOP_PID_KI, CLOSED_LOOP_PID_KD,
                 CLOSED_LOOP_PID_OUT_MIN, CLOSED_LOOP_PID_OUT_MAX);
}

void ClosedLoop_Start(char direction, float target_deg, ClosedLoopAlgo_t algo)
{
    // 控制器由编译期宏 CLOSED_LOOP_DEFAULT_ALGO 固定，忽略运行时传入的 algo
    (void)algo;
    s_cl.algo = CLOSED_LOOP_DEFAULT_ALGO;   // 控制算法配置

    s_cl.direction = direction;
    s_cl.target_total_val = target_deg;
    // 目标超物理上限 → 钳到可达上限（避免 PID 追不可达目标而积分饱和）
    if (s_cl.target_total_val > CLOSED_LOOP_ACHIEVABLE_MAX_DEG) {
        s_cl.target_total_val = CLOSED_LOOP_ACHIEVABLE_MAX_DEG;
    }
    s_cl.theory_total_val = s_cl.target_total_val;
    s_cl.iteration = 0;
    s_cl.filter_init = false;   // 新会话重新初始化滤波
    s_cl.filtered_meas = 0.0f;
    s_cl.settle_count = 0;      // 新会话复位稳定计数
    Control_Init(&s_cl.pid,
                 CLOSED_LOOP_PID_KP, CLOSED_LOOP_PID_KI, CLOSED_LOOP_PID_KD,
                 CLOSED_LOOP_PID_OUT_MIN, CLOSED_LOOP_PID_OUT_MAX);   // 重置 PID 积分
    s_last_cmd_ms = osKernelGetTickCount();   // 标记起点：首拍命令下发后进入到位等待
    s_cl.state = CLOSED_LOOP_RUNNING;
}

void ClosedLoop_Stop(void)
{
    s_cl.state = CLOSED_LOOP_STOPPED;
}

void ClosedLoop_SetPid(float kp, float ki, float kd, float out_min, float out_max)
{
    Control_Init(&s_cl.pid, kp, ki, kd, out_min, out_max);
}

void ClosedLoop_SetFilter(bool enable)
{
    s_filter_enable = enable;
}

// 一阶低通滤波：y[n] = y[n-1] + alpha*(x - y[n-1])
static float closedloop_lowpass(float x, float *prev, bool *init)
{
    if (!*init) {
        *prev = x;
        *init = true;
        return x;
    }
    *prev += CLOSED_LOOP_FILTER_ALPHA * (x - *prev);
    return *prev;
}

// PID 更新（带积分限幅 + 逼近减速，严格抑制超调 ≤0.1°）
static float closedloop_pid_update(PID_Handle *pid, float setpoint, float measured, float dt)
{
    // 先钳制积分（防 Ki·integral 过大）
    if (pid->integral > CLOSED_LOOP_PID_INT_LIMIT) pid->integral = CLOSED_LOOP_PID_INT_LIMIT;
    else if (pid->integral < -CLOSED_LOOP_PID_INT_LIMIT) pid->integral = -CLOSED_LOOP_PID_INT_LIMIT;

    float out = PID_Update(pid, setpoint, measured, dt);

    // 逼近减速：|误差| < 减速带时衰减输出，保证最后一步不冲过头
    float error = setpoint - measured;
    if (fabsf(error) < CLOSED_LOOP_PID_SLOW_ZONE) {
        out *= CLOSED_LOOP_PID_SLOW_FACTOR;
    }

    return out;
}

void ClosedLoop_Tick(void)
{
    if (s_cl.state != CLOSED_LOOP_RUNNING) return;

    // 首拍：下发目标角并立即进入迭代（否则 iteration 恒为 0，永远重发同一命令）
    if (s_cl.iteration == 0 && s_cbs.apply) {
        s_cbs.apply(s_cl.direction, s_cl.theory_total_val);
        s_cl.iteration = 1;
        s_last_cmd_ms = osKernelGetTickCount();   // 记录本次命令下发时刻
        return;
    }

    // 迭代次数耗尽
    if (s_cl.iteration >= CLOSED_LOOP_MAX_ITER) {
        s_cl.state = CLOSED_LOOP_EXHAUSTED;
        return;
    }

    // 等电机真正到位再采样：命令下发后未满最短运动窗口，或电机速度未归零（is_busy）则等待，
    // 避免把电机运动中的瞬态当成稳态采样。超时上限强制放行，防速度反馈失配导致卡死。
    // 到位门由注入的 is_busy 判定（现为电机速度归零；IMU 稳定已在 get_measured_deg 采样处处理，
    // 不在此处逐拍等 IMU，避免电机到位后仍被角度抖动拖住，造成连续发令下卡顿）。
    if (osKernelGetTickCount() - s_last_cmd_ms < CLOSED_LOOP_SETTLE_MS) return;

    // 读实测角（raw 精确值），可选一阶低通滤波（滤波仅用于 PID 计算，避免滞后影响到位判定）
    float raw_meas = s_cbs.get_measured_deg ? s_cbs.get_measured_deg() : 0.0f;
    float measured = s_filter_enable
                     ? closedloop_lowpass(raw_meas, &s_cl.filtered_meas, &s_cl.filter_init)
                     : raw_meas;

    // 到位容差判定：误差需连续满足容差 ≥ CLOSED_LOOP_SETTLE_COUNT 次才判到位
    // 用 raw 精确值（滤波有滞后，误判会留 1~2° 残差）；计数防止单次碰巧擦过就停
    if (fabsf(s_cl.target_total_val - raw_meas) <= CLOSED_LOOP_TOLERANCE_DEG) {
        if (++s_cl.settle_count >= CLOSED_LOOP_SETTLE_COUNT) {
            s_cl.state = CLOSED_LOOP_DONE;
        }
        return;   // 已进入容差带：本轮不发新命令，静置观察（等下一次采样确认稳定）
    }
    s_cl.settle_count = 0;   // 超出容差：重置计数

    float error = s_cl.target_total_val - measured;

    switch (s_cl.algo) {
    case CLOSED_LOOP_ALGO_BANG:
        // 经典 bang-bang：只看误差符号，固定步长
        if (error < 0.0f) {
            s_cl.theory_total_val -= CLOSED_LOOP_BANG_STEP;
        } else {
            s_cl.theory_total_val += CLOSED_LOOP_BANG_STEP;
        }
        break;

    case CLOSED_LOOP_ALGO_VAR_BANG:
        // 变步长 bang-bang：误差大时大步，误差小时细步（避免极限环/过冲）
        {
            float step = (fabsf(error) > CLOSED_LOOP_VAR_BANG_BAND)
                         ? CLOSED_LOOP_VAR_BANG_COARSE
                         : CLOSED_LOOP_VAR_BANG_FINE;
            if (error < 0.0f) {
                s_cl.theory_total_val -= step;
            } else {
                s_cl.theory_total_val += step;
            }
        }
        break;

    case CLOSED_LOOP_ALGO_PID:
        // PID：输出即命令角增量，带积分限幅（防超调）
        s_cl.theory_total_val += closedloop_pid_update(&s_cl.pid, s_cl.target_total_val, measured, CLOSED_LOOP_PID_DT);
        break;

    case CLOSED_LOOP_ALGO_PID_FF:
        // PID + 前馈：命令角 = 目标角 + PID 修正项（不累加，无增量漂移；带积分限幅）
        {
            float correction = closedloop_pid_update(&s_cl.pid, s_cl.target_total_val, measured, CLOSED_LOOP_PID_DT);
            s_cl.theory_total_val = s_cl.target_total_val + correction;
        }
        break;

    default:
        break;
    }

    // 命令角钳制在物理可达上限内（允许负值补偿 BIAS；超可达上限命令角不再增实际弯曲）
    if (s_cl.theory_total_val > CLOSED_LOOP_ACHIEVABLE_MAX_DEG) {
        s_cl.theory_total_val = CLOSED_LOOP_ACHIEVABLE_MAX_DEG;
    }

    if (s_cbs.apply) {
        s_cbs.apply(s_cl.direction, s_cl.theory_total_val);
        s_last_cmd_ms = osKernelGetTickCount();   // 记录本次命令下发时刻
    }
    s_cl.iteration++;
}

ClosedLoopState_t ClosedLoop_GetState(void)
{
    return s_cl.state;
}

ClosedLoop_t *ClosedLoop_Get(void)
{
    return &s_cl;
}
