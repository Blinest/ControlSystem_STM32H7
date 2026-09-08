/**
 * @file ClosedLoop.h
 * @brief 喷管弯曲闭环控制模块（多算法，独立可复用）
 *
 * 本模块把"闭环控制模型"独立出来，支持运行时切换多种控制算法：
 *   CLOSED_LOOP_ALGO_BANG     —— 经典 bang-bang（固定步长）
 *   CLOSED_LOOP_ALGO_VAR_BANG —— 变步长 bang-bang（误差大加快、误差小变细）
 *   CLOSED_LOOP_ALGO_PID      —— PID（抗积分饱和）
 *   CLOSED_LOOP_ALGO_PID_FF   —— PID + 开环前馈（加快响应）
 * 反馈可先经一阶低通滤波（可开关）再送入控制器，抑制 IMU 抖动。
 *
 * 与反馈源/执行器完全解耦：通过注入的回调读实测角、查是否到位、下发命令角。
 * 实机接 IMU、模拟接 MotorSim，均由调用方提供回调，本模块不感知。
 *
 * @date 2026-08-25
 */
#ifndef CONTROLSYSTEM_CLOSEDLOOP_H
#define CONTROLSYSTEM_CLOSEDLOOP_H

#include <stdint.h>
#include <stdbool.h>

#include "CR/Control.h"

#ifdef __cplusplus
extern "C" {
#endif

// ==================== 控制算法枚举 ====================
typedef enum {
    CLOSED_LOOP_ALGO_BANG = 0,     // 经典 bang-bang（固定步长）
    CLOSED_LOOP_ALGO_PID,          // PID（抗积分饱和）
    CLOSED_LOOP_ALGO_PID_FF,       // PID + 开环前馈
    CLOSED_LOOP_ALGO_VAR_BANG,     // 变步长 bang-bang
} ClosedLoopAlgo_t;

// ==================== 控制器选择（底层内部，不暴露给上层） ====================
// CLOSED_LOOP_DEFAULT_ALGO：手动切换的默认控制器。改此宏 + 重新编译即可切换算法。
#define CLOSED_LOOP_DEFAULT_ALGO   CLOSED_LOOP_ALGO_PID_FF

// ==================== 控制参数（底层写死，可运行时整定 PID） ====================
#define CLOSED_LOOP_BANG_STEP       0.5f   // 经典 bang-bang 固定步长（度）
#define CLOSED_LOOP_TOLERANCE_DEG   0.2f   // 到位容差（度）
#define CLOSED_LOOP_SETTLE_COUNT    3u     // 误差连续满足容差 ≥ 该次数才判到位（防单次擦过误停）
#define CLOSED_LOOP_MAX_ITER        30u    // 最大迭代次数
#define TOTAL_ANGLE_LIMIT_DEG       120.0f // 命令角上限（度；允许负值以补偿 BIAS）

// 物理可达上限（度）：由新标定实测确定——命令 60° → 实测 58.744°，
// 臂体有效区至少到 60°+。闭环目标与命令角钳制到该值（略低于 TOTAL_ANGLE_LIMIT
// 留余量），避免 PID 追不可达目标而积分饱和/堵转。
#define CLOSED_LOOP_ACHIEVABLE_MAX_DEG  65.0f

// 等电机真正到位再采样：命令下发后等待该时长（ms）再采样。
// 依据：motor_status_check() 每 200ms 轮询刷新电机反馈，加上 CAN 同步动作 ~28ms，
//       取 300ms 覆盖一轮反馈刷新，保证采样时电机已运动完毕。
#define CLOSED_LOOP_SETTLE_MS       300u

// 到位等待超时上限（ms）：is_busy() 采用电机速度归零判定，若某电机速度反馈
// 失配/失步导致速度恒不为零，此处强制放行，避免闭环卡死（保底仍能收敛）。
#define CLOSED_LOOP_SETTLE_TIMEOUT_MS  (CLOSED_LOOP_SETTLE_MS * 3u)   // 900ms

// 变步长 bang-bang 参数
#define CLOSED_LOOP_VAR_BANG_COARSE 1.0f   // 误差大时步长（度）
#define CLOSED_LOOP_VAR_BANG_FINE   0.2f   // 误差小时步长（度）
#define CLOSED_LOOP_VAR_BANG_BAND   2.0f   // 切换细步长的误差带（度）

// PID 默认参数（运行时可用 ClosedLoop_SetPid 整定）
// 整定目标：快速收敛 + 抑制超调 + 不抖动
#define CLOSED_LOOP_PID_KP          0.3f   // 比例：把前馈固定误差压小（残差≈ff_err/(1+Kp)）
#define CLOSED_LOOP_PID_KI          0.1f   // 积分：消除前馈/机械固定残差（原0.0001过小，无法消残差）
#define CLOSED_LOOP_PID_KD          0.0f   // 微分：抑制超调
#define CLOSED_LOOP_PID_OUT_MIN     -10.0f // 输出限幅（修正量）
#define CLOSED_LOOP_PID_OUT_MAX     10.0f
#define CLOSED_LOOP_PID_INT_LIMIT   20.0f // 积分限幅：容纳消除稳态误差的积分量
#define CLOSED_LOOP_PID_DT          0.5f   // 采样步长(s)：与"等到位再采样"的实际间隔匹配

// 逼近减速：|误差| 小于该值时，PID 输出额外衰减，确保最后一步超调 ≤0.1°
#define CLOSED_LOOP_PID_SLOW_ZONE   0.5f   // 减速带（度）
#define CLOSED_LOOP_PID_SLOW_FACTOR 0.5f   // 减速带内输出衰减系数

// 一阶低通滤波：滤波系数 alpha（0~1，越大越平滑）
#define CLOSED_LOOP_FILTER_ALPHA    0.6f

// ==================== 闭环状态 ====================
typedef enum {
    CLOSED_LOOP_IDLE = 0,      // 未启动
    CLOSED_LOOP_RUNNING,       // 运行中（等待到位 或 采样下发）
    CLOSED_LOOP_DONE,          // 达到容差，结束
    CLOSED_LOOP_EXHAUSTED,     // 迭代次数耗尽，结束
    CLOSED_LOOP_STOPPED,       // 被显式停止
} ClosedLoopState_t;

typedef struct {
    ClosedLoopState_t state;    // 当前状态
    ClosedLoopAlgo_t  algo;     // 当前算法
    char    direction;          // 弯曲方向：'u'/'d'/'l'/'r'
    float   target_total_val;   // 目标弯曲角（度）
    float   theory_total_val;   // 当前下发命令角（度）
    uint8_t iteration;          // 已执行迭代次数

    // 算法内部状态
    float   filtered_meas;      // 一阶低通后的实测角（度）
    bool    filter_init;        // 滤波是否已初始化
    PID_Handle pid;             // PID 控制器（PID / PID_FF 使用）
    uint8_t settle_count;       // 误差连续满足容差的次数（≥SETTLE_COUNT 才判到位）
} ClosedLoop_t;

// ==================== 注入回调（由调用方提供，解耦反馈源/执行器） ====================
typedef struct {
    /** 读当前实测弯曲角（度，命令角坐标）。实机=IMU 读数；模拟=调用方换算后的实际角 */
    float  (*get_measured_deg)(void);
    /** 查上一次驱动是否已完全到位（未到位返回 true）。模拟= MotorSim_IsActive()；实机=电机到位状态 */
    bool   (*is_busy)(void);
    /** 下发一个命令角（度）驱动弯曲。实机/模拟均接 armBend_total */
    void   (*apply)(char direction, float cmd_deg);
} ClosedLoopCallbacks_t;

// ==================== 公开 API ====================

/** @brief 初始化闭环模块（传入回调），通常开机时调用一次 */
void ClosedLoop_Init(const ClosedLoopCallbacks_t *cbs);

/** @brief 启动一次闭环：向目标角 target_deg（度）收敛，用指定算法 */
void ClosedLoop_Start(char direction, float target_deg, ClosedLoopAlgo_t algo);

/** @brief 显式停止闭环 */
void ClosedLoop_Stop(void);

/** @brief 周期推进（供任务每 10ms 调用）；空闲时立即返回 */
void ClosedLoop_Tick(void);

/** @brief 当前闭环状态 */
ClosedLoopState_t ClosedLoop_GetState(void);

/** @brief 当前闭环实例（供解析器写入目标/方向/算法） */
ClosedLoop_t *ClosedLoop_Get(void);

/** @brief 运行时整定 PID 参数（仅对 PID / PID_FF 算法生效） */
void ClosedLoop_SetPid(float kp, float ki, float kd, float out_min, float out_max);

/** @brief 开关反馈一阶低通滤波（默认开启） */
void ClosedLoop_SetFilter(bool enable);

#ifdef __cplusplus
}
#endif

#endif /* CONTROLSYSTEM_CLOSEDLOOP_H */
