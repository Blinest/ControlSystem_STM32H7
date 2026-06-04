/***
*************************************************************************************************
	*	@file  	main.c
	*	@version V1.0
	*
   *************************************************************************************************
   *  @description
	*
	*+ OV5640模块（型号：OV5640M1-500W）
	*
	*
	*
	*	驱动参考	Arduino/ArduCAM 和 OpenMV 的源码
	*
>>>>> 驱动说明：
	*
	*  1.例程默认配置 OV5640  为 4:3(1280*960) 43帧 的配置（JPG模式2、3情况下帧率会减半）
	*	2.开启了DMA并使能了中断，移植的时候需要移植对应的中断
	*
	******************************************************************************************************************************************************************************************************************************************************************************************
***/

#include "dcmi_ov5640.h"
#include "dcmi_ov5640_cfg.h"
#include "dcmi.h"
#include "stdio.h"
#include "string.h"

/***************************************************************************************************************************************
*  函 数 名: OV5640_DMA_Init
*
*  函数功能: 配置 DMA 相关参数
*
*  说   明: 使用的是DMA2，外设到存储器模式，数据位宽32bit，并使能中断
*****************************************************************************************************************************************/
void OV5640_DMA_Init(void)
{
	__HAL_RCC_DMA2_CLK_ENABLE();   // 使能 DMA2 时钟

	DMA_Handle_dcmi.Instance                     = DMA2_Stream7;               // DMA2数据流7
	DMA_Handle_dcmi.Init.Request                 = DMA_REQUEST_DCMI;           // DMA请求映射到DCMI
	DMA_Handle_dcmi.Init.Direction               = DMA_PERIPH_TO_MEMORY;       // 外设到存储器模式
	DMA_Handle_dcmi.Init.PeriphInc               = DMA_PINC_DISABLE;           // 外设地址固定不变
	DMA_Handle_dcmi.Init.MemInc                  = DMA_MINC_ENABLE;            // 存储器地址自增
	DMA_Handle_dcmi.Init.PeriphDataAlignment     = DMA_PDATAALIGN_WORD;        // DCMI数据位宽 32位
	DMA_Handle_dcmi.Init.MemDataAlignment        = DMA_MDATAALIGN_WORD;        // 存储器数据位宽 32位
	DMA_Handle_dcmi.Init.Mode                    = DMA_CIRCULAR;               // 循环模式
	DMA_Handle_dcmi.Init.Priority                = DMA_PRIORITY_LOW;		   // 优先级低
	DMA_Handle_dcmi.Init.FIFOMode                = DMA_FIFOMODE_ENABLE;        // 使能fifo
	DMA_Handle_dcmi.Init.FIFOThreshold           = DMA_FIFO_THRESHOLD_FULL;    // 全fifo模式，4*32bit大小
	DMA_Handle_dcmi.Init.MemBurst                = DMA_MBURST_SINGLE;          // 单次传输
	DMA_Handle_dcmi.Init.PeriphBurst             = DMA_PBURST_SINGLE;          // 单次传输
	HAL_DMA_Init(&DMA_Handle_dcmi);                        // 初始化DMA
	__HAL_LINKDMA(&hdcmi, DMA_Handle, DMA_Handle_dcmi);    // 关联DCMI句柄

	HAL_NVIC_SetPriority(DMA2_Stream7_IRQn, 0, 0);         // 设置中断优先级
	HAL_NVIC_EnableIRQ(DMA2_Stream7_IRQn);                 // 使能中断
}

/***************************************************************************************************************************************
*	函 数 名: OV5640_Delay
*	入口参数: Delay - 延时时间，单位 ms
*	函数功能: 简单延时函数，不是很精确
*	说    明: 为了移植的简便性,此处采用软件延时，实际项目中可以替换成RTOS的延时或者HAL库的延时
*****************************************************************************************************************************************/
void OV5640_Delay(uint32_t Delay)
{
	volatile uint16_t i;

	while (Delay --)				
	{
		for (i = 0; i < 40000; i++);
	}	
//	HAL_Delay(Delay);	  // 可使用HAL库的延时
}

/***************************************************************************************************************************************
*  函 数 名: DCMI_OV5640_Init
*
*  函数功能: 初始化SCCB、DCMI、DMA以及配置OV5640
*
*****************************************************************************************************************************************/
int8_t DCMI_OV5640_Init(void)
{
    uint16_t Device_ID;        // 用于存储读取到的ID

    SCCB_GPIO_Config();                     // SCCB引脚初始化
    MX_DCMI_OV5640_Init();                 // 初始化DCMI(硬件同步模式,dcmi.c)

    /* 重新配置 DCMI 引脚为 VERY_HIGH 速度 (CubeMX默认是LOW)。
       在80MHz PCLK下，低速GPIO无法及时翻转 → 导致位错误 → 显示错乱。
       参考工程对所有DCMI引脚使用 VERY_HIGH。 */
    {
        GPIO_InitTypeDef GPIO_InitStruct = {0};
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF13_DCMI;

        /* PA6 (PIXCLK) — 对80MHz信号完整性最关键 */
        GPIO_InitStruct.Pin = GPIO_PIN_6;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

        /* PE4(D4), PE5(D6), PE6(D7) */
        GPIO_InitStruct.Pin = GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6;
        HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

        /* PC6(D0), PC7(D1) */
        GPIO_InitStruct.Pin = GPIO_PIN_6 | GPIO_PIN_7;
        HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

        /* PD3(D5) */
        GPIO_InitStruct.Pin = GPIO_PIN_3;
        HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

        /* PA4(HSYNC), PG9(VSYNC), PG10(D2), PG11(D3) — 同样强制 VERY_HIGH */
        GPIO_InitStruct.Pin = GPIO_PIN_4;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

        GPIO_InitStruct.Pin = GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11;
        HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);
    }

    OV5640_DMA_Init();                     // 初始化DMA
    OV5640_Reset();                        // 执行摄像头复位
    Device_ID =  OV5640_ReadID();          // 读取芯片ID
    char str[40];
    if( Device_ID == 0x5640 )             // ID匹配
    {
        sprintf(str, "OV5640 OK,ID:0x%X\r\n", Device_ID);
        Usart_SendString(&huart1, str, strlen(str));

        OV5640_Config();                                                 // 配置寄存器表
        OV5640_Set_Framesize(OV5640_Width,OV5640_Height);                // 设置OV5640的输出图像大小
        OV5640_DCMI_Crop( Display_Width, Display_Height, OV5640_Width, OV5640_Height );  // 裁剪图像以适配屏幕大小（JPG模式下不需要裁剪）
        return OV5640_Success;   // 返回成功标志
    }
    else
    {
        sprintf(str, "OV5640 ERROR!!!!!  ID:%X\r\n", Device_ID); // 读取ID错误
        Usart_SendString(&huart1, str, strlen(str));
        return  OV5640_Error;    // 返回错误标志
    }
}
/***************************************************************************************************************************************
*	函 数 名: OV5640_DMA_Transmit_Continuous
*
*	入口参数:  DMA_Buffer - DMA将要传输的地址，即用于存储摄像头数据的存储区地址
*            DMA_BufferSize - 传输的数据大小，32位宽
*
*	函数功能: 启动DMA传输，连续模式
*
*	说    明: 1. 开启连续模式之后，会一直进行传输，除非挂起或者停止DCMI
*            2. OV5640使用RGB565模式时，1个像素点需要2个字节来存储
*				 3. 因为DMA配置传输数据为32位宽，计算 DMA_BufferSize 时，需要除以4，例如：
*               要获取 240*240分辨率 的图像，需要传输 240*240*2 = 115200 字节的数据，
*               则 DMA_BufferSize = 115200 / 4 = 28800 。
*
*****************************************************************************************************************************************/
/* Local callback replicas of static HAL DCMI functions, needed so we can
   start DMA before DCMI without calling HAL_DCMI_Start_DMA. */
static void ov5640_dma_xfer_cplt(DMA_HandleTypeDef *hdma)
{
    DCMI_HandleTypeDef *hdcmi = (DCMI_HandleTypeDef *)hdma->Parent;
    if (hdcmi->XferCount == hdcmi->XferTransferNumber)
    {
        __HAL_DCMI_ENABLE_IT(hdcmi, DCMI_IT_FRAME);
        if ((hdcmi->Instance->CR & DCMI_CR_CM) == DCMI_MODE_SNAPSHOT)
            hdcmi->State = HAL_DCMI_STATE_READY;
    }
}

static void ov5640_dma_xfer_error(DMA_HandleTypeDef *hdma)
{
    DCMI_HandleTypeDef *hdcmi = (DCMI_HandleTypeDef *)hdma->Parent;
    if (hdcmi->DMA_Handle->ErrorCode != HAL_DMA_ERROR_FE)
    {
        hdcmi->State = HAL_DCMI_STATE_READY;
        hdcmi->ErrorCode |= HAL_DCMI_ERROR_DMA;
    }
    HAL_DCMI_ErrorCallback(hdcmi);
}

void OV5640_DMA_Transmit_Continuous(uint32_t DMA_Buffer,uint32_t DMA_BufferSize)
{
   char diag[64];

   sprintf(diag, "DMA_State:%d DCMI_State:%d\r\n",
           (int)DMA_Handle_dcmi.State, (int)hdcmi.State);
   Usart_SendString(&huart1, diag, strlen(diag));

   /* DMA already configured by OV5640_DMA_Init() — skip redundant HAL_DMA_Init */

   /* Clear stale error state */
   hdcmi.ErrorCode = HAL_DCMI_ERROR_NONE;

   /* No __HAL_LOCK — called before RTOS starts, no concurrency */

   hdcmi.State = HAL_DCMI_STATE_BUSY;

   /* Configure continuous mode */
   hdcmi.Instance->CR &= ~(DCMI_CR_CM);

   /* Set DMA callbacks on the active handle */
   hdcmi.DMA_Handle->XferCpltCallback  = ov5640_dma_xfer_cplt;
   hdcmi.DMA_Handle->XferErrorCallback = ov5640_dma_xfer_error;
   hdcmi.DMA_Handle->XferAbortCallback = NULL;

   /* Reset transfer counters */
   hdcmi.XferCount = 0;
   hdcmi.XferTransferNumber = 0;
   hdcmi.XferSize = 0;
   hdcmi.pBuffPtr = 0;

   /* Start DMA BEFORE enabling DCMI, so the stream is armed and ready to
      drain the DCMI FIFO the instant capture begins. */
   if (HAL_DMA_Start_IT(hdcmi.DMA_Handle,
                        (uint32_t)&hdcmi.Instance->DR,
                        (uint32_t)DMA_Buffer,
                        DMA_BufferSize) != HAL_OK)
   {
       hdcmi.ErrorCode = HAL_DCMI_ERROR_DMA;
       hdcmi.State = HAL_DCMI_STATE_READY;

       sprintf(diag, "DMA_START_FAILED DCMI_EC:0x%lX\r\n",
               (unsigned long)hdcmi.ErrorCode);
       Usart_SendString(&huart1, diag, strlen(diag));
       return;
   }

   /* DMA is running — now enable DCMI and start capture */
   __HAL_DCMI_ENABLE(&hdcmi);
   hdcmi.Instance->CR |= DCMI_CR_CAPTURE;

   sprintf(diag, "Start_DMA OK DCMI_State:%d\r\n", (int)hdcmi.State);
   Usart_SendString(&huart1, diag, strlen(diag));
}

void OV5640_DMA_Transmit_Snapshot(uint32_t DMA_Buffer,uint32_t DMA_BufferSize)
{
   DMA_Handle_dcmi.Init.Mode  = DMA_NORMAL;  // ����ģʽ					

   HAL_DMA_Init(&DMA_Handle_dcmi);    // ����DMA

   HAL_DCMI_Start_DMA(&hdcmi, DCMI_MODE_SNAPSHOT, (uint32_t)DMA_Buffer,DMA_BufferSize);
}

/***************************************************************************************************************************************
*	函 数 名: OV5640_DCMI_Suspend
*
*	函数功能: 挂起DCMI，停止捕获数据
*
*	说    明: 1. 开启连续模式之后，再调用该函数，会停止捕获DCMI的数据
*            2. 可以调用 OV5640_DCMI_Resume() 恢复DCMI
*				 3. 需要注意的，挂起DCMI期间，DMA是没有停止工作的
*
*****************************************************************************************************************************************/
void OV5640_DCMI_Suspend(void)
{
	HAL_DCMI_Suspend(&hdcmi);    // 挂起DCMI
}

/***************************************************************************************************************************************
*	函 数 名: OV5640_DCMI_Resume
*
*	函数功能: 恢复DCMI，开始捕获数据
*
*	说    明: 1. 当DCMI被挂起时，可以调用该函数恢复
*            2. 使用 OV5640_DMA_Transmit_Snapshot() 快照模式，传输完成之后，DCMI也会被挂起，再次启用传输之前，
*				    需要调用本函数恢复DCMI捕获
*
*****************************************************************************************************************************************/
void  OV5640_DCMI_Resume(void)
{
	(&hdcmi)->State = HAL_DCMI_STATE_BUSY;       // 变更DCMI标志
	(&hdcmi)->Instance->CR |= DCMI_CR_CAPTURE;   // 开启DCMI捕获
}


/***************************************************************************************************************************************
*	函 数 名: OV5640_DCMI_Stop
*
*	函数功能: 禁止DCMI的DMA请求，停止DCMI捕获，禁止DCMI外设
*
*****************************************************************************************************************************************/
void  OV5640_DCMI_Stop(void)
{
	HAL_DCMI_Stop(&hdcmi);
}


/***************************************************************************************************************************************
*  函 数 名: OV5640_DCMI_Crop
*
*  入口参数:  Display_XSize 、Display_YSize - 显示器的尺寸
*             Sensor_XSize、Sensor_YSize - 摄像头输出的图像尺寸
*
*  函数功能: 使用DCMI的裁剪功能，将传感器输出的图像裁剪为适应屏幕的大小
*
*  说   明: 1. 因为摄像头和屏幕分辨率不一定匹配，需要裁剪。
*            2. 摄像头的输出分辨率由 OV5640_Config() 参数决定，最终图像大小由 OV5640_Set_Framesize() 设置。
*            3. DCMI的水平有效像素值必须能被4整除。
*            4. 可设定水平和垂直偏移，方便用户进行裁剪。
*****************************************************************************************************************************************/
int8_t OV5640_DCMI_Crop(uint16_t Display_XSize,uint16_t Display_YSize,uint16_t Sensor_XSize,uint16_t Sensor_YSize )
{
	uint16_t DCMI_X_Offset,DCMI_Y_Offset;    // 水平和垂直偏移
	uint16_t DCMI_CAPCNT;                    // 水平有效像素时钟数(PCLK cycles)-1
	uint16_t DCMI_VLINE;                     // 垂直有效行数-1

	if( (Display_XSize>=Sensor_XSize)|| (Display_YSize>=Sensor_YSize) )
	{
		return OV5640_Error;
	}

	// X offset in PCLK cycles: each RGB565 pixel = 2 PCLK cycles.
	// Center crop: skip (Sensor-Display)/2 pixels, so 2× that many PCLK cycles.
	DCMI_X_Offset = (Sensor_XSize - Display_XSize);  // = (difference)/2 * 2 = PCLK cycles for center

	// Y offset in lines. VST register is 0-based; center = (Sensor-Display)/2.
	// The HAL writes Y0 directly to VST without subtracting 1.
	DCMI_Y_Offset = (Sensor_YSize - Display_YSize) / 2;

	// CAPCNT: number of PCLK cycles per line - 1. RGB565: 2 cycles/pixel.
	DCMI_CAPCNT = Display_XSize * 2 - 1;

	// VLINE: number of lines - 1.
	DCMI_VLINE = Display_YSize - 1;

	HAL_DCMI_ConfigCrop(&hdcmi, DCMI_X_Offset, DCMI_Y_Offset, DCMI_CAPCNT, DCMI_VLINE);
	HAL_DCMI_EnableCrop(&hdcmi);

	return OV5640_Success;
}

/***************************************************************************************************************************************
*	函 数 名: OV5640_Reset
*
*	函数功能: 执行软件复位
*
*	说    明: 期间有多个延时操作
*
*****************************************************************************************************************************************/
void OV5640_Reset(void)
{
	OV5640_Delay(30);  // 等待模块上电稳定，最少5ms，然后拉低PWDN

	OV5640_PWDN_OFF;  // PWDN 引脚输出低电平，不开启掉电模式，摄像头正常工作，此时摄像头模块的白色LED会点亮

	// 根据OV5640的上电时序，PWDN拉低之后，要等待1ms再去拉高RESET，鹿小班的OV5640模块采用硬件RC复位，持续时间大概在6~10ms，
	// 因此加入延时，等待硬件复位完成并稳定下来
	OV5640_Delay(5);

	// 复位完成之后，要>=20ms方可执行SCCB配置
	OV5640_Delay(20);

	SCCB_WriteReg_16Bit(0x3103, 0x11);	// 根据手册的建议，复位之前，直接将时钟输入引脚的时钟作为主时钟
	SCCB_WriteReg_16Bit(0x3008, 0x82);	// 执行一次软复位
	OV5640_Delay(5);  //延时5ms

}

/***************************************************************************************************************************************
*	函 数 名: OV5640_ReadID
*
*	函数功能: 读取 OV5640 的器件ID
*
*****************************************************************************************************************************************/
uint16_t OV5640_ReadID(void)
{
	uint8_t PID_H,PID_L;     // ID变量

	PID_H = SCCB_ReadReg_16Bit(OV5640_ChipID_H); // 读取ID高字节
	PID_L = SCCB_ReadReg_16Bit(OV5640_ChipID_L); // 读取ID低字节

	return(PID_H<<8)|PID_L; // 返回完整的器件ID
}

/***************************************************************************************************************************************
*	函 数 名: OV5640_Config
*
*	函数功能: 配置 OV5640 各个寄存器参数
*
*	说    明: 参数定义在 dcmi_ov5640_cfg.h
*
*****************************************************************************************************************************************/

void OV5640_Config(void)
{
	uint32_t i;	// 计数变量

	uint8_t	read_reg; // 读取配置，用于调试

	for(i=0; i<(sizeof(OV5640_INIT_Config)/4); i++)
	{
		SCCB_WriteReg_16Bit(OV5640_INIT_Config[i][0], OV5640_INIT_Config[i][1]); // 写入配置

		read_reg = SCCB_ReadReg_16Bit(OV5640_INIT_Config[i][0]);	// 读取配置，用于调试

		if(OV5640_INIT_Config[i][1] != read_reg )	// 配置不成功
		{
			printf("出错位置：%d\r\n",i);	// 打印出错位置
			printf("0x%x-0x%x-0x%x\r\n",OV5640_INIT_Config[i][0],OV5640_INIT_Config[i][1],read_reg);
		}
	}
}

/***************************************************************************************************************************************
*  函 数 名: OV5640_Set_Pixformat
*
*  入口参数:  pixformat - 像素格式，可选 Pixformat_RGB565、Pixformat_GRAY、Pixformat_JPEG
*
*  函数功能: 设置摄像头的输出像素格式
*
*****************************************************************************************************************************************/
void OV5640_Set_Pixformat(uint8_t pixformat)
{
   uint8_t OV5640_Reg;  // 寄存器临时值

   if( pixformat == Pixformat_JPEG )
   {
      SCCB_WriteReg_16Bit(OV5640_FORMAT_CONTROL,       0x30);   // 设置数据接口输出的格式
      SCCB_WriteReg_16Bit(OV5640_FORMAT_CONTROL_MUX,   0x00);   // 设置ISP的格式

      SCCB_WriteReg_16Bit(OV5640_JPEG_MODE_SELECT, 0x02);      // JPEG 模式2

      SCCB_WriteReg_16Bit(OV5640_JPEG_VFIFO_CTRL00, 0xA0);     // JPEG 固定设置

      SCCB_WriteReg_16Bit(OV5640_JPEG_VFIFO_HSIZE_H, OV5640_Width>>8);          // JPEG 最大水平尺寸,高字节
      SCCB_WriteReg_16Bit(OV5640_JPEG_VFIFO_HSIZE_L, (uint8_t)OV5640_Width);    // JPEG 最大水平尺寸,低字节
      SCCB_WriteReg_16Bit(OV5640_JPEG_VFIFO_VSIZE_H, OV5640_Height>>8);         // JPEG 最大垂直尺寸,高字节
      SCCB_WriteReg_16Bit(OV5640_JPEG_VFIFO_VSIZE_L, (uint8_t)OV5640_Height);   // JPEG 最大垂直尺寸,低字节
   }
   else if( pixformat == Pixformat_GRAY )
   {
      SCCB_WriteReg_16Bit(OV5640_FORMAT_CONTROL,       0x10);   // 设置数据接口输出的格式
      SCCB_WriteReg_16Bit(OV5640_FORMAT_CONTROL_MUX,   0x00);   // 设置ISP的格式
   }
   else   // RGB565
   {
      SCCB_WriteReg_16Bit(OV5640_FORMAT_CONTROL,       0x6F);   // 此处设置为RGB565格式，顺序为 G[2:0]B[4:0], R[4:0]G[5:3]
      SCCB_WriteReg_16Bit(OV5640_FORMAT_CONTROL_MUX,   0x01);   // 设置ISP的格式
   }

   OV5640_Reg = SCCB_ReadReg_16Bit(0x3821);   // 读取寄存器值，Bit[5]控制是否使用JPEG模式
   SCCB_WriteReg_16Bit(0x3821, (OV5640_Reg & 0xDF) | ((pixformat == Pixformat_JPEG) ? 0x20 : 0x00));

   OV5640_Reg = SCCB_ReadReg_16Bit(0x3002);   // Bit[7]、Bit[4]、Bit[2]使能 VFIFO、JFIFO、JPG
   SCCB_WriteReg_16Bit(0x3002, (OV5640_Reg & 0xE3) | ((pixformat == Pixformat_JPEG) ? 0x00 : 0x1C));

   OV5640_Reg = SCCB_ReadReg_16Bit(0x3006);   // Bit[5]、Bit[3] 设置是否使用JPG时钟
   SCCB_WriteReg_16Bit(0x3006, (OV5640_Reg & 0xD7) | ((pixformat == Pixformat_JPEG) ? 0x28 : 0x00));
}

/***************************************************************************************************************************************
*	函 数 名: OV5640_Set_JPEG_QuantizationScale
*
*	入口参数: scale - 压缩等级，取值 0x01~0x3F
*
*	函数功能: 数值越大，压缩就越厉害，得到的图片占用空间就越小，但相应的画质会变差，客户可自行调节
*
*****************************************************************************************************************************************/

void OV5640_Set_JPEG_QuantizationScale(uint8_t scale)
{
	SCCB_WriteReg_16Bit(0x4407, scale); 	// JPEG 压缩等级
}


/***************************************************************************************************************************************
*	函 数 名: OV5640_Set_Framesize
*
*	入口参数:  width - 实际输出图像的长度，height - 实际输出图像的宽度
*
*	函数功能: 设置实际输出的图像大小（缩放后）
*
*	说    明: 1. 需要注意的是，要设置的图像长、宽需要满足初始化配置时ISP窗口的比例，不然图像会变形
*            2. 并不是设置输出的图像分辨率越小帧率就越高，帧率只和初始化的配置（PLL、HTS和VTS）有关
*
*****************************************************************************************************************************************/

int8_t OV5640_Set_Framesize(uint16_t width,uint16_t height)
{
	// OV5640的很多操作，都要加上这种对应 group 的配置
	SCCB_WriteReg_16Bit(OV5640_GroupAccess,0X03);  	// 开始 group 3 的配置

	SCCB_WriteReg_16Bit(OV5640_TIMING_DVPHO_H,width>>8);			// DVPHO，设置输出水平尺寸
	SCCB_WriteReg_16Bit(OV5640_TIMING_DVPHO_L,width&0xff);
	SCCB_WriteReg_16Bit(OV5640_TIMING_DVPVO_H,height>>8);		// DVPVO，设置输出垂直尺寸
	SCCB_WriteReg_16Bit(OV5640_TIMING_DVPVO_L,height&0xff);

	SCCB_WriteReg_16Bit(OV5640_GroupAccess,0X13);		// 结束配置
	SCCB_WriteReg_16Bit(OV5640_GroupAccess,0Xa3);		// 启用设置

	return OV5640_Success;
}

/***************************************************************************************************************************************
*	函 数 名: OV5640_Set_Horizontal_Mirror
*
*	入口参数:  ConfigState - 置1时，图像会水平镜像，置0时恢复正常
*
*	函数功能: 用于设置输出的图像是否进行水平镜像
*
*****************************************************************************************************************************************/
int8_t OV5640_Set_Horizontal_Mirror( int8_t ConfigState )
{
	uint8_t OV5640_Reg;  // 寄存器的值

	OV5640_Reg = SCCB_ReadReg_16Bit(OV5640_TIMING_Mirror);   // 读取寄存器值

	// Bit[2:1]用于设置是否水平镜像
	if ( ConfigState == OV5640_Enable )    // 如果使能镜像
	{
		OV5640_Reg |= 0X06;
	}
	else                    // 取消镜像
	{
		OV5640_Reg &= 0xF9;
	}
	return  SCCB_WriteReg_16Bit(OV5640_TIMING_Mirror,OV5640_Reg);   // 写入寄存器
}

/***************************************************************************************************************************************
*	函 数 名: OV5640_Set_Vertical_Flip
*
*	入口参数:  ConfigState - 置1时，图像会垂直翻转，置0时恢复正常
*
*	函数功能: 用于设置输出的图像是否进行垂直翻转
*
*****************************************************************************************************************************************/
int8_t OV5640_Set_Vertical_Flip( int8_t ConfigState )
{
	uint8_t OV5640_Reg;  // 寄存器的值

	OV5640_Reg = SCCB_ReadReg_16Bit(OV5640_TIMING_Flip);          // 读取寄存器值

	// Bit[2:1]用于设置是否垂直翻转
	if ( ConfigState == OV5640_Enable )
	{
		OV5640_Reg |= 0X06;
	}
	else   // 取消翻转
	{
		OV5640_Reg &= 0xF9;
	}
	return  SCCB_WriteReg_16Bit(OV5640_TIMING_Flip,OV5640_Reg);   // 写入寄存器
}



/***************************************************************************************************************************************
*	函 数 名: OV5640_Set_Brightness
*
*	入口参数:  Brightness - 亮度，可设置为9个等级：4，3，2，1，0，-1，-2，-3，-4   ，数字越大亮度越高
*
*	说    明: 1. 直接使用OV5640手册给出的代码
*            2. 亮度越高，画面就越明亮，但是会变模糊一些
*				 2. 亮度太低，噪点会增多
*
*****************************************************************************************************************************************/
void OV5640_Set_Brightness(int8_t Brightness)
{
	Brightness = Brightness+4;
	SCCB_WriteReg_16Bit(OV5640_GroupAccess,0X03);  	// 开始 group 3 的配置

	SCCB_WriteReg_16Bit( 0x5587, OV5640_Brightness_Config[Brightness][0]);
	SCCB_WriteReg_16Bit( 0x5588, OV5640_Brightness_Config[Brightness][1]);

	SCCB_WriteReg_16Bit(OV5640_GroupAccess,0X13);		// 结束配置
	SCCB_WriteReg_16Bit(OV5640_GroupAccess,0Xa3);		// 启用设置
}

/***************************************************************************************************************************************
*	函 数 名: OV5640_Set_Contrast
*
*	入口参数: Contrast - 对比度，可设置为7个等级：3，2，1，0，-1，-2 ，-3
*
*	说    明: 1. 直接使用OV5640手册给出的代码
*            2. 对比度越高，画面越清晰，黑白越加分明
*
*****************************************************************************************************************************************/
void OV5640_Set_Contrast(int8_t Contrast)
{
	Contrast = Contrast+3;
	SCCB_WriteReg_16Bit(OV5640_GroupAccess,0X03);  	// 开始 group 3 的配置

	SCCB_WriteReg_16Bit( 0x5586, OV5640_Contrast_Config[Contrast][0]);
	SCCB_WriteReg_16Bit( 0x5585, OV5640_Contrast_Config[Contrast][1]);

	SCCB_WriteReg_16Bit(OV5640_GroupAccess,0X13);		// 结束配置
	SCCB_WriteReg_16Bit(OV5640_GroupAccess,0Xa3);		// 启用设置
}
/***************************************************************************************************************************************
*	函 数 名: OV5640_Set_Effect
*
*	入口参数:  effect_Mode - 特效模式，可选择参数 OV5640_Effect_Normal、OV5640_Effect_Negative、
*                          OV5640_Effect_BW、OV5640_Effect_Solarize
*
*	函数功能: 用于设置OV5640的特效，正常、负片、黑白、正负片叠加模式
*
*	说    明: 这里仅列举了4个模式，更多特效模式可以参考手册进行配置
*
*****************************************************************************************************************************************/
void OV5640_Set_Effect(uint8_t effect_Mode)
{
	SCCB_WriteReg_16Bit(OV5640_GroupAccess,0X03);  	// 开始 group 3 的配置

	SCCB_WriteReg_16Bit( 0x5580, OV5640_Effect_Config[effect_Mode][0]);
	SCCB_WriteReg_16Bit( 0x5583, OV5640_Effect_Config[effect_Mode][1]);
	SCCB_WriteReg_16Bit( 0x5584, OV5640_Effect_Config[effect_Mode][2]);
	SCCB_WriteReg_16Bit( 0x5003, OV5640_Effect_Config[effect_Mode][3]);

	SCCB_WriteReg_16Bit(OV5640_GroupAccess,0X13);		// 结束配置
	SCCB_WriteReg_16Bit(OV5640_GroupAccess,0Xa3);		// 启用设置
	
}
/***************************************************************************************************************************************
*  函 数 名: OV5640_AF_Download_Firmware
*
*  函数功能: 将自动对焦固件写入OV5640
*
*  说   明: 因为OV5640片内没有flash，无法保存固件，所以每次上电都要写入一次
*
*****************************************************************************************************************************************/
int8_t OV5640_AF_Download_Firmware(void)
{
	uint8_t  AF_Status = 0;		// 对焦状态
	uint16_t i = 0; 				// 循环计数
	uint16_t OV5640_MCU_Addr = 0x8000;	// OV5640 MCU 存储区域起始地址为 0x8000，大小为4KB

	SCCB_WriteReg_16Bit(0x3000, 0x20);	// Bit[5]复位MCU，写固件之前需要执行此步骤
// ��ʼд��̼�������д�룬���д���ٶ�
	SCCB_WriteBuffer_16Bit( OV5640_MCU_Addr,(uint8_t *)OV5640_AF_Firmware,sizeof(OV5640_AF_Firmware) );
	SCCB_WriteReg_16Bit(0x3000,0x00);  // Bit[5]写完成，写0使能MCU

	// 写固件之后，会有一个初始化的过程，因此尝试读取100次状态，根据状态判断
	for(i=0; i<100; i++)
	{
		AF_Status = SCCB_ReadReg_16Bit(OV5640_AF_FW_STATUS);   // 读取状态寄存器
		if( AF_Status == 0x7E)
		{
			printf("AF固件初始化中>>>\r\n");
		}
		if( AF_Status == 0x70)   // 镜头回到初始对焦为无穷远的位置，意味着固件写入成功
		{
			printf("AF固件写入成功！\r\n");
			return OV5640_Success;
		}
	}
	// 超过100次读取之后，还是没有读到0x70状态，说明固件没写入成功
	printf("自动对焦固件写入失败，请检查error！\r\n");
	return OV5640_Error;
}

/***************************************************************************************************************************************
*	函 数 名: OV5640_AF_QueryStatus
*
*	返 回 值：OV5640_AF_End - 对焦结束， OV5640_AF_Focusing - 正在对焦
*
*	函数功能: 对焦状态查询
*
*	说    明: 1. 对焦过程大概会持续500多ms
*				 2. 对焦没完成时，采集到的的图像不在焦点，会非常模糊
*
*****************************************************************************************************************************************/

int8_t OV5640_AF_QueryStatus(void)
{
	uint8_t  AF_Status = 0;		// 对焦状态

	AF_Status = SCCB_ReadReg_16Bit(OV5640_AF_FW_STATUS);	// 读取状态寄存器
	printf("AF_Status:0x%x\r\n",AF_Status);

// 单次对焦模式	下，返回 0x10，持续对焦模式下，返回0x20
	if( (AF_Status == 0x10)||(AF_Status == 0x20) )
	{
		return OV5640_AF_End;	// 返回 对焦结束 标志
	}
	else
	{
		return OV5640_AF_Focusing;	// 返回 正在对焦 标志
	}
}

/***************************************************************************************************************************************
*	函 数 名: OV5640_AF_Trigger_Constant
*
*	函数功能: 持续触发对焦，当OV5640检测到当前画面不在焦点时，会一直触发对焦，无需用户干预
*
*	说    明: 1.可以调用 OV5640_AF_QueryStatus() 函数查询对焦状态
*				 2.可以调用 OV5640_AF_Release() 退出持续对焦模式
*				 3.对焦过程大概会持续500多ms
*				 4.有时环境光线太暗，OV5640会反复的进行对焦，用户可根据实际情况切换到单次对焦模式
*
*****************************************************************************************************************************************/

void OV5640_AF_Trigger_Constant(void)
{
	SCCB_WriteReg_16Bit(0x3022,0x04);	//	持续对焦
}

/***************************************************************************************************************************************
*	函 数 名: OV5640_AF_Trigger_Single
*
*	函数功能: 触发一次自动对焦
*
*	说    明: 对焦过程大概会持续500多ms，用户可以调用 OV5640_AF_QueryStatus() 函数查询对焦状态
*
*****************************************************************************************************************************************/

void OV5640_AF_Trigger_Single(void)
{
	SCCB_WriteReg_16Bit(OV5640_AF_CMD_MAIN,0x03);	// 触发一次自动对焦
}

/***************************************************************************************************************************************
*	函 数 名: OV5640_AF_Release
*
*	函数功能: 释放马达，镜头回到初始（对焦为无穷远处）位置
*
*****************************************************************************************************************************************/

void OV5640_AF_Release(void)
{
	SCCB_WriteReg_16Bit(OV5640_AF_CMD_MAIN,0x08);	// 对焦释放指令
}


