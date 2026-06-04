/***
*************************************************************************************************
	*	@file  	main.c
	*	@version V1.0
	*  @date    2023-10-24
   *************************************************************************************************
   *  @description
	*
	* + 1.54寸液晶模块（型号：SPI154M1-240*240）
	*
	*鹿小班
	*
>>>>> 主要说明：
	*
	*  1. 屏幕格式为16位RGB565方式
	*  2. SPI通讯速度为60M
   *
>>>>> 其他说明：
	*
	*	1. 汉字库使用的是小字库，用到了对应的函数去取模，用户可以根据汉字数量增加或删减
	*	2. 其他所有的功能和使用可以参考函数说明
	*
	*********************************************************************************************************************************************************************************************鹿小班*****
***/

#include "lcd_spi_154.h"

/* DWT-based busy-wait delay — does NOT depend on interrupts (uwTick / TIM17).
   Used in SPI_LCD_Init() which runs before FreeRTOS starts.              */
void LCD_DWT_Delay_ms(uint32_t ms)
{
    if ((CoreDebug->DEMCR & CoreDebug_DEMCR_TRCENA_Msk) == 0U) {
        CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    }
    if ((DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk) == 0U) {
        DWT->CYCCNT = 0U;
        DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    }
    uint32_t cycles = ms * (SystemCoreClock / 1000U);
    uint32_t start  = DWT->CYCCNT;
    while ((DWT->CYCCNT - start) < cycles) {
        __NOP();
    }
}

SPI_HandleTypeDef hSPI6;	      // SPI_HandleTypeDef 结构体变量

#define  LCD_SPI hSPI6           // SPI宏定义，便于修改和移植

static pFONT *LCD_AsciiFonts;		// 英文字体，ASCII字符用
static pFONT *LCD_CHFonts;		   // 中文字体（同时也是英文字体）

// 因为是SPI接口屏幕，每次更新显示时，都需要发送命令和写显存，
// 而显示字符时，需要一个一个点去写，写显存非常慢。
// 因此开辟一片缓冲区，先将要显示的点写入缓冲区，最后一次性写入显存。
// 用户可以根据实际需要去修改此处缓冲区的大小。
// 例如，用户需要显示32*32的汉字时，需要的缓存大小为 32*32*2 = 2048 字节（每个像素点占2字节）
uint16_t  LCD_Buff[1024];       // LCD缓冲区，16位，每个像素点占2字节

struct	// LCD相关参数结构体
{
	uint32_t Color;  			//	LCD当前前景色
	uint32_t BackColor;			//	背景色
   uint8_t  ShowNum_Mode;		// 数字显示模式
	uint8_t  Direction;			//	显示方向
   uint16_t Width;				// 屏幕像素宽度
   uint16_t Height;				// 屏幕像素高度
   uint8_t  X_Offset;			// X方向偏移，取决于不同屏幕厂商设置的写入方式
   uint8_t  Y_Offset;			// Y方向偏移，取决于不同屏幕厂商设置的写入方式
}LCD;

// 该函数修改自HAL的SPI库函数，专为 LCD_Clear() 等函数修改，
// 目的是为了SPI发送固定颜色值不限制数据长度的写入
HAL_StatusTypeDef LCD_SPI_Transmit(SPI_HandleTypeDef *hspi, uint16_t pData, uint32_t Size);
HAL_StatusTypeDef LCD_SPI_TransmitBuffer (SPI_HandleTypeDef *hspi, uint16_t *pData, uint32_t Size);

static void LCD_MX_SPI6_Init(void)
{
	LCD_SPI.Instance 						= SPI6;							   			// 使用SPI6
	LCD_SPI.Init.Mode 						= SPI_MODE_MASTER;            				// 主机模式
	LCD_SPI.Init.Direction 					= SPI_DIRECTION_1LINE;      		// 单发
	LCD_SPI.Init.DataSize 					= SPI_DATASIZE_8BIT;          				// 8位数据宽度
	LCD_SPI.Init.CLKPolarity 				= SPI_POLARITY_LOW;           				// CLK空闲时保持低电平
	LCD_SPI.Init.CLKPhase 					= SPI_PHASE_1EDGE;            				// 数据在CLK第一个跳变沿有效
	LCD_SPI.Init.NSS 						= SPI_NSS_HARD_OUTPUT;        				// 使用硬件片选

	// SPI内核时钟频率为120M，再经过2分频得到 60M 的SCK时钟频率
	LCD_SPI.Init.BaudRatePrescaler 			= SPI_BAUDRATEPRESCALER_2;

	LCD_SPI.Init.FirstBit	 				= SPI_FIRSTBIT_MSB;							// 高位在前
	LCD_SPI.Init.TIMode 					= SPI_TIMODE_DISABLE;         				// 禁止TI模式
	LCD_SPI.Init.CRCCalculation				= SPI_CRCCALCULATION_DISABLE; 				// 禁止CRC
	LCD_SPI.Init.CRCPolynomial 				= 0x0;                        				// CRC校验多项式，此处不配置
	LCD_SPI.Init.NSSPMode 					= SPI_NSS_PULSE_ENABLE;      				// 使能片选脉冲模式
	LCD_SPI.Init.NSSPolarity 				= SPI_NSS_POLARITY_LOW;      				// 片选低电平有效
	LCD_SPI.Init.FifoThreshold 				= SPI_FIFO_THRESHOLD_02DATA;  				// FIFO阈值
	LCD_SPI.Init.TxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;  // 发送端CRC初始化模式，此处不配置
	LCD_SPI.Init.RxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;  // 接收端CRC初始化模式，此处不配置
	LCD_SPI.Init.MasterSSIdleness 			= SPI_MASTER_SS_IDLENESS_00CYCLE;           // 帧间延迟时间为0
	LCD_SPI.Init.MasterInterDataIdleness 	= SPI_MASTER_INTERDATA_IDLENESS_00CYCLE;    // 主机模式下，连续数据帧之间的延迟时间
	LCD_SPI.Init.MasterReceiverAutoSusp 	= SPI_MASTER_RX_AUTOSUSP_DISABLE;           // 禁止自动挂起
	LCD_SPI.Init.MasterKeepIOState 			= SPI_MASTER_KEEP_IO_STATE_DISABLE; 	 	// 主机模式下，禁止SPI保持当前引脚状态
	LCD_SPI.Init.IOSwap 					= SPI_IO_SWAP_DISABLE;				        // 禁止MOSI与MISO交换
	
   HAL_SPI_Init(&LCD_SPI);
}

/****************************************************************************************************************************************
*	函 数 名: LCD_WriteCommand
*
*	入口参数: lcd_command - 需要写入的指令命令
*
*	函数功能: 向液晶屏驱动IC写入指令
*
****************************************************************************************************************************************/
void  LCD_WriteCommand(uint8_t lcd_command)
{
   LCD_DC_Command;     // 拉低指令选择 引脚（推挽输出低电平），代表接下来发送 指令

   LCD_SPI_Transmit(&LCD_SPI, (uint16_t)lcd_command, 1);
}

/****************************************************************************************************************************************
*	函 数 名: LCD_WriteData_8bit
*
*	入口参数: lcd_data - 需要写入的数据，8位
*
*	函数功能: 写入8位数据
*
****************************************************************************************************************************************/
void  LCD_WriteData_8bit(uint8_t lcd_data)
{
   LCD_DC_Data;     // 拉高指令选择 引脚（推挽输出高电平），代表接下来发送 数据

   LCD_SPI_Transmit(&LCD_SPI, (uint16_t)lcd_data, 1) ; // 调用SPI发送
}

/****************************************************************************************************************************************
*	函 数 名: LCD_WriteData_16bit
*
*	入口参数: lcd_data - 需要写入的数据，16位
*
*	函数功能: 写入16位数据
*
****************************************************************************************************************************************/
void  LCD_WriteData_16bit(uint16_t lcd_data)
{
   uint8_t lcd_data_buff[2];    // 数据拆分数组
   LCD_DC_Data;      // 拉高指令选择 引脚（推挽输出高电平），代表接下来发送 数据
 
   lcd_data_buff[0] = lcd_data>>8;  // 高数据在前
   lcd_data_buff[1] = lcd_data;
		
	LCD_SPI_TransmitBuffer(&LCD_SPI, (uint16_t *)lcd_data_buff, 1) ; // 16位拆成2个8位发送
}

/****************************************************************************************************************************************
*	函 数 名: LCD_WriteBuff
*
*	入口参数: DataBuff - 数据内容  DataSize - 数据长度
*
*	函数功能: 批量写入数据到屏幕
*
****************************************************************************************************************************************/
void  LCD_WriteBuff(uint16_t *DataBuff, uint16_t DataSize)
{
	LCD_DC_Data;     // 拉高指令选择 引脚（推挽输出高电平），代表接下来发送 数据

	// 修改为16位数据宽度，写入数据更高效，减少传输次数
   LCD_SPI.Init.DataSize 	= SPI_DATASIZE_16BIT;   //	16位数据宽度
   HAL_SPI_Init(&LCD_SPI);		
	
	LCD_SPI_TransmitBuffer(&LCD_SPI, DataBuff, DataSize) ; // 使用自定义传输
	
	// 改回8位数据宽度，因为指令和参数数据都是按8位传输的
	LCD_SPI.Init.DataSize 	= SPI_DATASIZE_8BIT;    //	8位数据宽度
   HAL_SPI_Init(&LCD_SPI);	
}

/****************************************************************************************************************************************
*	函 数 名: SPI_LCD_Init
*
*	函数功能: 初始化SPI以及屏幕驱动的各种参数
*
****************************************************************************************************************************************/
void SPI_LCD_Init(void)
{
   LCD_MX_SPI6_Init();

   // Init backlight (PG12) and DC (PG15)
   {
       GPIO_InitTypeDef g = {0};
       __HAL_RCC_GPIOG_CLK_ENABLE();
       g.Mode  = GPIO_MODE_OUTPUT_PP;
       g.Pull  = GPIO_NOPULL;
       g.Speed = GPIO_SPEED_FREQ_LOW;
       g.Pin   = LCD_Backlight_PIN;
       HAL_GPIO_Init(LCD_Backlight_PORT, &g);
       g.Pin   = LCD_DC_PIN;
       HAL_GPIO_Init(LCD_DC_PORT, &g);
       g.Pin       = GPIO_PIN_8|GPIO_PIN_13|GPIO_PIN_14;
       g.Mode      = GPIO_MODE_AF_PP;
       g.Pull      = GPIO_NOPULL;
       g.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
       g.Alternate = GPIO_AF5_SPI6;
       HAL_GPIO_Init(GPIOG, &g);
   }           // 初始化SPI和控制引脚
	// HAL_Delay(10);					// 屏幕可能刚复位，上电复位后需要等待至少5ms才能发送指令
	LCD_DWT_Delay_ms(10);

 	LCD_WriteCommand(0x36);			// 显存扫描方向 指令，用来设置写显存的方向
	Usart_SendString(&huart1, "1", 1);
	LCD_WriteData_8bit(0x00);		// 设置成 从上到下、从左到右，RGB像素格式
	Usart_SendString(&huart1, "2", 1);
	LCD_WriteCommand(0x3A);			// 接口像素格式 指令，可以设置使用 12位、16位和18位色
	Usart_SendString(&huart1, "3", 1);
	LCD_WriteData_8bit(0x05);		// 此处设置成 16位 像素格式


	// 以下大多是电压配置指令，直接使用厂家给定值
 	LCD_WriteCommand(0xB2);			
	LCD_WriteData_8bit(0x0C);
	LCD_WriteData_8bit(0x0C); 
	LCD_WriteData_8bit(0x00); 
	LCD_WriteData_8bit(0x33); 
	LCD_WriteData_8bit(0x33); 			

	LCD_WriteCommand(0xB7);		   // 栅极电压控制指令
	LCD_WriteData_8bit(0x35);     // VGH = 13.26V，VGL = -10.43V

	LCD_WriteCommand(0xBB);		  // 共模电压控制指令
	LCD_WriteData_8bit(0x19);     // VCOM = 1.35V

	LCD_WriteCommand(0xC0);
	LCD_WriteData_8bit(0x2C);

	LCD_WriteCommand(0xC2);			// VDV 和 VRH 电源控制
	LCD_WriteData_8bit(0x01);		// VDV 和 VRH 的基本设置

	LCD_WriteCommand(0xC3);			// VRH电压 设置指令
	LCD_WriteData_8bit(0x12);		// VRH电压 = 4.6+( vcom+vcom offset+vdv)
				
	LCD_WriteCommand(0xC4);			// VDV电压 设置指令
	LCD_WriteData_8bit(0x20);		// VDV电压 = 0v

	LCD_WriteCommand(0xC6);			// 普通模式下帧率控制指令
	LCD_WriteData_8bit(0x0F);   	// 设置屏幕默认刷新帧率为60帧

	LCD_WriteCommand(0xD0);			// 电源控制指令
	LCD_WriteData_8bit(0xA4);		// 特定数据，固定写入0xA4
	LCD_WriteData_8bit(0xA1);		// AVDD = 6.8V ，AVDD = -4.8V ，VDS = 2.3V

	LCD_WriteCommand(0xE0);			// 正电压伽马值设定
	LCD_WriteData_8bit(0xD0);
	LCD_WriteData_8bit(0x04);
	LCD_WriteData_8bit(0x0D);
	LCD_WriteData_8bit(0x11);
	LCD_WriteData_8bit(0x13);
	LCD_WriteData_8bit(0x2B);
	LCD_WriteData_8bit(0x3F);
	LCD_WriteData_8bit(0x54);
	LCD_WriteData_8bit(0x4C);
	LCD_WriteData_8bit(0x18);
	LCD_WriteData_8bit(0x0D);
	LCD_WriteData_8bit(0x0B);
	LCD_WriteData_8bit(0x1F);
	LCD_WriteData_8bit(0x23);

	LCD_WriteCommand(0xE1);			// 负电压伽马值设定
	LCD_WriteData_8bit(0xD0);
	LCD_WriteData_8bit(0x04);
	LCD_WriteData_8bit(0x0C);
	LCD_WriteData_8bit(0x11);
	LCD_WriteData_8bit(0x13);
	LCD_WriteData_8bit(0x2C);
	LCD_WriteData_8bit(0x3F);
	LCD_WriteData_8bit(0x44);
	LCD_WriteData_8bit(0x51);
	LCD_WriteData_8bit(0x2F);
	LCD_WriteData_8bit(0x1F);
	LCD_WriteData_8bit(0x1F);
	LCD_WriteData_8bit(0x20);
	LCD_WriteData_8bit(0x23);

	LCD_WriteCommand(0x21);       // 打开反色，因为屏幕是常黑型，所以需要反转

	// 退出休眠指令，LCD在上电、复位时会自动进入休眠模式 ，因此点亮屏幕之前需要退出休眠
	LCD_WriteCommand(0x11);       // 退出休眠 指令
	//HAL_Delay(120);
	LCD_DWT_Delay_ms(120);        // 需要等待120ms，等电源电压、时钟电路稳定后
	Usart_SendString(&huart1, "4", 1);
	// 开显示指令，LCD在上电、复位时会自动关闭显示
	LCD_WriteCommand(0x29);       // 开显示
	
	// 接下来设置一些驱动默认参数
	LCD_SetDirection(Direction_V);		// 设置显示方向
	LCD_SetBackColor(LCD_BLACK);    	// 设置背景色
 	LCD_SetColor(LCD_WHITE);        	// 设置前景色
	LCD_Clear();                    	// 清屏
	LCD_SetAsciiFont(&ASCII_Font24);	// 设置默认字体
	LCD_ShowNumMode(Fill_Zero);			// 设置数字显示模式，空位补空格或补0

	// 全部配置完成之后，打开背光
   LCD_Backlight_ON;  // 背光控制引脚输出高电平
}

/****************************************************************************************************************************************
*	函 数 名:	 LCD_SetAddress
*
*	入口参数:	 x1 - 起始水平坐标   y1 - 起始垂直坐标
*              x2 - 终点水平坐标   y2 - 终点垂直坐标
*
*	函数功能:   设置需要显示的区域
*****************************************************************************************************************************************/

void LCD_SetAddress(uint16_t x1,uint16_t y1,uint16_t x2,uint16_t y2)
{
	LCD_WriteCommand(0x2a);			//	列地址设置（X方向）
	LCD_WriteData_16bit(x1+LCD.X_Offset);
	LCD_WriteData_16bit(x2+LCD.X_Offset);

	LCD_WriteCommand(0x2b);			//	行地址设置（Y方向）
	LCD_WriteData_16bit(y1+LCD.Y_Offset);
	LCD_WriteData_16bit(y2+LCD.Y_Offset);

	LCD_WriteCommand(0x2c);			//	开始写显存，接下来要发送像素颜色
}

/****************************************************************************************************************************************
*	函 数 名:	LCD_SetColor
*
*	入口参数:	Color - 要显示的颜色，例如0x0000FF 表示红色
*
*	函数功能:	此函数用于让用户设置前景色，之后显示字符、画点、画线、绘图都使用此颜色
*
*	说   明:	1. 为了方便用户使用自定义颜色，入口参数 Color 使用24位 RGB888真彩色格式，用户无需进行格式转换
*					2. 24位真彩色中，从高位到低位分别对应 R、G、B  3个颜色通道
*
*****************************************************************************************************************************************/

void LCD_SetColor(uint32_t Color)
{
	uint16_t Red_Value = 0, Green_Value = 0, Blue_Value = 0; // 三个颜色通道的值

	Red_Value   = (uint16_t)((Color&0x00F80000)>>8);   // 转换成 16位 RGB565颜色
	Green_Value = (uint16_t)((Color&0x0000FC00)>>5);
	Blue_Value  = (uint16_t)((Color&0x000000F8)>>3);

	LCD.Color = (uint16_t)(Red_Value | Green_Value | Blue_Value);  // 将颜色写入全局LCD变量
}

/****************************************************************************************************************************************
*	函 数 名:	LCD_SetBackColor
*
*	入口参数:	Color - 要显示的颜色，例如0x0000FF 表示红色
*
*	函数功能:	设置背景色,此函数用于让用户自定义显示字符的背景色
*
*	说   明:	1. 为了方便用户使用自定义颜色，入口参数 Color 使用24位 RGB888真彩色格式，用户无需进行格式转换
*					2. 24位真彩色中，从高位到低位分别对应 R、G、B  3个颜色通道
*
*****************************************************************************************************************************************/

void LCD_SetBackColor(uint32_t Color)
{
	uint16_t Red_Value = 0, Green_Value = 0, Blue_Value = 0; // 三个颜色通道的值

	Red_Value   = (uint16_t)((Color&0x00F80000)>>8);   // 转换成 16位 RGB565颜色
	Green_Value = (uint16_t)((Color&0x0000FC00)>>5);
	Blue_Value  = (uint16_t)((Color&0x000000F8)>>3);

	LCD.BackColor = (uint16_t)(Red_Value | Green_Value | Blue_Value);	// 将颜色写入全局LCD变量
}

/****************************************************************************************************************************************
*	函 数 名:	LCD_SetDirection
*
*	入口参数:	direction - 要显示的方向
*
*	函数功能:	设置要显示的方向
*
*	说   明:   1. 可用参数为 Direction_H 、Direction_V 、Direction_H_Flip 、Direction_V_Flip
*              2. 使用示例 LCD_DisplayDirection(Direction_H) 即可设置屏幕水平显示
*
*****************************************************************************************************************************************/

void LCD_SetDirection(uint8_t direction)
{
	LCD.Direction = direction;    // 写入全局LCD变量

   if( direction == Direction_H )   // 水平显示
   {
      LCD_WriteCommand(0x36);    		// 显存扫描方向 指令，用来设置写显存的方向
      LCD_WriteData_8bit(0x70);        // 水平显示
      LCD.X_Offset   = 0;             // 设置显存扫描偏移量
      LCD.Y_Offset   = 0;
      LCD.Width      = LCD_Height;		// 重新赋值宽高
      LCD.Height     = LCD_Width;
   }
   else if( direction == Direction_V )
   {
      LCD_WriteCommand(0x36);    		// 显存扫描方向 指令，用来设置写显存的方向
      LCD_WriteData_8bit(0x00);        // 垂直显示
      LCD.X_Offset   = 0;              // 设置显存扫描偏移量
      LCD.Y_Offset   = 0;
      LCD.Width      = LCD_Width;		// 重新赋值宽高
      LCD.Height     = LCD_Height;
   }
   else if( direction == Direction_H_Flip )
   {
      LCD_WriteCommand(0x36);   			 // 显存扫描方向 指令，用来设置写显存的方向
      LCD_WriteData_8bit(0xA0);         // 水平显示（横向翻转），RGB像素格式
      LCD.X_Offset   = 80;              // 设置显存扫描偏移量
      LCD.Y_Offset   = 0;
      LCD.Width      = LCD_Height;		 // 重新赋值宽高
      LCD.Height     = LCD_Width;
   }
   else if( direction == Direction_V_Flip )
   {
      LCD_WriteCommand(0x36);    		// 显存扫描方向 指令，用来设置写显存的方向
      LCD_WriteData_8bit(0xC0);        // 垂直显示 （纵向翻转），RGB像素格式
      LCD.X_Offset   = 0;              // 设置显存扫描偏移量
      LCD.Y_Offset   = 80;
      LCD.Width      = LCD_Width;		// 重新赋值宽高
      LCD.Height     = LCD_Height;
   }
}

/****************************************************************************************************************************************
*	函 数 名:	LCD_SetAsciiFont
*
*	入口参数:	*fonts - 要设置的ASCII字体
*
*	函数功能:	设置ASCII字体，可选择使用 3216/2412/2010/1608/1206 等字体大小
*
*	说   明:	1. 使用示例 LCD_SetAsciiFont(&ASCII_Font24) 即可设置 2412号 ASCII字体
*					2. 字体文件在 lcd_fonts.c
*
*****************************************************************************************************************************************/

void LCD_SetAsciiFont(pFONT *Asciifonts)
{
  LCD_AsciiFonts = Asciifonts;
}

/****************************************************************************************************************************************
*	函 数 名:	LCD_Clear
*
*	函数功能:	将整个屏幕填充为 LCD.BackColor 背景色
*
*	说   明:	先用 LCD_SetBackColor() 设定需要的背景色，再调用该函数完成清屏
*
*****************************************************************************************************************************************/

void LCD_Clear(void)
{
   LCD_SetAddress(0,0,LCD.Width-1,LCD.Height-1);	// 设置全屏区域

	LCD_DC_Data;     // 拉高指令选择 引脚（推挽输出高电平），代表接下来发送 数据

// 修改为16位数据宽度，写入数据更高效，减少传输次数
   LCD_SPI.Init.DataSize 	= SPI_DATASIZE_16BIT;   //	16位数据宽度
   HAL_SPI_Init(&LCD_SPI);

   LCD_SPI_Transmit(&LCD_SPI, LCD.BackColor, LCD.Width * LCD.Height) ;   // 填充背景色

// 改回8位数据宽度，因为指令和参数数据都是按8位传输的
	LCD_SPI.Init.DataSize 	= SPI_DATASIZE_8BIT;    //	8位数据宽度
   HAL_SPI_Init(&LCD_SPI);
}

/****************************************************************************************************************************************
*	函 数 名:	LCD_ClearRect
*
*	入口参数:	x - 起始水平坐标
*					y - 起始垂直坐标
*					width  - 要清除区域的横向长度
*					height - 要清除区域的纵向高度
*
*	函数功能:	局部清除：将指定位置对应的矩形区域填充为 LCD.BackColor 背景色
*
*	说   明:	1. 先用 LCD_SetBackColor() 设定需要的背景色，再调用该函数完成填充
*				   2. 使用示例 LCD_ClearRect( 10, 10, 100, 50) 可填充从(10,10)开始的宽100高50的矩形
*
*****************************************************************************************************************************************/

void LCD_ClearRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height)
{
   LCD_SetAddress( x, y, x+width-1, y+height-1);	// 设置矩形区域

	LCD_DC_Data;     // 拉高指令选择 引脚（推挽输出高电平），代表接下来发送 数据

// 修改为16位数据宽度，写入数据更高效，减少传输次数
   LCD_SPI.Init.DataSize 	= SPI_DATASIZE_16BIT;   //	16位数据宽度
   HAL_SPI_Init(&LCD_SPI);

   LCD_SPI_Transmit(&LCD_SPI, LCD.BackColor, width*height) ;  // 填充背景色

// 改回8位数据宽度，因为指令和参数数据都是按8位传输的
	LCD_SPI.Init.DataSize 	= SPI_DATASIZE_8BIT;    //	8位数据宽度
   HAL_SPI_Init(&LCD_SPI);

}

/****************************************************************************************************************************************
*	函 数 名:	LCD_DrawPoint
*
*	入口参数:	x - 起始水平坐标
*					y - 起始垂直坐标
*					color  - 要绘制的颜色，使用 24位 RGB888 真彩色格式，用户无需进行格式转换
*
*	函数功能:	在指定坐标画指定颜色的点
*
*	说   明:	使用示例 LCD_DrawPoint( 10, 10, 0x0000FF) 可在坐标(10,10)画红色的点
*
*****************************************************************************************************************************************/

void LCD_DrawPoint(uint16_t x,uint16_t y,uint32_t color)
{
	LCD_SetAddress(x,y,x,y);	//	设置单点区域

	LCD_WriteData_16bit(color)	;
}

/****************************************************************************************************************************************
*	函 数 名:	LCD_DisplayChar
*
*	入口参数:	x - 起始水平坐标
*					y - 起始垂直坐标
*					c  - ASCII字符
*
*	函数功能:	在指定位置显示指定的字符
*
*	说   明:	1. 如需要设置字体，请先使用 LCD_SetAsciiFont(&ASCII_Font24) 设置为 2412号ASCII字体
*					2.	如需要设置前景色，请先使用 LCD_SetColor(0xff0000FF) 设置为红色
*					3. 请设置对应的背景色，请先使用 LCD_SetBackColor(0x000000) 设置为黑色的背景色
*					4. 使用示例 LCD_DisplayChar( 10, 10, 'a') 可在坐标(10,10)显示字符 'a'
*
*****************************************************************************************************************************************/

void LCD_DisplayChar(uint16_t x, uint16_t y,uint8_t c)
{
	uint16_t  index = 0, counter = 0 ,i = 0, w = 0;		// 循环计数器
   uint8_t   disChar;		//存储字符的点阵数据

	c = c - 32; 	// 减去ASCII字符的偏移

	for(index = 0; index < LCD_AsciiFonts->Sizes; index++)
	{
		disChar = LCD_AsciiFonts->pTable[c*LCD_AsciiFonts->Sizes + index]; //读取字符模值
		for(counter = 0; counter < 8; counter++)
		{
			if(disChar & 0x01)
			{
            LCD_Buff[i] =  LCD.Color;			// 当前模值位为1时，使用前景色填充
			}
			else
			{
            LCD_Buff[i] = LCD.BackColor;		//否则使用背景色填充点
			}
			disChar >>= 1;
			i++;
         w++;
 			if( w == LCD_AsciiFonts->Width ) // 当前行填充数据达到该字体宽度，则退出当前循环
			{								   // 进行下一行的填充
				w = 0;
				break;
			}
		}
	}
   LCD_SetAddress( x, y, x+LCD_AsciiFonts->Width-1, y+LCD_AsciiFonts->Height-1);	   // 设置显示区域
   LCD_WriteBuff(LCD_Buff,LCD_AsciiFonts->Width*LCD_AsciiFonts->Height);          // 写显存
}

/****************************************************************************************************************************************
*	函 数 名:	LCD_DisplayString
*
*	入口参数:	x - 起始水平坐标
*					y - 起始垂直坐标
*					p - ASCII字符串首地址
*
*	函数功能:	在指定位置显示指定的字符串
*
*	说   明:	1. 如需要设置字体，请先使用 LCD_SetAsciiFont(&ASCII_Font24) 设置为 2412号ASCII字体
*					2.	如需要设置前景色，请先使用 LCD_SetColor(0x0000FF) 设置为红色
*					3. 请设置对应的背景色，请先使用 LCD_SetBackColor(0x000000) 设置为黑色的背景色
*					4. 使用示例 LCD_DisplayString( 10, 10, "鹿小班") 可在起始坐标为(10,10)的地方显示字符串"鹿小班"
*
*****************************************************************************************************************************************/

void LCD_DisplayString( uint16_t x, uint16_t y, char *p)
{
	while ((x < LCD.Width) && (*p != 0))	//判断显示坐标是否超出显示区域且字符是否为空字符
	{
		 LCD_DisplayChar( x,y,*p);
		 x += LCD_AsciiFonts->Width; //显示下一个字符
		 p++;	//取下一个字符地址
	}
}

/****************************************************************************************************************************************
*	函 数 名:	LCD_SetTextFont
*
*	入口参数:	*fonts - 要设置的文本字体
*
*	函数功能:	设置文本字体，包含中文和ASCII字符
*
*	说   明:	1. 可选择使用 3232/2424/2020/1616/1212 等字体大小的中文字体，
*					并且对应的ASCII字体为 3216/2412/2010/1608/1206
*					2. 字体文件在 lcd_fonts.c
*					3. 汉字库使用的是小字库，用到了对应的函数去取模
*					4. 使用示例 LCD_SetTextFont(&CH_Font24) 即可设置 2424号中文字体以及2412号ASCII字符字体
*
*****************************************************************************************************************************************/

void LCD_SetTextFont(pFONT *fonts)
{
	LCD_CHFonts = fonts;		// 设置中文字体
	switch(fonts->Width )
	{
		case 12:	LCD_AsciiFonts = &ASCII_Font12;	break;	// 设置ASCII字符字体为 1206
		case 16:	LCD_AsciiFonts = &ASCII_Font16;	break;	// 设置ASCII字符字体为 1608
		case 20:	LCD_AsciiFonts = &ASCII_Font20;	break;	// 设置ASCII字符字体为 2010
		case 24:	LCD_AsciiFonts = &ASCII_Font24;	break;	// 设置ASCII字符字体为 2412
		case 32:	LCD_AsciiFonts = &ASCII_Font32;	break;	// 设置ASCII字符字体为 3216
		default: break;
	}
}
/******************************************************************************************************************************************
*	函 数 名:	LCD_DisplayChinese
*
*	入口参数:	x - 起始水平坐标
*					y - 起始垂直坐标
*					pText - 汉字字符串
*
*	函数功能:	在指定位置显示指定的单个汉字
*
*	说   明:	1. 如需要设置字体，请先使用 LCD_SetTextFont(&CH_Font24) 设置为 2424号中文字体以及2412号ASCII字符字体
*					2.	如需要设置前景色，请先使用 LCD_SetColor(0xff0000FF) 设置为红色
*					3. 请设置对应的背景色，请先使用 LCD_SetBackColor(0xff000000) 设置为黑色的背景色
*					4. 使用示例 LCD_DisplayChinese( 10, 10, "鹿") 可在坐标(10,10)显示汉字"鹿"
*
*****************************************************************************************************************************************/

void LCD_DisplayChinese(uint16_t x, uint16_t y, char *pText)
{
	uint16_t  i=0,index = 0, counter = 0;	// 循环计数器
	uint16_t  addr;	// 字模地址
   uint8_t   disChar;	//字模数值
	uint16_t  Xaddress = 0; //水平坐标

	while(1)
	{
		// 遍历列表中的汉字编码，来定位该汉字模的地址
		if ( *(LCD_CHFonts->pTable + (i+1)*LCD_CHFonts->Sizes + 0)==*pText && *(LCD_CHFonts->pTable + (i+1)*LCD_CHFonts->Sizes + 1)==*(pText+1) )
		{
			addr=i;	// 字模地址偏移
			break;
		}
		i+=2;	// 每个汉字编码占用两字节

		if(i >= LCD_CHFonts->Table_Rows)	break;	// 字模列表没有对应的汉字
	}
	i=0;
	for(index = 0; index <LCD_CHFonts->Sizes; index++)
	{
		disChar = *(LCD_CHFonts->pTable + (addr)*LCD_CHFonts->Sizes + index);	// 读取对应汉字的模值

		for(counter = 0; counter < 8; counter++)
		{
			if(disChar & 0x01)
			{
            LCD_Buff[i] =  LCD.Color;			// 当前模值位为1时，使用前景色填充
			}
			else
			{
            LCD_Buff[i] = LCD.BackColor;		// 否则使用背景色填充点
			}
         i++;
			disChar >>= 1;
			Xaddress++;  //水平坐标自加

			if( Xaddress == LCD_CHFonts->Width ) 	//	如果水平坐标达到该字体宽度，则退出当前循环
			{														//	进行下一行的填充
				Xaddress = 0;
				break;
			}
		}
	}
   LCD_SetAddress( x, y, x+LCD_CHFonts->Width-1, y+LCD_CHFonts->Height-1);	   // 设置显示区域
   LCD_WriteBuff(LCD_Buff,LCD_CHFonts->Width*LCD_CHFonts->Height);            // 写显存
}

/*****************************************************************************************************************************************
*	函 数 名:	LCD_DisplayText
*
*	入口参数:	x - 起始水平坐标
*					y - 起始垂直坐标
*					pText - 字符串（可同时显示中文和ASCII字符）
*
*	函数功能:	在指定位置显示指定的字符串
*
*	说   明:	1. 如需要设置字体，请先使用 LCD_SetTextFont(&CH_Font24) 设置为 2424号中文字体以及2412号ASCII字符字体
*					2.	如需要设置前景色，请先使用 LCD_SetColor(0xff0000FF) 设置为红色
*					3. 请设置对应的背景色，请先使用 LCD_SetBackColor(0xff000000) 设置为黑色的背景色
*					4. 使用示例 LCD_DisplayChinese( 10, 10, "鹿小班科技STM32") 可在坐标(10,10)显示字符串"鹿小班科技STM32"
*
**********************************************************************************************************************************鹿小班*******/

void LCD_DisplayText(uint16_t x, uint16_t y, char *pText)
{

	while(*pText != 0)	// 判断是否为空字符
	{
		if(*pText<=0x7F)	// 判断是否为ASCII码
		{
			LCD_DisplayChar(x,y,*pText);	// 显示ASCII
			x+=LCD_AsciiFonts->Width;				// 水平坐标加上一个字符的宽度
			pText++;								// 字符串指针+1
		}
		else					// 若字符为汉字
		{
			LCD_DisplayChinese(x,y,pText);	// 显示汉字
			x+=LCD_CHFonts->Width;				// 水平坐标加上一个字符的宽度
			pText+=2;								// 字符串指针+2，汉字的编码要2字节
		}
	}	
}

/*****************************************************************************************************************************************
*	函 数 名:	LCD_ShowNumMode
*
*	入口参数:	mode - 设置数字显示模式
*
*	函数功能:	设置数字显示时空白位补0还是补空格，可选择 Fill_Space 补空格、Fill_Zero 补零
*
*	说   明:   1. 只在 LCD_DisplayNumber() 显示整数 和 LCD_DisplayDecimals()显示小数 两个函数中生效
*					2. 使用示例 LCD_ShowNumMode(Fill_Zero) 设置空位补零，如 123 可以显示为 000123
*
*****************************************************************************************************************************************/

void LCD_ShowNumMode(uint8_t mode)
{
	LCD.ShowNum_Mode = mode;
}


/*****************************************************************************************************************************************
*	函 数 名:	LCD_DisplayNumber
*
*	入口参数:	x - 起始水平坐标
*					y - 起始垂直坐标
*					number - 要显示的数字，范围在 -2147483648~2147483647 之间
*					len - 数字的位数，如果位数len超过数字实际长度，则根据设置补0或空格，如果要显示负数需要预留一位的符号显示空间
*
*	函数功能:	在指定位置显示指定的整数数字
*
*	说   明:	1. 如需要设置字体，请先使用 LCD_SetAsciiFont(&ASCII_Font24) 设置为相关ASCII字符字体
*					2.	如需要设置前景色，请先使用 LCD_SetColor(0x0000FF) 设置为红色
*					3. 请设置对应的背景色，请先使用 LCD_SetBackColor(0x000000) 设置为黑色的背景色
*					4. 使用示例 LCD_DisplayNumber( 10, 10, a, 5) 可在坐标(10,10)显示指定数字a,共5位，空位补0或空格
*						如 a=123 时，根据 LCD_ShowNumMode()设置可能显示  123(前面两个空格位) 或00123
*
*****************************************************************************************************************************************/

void  LCD_DisplayNumber( uint16_t x, uint16_t y, int32_t number, uint8_t len)
{
	char   Number_Buffer[15];				// 用于存储转换后的字符串

	if( LCD.ShowNum_Mode == Fill_Zero)	// 空白位补0
	{
		sprintf( Number_Buffer , "%0.*d",len, number );	// 把 number 转换成字符串并格式化显示
	}
	else			// 空白位补空格
	{
		sprintf( Number_Buffer , "%*d",len, number );	// 把 number 转换成字符串并格式化显示
	}

	LCD_DisplayString( x, y,(char *)Number_Buffer) ;  // 把转换得到的字符串显示出来

}

/***************************************************************************************************************************************
*	函 数 名:	LCD_DisplayDecimals
*
*	入口参数:	x - 起始水平坐标
*					y - 起始垂直坐标
*					decimals - 要显示的数字, double型取值范围1.7 x 10^（-308）~ 1.7 x 10^（+308），实际有效精度为15~16位
*
*       			len - 数字的总显示位数（包含小数点与符号），如果实际的总位数小于指定的位数，则根据设置补0或空格
*						示例1：小数 -123.123 ，指定 len <=8 的话将实际输出 -123.123
*						示例2：小数 -123.123 ，指定 len =10 的话将实际输出   -123.123(数值前面有三个空格位)
*						示例3：小数 -123.123 ，指定 len =10 ，并且调用函数 LCD_ShowNumMode() 设置为补零模式时，实际输出 -00123.123
*
*					decs - 要保留的小数位数，如果小数实际位数多于指定的小数位数，将四舍五入
*							 示例：1.12345 ，指定 decs 为4位的话将输出为1.1235
*
*	函数功能:	在指定位置显示指定的浮点数（小数）
*
*	说   明:	1. 如需要设置字体，请先使用 LCD_SetAsciiFont(&ASCII_Font24) 设置为相关ASCII字符字体
*					2.	如需要设置前景色，请先使用 LCD_SetColor(0x0000FF) 设置为红色
*					3. 请设置对应的背景色，请先使用 LCD_SetBackColor(0x000000) 设置为黑色的背景色
*					4. 使用示例 LCD_DisplayDecimals( 10, 10, a, 5, 3) 可在坐标(10,10)显示浮点数a,总长度为5位，其中保留3位小数
*
*****************************************************************************************************************************************/

void  LCD_DisplayDecimals( uint16_t x, uint16_t y, double decimals, uint8_t len, uint8_t decs)
{
	char  Number_Buffer[20];				// 用于存储转换后的字符串

	if( LCD.ShowNum_Mode == Fill_Zero)	// 空白位补零模式
	{
		sprintf( Number_Buffer , "%0*.*lf",len,decs, decimals );	// 把 number 转换成字符串并格式化显示
	}
	else		// 空白位补空格
	{
		sprintf( Number_Buffer , "%*.*lf",len,decs, decimals );	// 把 number 转换成字符串并格式化显示
	}

	LCD_DisplayString( x, y,(char *)Number_Buffer) ;	// 把转换得到的字符串显示出来
}


/***************************************************************************************************************************************
*	函 数 名: LCD_DrawLine
*
*	入口参数: x1 - 起点 水平坐标
*			 	 y1 - 起点 垂直坐标
*
*				 x2 - 终点 水平坐标
*            y2 - 终点 垂直坐标
*
*	函数功能: 在两点之间画线
*
*	说   明: 该函数移植于ST官方例程中的函数
*
*****************************************************************************************************************************************/

#define ABS(X)  ((X) > 0 ? (X) : -(X))

void LCD_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
	int16_t deltax = 0, deltay = 0, x = 0, y = 0, xinc1 = 0, xinc2 = 0,
	yinc1 = 0, yinc2 = 0, den = 0, num = 0, numadd = 0, numpixels = 0,
	curpixel = 0;

	deltax = ABS(x2 - x1);        /* The difference between the x's */
	deltay = ABS(y2 - y1);        /* The difference between the y's */
	x = x1;                       /* Start x off at the first pixel */
	y = y1;                       /* Start y off at the first pixel */

	if (x2 >= x1)                 /* The x-values are increasing */
	{
	 xinc1 = 1;
	 xinc2 = 1;
	}
	else                          /* The x-values are decreasing */
	{
	 xinc1 = -1;
	 xinc2 = -1;
	}

	if (y2 >= y1)                 /* The y-values are increasing */
	{
	 yinc1 = 1;
	 yinc2 = 1;
	}
	else                          /* The y-values are decreasing */
	{
	 yinc1 = -1;
	 yinc2 = -1;
	}

	if (deltax >= deltay)         /* There is at least one x-value for every y-value */
	{
	 xinc1 = 0;                  /* Don't change the x when numerator >= denominator */
	 yinc2 = 0;                  /* Don't change the y for every iteration */
	 den = deltax;
	 num = deltax / 2;
	 numadd = deltay;
	 numpixels = deltax;         /* There are more x-values than y-values */
	}
	else                          /* There is at least one y-value for every x-value */
	{
	 xinc2 = 0;                  /* Don't change the x for every iteration */
	 yinc1 = 0;                  /* Don't change the y when numerator >= denominator */
	 den = deltay;
	 num = deltay / 2;
	 numadd = deltax;
	 numpixels = deltay;         /* There are more y-values than x-values */
	}
	for (curpixel = 0; curpixel <= numpixels; curpixel++)
	{
	 LCD_DrawPoint(x,y,LCD.Color);             /* Draw the current pixel */
	 num += numadd;              /* Increase the numerator by the top of the fraction */
	 if (num >= den)             /* Check if numerator >= denominator */
	 {
		num -= den;               /* Calculate the new numerator value */
		x += xinc1;               /* Change the x as appropriate */
		y += yinc1;               /* Change the y as appropriate */
	 }
	 x += xinc2;                 /* Change the x as appropriate */
	 y += yinc2;                 /* Change the y as appropriate */
	}
}

/***************************************************************************************************************************************
*	函 数 名: LCD_DrawLine_V
*
*	入口参数: x - 水平坐标
*			 	 y - 垂直坐标
*				 height - 垂直长度
*
*	函数功能: 在指定位置画指定长度的 垂直 线
*
*	说   明: 1. 该函数移植于ST官方例程中的函数
*				 2. 要绘制的线不能超出屏幕显示范围
*            3. 如果只是画垂直线，建议使用此函数，速度比 LCD_DrawLine 快很多
*  性能测试：
*****************************************************************************************************************************************/

void LCD_DrawLine_V(uint16_t x, uint16_t y, uint16_t height)
{
   uint16_t i ; // 循环计数器

	for (i = 0; i < height; i++)
	{
       LCD_Buff[i] =  LCD.Color;  // 写入缓冲区
   }
   LCD_SetAddress( x, y, x, y+height-1);	     // 设置显示区域

   LCD_WriteBuff(LCD_Buff,height);          // 写显存
}

/***************************************************************************************************************************************
*	函 数 名: LCD_DrawLine_H
*
*	入口参数: x - 水平坐标
*			 	 y - 垂直坐标
*				 width  - 水平长度
*
*	函数功能: 在指定位置画指定长度的 水平 线
*
*	说   明: 1. 该函数移植于ST官方例程中的函数
*				 2. 要绘制的线不能超出屏幕显示范围
*            3. 如果只是画 水平 线，建议使用此函数，速度比 LCD_DrawLine 快很多
*  性能测试：
**********************************************************************************************************************************鹿小班*******/

void LCD_DrawLine_H(uint16_t x, uint16_t y, uint16_t width)
{
   uint16_t i ; // 循环计数器

	for (i = 0; i < width; i++)
	{
       LCD_Buff[i] =  LCD.Color;  // 写入缓冲区
   }
   LCD_SetAddress( x, y, x+width-1, y);	     // 设置显示区域

   LCD_WriteBuff(LCD_Buff,width);          // 写显存
}
/***************************************************************************************************************************************
*	函 数 名: LCD_DrawRect
*
*	入口参数: x - 水平坐标
*			 	 y - 垂直坐标
*			 	 width  - 水平长度
*				 height - 垂直长度
*
*	函数功能: 在指定位置画指定大小的矩形边框
*
*	说   明: 1. 该函数移植于ST官方例程中的函数
*				 2. 要绘制的线不能超出屏幕显示范围
*
*****************************************************************************************************************************************/

void LCD_DrawRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height)
{
   // 绘制水平线
   LCD_DrawLine_H( x,  y,  width);
   LCD_DrawLine_H( x,  y+height-1,  width);

   // 绘制垂直线
   LCD_DrawLine_V( x,  y,  height);
   LCD_DrawLine_V( x+width-1,  y,  height);
}


/***************************************************************************************************************************************
*	函 数 名: LCD_DrawCircle
*
*	入口参数: x - 圆心 水平坐标
*			 	 y - 圆心 垂直坐标
*			 	 r  - 半径
*
*	函数功能: 以 (x,y) 为圆心画半径为 r 的圆形边框
*
*	说   明: 1. 该函数移植于ST官方例程中的函数
*				 2. 要绘制的线不能超出屏幕显示范围
*
*****************************************************************************************************************************************/

void LCD_DrawCircle(uint16_t x, uint16_t y, uint16_t r)
{
	int Xadd = -r, Yadd = 0, err = 2-2*r, e2;
	do {

		LCD_DrawPoint(x-Xadd,y+Yadd,LCD.Color);
		LCD_DrawPoint(x+Xadd,y+Yadd,LCD.Color);
		LCD_DrawPoint(x+Xadd,y-Yadd,LCD.Color);
		LCD_DrawPoint(x-Xadd,y-Yadd,LCD.Color);

		e2 = err;
		if (e2 <= Yadd) {
			err += ++Yadd*2+1;
			if (-Xadd == Yadd && e2 <= Xadd) e2 = 0;
		}
		if (e2 > Xadd) err += ++Xadd*2+1;
	}
	while (Xadd <= 0);
}


/***************************************************************************************************************************************
*	函 数 名: LCD_DrawEllipse
*
*	入口参数: x - 圆心 水平坐标
*			 	 y - 圆心 垂直坐标
*			 	 r1  - 水平半轴的长度
*				 r2  - 垂直半轴的长度
*
*	函数功能: 以 (x,y) 为中心画水平半轴为 r1 垂直半轴为 r2 的椭圆边框
*
*	说   明: 1. 该函数移植于ST官方例程中的函数
*				 2. 要绘制的线不能超出屏幕显示范围
*
*****************************************************************************************************************************************/

void LCD_DrawEllipse(int x, int y, int r1, int r2)
{
	int Xadd = -r1, Yadd = 0, err = 2-2*r1, e2;
	float K = 0, rad1 = 0, rad2 = 0;

	rad1 = r1;
	rad2 = r2;

	if (r1 > r2)
	{
		do {
			K = (float)(rad1/rad2);

			LCD_DrawPoint(x-Xadd,y+(uint16_t)(Yadd/K),LCD.Color);
			LCD_DrawPoint(x+Xadd,y+(uint16_t)(Yadd/K),LCD.Color);
			LCD_DrawPoint(x+Xadd,y-(uint16_t)(Yadd/K),LCD.Color);
			LCD_DrawPoint(x-Xadd,y-(uint16_t)(Yadd/K),LCD.Color);

			e2 = err;
			if (e2 <= Yadd) {
				err += ++Yadd*2+1;
				if (-Xadd == Yadd && e2 <= Xadd) e2 = 0;
			}
			if (e2 > Xadd) err += ++Xadd*2+1;
		}
		while (Xadd <= 0);
	}
	else
	{
		Yadd = -r2;
		Xadd = 0;
		do {
			K = (float)(rad2/rad1);

			LCD_DrawPoint(x-(uint16_t)(Xadd/K),y+Yadd,LCD.Color);
			LCD_DrawPoint(x+(uint16_t)(Xadd/K),y+Yadd,LCD.Color);
			LCD_DrawPoint(x+(uint16_t)(Xadd/K),y-Yadd,LCD.Color);
			LCD_DrawPoint(x-(uint16_t)(Xadd/K),y-Yadd,LCD.Color);

			e2 = err;
			if (e2 <= Xadd) {
				err += ++Xadd*3+1;
				if (-Yadd == Xadd && e2 <= Yadd) e2 = 0;
			}
			if (e2 > Yadd) err += ++Yadd*3+1;
		}
		while (Yadd <= 0);
	}
}

/***************************************************************************************************************************************
*	函 数 名: LCD_FillRect
*
*	入口参数: x - 水平坐标
*			 	 y - 垂直坐标
*			 	 width  - 水平长度
*				 height -垂直长度
*
*	函数功能: 以 (x,y) 为起点画指定大小的实心矩形
*
*	说   明: 要绘制的线不能超出屏幕显示范围
*
*****************************************************************************************************************************************/

void LCD_FillRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height)
{
   LCD_SetAddress( x, y, x+width-1, y+height-1);	// 设置矩形区域

	LCD_DC_Data;     // 拉高指令选择 引脚（推挽输出高电平），代表接下来发送 数据

// 修改为16位数据宽度，写入数据更高效，减少传输次数
   LCD_SPI.Init.DataSize 	= SPI_DATASIZE_16BIT;   //	16位数据宽度
   HAL_SPI_Init(&LCD_SPI);

   LCD_SPI_Transmit(&LCD_SPI, LCD.Color, width*height) ;

// 改回8位数据宽度，因为指令和参数数据都是按8位传输的
	LCD_SPI.Init.DataSize 	= SPI_DATASIZE_8BIT;    //	8位数据宽度
   HAL_SPI_Init(&LCD_SPI);
}


/***************************************************************************************************************************************
*	函 数 名: LCD_DrawImage
*
*	入口参数: x - 起始水平坐标
*				 y - 起始垂直坐标
*			 	 width  - 图片的水平长度
*				 height - 图片的垂直高度
*				*pImage - 图片数据存储数组首地址
*
*	函数功能: 在指定坐标处显示图片
*
*	说   明: 1.要显示的图片需要先进行取模，知晓图片的长度和宽度
*            2.使用 LCD_SetColor() 函数设置前景色、LCD_SetBackColor() 设置背景色
*
*****************************************************************************************************************************************/

void 	LCD_DrawImage(uint16_t x,uint16_t y,uint16_t width,uint16_t height,const uint8_t *pImage)
{
   uint8_t   disChar;	         // 字模数值
	uint16_t  Xaddress = x;       // 水平坐标
 	uint16_t  Yaddress = y;       // 垂直坐标
	uint16_t  i=0,j=0,m=0;        // 循环计数器
	uint16_t  BuffCount = 0;      // 缓冲区计数
   uint16_t  Buff_Height = 0;    // 缓冲区可容纳的行数

// 因为缓冲区大小有限，需要分段写入
   Buff_Height = (sizeof(LCD_Buff)/2) / height;    // 计算缓冲区能够写入图片的多少行

	for(i = 0; i <height; i++)             // 循环分段写入
	{
		for(j = 0; j <(float)width/8; j++)
		{
			disChar = *pImage;

			for(m = 0; m < 8; m++)
			{
				if(disChar & 0x01)
				{
               LCD_Buff[BuffCount] =  LCD.Color;			// 当前模值位为1时，使用前景色填充
				}
				else
				{
				   LCD_Buff[BuffCount] = LCD.BackColor;		//否则使用背景色填充点
				}
				disChar >>= 1;     // 模值右移
				Xaddress++;        // 水平坐标自加
				BuffCount++;       // 缓冲区计数自加
				if( (Xaddress - x)==width ) // 如果水平坐标达到图片宽度，则退出当前循环，进行下一行的填充
				{
					Xaddress = x;
					break;
				}
			}
			pImage++;
		}
      if( BuffCount == Buff_Height*width  )  // 达到缓冲区最大可填充数量时
      {
         BuffCount = 0; // 缓冲区计数归零

         LCD_SetAddress( x, Yaddress , x+width-1, Yaddress+Buff_Height-1);	// 设置显示区域
         LCD_WriteBuff(LCD_Buff,width*Buff_Height);          // 写显存

         Yaddress = Yaddress+Buff_Height;    // 坐标偏移，开始写下一段数据
      }
      if( (i+1)== height ) // 如果是最后一行
      {
         LCD_SetAddress( x, Yaddress , x+width-1,i+y);	   // 设置显示区域
         LCD_WriteBuff(LCD_Buff,width*(i+1+y-Yaddress));    // 写显存
      }
	}
}


/***************************************************************************************************************************************
*	函 数 名: LCD_CopyBuffer
*
*	入口参数: x - 起始水平坐标
*				 y - 起始垂直坐标
*			 	 width  - 目标区域的水平长度
*				 height - 目标区域的垂直高度
*				*pImage - 数据存储数组首地址
*
*	函数功能: 在指定坐标处直接将数据拷贝到屏幕显存
*
*	说   明: 该拷贝函数可以用于移植 LVGL 图形库和摄像头采集图像显示使用
*
*****************************************************************************************************************************************/

void	LCD_CopyBuffer(uint16_t x, uint16_t y,uint16_t width,uint16_t height,uint16_t *DataBuff)
{

	LCD_SetAddress(x,y,x+width-1,y+height-1);

	LCD_DC_Data;     // 拉高指令选择 引脚（推挽输出高电平），代表接下来发送 数据

// 修改为16位数据宽度，写入数据更高效，减少传输次数
   LCD_SPI.Init.DataSize 	= SPI_DATASIZE_16BIT;   //	16位数据宽度
   HAL_SPI_Init(&LCD_SPI);

	LCD_SPI_TransmitBuffer(&LCD_SPI, DataBuff,width * height) ;

//	HAL_SPI_Transmit(&hspi5, (uint8_t *)DataBuff, (x2-x1+1) * (y2-y1+1), 1000) ;

// 改回8位数据宽度，因为指令和参数数据都是按8位传输的
	LCD_SPI.Init.DataSize 	= SPI_DATASIZE_8BIT;    //	8位数据宽度
   HAL_SPI_Init(&LCD_SPI);

}

/**********************************************************************************************************************************
*
* 下面几个函数是修改自HAL的库函数，目的是为了SPI发送数据不限制数据长度的写入，从而提高刷新速度
*
*****************************************************************************************************************鹿小班************/


/**
  * @brief Handle SPI Communication Timeout.
  * @param hspi: pointer to a SPI_HandleTypeDef structure that contains
  *              the configuration information for SPI module.
  * @param Flag: SPI flag to check
  * @param Status: flag state to check
  * @param Timeout: Timeout duration
  * @param Tickstart: Tick start value
  * @retval HAL status
  */
HAL_StatusTypeDef LCD_SPI_WaitOnFlagUntilTimeout(SPI_HandleTypeDef *hspi, uint32_t Flag, FlagStatus Status,
                                                    uint32_t Tickstart, uint32_t Timeout)
{
   /* Wait until flag is set */
   while ((__HAL_SPI_GET_FLAG(hspi, Flag) ? SET : RESET) == Status)
   {
      /* Check for the Timeout */
      if ((((HAL_GetTick() - Tickstart) >=  Timeout) && (Timeout != HAL_MAX_DELAY)) || (Timeout == 0U))
      {
         return HAL_TIMEOUT;
      }
   }
   return HAL_OK;
}


/**
 * @brief  Close Transfer and clear flags.
 * @param  hspi: pointer to a SPI_HandleTypeDef structure that contains
 *               the configuration information for SPI module.
 * @retval HAL_ERROR: if any error detected
 *         HAL_OK: if nothing detected
 */
 void LCD_SPI_CloseTransfer(SPI_HandleTypeDef *hspi)
{
  uint32_t itflag = hspi->Instance->SR;

  __HAL_SPI_CLEAR_EOTFLAG(hspi);
  __HAL_SPI_CLEAR_TXTFFLAG(hspi);

  /* Disable SPI peripheral */
  __HAL_SPI_DISABLE(hspi);

  /* Disable ITs */
  __HAL_SPI_DISABLE_IT(hspi, (SPI_IT_EOT | SPI_IT_TXP | SPI_IT_RXP | SPI_IT_DXP | SPI_IT_UDR | SPI_IT_OVR | SPI_IT_FRE | SPI_IT_MODF));

  /* Disable Tx DMA Request */
  CLEAR_BIT(hspi->Instance->CFG1, SPI_CFG1_TXDMAEN | SPI_CFG1_RXDMAEN);

  /* Report UnderRun error for non RX Only communication */
  if (hspi->State != HAL_SPI_STATE_BUSY_RX)
  {
    if ((itflag & SPI_FLAG_UDR) != 0UL)
    {
      SET_BIT(hspi->ErrorCode, HAL_SPI_ERROR_UDR);
      __HAL_SPI_CLEAR_UDRFLAG(hspi);
    }
  }

  /* Report OverRun error for non TX Only communication */
  if (hspi->State != HAL_SPI_STATE_BUSY_TX)
  {
    if ((itflag & SPI_FLAG_OVR) != 0UL)
    {
      SET_BIT(hspi->ErrorCode, HAL_SPI_ERROR_OVR);
      __HAL_SPI_CLEAR_OVRFLAG(hspi);
    }
  }

  /* SPI Mode Fault error interrupt occurred -------------------------------*/
  if ((itflag & SPI_FLAG_MODF) != 0UL)
  {
    SET_BIT(hspi->ErrorCode, HAL_SPI_ERROR_MODF);
    __HAL_SPI_CLEAR_MODFFLAG(hspi);
  }

  /* SPI Frame error interrupt occurred ------------------------------------*/
  if ((itflag & SPI_FLAG_FRE) != 0UL)
  {
    SET_BIT(hspi->ErrorCode, HAL_SPI_ERROR_FRE);
    __HAL_SPI_CLEAR_FREFLAG(hspi);
  }

  hspi->TxXferCount = (uint16_t)0UL;
  hspi->RxXferCount = (uint16_t)0UL;
}


/**
  * @brief  专为屏幕填充修改，主要用于大量同色填充
  * @param  hspi   : spi的句柄
  * @param  pData  : 要写入的数据
  * @param  Size   : 数据大小
  * @retval HAL status
  */

HAL_StatusTypeDef LCD_SPI_Transmit(SPI_HandleTypeDef *hspi,uint16_t pData, uint32_t Size)
{
   uint32_t    tickstart;
   uint32_t    Timeout = 1000;      // 超时判断
   uint32_t    LCD_pData_32bit;     // 写32位数据时合并
   uint32_t    LCD_TxDataCount;     // 发送计数
   HAL_StatusTypeDef errorcode = HAL_OK;

	/* Check Direction parameter */
	assert_param(IS_SPI_DIRECTION_2LINES_OR_1LINE_2LINES_TXONLY(hspi->Init.Direction));

	/* Process Locked */
	__HAL_LOCK(hspi);

	/* Init tickstart for timeout management*/
	tickstart = HAL_GetTick();

	if (hspi->State != HAL_SPI_STATE_READY)
	{
		errorcode = HAL_BUSY;
		__HAL_UNLOCK(hspi);
		return errorcode;
	}

	if ( Size == 0UL)
	{
		errorcode = HAL_ERROR;
		__HAL_UNLOCK(hspi);
		return errorcode;
	}

	/* Set the transaction information */
	hspi->State       = HAL_SPI_STATE_BUSY_TX;
	hspi->ErrorCode   = HAL_SPI_ERROR_NONE;

	LCD_TxDataCount   = Size;                // 需要发送的数据长度
	LCD_pData_32bit   = (pData<<16)|pData ;  // 用32位传输时，合并2个像素的相同颜色

	/*Init field not used in handle to zero */
	hspi->pRxBuffPtr  = NULL;
	hspi->RxXferSize  = (uint16_t) 0UL;
	hspi->RxXferCount = (uint16_t) 0UL;
	hspi->TxISR       = NULL;
	hspi->RxISR       = NULL;

	/* Configure communication direction : 1Line */
	if (hspi->Init.Direction == SPI_DIRECTION_1LINE)
	{
		SPI_1LINE_TX(hspi);
	}

// 不使用硬件 TSIZE 控制，此处设置为0，不限制传输数据长度
	MODIFY_REG(hspi->Instance->CR2, SPI_CR2_TSIZE, 0);

	/* Enable SPI peripheral */
	__HAL_SPI_ENABLE(hspi);

	if (hspi->Init.Mode == SPI_MODE_MASTER)
	{
		 /* Master transfer start */
		 SET_BIT(hspi->Instance->CR1, SPI_CR1_CSTART);
	}

	/* Transmit data in 16 Bit mode */
	while (LCD_TxDataCount > 0UL)
	{
		/* Wait until TXP flag is set to send data */
		if (__HAL_SPI_GET_FLAG(hspi, SPI_FLAG_TXP))
		{
			if ((LCD_TxDataCount > 1UL) && (hspi->Init.DataSize > SPI_DATASIZE_8BIT) && (hspi->Init.FifoThreshold > SPI_FIFO_THRESHOLD_01DATA))
			{
				*((__IO uint32_t *)&hspi->Instance->TXDR) = (uint32_t )LCD_pData_32bit;
				LCD_TxDataCount -= (uint16_t)2UL;
			}
			else if (hspi->Init.DataSize > SPI_DATASIZE_8BIT)
			{
				*((__IO uint16_t *)&hspi->Instance->TXDR) = (uint16_t)pData;
				LCD_TxDataCount--;
			}
			else
			{
				*((__IO uint8_t *)&hspi->Instance->TXDR) = (uint8_t)pData;
				LCD_TxDataCount--;
			}
		}
		else
		{
			/* Timeout management */
			if ((((HAL_GetTick() - tickstart) >=  Timeout) && (Timeout != HAL_MAX_DELAY)) || (Timeout == 0U))
			{
				/* Call standard close procedure with error check */
				LCD_SPI_CloseTransfer(hspi);

				/* Process Unlocked */
				__HAL_UNLOCK(hspi);

				SET_BIT(hspi->ErrorCode, HAL_SPI_ERROR_TIMEOUT);
				hspi->State = HAL_SPI_STATE_READY;
				return HAL_ERROR;
			}
		}
	}

	if (LCD_SPI_WaitOnFlagUntilTimeout(hspi, SPI_SR_TXC, RESET, tickstart, Timeout) != HAL_OK)
	{
		SET_BIT(hspi->ErrorCode, HAL_SPI_ERROR_FLAG);
	}

	SET_BIT((hspi)->Instance->CR1 , SPI_CR1_CSUSP); // 请求挂起SPI传输
	/* 等待SPI挂起 */
	if (LCD_SPI_WaitOnFlagUntilTimeout(hspi, SPI_FLAG_SUSP, RESET, tickstart, Timeout) != HAL_OK)
	{
		SET_BIT(hspi->ErrorCode, HAL_SPI_ERROR_FLAG);
	}
	LCD_SPI_CloseTransfer(hspi);   /* Call standard close procedure with error check */

	SET_BIT((hspi)->Instance->IFCR , SPI_IFCR_SUSPC);  // 清除挂起标志位


	/* Process Unlocked */
	__HAL_UNLOCK(hspi);

	hspi->State = HAL_SPI_STATE_READY;

	if (hspi->ErrorCode != HAL_SPI_ERROR_NONE)
	{
		return HAL_ERROR;
	}
	return errorcode;
}

/**
  * @brief  专为数据写入修改，使之支持不定长度的数据传输
  * @param  hspi   : spi的句柄
  * @param  pData  : 要写入的数据
  * @param  Size   : 数据大小
  * @retval HAL status
  */
HAL_StatusTypeDef LCD_SPI_TransmitBuffer (SPI_HandleTypeDef *hspi, uint16_t *pData, uint32_t Size)
{
   uint32_t    tickstart;

   uint32_t    Timeout = 1000;      // 超时判断
   uint32_t    LCD_TxDataCount;     // 发送计数
   HAL_StatusTypeDef errorcode = HAL_OK;

	/* Check Direction parameter */
	assert_param(IS_SPI_DIRECTION_2LINES_OR_1LINE_2LINES_TXONLY(hspi->Init.Direction));

	/* Process Locked */
	__HAL_LOCK(hspi);

	/* Init tickstart for timeout management*/
	tickstart = HAL_GetTick();

	if (hspi->State != HAL_SPI_STATE_READY)
	{
		errorcode = HAL_BUSY;
		__HAL_UNLOCK(hspi);
		return errorcode;
	}

	if ( Size == 0UL)
	{
		errorcode = HAL_ERROR;
		__HAL_UNLOCK(hspi);
		return errorcode;
	}

	/* Set the transaction information */
	hspi->State       = HAL_SPI_STATE_BUSY_TX;
	hspi->ErrorCode   = HAL_SPI_ERROR_NONE;

	LCD_TxDataCount   = Size;                // 需要发送的数据长度

	/*Init field not used in handle to zero */
	hspi->pRxBuffPtr  = NULL;
	hspi->RxXferSize  = (uint16_t) 0UL;
	hspi->RxXferCount = (uint16_t) 0UL;
	hspi->TxISR       = NULL;
	hspi->RxISR       = NULL;

	/* Configure communication direction : 1Line */
	if (hspi->Init.Direction == SPI_DIRECTION_1LINE)
	{
		SPI_1LINE_TX(hspi);
	}

// 不使用硬件 TSIZE 控制，此处设置为0，不限制传输数据长度
	MODIFY_REG(hspi->Instance->CR2, SPI_CR2_TSIZE, 0);

	/* Enable SPI peripheral */
	__HAL_SPI_ENABLE(hspi);

	if (hspi->Init.Mode == SPI_MODE_MASTER)
	{
		 /* Master transfer start */
		 SET_BIT(hspi->Instance->CR1, SPI_CR1_CSTART);
	}

	/* Transmit data in 16 Bit mode */
	while (LCD_TxDataCount > 0UL)
	{
		/* Wait until TXP flag is set to send data */
		if (__HAL_SPI_GET_FLAG(hspi, SPI_FLAG_TXP))
		{
			if ((LCD_TxDataCount > 1UL) && (hspi->Init.FifoThreshold > SPI_FIFO_THRESHOLD_01DATA))
			{
				*((__IO uint32_t *)&hspi->Instance->TXDR) = *((uint32_t *)pData);
				pData += 2;
				LCD_TxDataCount -= 2;
			}
			else
			{
				*((__IO uint16_t *)&hspi->Instance->TXDR) = *((uint16_t *)pData);
				pData += 1;
				LCD_TxDataCount--;
			}
		}
		else
		{
			/* Timeout management */
			if ((((HAL_GetTick() - tickstart) >=  Timeout) && (Timeout != HAL_MAX_DELAY)) || (Timeout == 0U))
			{
				/* Call standard close procedure with error check */
				LCD_SPI_CloseTransfer(hspi);

				/* Process Unlocked */
				__HAL_UNLOCK(hspi);

				SET_BIT(hspi->ErrorCode, HAL_SPI_ERROR_TIMEOUT);
				hspi->State = HAL_SPI_STATE_READY;
				return HAL_ERROR;
			}
		}
	}

	if (LCD_SPI_WaitOnFlagUntilTimeout(hspi, SPI_SR_TXC, RESET, tickstart, Timeout) != HAL_OK)
	{
		SET_BIT(hspi->ErrorCode, HAL_SPI_ERROR_FLAG);
	}

	SET_BIT((hspi)->Instance->CR1 , SPI_CR1_CSUSP); // 请求挂起SPI传输
	/* 等待SPI挂起 */
	if (LCD_SPI_WaitOnFlagUntilTimeout(hspi, SPI_FLAG_SUSP, RESET, tickstart, Timeout) != HAL_OK)
	{
		SET_BIT(hspi->ErrorCode, HAL_SPI_ERROR_FLAG);
	}
	LCD_SPI_CloseTransfer(hspi);   /* Call standard close procedure with error check */

	SET_BIT((hspi)->Instance->IFCR , SPI_IFCR_SUSPC);  // 清除挂起标志位


	/* Process Unlocked */
	__HAL_UNLOCK(hspi);

	hspi->State = HAL_SPI_STATE_READY;

	if (hspi->ErrorCode != HAL_SPI_ERROR_NONE)
	{
		return HAL_ERROR;
	}
	return errorcode;
}


