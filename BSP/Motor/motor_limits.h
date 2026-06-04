/**
 * @file motor_limits.h
 * @brief 电机参数限制条件定义
 * 
 * 防止因信号干扰出现异常问题，对加速度、速度、位移距离添加限制条件
 * 
 * @date 2026-04-02
 * @author Psyduck
 */

#ifndef MOTOR_LIMITS_H
#define MOTOR_LIMITS_H

#include <stdint.h>
#include <stdbool.h>
#include <math.h>

// ==================== 电机安全限制参数 ====================

// 物理限制 (基于42步进电机规格)
#define MOTOR_MAX_VELOCITY_RPM         50.0f    // 最大转速 (rpm) - 超过50rpm会出现抖动
#define MOTOR_MAX_VELOCITY_MM_S        16.67f   // 最大线速度 (mm/s) - 2mm导程 * 50rpm / 60
#define MOTOR_MAX_ACCELERATION_MM_S2   500.0f   // 最大加速度 (mm/s²) - 保守值，防止失步
#define MOTOR_MAX_DECELERATION_MM_S2   500.0f   // 最大减速度 (mm/s²)

// 位移限制 (基于喷管结构)
#define MOTOR_MAX_DISPLACEMENT_MM      100.0f   // 最大单次位移 (mm)
#define MOTOR_MIN_DISPLACEMENT_MM      0.1f     // 最小有效位移 (mm) - 防止微小抖动

// 位置限制 (机械限位)
#define MOTOR_POSITIVE_LIMIT_MM        120.0f   // 正向机械限位 (mm)
#define MOTOR_NEGATIVE_LIMIT_MM        -120.0f  // 负向机械限位 (mm)

// 时间限制
#define MOTOR_MIN_MOVE_TIME_MS         10       // 最小移动时间 (ms) - 防止过短脉冲
#define MOTOR_MAX_CONTINUOUS_TIME_S    30       // 最大连续运行时间 (s) - 防止过热

// 温度限制 (如果支持温度传感器)
#define MOTOR_MAX_TEMPERATURE_C        80       // 最高工作温度 (°C)

// ==================== 信号干扰防护参数 ====================

// 输入信号范围检查
#define MOTOR_VELOCITY_RANGE_MIN       0.0f     // 最小有效速度
#define MOTOR_VELOCITY_RANGE_MAX       60.0f    // 最大允许速度 (稍大于物理限制)
#define MOTOR_ACCELERATION_RANGE_MIN   0.0f     // 最小有效加速度
#define MOTOR_ACCELERATION_RANGE_MAX   600.0f   // 最大允许加速度

// 突变检测阈值
#define MOTOR_VELOCITY_CHANGE_MAX      20.0f    // 最大速度突变 (rpm/控制周期)
#define MOTOR_ACCELERATION_CHANGE_MAX  200.0f   // 最大加速度突变 (mm/s²/控制周期)
#define MOTOR_POSITION_CHANGE_MAX      50.0f    // 最大位置突变 (mm/控制周期)

// 异常检测计数器
#define MOTOR_ERROR_COUNT_MAX          3        // 最大连续错误次数
#define MOTOR_RECOVERY_DELAY_MS        100      // 错误恢复延迟 (ms)

// ==================== 限制检查函数声明 ====================

/**
 * @brief 检查速度是否在安全范围内
 * @param velocity_rpm 速度值 (rpm)
 * @return true: 安全, false: 不安全
 */
bool motor_check_velocity_limit(float velocity_rpm);

/**
 * @brief 检查加速度是否在安全范围内
 * @param acceleration_mm_s2 加速度值 (mm/s²)
 * @return true: 安全, false: 不安全
 */
bool motor_check_acceleration_limit(float acceleration_mm_s2);

/**
 * @brief 检查位移是否在安全范围内
 * @param displacement_mm 位移值 (mm)
 * @param current_pos_mm 当前位置 (mm)
 * @return true: 安全, false: 不安全
 */
bool motor_check_displacement_limit(float displacement_mm, float current_pos_mm);

/**
 * @brief 检查位置是否在机械限位内
 * @param position_mm 目标位置 (mm)
 * @return true: 在限位内, false: 超出限位
 */
bool motor_check_position_limit(float position_mm);

/**
 * @brief 检查速度突变是否过大
 * @param new_velocity 新速度值
 * @param old_velocity 旧速度值
 * @param delta_time_ms 时间间隔 (ms)
 * @return true: 突变正常, false: 突变过大
 */
bool motor_check_velocity_change(float new_velocity, float old_velocity, uint32_t delta_time_ms);

/**
 * @brief 检查加速度突变是否过大
 * @param new_acceleration 新加速度值
 * @param old_acceleration 旧加速度值
 * @param delta_time_ms 时间间隔 (ms)
 * @return true: 突变正常, false: 突变过大
 */
bool motor_check_acceleration_change(float new_acceleration, float old_acceleration, uint32_t delta_time_ms);

/**
 * @brief 应用速度限制 (钳位到安全范围)
 * @param velocity_rpm 原始速度值
 * @return 限制后的速度值
 */
float motor_apply_velocity_limit(float velocity_rpm);

/**
 * @brief 应用加速度限制 (钳位到安全范围)
 * @param acceleration_mm_s2 原始加速度值
 * @return 限制后的加速度值
 */
float motor_apply_acceleration_limit(float acceleration_mm_s2);

/**
 * @brief 应用位移限制 (钳位到安全范围)
 * @param displacement_mm 原始位移值
 * @param current_pos_mm 当前位置
 * @return 限制后的位移值
 */
float motor_apply_displacement_limit(float displacement_mm, float current_pos_mm);

/**
 * @brief 检查电机参数是否全部安全
 * @param velocity_rpm 速度
 * @param acceleration_mm_s2 加速度
 * @param displacement_mm 位移
 * @param current_pos_mm 当前位置
 * @return 错误码: 0=安全, 其他=具体错误
 */
uint8_t motor_check_all_limits(float velocity_rpm, float acceleration_mm_s2, 
                               float displacement_mm, float current_pos_mm);

/**
 * @brief 获取限制检查的错误描述
 * @param error_code 错误码
 * @return 错误描述字符串
 */
const char* motor_get_limit_error_string(uint8_t error_code);

// ==================== 错误码定义 ====================

typedef enum {
    MOTOR_LIMIT_OK = 0,              // 正常
    MOTOR_LIMIT_VELOCITY_HIGH,       // 速度过高
    MOTOR_LIMIT_VELOCITY_LOW,        // 速度过低(无效)
    MOTOR_LIMIT_ACCELERATION_HIGH,   // 加速度过高
    MOTOR_LIMIT_ACCELERATION_LOW,    // 加速度过低(无效)
    MOTOR_LIMIT_DISPLACEMENT_HIGH,   // 位移过大
    MOTOR_LIMIT_DISPLACEMENT_LOW,    // 位移过小(无效)
    MOTOR_LIMIT_POSITION_HIGH,       // 位置超出正向限位
    MOTOR_LIMIT_POSITION_LOW,        // 位置超出负向限位
    MOTOR_LIMIT_VELOCITY_CHANGE,     // 速度突变过大
    MOTOR_LIMIT_ACCELERATION_CHANGE, // 加速度突变过大
    MOTOR_LIMIT_POSITION_CHANGE,     // 位置突变过大
    MOTOR_LIMIT_ERROR_COUNT,         // 错误次数过多
} MotorLimitError_t;

#endif /* MOTOR_LIMITS_H */