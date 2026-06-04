#ifndef XV2_CMD_PARSER_H
#define XV2_CMD_PARSER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- 功能码定义（保持与原 CAN 版本一致） ---------- */
#define X_V2_FC_MOTOR_ENABLE           0xF3
#define X_V2_FC_READ_REALTIME_SPD      0x35
#define X_V2_FC_READ_REALTIME_POS      0x36
#define X_V2_FC_READ_TARGET_POS        0x33
#define X_V2_FC_READ_REALTIME_TPOS     0x34
#define X_V2_FC_READ_MOTOR_STATUS      0x3A
#define X_V2_FC_READ_FW_VERSION        0x1F
#define X_V2_FC_READ_HOMING_PARAM      0x22
#define X_V2_FC_READ_DRV_CONFIG        0x42
#define X_V2_FC_READ_SYS_STATUS        0x43
#define X_V2_FC_READ_PHASE_RL          0x45
#define X_V2_FC_READ_POS_PID           0x30
#define X_V2_FC_READ_BUS_VOLTAGE       0x3B
#define X_V2_FC_READ_PHASE_CURRENT     0x3C
#define X_V2_FC_READ_ENCODER           0x31
#define X_V2_FC_READ_INPUT_PULSES      0x32
#define X_V2_FC_READ_POS_ERROR         0x37
#define X_V2_FC_WRITE_MICROSTEP        0x84
#define X_V2_FC_WRITE_ID_ADDR          0xAE
#define X_V2_FC_WRITE_LOOP_MODE        0x46
#define X_V2_FC_IMMEDIATE_STOP         0xFE
#define X_V2_FC_SYNC_MOTION            0xFF
#define X_V2_FC_SET_HOME               0x93
#define X_V2_FC_TRIGGER_HOMING         0x9A
#define X_V2_FC_ABORT_HOMING           0x9C
#define X_V2_FC_TRIGGER_CALIB          0x06
#define X_V2_FC_CLEAR_POSITION         0x0A
#define X_V2_FC_RELEASE_STALL          0x0E
#define X_V2_FC_FACTORY_RESET          0x0F

/* ---------- 协议常量 ---------- */
#define X_V2_MAX_DATA_LEN              8       // 最大数据长度（不含地址、功能码、校验）
#define X_V2_BROADCAST_ADDR            0x00    // 广播地址（根据需要处理）

/* ---------- 解析器状态 ---------- */
typedef enum {
    X_V2_STATE_IDLE,        // 等待地址字节
    X_V2_STATE_FC,          // 等待功能码
    X_V2_STATE_DATA,        // 接收数据
    X_V2_STATE_CHECK        // 等待校验字节
} X_V2_ParserState;

/* ---------- 解析结果 ---------- */
typedef enum {
    X_V2_PARSE_OK = 0,
    X_V2_PARSE_INCOMPLETE,      // 帧未完成，需继续接收
    X_V2_PARSE_ADDR_MISMATCH,   // 地址不匹配（非本电机或广播）
    X_V2_PARSE_CHECKSUM_ERR,    // 校验错误
    X_V2_PARSE_LEN_OVERFLOW,    // 数据长度溢出
    X_V2_PARSE_UNKNOWN_FC       // 未知功能码（仅警告，不影响状态机）
} X_V2_ParseResult;

/* ---------- 电机结构体前向声明 ---------- */
struct GlobalMotor;
typedef struct GlobalMotor GlobalMotor;

/* ---------- 解析器上下文 ---------- */
typedef struct {
    X_V2_ParserState state;                     // 当前状态
    uint8_t addr;                               // 本帧地址
    uint8_t fc;                                 // 功能码（暂存，用于长度判断）
    uint8_t buffer[X_V2_MAX_DATA_LEN];          // 数据缓冲区（仅数据部分，不含功能码）
    uint8_t data_index;                         // 当前已接收数据字节数
    uint8_t expected_data_len;                  // 期望的数据长度（根据FC查表）
    uint8_t checksum;                           // 累加和（若校验方式为累加和）
} X_V2_SerialParser;

/* ---------- 对外接口 ---------- */

/**
 * @brief 初始化解析器实例
 */
void X_V2_SerialParser_Init(X_V2_SerialParser *parser);

/**
 * @brief 重置解析器（丢弃当前帧，回到IDLE状态）
 */
void X_V2_SerialParser_Reset(X_V2_SerialParser *parser);

/**
 * @brief 喂入一个字节，状态机自动处理
 * @param parser    解析器实例指针
 * @param byte      接收到的字节
 * @param motor     对应的电机对象（用于地址匹配与数据更新）
 * @param verify_cs 是否进行校验（通常为 true）
 * @return 解析结果，X_V2_PARSE_OK 表示一帧处理完成且已更新电机数据
 */
X_V2_ParseResult X_V2_SerialParser_Feed(X_V2_SerialParser *parser, uint8_t byte,
                                        GlobalMotor *motor, bool verify_cs);

#ifdef __cplusplus
}
#endif

#endif /* XV2_CMD_PARSER_H */