//
// Created on 2026/6/5.
//
/**
 * @file servo.c
 * @brief SCSCL 串行舵机应用层实现
 *
 * 使用 SyncWritePos 广播模式（0xFE）控制舵机，
 * 广播指令不期望应答，避免与 UART1 共享队列（MotorDataParseQueueHandle）的读取冲突。
 */

#include "servo.h"
#include "SCSCL.h"
#include "SCS.h"
#include "usart.h"
#include "cmsis_os2.h"
#include <string.h>

/* 舵机 ID 映射：索引 0~8 对应舵机 ID 1~9 */
static uint8_t servo_ids[SERVO_NUM];

void servo_init(void)
{


    /* 初始化 ID 映射 */
    for (uint8_t i = 0; i < SERVO_NUM; i++) {
        servo_ids[i] = i + 1;
    }

    /* 设置 SCSCL 协议字节序（小端，飞特默认） */
    setEnd(0);

    /*
     * 关闭应答模式：使 genWrite / writeByte 等不再等待舵机应答包，
     * 避免与 DataTask 共享 MotorDataParseQueueHandle 时阻塞 200ms。
     * SyncWritePos 广播本身就不期望应答，此设置保持一致。
     */
    setLevel(0);

    /* 使能所有舵机扭矩 */
    for (uint8_t i = 0; i < SERVO_NUM; i++) {
        EnableTorque(servo_ids[i], 1);
    }

    /* 所有舵机回中位 */
    uint16_t pos[SERVO_NUM];
    for (uint8_t i = 0; i < SERVO_NUM; i++) {
        pos[i] = SERVO_POS_CENTER;
    }
    SyncWritePos(servo_ids, SERVO_NUM, pos, NULL, NULL);
}

void servo_set_pos(uint8_t idx, uint16_t pos)
{
    if (idx >= SERVO_NUM) return;
    if (pos > SERVO_POS_MAX) pos = SERVO_POS_MAX;

    uint16_t pos_arr[SERVO_NUM];
    /* 保持其他舵机当前位置不变（这里统一用传入的 pos 设置目标舵机） */
    for (uint8_t i = 0; i < SERVO_NUM; i++) {
        pos_arr[i] = SERVO_POS_CENTER; /* 简化：未指定的舵机保持中位 */
    }
    pos_arr[idx] = pos;
    SyncWritePos(servo_ids, SERVO_NUM, pos_arr, NULL, NULL);
}

void servo_set_pos_all(uint16_t pos[])
{
    if (pos == NULL) return;

    /* 范围限制 */
    for (uint8_t i = 0; i < SERVO_NUM; i++) {
        if (pos[i] > SERVO_POS_MAX) pos[i] = SERVO_POS_MAX;
    }

    SyncWritePos(servo_ids, SERVO_NUM, pos, NULL, NULL);
}

void servo_set_pos_sync(uint16_t pos[], uint16_t time[], uint16_t speed[])
{
    if (pos == NULL) return;

    /* 范围限制 */
    for (uint8_t i = 0; i < SERVO_NUM; i++) {
        if (pos[i] > SERVO_POS_MAX) pos[i] = SERVO_POS_MAX;
    }

    SyncWritePos(servo_ids, SERVO_NUM, pos, time, speed);
}

void servo_disable_all(void)
{
    for (uint8_t i = 0; i < SERVO_NUM; i++) {
        EnableTorque(servo_ids[i], 0);
    }
}

void servo_test_demo(void)
{
    uint16_t pos[SERVO_NUM];
    const uint16_t offset = 100; /* 偏移量：约 ±18° */

    /* 第一轮：逐个舵机测试 */
    for (uint8_t idx = 0; idx < SERVO_NUM; idx++) {
        /* 回中位 */
        for (uint8_t i = 0; i < SERVO_NUM; i++) pos[i] = SERVO_POS_CENTER;
        SyncWritePos(servo_ids, SERVO_NUM, pos, NULL, NULL);
        osDelay(500);

        /* 正偏移 */
        for (uint8_t i = 0; i < SERVO_NUM; i++) pos[i] = SERVO_POS_CENTER;
        pos[idx] = SERVO_POS_CENTER + offset;
        SyncWritePos(servo_ids, SERVO_NUM, pos, NULL, NULL);
        osDelay(500);

        /* 负偏移 */
        for (uint8_t i = 0; i < SERVO_NUM; i++) pos[i] = SERVO_POS_CENTER;
        pos[idx] = SERVO_POS_CENTER - offset;
        SyncWritePos(servo_ids, SERVO_NUM, pos, NULL, NULL);
        osDelay(500);
    }

    /* 全部回中位 */
    for (uint8_t i = 0; i < SERVO_NUM; i++) pos[i] = SERVO_POS_CENTER;
    SyncWritePos(servo_ids, SERVO_NUM, pos, NULL, NULL);
    osDelay(300);
}
