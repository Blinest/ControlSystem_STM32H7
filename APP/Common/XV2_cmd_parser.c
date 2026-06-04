#include "XV2_cmd_parser.h"

#include <stdio.h>
#include <string.h>

#include "usart.h"
#include "Motor/Motor.h"

/* ---------- 内部辅助函数 ---------- */

/* 大端转主机字节序 */
static inline uint16_t be16_to_host(const uint8_t *p) {
    return ((uint16_t)p[0] << 8) | p[1];
}

static inline uint32_t be32_to_host(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8)  | p[3];
}

/* 根据功能码获取期望的数据长度（字节数，不含功能码和校验） */
static uint8_t get_expected_data_len(uint8_t fc) {
    switch (fc) {
        case X_V2_FC_MOTOR_ENABLE:          return 1;
        case X_V2_FC_READ_REALTIME_SPD:     return 3;
        case X_V2_FC_READ_REALTIME_POS:     return 5;
        case X_V2_FC_READ_TARGET_POS:       return 5;
        case X_V2_FC_READ_REALTIME_TPOS:    return 5;
        case X_V2_FC_READ_MOTOR_STATUS:     return 1;
        case X_V2_FC_READ_FW_VERSION:       return 4;   // 示例，可根据实际调整
        case X_V2_FC_READ_HOMING_PARAM:     return 6;   // 示例
        case X_V2_FC_READ_DRV_CONFIG:       return 5;
        case X_V2_FC_READ_SYS_STATUS:       return 6;
        case X_V2_FC_READ_PHASE_RL:         return 4;
        case X_V2_FC_READ_POS_PID:          return 6;
        case X_V2_FC_READ_BUS_VOLTAGE:      return 2;
        case X_V2_FC_READ_PHASE_CURRENT:    return 4;
        case X_V2_FC_READ_ENCODER:          return 5;
        case X_V2_FC_READ_INPUT_PULSES:     return 5;
        case X_V2_FC_READ_POS_ERROR:        return 5;
        case X_V2_FC_WRITE_MICROSTEP:       return 1;
        case X_V2_FC_WRITE_ID_ADDR:         return 1;
        case X_V2_FC_WRITE_LOOP_MODE:       return 1;
        case X_V2_FC_IMMEDIATE_STOP:        return 1;
        case X_V2_FC_SYNC_MOTION:           return 1;
        case X_V2_FC_SET_HOME:              return 1;
        case X_V2_FC_TRIGGER_HOMING:        return 1;
        case X_V2_FC_ABORT_HOMING:          return 1;
        case X_V2_FC_TRIGGER_CALIB:         return 1;
        case X_V2_FC_CLEAR_POSITION:        return 1;
        case X_V2_FC_RELEASE_STALL:         return 1;
        case X_V2_FC_FACTORY_RESET:         return 1;
        default:
            return 0;   // 未知功能码，假定无数据
    }
}

/* 计算累加和校验（地址 + 功能码 + 数据） */
static uint8_t calc_checksum(uint8_t addr, uint8_t fc, const uint8_t *data, uint8_t data_len) {
    uint8_t sum = addr + fc;
    for (uint8_t i = 0; i < data_len; i++) {
        sum += data[i];
    }
    return sum;
}

/* 帧解析完成后的数据处理 */
static void process_frame_data(GlobalMotor *motor, uint8_t fc, const uint8_t *data, uint8_t data_len) {
    switch (fc) {
        case X_V2_FC_MOTOR_ENABLE:
            if (data_len >= 1) {
                motor->state = (data[0] == 0x02) ? 1 : 0;
            	// char msg[20];
            	// int len = sprintf(msg, "id: %d\r\n", motor->id);
            	// Usart_SendString(&huart1, (uint8_t*)msg, len);   // 发送到调试串口
            }
            break;

        case X_V2_FC_READ_REALTIME_SPD:
            if (data_len >= 3) {
                uint8_t dir = data[0];
                uint16_t raw = be16_to_host(&data[1]);
                motor->current_vel = (dir == 0) ? (float)raw / 10.0f : -(float)raw / 10.0f;
                motor->motor.stepper_motor.current_vel = motor->current_vel * motor->motor.stepper_motor.daocheng / 60.0f;
            }
            break;

        case X_V2_FC_READ_REALTIME_POS:
            if (data_len == 5) {
                uint8_t dir = data[0];
                uint32_t raw = be32_to_host(&data[1]);
                motor->current_pos = (dir == 0) ? (float)raw * 360 / 65536: -(float)raw * 360 / 65536;
                motor->motor.stepper_motor.current_pos = motor->current_pos * motor->motor.stepper_motor.daocheng / 360.0f;

            }
            break;

        case X_V2_FC_READ_TARGET_POS:
        case X_V2_FC_READ_REALTIME_TPOS:
            if (data_len >= 5) {
                int32_t raw = (int32_t)be32_to_host(&data[1]);
                motor->target_pos = (float)(raw & 0xFFFF) * 360.0f / 65536.0f;
            }
            break;

        case X_V2_FC_READ_MOTOR_STATUS:
            if (data_len >= 1) {
                motor->state = (data[0] & 0x01) ? 1 : 0;
            }
            break;

        // 以下功能码暂不解析，仅作为占位（可按需添加）
        case X_V2_FC_READ_FW_VERSION:
        case X_V2_FC_READ_HOMING_PARAM:
        case X_V2_FC_READ_DRV_CONFIG:
        case X_V2_FC_READ_SYS_STATUS:
        case X_V2_FC_READ_PHASE_RL:
        case X_V2_FC_READ_POS_PID:
        case X_V2_FC_READ_BUS_VOLTAGE:
        case X_V2_FC_READ_PHASE_CURRENT:
        case X_V2_FC_READ_ENCODER:
        case X_V2_FC_READ_INPUT_PULSES:
        case X_V2_FC_READ_POS_ERROR:
        case X_V2_FC_WRITE_MICROSTEP:
        case X_V2_FC_WRITE_ID_ADDR:
        case X_V2_FC_WRITE_LOOP_MODE:
        case X_V2_FC_IMMEDIATE_STOP:
        case X_V2_FC_SYNC_MOTION:
        case X_V2_FC_SET_HOME:
        case X_V2_FC_TRIGGER_HOMING:
        case X_V2_FC_ABORT_HOMING:
        case X_V2_FC_TRIGGER_CALIB:
        case X_V2_FC_CLEAR_POSITION:
        case X_V2_FC_RELEASE_STALL:
        case X_V2_FC_FACTORY_RESET:
            break;

        default:
            // 未知功能码，不处理
            break;
    }
}

/* ---------- 对外接口实现 ---------- */

void X_V2_SerialParser_Init(X_V2_SerialParser *parser) {
    if (!parser) return;
    memset(parser, 0, sizeof(X_V2_SerialParser));
    parser->state = X_V2_STATE_IDLE;
}

/**
 * 重置解析器状态
 * @param parser ：解析器指针
 */
void X_V2_SerialParser_Reset(X_V2_SerialParser *parser) {
    X_V2_SerialParser_Init(parser);
}

X_V2_ParseResult X_V2_SerialParser_Feed(X_V2_SerialParser *parser, uint8_t byte,
                                        GlobalMotor *motor, bool verify_cs) {
    if (!parser || !motor) {
        return X_V2_PARSE_INCOMPLETE;
    }

    switch (parser->state) {
        case X_V2_STATE_IDLE:
            // 第一个字节：地址
            if (byte == motor->id || byte == X_V2_BROADCAST_ADDR) {
                parser->addr = byte;
                parser->checksum = byte;    // 地址参与校验累加
                parser->state = X_V2_STATE_FC;
            } else {
                // 地址不匹配，但不清除状态，因为后续字节可能仍属于上一帧？
                // 通常地址不匹配意味着该帧不是发往本机的，直接丢弃并保持IDLE
                return X_V2_PARSE_ADDR_MISMATCH;
            }
            break;

        case X_V2_STATE_FC:
            // 第二个字节：功能码

            parser->fc = byte;
            parser->checksum += byte;


    		// 获取字节长度
            parser->expected_data_len = get_expected_data_len(byte);
            if (parser->expected_data_len > X_V2_MAX_DATA_LEN) {
                X_V2_SerialParser_Reset(parser);
                return X_V2_PARSE_LEN_OVERFLOW;
            }
            parser->data_index = 0;
            if (parser->expected_data_len == 0) {
                // 无数据段，直接跳转到等待校验
                parser->state = X_V2_STATE_CHECK;
            } else {
                parser->state = X_V2_STATE_DATA;
            }
            break;

        case X_V2_STATE_DATA:
            // 数据字节
            if (parser->data_index < parser->expected_data_len) {
                parser->buffer[parser->data_index++] = byte;
                parser->checksum += byte;
            }
            if (parser->data_index >= parser->expected_data_len) {
                parser->state = X_V2_STATE_CHECK;
            }
            break;

        case X_V2_STATE_CHECK:
            // 校验字节
            {
                uint8_t expected_cs = 0;
                if (verify_cs) {
                    // 根据用户配置选择校验算法
                	// 1、方式一（当前为累加和）
                    // expected_cs = calc_checksum(parser->addr, parser->fc,
                    //                             parser->buffer, parser->expected_data_len);
                	// 2、方式二(6B)
                	expected_cs = 0x6B;

                    if (byte != expected_cs) {
                        X_V2_SerialParser_Reset(parser);
                        return X_V2_PARSE_CHECKSUM_ERR;
                    }
                }
                // 校验通过（或跳过），处理数据
                process_frame_data(motor, parser->fc, parser->buffer, parser->expected_data_len);
                X_V2_SerialParser_Reset(parser);
                return X_V2_PARSE_OK;
            }
            break;

        default:
            X_V2_SerialParser_Reset(parser);
            break;
    }

    return X_V2_PARSE_INCOMPLETE;
}