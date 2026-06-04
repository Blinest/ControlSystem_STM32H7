/**
 * @file motor_limits.c
 * @brief 电机参数限制条件实现
 * 
 * 防止因信号干扰出现异常问题，对加速度、速度、位移距离添加限制条件
 * 
 * @date 2026-04-02
 * @author Psyduck
 */

#include "motor_limits.h"
#include "Motor.h"
#include <stdio.h>

// ==================== 全局变量 ====================

// 历史数据记录 (用于突变检测)
typedef struct {
    float last_velocity;
    float last_acceleration;
    float last_position;
    uint32_t last_update_time;
    uint8_t error_count;
} MotorHistory_t;

static MotorHistory_t motor_history[MOTOR_NUM];

// ==================== 限制检查函数实现 ====================

bool motor_check_velocity_limit(float velocity_rpm)
{
    // 检查是否为有效数值
    if (isnan(velocity_rpm) || isinf(velocity_rpm)) {
        return false;
    }
    
    // 检查范围
    if (velocity_rpm < MOTOR_VELOCITY_RANGE_MIN || 
        velocity_rpm > MOTOR_VELOCITY_RANGE_MAX) {
        return false;
    }
    
    // 检查物理限制
    if (velocity_rpm > MOTOR_MAX_VELOCITY_RPM) {
        return false;
    }
    
    return true;
}

bool motor_check_acceleration_limit(float acceleration_mm_s2)
{
    // 检查是否为有效数值
    if (isnan(acceleration_mm_s2) || isinf(acceleration_mm_s2)) {
        return false;
    }
    
    // 检查范围
    if (acceleration_mm_s2 < MOTOR_ACCELERATION_RANGE_MIN || 
        acceleration_mm_s2 > MOTOR_ACCELERATION_RANGE_MAX) {
        return false;
    }
    
    // 检查物理限制 (取绝对值)
    float abs_acc = fabsf(acceleration_mm_s2);
    if (abs_acc > MOTOR_MAX_ACCELERATION_MM_S2) {
        return false;
    }
    
    return true;
}

bool motor_check_displacement_limit(float displacement_mm, float current_pos_mm)
{
    // 检查是否为有效数值
    if (isnan(displacement_mm) || isinf(displacement_mm) ||
        isnan(current_pos_mm) || isinf(current_pos_mm)) {
        return false;
    }
    
    // 检查位移大小
    float abs_displacement = fabsf(displacement_mm);
    if (abs_displacement < MOTOR_MIN_DISPLACEMENT_MM) {
        // 位移过小，可能是噪声
        return false;
    }
    
    if (abs_displacement > MOTOR_MAX_DISPLACEMENT_MM) {
        // 位移过大，可能是错误指令
        return false;
    }
    
    // 检查最终位置是否在限位内
    float target_pos = current_pos_mm + displacement_mm;
    if (!motor_check_position_limit(target_pos)) {
        return false;
    }
    
    return true;
}

bool motor_check_position_limit(float position_mm)
{
    // 检查是否为有效数值
    if (isnan(position_mm) || isinf(position_mm)) {
        return false;
    }
    
    // 检查机械限位
    if (position_mm > MOTOR_POSITIVE_LIMIT_MM || 
        position_mm < MOTOR_NEGATIVE_LIMIT_MM) {
        return false;
    }
    
    return true;
}

bool motor_check_velocity_change(float new_velocity, float old_velocity, uint32_t delta_time_ms)
{
    if (delta_time_ms == 0) {
        return true; // 避免除零
    }
    
    float delta_velocity = fabsf(new_velocity - old_velocity);
    float delta_time_s = delta_time_ms / 1000.0f;
    float velocity_change_rate = delta_velocity / delta_time_s;
    
    return (velocity_change_rate <= MOTOR_VELOCITY_CHANGE_MAX);
}

bool motor_check_acceleration_change(float new_acceleration, float old_acceleration, uint32_t delta_time_ms)
{
    if (delta_time_ms == 0) {
        return true; // 避免除零
    }
    
    float delta_acceleration = fabsf(new_acceleration - old_acceleration);
    float delta_time_s = delta_time_ms / 1000.0f;
    float acceleration_change_rate = delta_acceleration / delta_time_s;
    
    return (acceleration_change_rate <= MOTOR_ACCELERATION_CHANGE_MAX);
}

float motor_apply_velocity_limit(float velocity_rpm)
{
    // 处理无效数值
    if (isnan(velocity_rpm) || isinf(velocity_rpm)) {
        return 0.0f;
    }
    
    // 钳位到有效范围
    if (velocity_rpm < MOTOR_VELOCITY_RANGE_MIN) {
        velocity_rpm = MOTOR_VELOCITY_RANGE_MIN;
    }
    
    if (velocity_rpm > MOTOR_VELOCITY_RANGE_MAX) {
        velocity_rpm = MOTOR_VELOCITY_RANGE_MAX;
    }
    
    // 钳位到物理限制
    if (velocity_rpm > MOTOR_MAX_VELOCITY_RPM) {
        velocity_rpm = MOTOR_MAX_VELOCITY_RPM;
    }
    
    return velocity_rpm;
}

float motor_apply_acceleration_limit(float acceleration_mm_s2)
{
    // 处理无效数值
    if (isnan(acceleration_mm_s2) || isinf(acceleration_mm_s2)) {
        return 0.0f;
    }
    
    // 钳位到有效范围
    if (acceleration_mm_s2 < MOTOR_ACCELERATION_RANGE_MIN) {
        acceleration_mm_s2 = MOTOR_ACCELERATION_RANGE_MIN;
    }
    
    if (acceleration_mm_s2 > MOTOR_ACCELERATION_RANGE_MAX) {
        acceleration_mm_s2 = MOTOR_ACCELERATION_RANGE_MAX;
    }
    
    // 钳位到物理限制 (考虑正负)
    float max_acc = MOTOR_MAX_ACCELERATION_MM_S2;
    if (acceleration_mm_s2 > max_acc) {
        acceleration_mm_s2 = max_acc;
    } else if (acceleration_mm_s2 < -max_acc) {
        acceleration_mm_s2 = -max_acc;
    }
    
    return acceleration_mm_s2;
}

float motor_apply_displacement_limit(float displacement_mm, float current_pos_mm)
{
    // 处理无效数值
    if (isnan(displacement_mm) || isinf(displacement_mm) ||
        isnan(current_pos_mm) || isinf(current_pos_mm)) {
        return 0.0f;
    }
    
    // 计算目标位置
    float target_pos = current_pos_mm + displacement_mm;
    
    // 检查并钳位到机械限位
    if (target_pos > MOTOR_POSITIVE_LIMIT_MM) {
        target_pos = MOTOR_POSITIVE_LIMIT_MM;
    } else if (target_pos < MOTOR_NEGATIVE_LIMIT_MM) {
        target_pos = MOTOR_NEGATIVE_LIMIT_MM;
    }
    
    // 重新计算位移
    float limited_displacement = target_pos - current_pos_mm;
    
    // 检查位移大小
    float abs_displacement = fabsf(limited_displacement);
    if (abs_displacement < MOTOR_MIN_DISPLACEMENT_MM) {
        // 位移过小，忽略
        return 0.0f;
    }
    
    if (abs_displacement > MOTOR_MAX_DISPLACEMENT_MM) {
        // 位移过大，限制到最大值
        if (limited_displacement > 0) {
            limited_displacement = MOTOR_MAX_DISPLACEMENT_MM;
        } else {
            limited_displacement = -MOTOR_MAX_DISPLACEMENT_MM;
        }
    }
    
    return limited_displacement;
}

uint8_t motor_check_all_limits(float velocity_rpm, float acceleration_mm_s2, 
                               float displacement_mm, float current_pos_mm)
{
    // 检查速度
    if (!motor_check_velocity_limit(velocity_rpm)) {
        if (velocity_rpm > MOTOR_MAX_VELOCITY_RPM) {
            return MOTOR_LIMIT_VELOCITY_HIGH;
        } else {
            return MOTOR_LIMIT_VELOCITY_LOW;
        }
    }
    
    // 检查加速度
    if (!motor_check_acceleration_limit(acceleration_mm_s2)) {
        if (fabsf(acceleration_mm_s2) > MOTOR_MAX_ACCELERATION_MM_S2) {
            return MOTOR_LIMIT_ACCELERATION_HIGH;
        } else {
            return MOTOR_LIMIT_ACCELERATION_LOW;
        }
    }
    
    // 检查位移
    if (!motor_check_displacement_limit(displacement_mm, current_pos_mm)) {
        float abs_displacement = fabsf(displacement_mm);
        if (abs_displacement > MOTOR_MAX_DISPLACEMENT_MM) {
            return MOTOR_LIMIT_DISPLACEMENT_HIGH;
        } else if (abs_displacement < MOTOR_MIN_DISPLACEMENT_MM) {
            return MOTOR_LIMIT_DISPLACEMENT_LOW;
        }
        
        // 检查位置限位
        float target_pos = current_pos_mm + displacement_mm;
        if (target_pos > MOTOR_POSITIVE_LIMIT_MM) {
            return MOTOR_LIMIT_POSITION_HIGH;
        } else if (target_pos < MOTOR_NEGATIVE_LIMIT_MM) {
            return MOTOR_LIMIT_POSITION_LOW;
        }
    }
    
    return MOTOR_LIMIT_OK;
}

