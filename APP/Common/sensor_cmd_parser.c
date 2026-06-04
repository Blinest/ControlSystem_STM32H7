/**
 * @file sensor_cmd_parser.c
 * @brief 下位机指令解析库
 *
 * 负责解析外设（传感器）反馈的指令，更新全局结构体
 *
 * @date 2026-04-23
 * @author blin
 */

#include "sensor_cmd_parser.h"
#include <string.h>

/**
 * @brief 重置解析器状态
 */
static void SensorParser_Reset(SensorParser *parser) {
    parser->state = SENSOR_STATE_HEAD;
    parser->data_len = 0;
    parser->idx = 0;
    // 缓冲区不清零以提高性能
}

void SensorParser_Init(SensorParser *parser) {
    if (!parser) return;
    memset(parser, 0, sizeof(SensorParser));
    parser->state = SENSOR_STATE_HEAD;
}

/**
 * @brief 计算校验和（累加所有字节的低8位）
 */
static uint8_t calc_checksum(const uint8_t *data, uint8_t len) {
    uint8_t sum = 0;
    for (uint8_t i = 0; i < len; i++) {
        sum += data[i];
    }
    return sum;
}

/**
 * @brief 解析传感器反馈数据并更新结构体，然后触发上报
 */
static void process_sensor_frame(const uint8_t *buf,
								 uint8_t len,
                                 GlobalSensor *sensors,
                                 const GlobalMotor *motors,
                                 const ContinuumRobot *CR) {
    // buf[0] = 地址(SENSOR_ID), buf[1] = 功能码, buf[2] = 数据长度L, buf[3] = 传感器子ID
    if (len < 10) return;  // 最小长度：地址+功能码+L(1)+子ID(1)+x(2)+y(2)+z(2)+校验(1) = 11？实际len不包含校验？这里重新计算
    // 根据协议：帧格式为 [addr][func][L][子ID][x高][x低][y高][y低][z高][z低][cs]
    // buf 中存储了 addr .. data .. cs，在我们的调用中，buf 包含了地址到校验的所有字节，长度 = 1+1+1+1+6+1 = 10？
    uint8_t func = buf[1];
    uint8_t data_len = buf[2];
    uint8_t sensor_id = buf[3];

    if (func == FUNC_SENSOR_FEEDBACK && data_len >= 7 && sensor_id >= 1 && sensor_id <= SENSOR_NUM) {
        int16_t x = (int16_t)((buf[4] << 8) | buf[5]);
        int16_t y = (int16_t)((buf[6] << 8) | buf[7]);
        int16_t z = (int16_t)((buf[8] << 8) | buf[9]);

        // 更新传感器数据
        sensors[sensor_id - 1].x = (uint16_t)x;
        sensors[sensor_id - 1].y = (uint16_t)y;
        sensors[sensor_id - 1].z = (uint16_t)z;

        // 触发系统状态上报（需传入电机、传感器、lqts 和系统状态）
        // 这里系统状态暂时使用 lqts->state，若需其他状态可调整
        cmd_packer_send_status_frame(motor_ctx, sensors, CR, CR->state);
    }
}

void SensorParser_Feed(SensorParser *parser, uint8_t byte,
                       GlobalSensor *sensors,
                       const GlobalMotor *motors,
                       const ContinuumRobot *CR) {
    if (!parser) return;

    switch (parser->state) {
        case SENSOR_STATE_HEAD:
            if (byte == SENSOR_ID) {
                parser->buf[0] = byte;
                parser->idx = 1;
                parser->state = SENSOR_STATE_FUNC;
            }
            break;

        case SENSOR_STATE_FUNC:
            parser->buf[1] = byte;
            parser->idx = 2;
            if (byte == FUNC_SENSOR_FEEDBACK) {
                parser->state = SENSOR_STATE_LEN;
            } else {
                SensorParser_Reset(parser);
            }
            break;

        case SENSOR_STATE_LEN:
            parser->data_len = byte;
            parser->buf[2] = byte;
            // 数据长度不能超过缓冲区剩余空间（总容量 - 帧头(1) - 功能码(1) - L(1) - 校验(1)）
            if (parser->data_len > (sizeof(parser->buf) - 4) || parser->data_len == 0) {
                SensorParser_Reset(parser);
                break;
            }
            parser->idx = 3;
            parser->state = SENSOR_STATE_DATA;
            break;

        case SENSOR_STATE_DATA:
            if (parser->idx < sizeof(parser->buf)) {
                parser->buf[parser->idx++] = byte;
            }
            // 数据长度+起始偏移3（地址、功能码、L）等于当前索引时，数据接收完毕
            if (parser->idx >= (uint8_t)(3 + parser->data_len)) {
                parser->state = SENSOR_STATE_CHECK;
            }
            break;

        case SENSOR_STATE_CHECK:
	        {
		        // 计算已接收字节的校验和（不含本校验字节）
        		uint8_t sum = calc_checksum(parser->buf, parser->idx);
        		if (sum == byte) {
        			// 校验通过，处理帧（buf包含地址到数据的所有字节，总长度为 parser->idx）
        			process_sensor_frame(parser->buf, parser->idx, sensors, motors, CR);
        		}
        		SensorParser_Reset(parser);
        		break;
	        }
        default:
            SensorParser_Reset(parser);
            break;
    }
}