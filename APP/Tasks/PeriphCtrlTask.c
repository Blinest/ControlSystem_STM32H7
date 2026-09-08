/**
*  @file PeriphCtrlTask.c
 * @brief 指令控制任务：从队列取串口数据，交给 cmd_parse 解析并执行
 * @author blin
 *
 * 指令解析与电机/传感器控制逻辑在 Common/cmd_parse 中实现。
 */

#include "cmsis_os2.h"
#include "main.h"
#include "string.h"
#include "Common/can_driver.h"

#include "stdio.h"
#include "usart.h"


#include "Common/pc_cmd_parser.h"
#include "CR/CR.h"
#include "Control/ClosedLoop.h"
#include "Sensor/Sensor.h"
#include <math.h>


bool is_connected = false;   // false:未连接 true:已连接

void StartPeriphCtrlTask(void *argument)
{
    uint8_t receive;

    // ==================== 指令解析任务 ====================
    for (;;)
    {
	    // Bus Off 延迟恢复（检查标志并在任务上下文中恢复CAN）
	    CAN_BusOff_Recovery();

	    // 从队列接收上位机指令
	    if (osMessageQueueGet(CmdCtrlQueueHandle, &receive, NULL, 0) == osOK) {
	        pc_cmd_parser_feed_byte(receive);
	    }
	    // 执行闭环控制器的非阻塞迭代（bang-bang 控制模型，独立模块）
	    ClosedLoop_Tick();
	    // 推进自动标定状态机（未标定时立即返回）
	    calibrate_auto_tick();
	    // 推进循环运动状态机（未启动时立即返回）
	    action_group_tick();

		osDelay(10); // 降低 CPU 占用
	}
}