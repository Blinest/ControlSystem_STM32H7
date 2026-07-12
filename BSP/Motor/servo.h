//
// Created on 2026/6/5.
//
/**
 * @file servo.h
 * @brief SCSCL 串行舵机应用层接口
 *
 * 基于飞特 SCSCL 协议栈，提供 9 路舵机控制接口。
 * 通过 SyncWritePos 广播模式避免 UART1 共享队列冲突。
 */

#ifndef CONTROLSYSTEM_SERVO_H
#define CONTROLSYSTEM_SERVO_H

#include <stdint.h>

#define SERVO_NUM         9     /* 肌腱舵机数量 */
#define SERVO_POS_MIN     0     /* 舵机位置最小值 */
#define SERVO_POS_MAX     1023  /* 舵机位置最大值 */
#define SERVO_POS_CENTER  512   /* 舵机中位 (90°) */

/**
 * @brief 舵机初始化：使能所有舵机扭矩，回到中位
 */
void servo_init(void);

/**
 * @brief 设置单个舵机位置（通过 SyncWritePos 广播）
 * @param idx    舵机索引 (0 ~ SERVO_NUM-1)
 * @param pos    目标位置 (0~1023)
 */
void servo_set_pos(uint8_t idx, uint16_t pos);

/**
 * @brief 9 路舵机同步位置控制
 * @param pos[SERVO_NUM]  各舵机目标位置数组
 */
void servo_set_pos_all(uint16_t pos[]);

/**
 * @brief 9 路舵机同步位置控制（带时间和速度）
 * @param pos[SERVO_NUM]    各舵机目标位置
 * @param time[SERVO_NUM]  各舵机运行时间 (ms)，可为 NULL
 * @param speed[SERVO_NUM] 各舵机运行速度，可为 NULL
 */
void servo_set_pos_sync(uint16_t pos[], uint16_t time[], uint16_t speed[]);

/**
 * @brief 关闭所有舵机扭矩
 */
void servo_disable_all(void);

/**
 * @brief 舵机测试 Demo
 *
 * 9 路舵机依次执行：中位 → 正偏移 → 中位 → 负偏移 → 中位
 * 用于验证 UART1 通信和舵机响应
 */
void servo_test_demo(void);

#endif /* CONTROLSYSTEM_SERVO_H */
