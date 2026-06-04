/**
*  @file CmdCtrlTask.c
 * @brief 指令控制任务：从队列取串口数据，交给 cmd_parse 解析并执行
 * @author blin
 *
 * 串口2 接收字节经 CmdCtrlQueueHandle 送入本任务，每字节调用 cmd_parse_feed_byte()，
 * 指令解析与电机/传感器控制逻辑在 Common/cmd_parse 中实现。
 */

#include "cmsis_os2.h"
#include "main.h"
#include "string.h"
#include "Common/can_driver.h"

#include "Motor/Motor.h"
#include "stdio.h"
#include "usart.h"
#include "Sensor/Sensor.h"
#include "Camera/dcmi_ov5640.h"
#include "Camera/led.h"
#include "dcmi.h"
#include "CR/skin_detect.h"
#include "CR/contour.h"
#include "CR/hand_gesture.h"
#include "Common/pc_cmd_parser.h"
#define RX_BUF_SIZE 256
#define Camera_Buffer	0x24000000    // 摄像头图像缓冲区（RGB565，DCMI DMA 写入）
#define SkinVis_Buffer	0x24020000    // 肤色检测可视化缓冲区（RGB565，128KB 偏移）

bool is_connected = false;   // false:未连接 true:已连接

void StartPeriphCtrlTask(void *argument)
{
	Usart_SendString(&huart1, "TASK START\r\n", 12);  /* diagnostic: task entry confirmed */

	uint8_t rx_buffer[RX_BUF_SIZE];
	uint16_t rx_len = 0;
	float motor_pos[MOTOR_NUM][3] = {0};
	float sensor_angle[SENSOR_NUM][3] = {0};
    // 测试串口用
    uint8_t test_msg[] = "send to usart1\r\n";
    uint8_t receive;

	// 初始化所有数组为 0.0
	memset(motor_pos, 0, sizeof(motor_pos));
	memset(sensor_angle, 0, sizeof(sensor_angle));

	// 初始化肤色检测（内部分配二值掩码缓冲区）
	skin_detect_init(Display_Width, Display_Height);

    for (;;)
    {
    	//摄像机 — 手势识别
    	if ( OV5640_FrameState == 1 )	// 采集到了一帧图像
    	{
    		OV5640_FrameState = 0;		// 清零标志位

    		// 1. 肤色二值化
    		skin_detect_process((const uint16_t *)Camera_Buffer,
    		                    Display_Width, Display_Height);

    		// 2. 手势识别（内部完成：找最大轮廓→凸包→缺陷→数手指）
    		int fingers = hand_gesture_recognize(skin_mask,
    		                                     Display_Width, Display_Height);

    		// 3. 生成可视化图像（肤色=白，背景=深灰）
    		skin_detect_visualize((uint16_t *)SkinVis_Buffer,
    		                      Display_Width, Display_Height,
    		                      0xFFFF,         // 肤色 → 白色
    		                      0x4208);        // 非肤色 → 深灰

    		// 4. 填充手部区域（暗绿色，先画底层）
    		if (gres.largest_contour >= 0) {
    			contour_draw_filled((uint16_t *)SkinVis_Buffer,
    			                    Display_Width, Display_Height,
    			                    0x03E0, gres.largest_contour);
    		}

    		// 5. 叠加轮廓边界（亮绿色）
    		contour_draw((uint16_t *)SkinVis_Buffer,
    		             Display_Width, Display_Height,
    		             0x07E0, -1);

    		// 6. 叠加凸包 + 缺陷（覆盖在最上层）
    		hand_gesture_draw((uint16_t *)SkinVis_Buffer,
    		                  Display_Width, Display_Height);

    		// 7. 显示
    		LCD_CopyBuffer(0, 0, Display_Width, Display_Height,
    		               (uint16_t *)SkinVis_Buffer);

    		// 8. HUD
    		LCD_SetColor(LCD_GREEN);
    		LCD_DisplayString( 10, 220, "F:");
    		LCD_DisplayNumber( 40, 220, OV5640_FPS, 2);
    		LCD_DisplayString( 80, 220, "H:");
    		LCD_DisplayNumber(110, 220, fingers, 1);

    		// 9. Output direction hint via USART1
    		{
    		    char buf[32];
    		    if (gres.n_fingers_bend > 0) {
    		        int cx = gres.hand_center.x, cy = gres.hand_center.y;
    		        int dx = cx - 120, dy = cy - 120;
    		        int dead = 20, off = 0;
    		        if      (dx < -dead) off += sprintf(buf + off, "R ");
    		        else if (dx >  dead) off += sprintf(buf + off, "L ");
    		        if      (dy < -dead) off += sprintf(buf + off, "D ");
    		        else if (dy >  dead) off += sprintf(buf + off, "U ");
    		        if (off == 0) off += sprintf(buf, "OK");
    		        off += sprintf(buf + off, "\r\n");
    		        Usart_SendString(&huart1, (uint8_t *)buf, (uint16_t)off);
    		    } else {
    		        // no hand detected, skip output
    		    }
    		}
    		LED1_Toggle;
    	}
        // 电机使能放在循环开始，确保每次循环都执行

	    // 从队列接收上位机指令
	    if (osMessageQueueGet(CmdCtrlQueueHandle, &receive, NULL, 0) == osOK) {
	        // 优先回显，提高响应性
	        if (rx_len < RX_BUF_SIZE) {
	            rx_buffer[rx_len++] = receive;
	        	// 进入指令解析函数，指令解析函数负责指令解析，而后再将解析好的指令传给电机进行解析
	        	pc_cmd_parser_feed_byte(receive);
	        } else {
                rx_len = 0; // 缓冲区溢出重置
            }
	    }
    	static uint32_t last_send_time = 0;
    	static uint32_t last_diag_time = 0;
    	uint32_t current_time = osKernelGetTickCount();

    	// 每2秒输出一次 DCMI 诊断信息
    	if ((current_time - last_diag_time) >= 2000)
    	{
    	    last_diag_time = current_time;
    	    char diag[64];
    	    sprintf(diag, "DCMI state:%d FS:%d FC:%lu FPS:%d EC:%lu\r\n",
    	            (int)hdcmi.State, (int)DCMI_FrameState,
    	            (unsigned long)DCMI_FrameCountTotal, (int)DCMI_FPS,
    	            (unsigned long)DCMI_ErrorCode);
    	    //Usart_SendString(&huart1, diag, strlen(diag));
    	}

    	// 每100ms读取一次位置信息
    	if ((current_time - last_send_time) >= 500)
    	{
    		// 电机状态检测 (使用 static 以节省堆栈空间)
    		motor_status_check();

    		last_send_time = current_time;
    	}
        // 添加小延时，避免过度占用CPU
		osDelay(100);
	  }
}