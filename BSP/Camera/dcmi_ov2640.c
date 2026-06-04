/***
	*************************************************************************************************
	*	@version V1.0
   *************************************************************************************************
   *  @description
	*
	*	ʵ��ƽ̨��¹С��STM32H743VGT6���İ� ���ͺţ�FK743M3-VGT6��+ OV2640ģ�飨�ͺţ�OV2640M1-200W�� 
	*
>>>>> �ֱ���˵����
	*
  *  SVGAģʽ  ------>  800*600��  ���֡��30֡
	*	 UXGAģʽ  ------>  1600*1200�����֡��15֡
	*************************************************************************************************************************************************************************************************************************************************************************************¹С��*****
***/

#include "dcmi_ov2640.h"
#include "dcmi_ov2640_cfg.h"
#include "dcmi.h"
#include <stdio.h>

extern DCMI_HandleTypeDef hdcmi;       // CubeMX自动生成，此处使用extern声明


/***************************************************************************************************************************************
*	�� �� ��: OV2640_DMA_Init
*
*	��������: ���� DMA ��ز���
*
*	˵    ��: ʹ�õ���DMA2�����赽�洢��ģʽ������λ��32bit���������ж� 			          
*
*****************************************************************************************************************************************/
void OV2640_DMA_Init(void)
{
   __HAL_RCC_DMA2_CLK_ENABLE();   // ʹ��DMA2ʱ��
	DMA_Handle_dcmi.Instance = DMA2_Stream7;   // 使用 DMA2 的数据流 7
	DMA_Handle_dcmi.Init.Request                 = DMA_REQUEST_DCMI;           // DMA 请求映射到 DCMI
   DMA_Handle_dcmi.Init.Direction               = DMA_PERIPH_TO_MEMORY;       // 外设到存储器模式
   DMA_Handle_dcmi.Init.PeriphInc               = DMA_PINC_DISABLE;           // 外设地址固定
   DMA_Handle_dcmi.Init.MemInc                  = DMA_MINC_ENABLE;			  // 存储器地址自增
   DMA_Handle_dcmi.Init.PeriphDataAlignment     = DMA_PDATAALIGN_WORD;        // 外设数据宽度 32 位（字）
   DMA_Handle_dcmi.Init.MemDataAlignment        = DMA_MDATAALIGN_WORD;        // 存储器数据宽度 32 位（字）
   DMA_Handle_dcmi.Init.Mode                    = DMA_CIRCULAR;               // 循环模式
   DMA_Handle_dcmi.Init.Priority                = DMA_PRIORITY_LOW;           // 优先级低
   DMA_Handle_dcmi.Init.FIFOMode                = DMA_FIFOMODE_ENABLE;        // 使能 FIFO
   DMA_Handle_dcmi.Init.FIFOThreshold           = DMA_FIFO_THRESHOLD_FULL;    // FIFO 满阈值（4×32bit）
   DMA_Handle_dcmi.Init.MemBurst                = DMA_MBURST_SINGLE;          // 存储器单次传输
   DMA_Handle_dcmi.Init.PeriphBurst             = DMA_PBURST_SINGLE;          // 外设单次传输

   HAL_DMA_Init(&DMA_Handle_dcmi);                        // ����DMA
   __HAL_LINKDMA(&hdcmi, DMA_Handle, DMA_Handle_dcmi);    // ����DCMI���
   HAL_NVIC_SetPriority(DMA2_Stream7_IRQn, 0, 0);         // �����ж����ȼ�
   HAL_NVIC_EnableIRQ(DMA2_Stream7_IRQn);                 // ʹ���ж�
}

/***************************************************************************************************************************************
*	�� �� ��: OV2640_Delay
*	��ڲ���: Delay - ��ʱʱ�䣬��λ ms 
*	��������: ����ʱ���������Ǻܾ�ȷ
*	˵    ��: Ϊ����ֲ�ļ����,�˴�����������ʱ��ʵ����Ŀ�п����滻��RTOS����ʱ����HAL�����ʱ
*****************************************************************************************************************************************/
void OV2640_Delay(volatile uint32_t Delay)
{
	volatile uint16_t i;

	while (Delay --)				
	{
		for (i = 0; i < 40000; i++);
	}	
//	HAL_Delay(Delay);	  // ��ʹ��HAL�����ʱ ������OS����ʱ
}

/***************************************************************************************************************************************
*	�� �� ��: DCMI_OV2640_Init
*
*	��������: ��ʼSCCB��DCMI��DMA�Լ�����OV2640
*
*****************************************************************************************************************************************/
int8_t DCMI_OV2640_Init(void)
{
	uint16_t	Device_ID;		// ��������洢����ID

   SCCB_GPIO_Config();		               // SCCB���ų�ʼ��
	MX_DCMI_Init();                        /* DCMI init (dcmi.c, configured for OV2640 hardware sync) */
   OV2640_DMA_Init();                     /* DMA init (DMA2_Stream7, circular mode) */
	OV2640_Reset();	                     // ִ��������λ
	Device_ID = OV2640_ReadID();		      // ��ȡ����ID

	if( (Device_ID == 0x2640) || (Device_ID == 0x2642) )		// ����ƥ�䣬ʵ�ʵ�����ID������ 0x2640 ���� 0x2642
	{
		printf ("OV2640 OK,ID:0x%X\r\n",Device_ID);		      // ƥ��ͨ��

      OV2640_Config( OV2640_SVGA_Config );             		// ���� SVGAģʽ  ------>  800*600��  ���֡��30֡
//		OV2640_Config( OV2640_UXGA_Config );                  // ���� UXGAģʽ  ------>  1600*1200�����֡��15֡

		OV2640_Set_Framesize(OV2640_Width,OV2640_Height);		// ����OV2640�����ͼ���С

		OV2640_DCMI_Crop( Display_Width, Display_Height, OV2640_Width, OV2640_Height );	// ��OV2640���ͼ��ü�����Ӧ��Ļ�Ĵ�С

		return OV2640_Success;	 // ���سɹ���־
	}
	else
	{
		printf ("OV2640 ERROR!!!!!  ID:%X\r\n",Device_ID);	   // ��ȡID����
		return  OV2640_Error;	 // ���ش����־
	}
}

/***************************************************************************************************************************************
*	�� �� ��: OV2640_DMA_Transmit_Continuous
*
*	��ڲ���:  DMA_Buffer - DMA��Ҫ����ĵ�ַ�������ڴ洢����ͷ���ݵĴ洢����ַ
*            DMA_BufferSize - ��������ݴ�С��32λ��
*
*	��������: ����DMA���䣬����ģʽ
*
*	˵    ��: 1. ��������ģʽ֮�󣬻�һֱ���д��䣬���ǹ������ֹͣDCMI
*            2. OV2640ʹ��RGB565ģʽʱ��1�����ص���Ҫ2���ֽ����洢
*				 3. ��ΪDMA���ô�������Ϊ32λ�������� DMA_BufferSize ʱ����Ҫ����4�����磺
*               Ҫ��ȡ 240*240�ֱ��� ��ͼ����Ҫ���� 240*240*2 = 115200 �ֽڵ����ݣ�
*               �� DMA_BufferSize = 115200 / 4 = 28800 ��
*¹С��
*****************************************************************************************************************************************/
void OV2640_DMA_Transmit_Continuous(uint32_t DMA_Buffer,uint32_t DMA_BufferSize)
{
   DMA_Handle_dcmi.Init.Mode  = DMA_CIRCULAR;  // ѭ��ģʽ					

   HAL_DMA_Init(&DMA_Handle_dcmi);    // ����DMA

  // ʹ��DCMI�ɼ�����,�����ɼ�ģʽ
   HAL_DCMI_Start_DMA(&hdcmi, DCMI_MODE_CONTINUOUS, (uint32_t)DMA_Buffer,DMA_BufferSize);
}

/***************************************************************************************************************************************
*	�� �� ��: OV2640_DMA_Transmit_Snapshot
*
*	��ڲ���:  DMA_Buffer - DMA��Ҫ����ĵ�ַ�������ڴ洢����ͷ���ݵĴ洢����ַ
*            DMA_BufferSize - ��������ݴ�С��32λ��
*
*	��������: ����DMA���䣬����ģʽ������һ֡ͼ���ֹͣ
*
*	˵    ��: 1. ����ģʽ��ֻ����һ֡������
*            2. OV2640ʹ��RGB565ģʽʱ��1�����ص���Ҫ2���ֽ����洢
*				 3. ��ΪDMA���ô�������Ϊ32λ�������� DMA_BufferSize ʱ����Ҫ����4�����磺
*               Ҫ��ȡ 240*240�ֱ��� ��ͼ����Ҫ���� 240*240*2 = 115200 �ֽڵ����ݣ�
*               �� DMA_BufferSize = 115200 / 4 = 28800 ��
*            4. ʹ�ø�ģʽ�������֮��DCMI�ᱻ�����ٴ����ô���֮ǰ����Ҫ���� OV2640_DCMI_Resume() �ָ�DCMI
*
*****************************************************************************************************************************************/
void OV2640_DMA_Transmit_Snapshot(uint32_t DMA_Buffer,uint32_t DMA_BufferSize)
{
   DMA_Handle_dcmi.Init.Mode  = DMA_NORMAL;  // ����ģʽ					

   HAL_DMA_Init(&DMA_Handle_dcmi);    // ����DMA

   HAL_DCMI_Start_DMA(&hdcmi, DCMI_MODE_SNAPSHOT, (uint32_t)DMA_Buffer,DMA_BufferSize);
}

/***************************************************************************************************************************************
*	�� �� ��: OV2640_DCMI_Suspend
*
*	��������: ����DCMI��ֹͣ��������
*
*	˵    ��: 1. ��������ģʽ֮���ٵ��øú�������ֹͣ����DCMI������
*            2. ���Ե��� OV2640_DCMI_Resume() �ָ�DCMI
*				 3. ��Ҫע��ģ�����DCMI�ڼ䣬DMA��û��ֹͣ������
*¹С��
*****************************************************************************************************************************************/
void OV2640_DCMI_Suspend(void) 
{
   HAL_DCMI_Suspend(&hdcmi);    // ����DCMI
}

/***************************************************************************************************************************************
*	�� �� ��: OV2640_DCMI_Resume
*
*	��������: �ָ�DCMI����ʼ��������
*
*	˵    ��: 1. ��DCMI������ʱ�����Ե��øú����ָ�
*            2. ʹ�� OV2640_DMA_Transmit_Snapshot() ����ģʽ���������֮��DCMIҲ�ᱻ�����ٴ����ô���֮ǰ��
*				    ��Ҫ���ñ������ָ�DCMI����
*
*****************************************************************************************************************************************/
void  OV2640_DCMI_Resume(void) 
{
   (&hdcmi)->State = HAL_DCMI_STATE_BUSY;       // ���DCMI��־
   (&hdcmi)->Instance->CR |= DCMI_CR_CAPTURE;   // ����DCMI����
}

/***************************************************************************************************************************************
*	�� �� ��: OV2640_DCMI_Stop
*
*	��������: ��ֹDCMI��DMA����ֹͣDCMI���񣬽�ֹDCMI����
*
*****************************************************************************************************************************************/
void  OV2640_DCMI_Stop(void) 
{
   HAL_DCMI_Stop(&hdcmi);
}

/***************************************************************************************************************************************
*	�� �� ��: OV2640_DCMI_Crop
*
*	��ڲ���:  Displey_XSize ��Displey_YSize - ��ʾ���ĳ���
*				  Sensor_XSize��Sensor_YSize - ����ͷ���������ͼ��ĳ���
*
*	��������: ʹ��DCMI�Ĳü����ܣ��������������ͼ��ü�����Ӧ��Ļ�Ĵ�С
*
*	˵    ��: 1. ��Ϊ����ͷ����Ļ�������̶�Ϊ4:3����һ��ƥ����ʾ��
*				 2. ��Ҫע����ǣ�����ͷ�����ͼ�񳤡�������Ҫ�ܱ�4�������� ʹ��OV2640_Set_Framesize������������ ��
*            3. DCMI��ˮƽ��Ч����Ҳ����Ҫ�ܱ�4������
*				 4. ���������ˮƽ�ʹ�ֱƫ�ƣ������û�����вü�
*****************************************************************************************************************************************/
int8_t OV2640_DCMI_Crop(uint16_t Displey_XSize,uint16_t Displey_YSize,uint16_t Sensor_XSize,uint16_t Sensor_YSize )
{
	uint16_t DCMI_X_Offset,DCMI_Y_Offset;	// ˮƽ�ʹ�ֱƫ�ƣ���ֱ��������������ˮƽ������������ʱ������PCLK��������
	uint16_t DCMI_CAPCNT;		// ˮƽ��Ч���أ�������������ʱ������PCLK��������
	uint16_t DCMI_VLINE;			// ��ֱ��Ч����

	if( (Displey_XSize>=Sensor_XSize)|| (Displey_YSize>=Sensor_YSize) )
	{
//		printf("ʵ����ʾ�ĳߴ���ڻ��������ͷ����ĳߴ磬�˳�DCMI�ü�\r\n");
		return OV2640_Error;  //���ʵ����ʾ�ĳߴ���ڻ��������ͷ����ĳߴ磬���˳���ǰ�����������вü�
	}
	
// ������ΪRGB565��ʽʱ��ˮƽƫ�ƣ�������������������ɫ�ʲ���ȷ��
// ��Ϊһ����Ч������2���ֽڣ���Ҫ2��PCLK���ڣ����Ա��������λ��ʼ����Ȼ���ݻ���ң�
// ��Ҫע����ǣ��Ĵ���ֵ�Ǵ�0��ʼ�����	��
	DCMI_X_Offset = Sensor_XSize - Displey_XSize; // ʵ�ʼ������Ϊ��Sensor_XSize - LCD_XSize��/2*2

// ���㴹ֱƫ�ƣ������û�����вü�����ֵ��������������	
	DCMI_Y_Offset = (Sensor_YSize - Displey_YSize)/2-1; // �Ĵ���ֵ�Ǵ�0��ʼ����ģ�����Ҫ-1

// ��Ϊһ����Ч������2���ֽڣ���Ҫ2��PCLK���ڣ�����Ҫ��2
// ���յõ��ļĴ���ֵ������Ҫ�ܱ�4������
	DCMI_CAPCNT = Displey_XSize*2-1;	// �Ĵ���ֵ�Ǵ�0��ʼ����ģ�����Ҫ-1
	
	DCMI_VLINE = Displey_YSize-1;		// ��ֱ��Ч����
	
//	printf("%d  %d  %d  %d\r\n",DCMI_X_Offset,DCMI_Y_Offset,DCMI_CAPCNT,DCMI_VLINE);
	
	HAL_DCMI_ConfigCrop (&hdcmi,DCMI_X_Offset,DCMI_Y_Offset,DCMI_CAPCNT,DCMI_VLINE);// ���òü�����
	HAL_DCMI_EnableCrop(&hdcmi);		// ʹ�ܲü�

	return OV2640_Success;	
}

/***************************************************************************************************************************************
*	�� �� ��: OV2640_Reset
*
*	��������: ִ��������λ
*
*	˵    ��: ������OV2640֮ǰ����Ҫִ��һ��������λ		 			          
*
*****************************************************************************************************************************************/
void OV2640_Reset(void)
{
	OV2640_Delay(5);  // �ȴ�ģ���ϵ��ȶ�������5ms��Ȼ������PWDN  	
	
	OV2640_PWDN_OFF;  // PWDN ��������͵�ƽ������������ģʽ������ͷ������������ʱ����ͷģ��İ�ɫLED�����
  
// ����OV2640���ϵ�ʱ��Ӳ����λ�ĳ���ʱ��Ҫ>=3ms��¹С���OV2640����Ӳ��RC��λ������ʱ������6ms����
// ��˼�����ʱ���ȴ�Ӳ����λ��ɲ��ȶ�����
	OV2640_Delay(5);    
	
	SCCB_WriteReg( OV2640_SEL_Registers, OV2640_SEL_SENSOR);   // ѡ�� SENSOR �Ĵ�����
	SCCB_WriteReg( OV2640_SENSOR_COM7, 0x80);                  // ����������λ

// ����OV2640��������λʱ��������λִ�к�Ҫ>=2ms����ִ��SCCB���ã��˴����ñ���һ��Ĳ�������ʱ10ms
	OV2640_Delay(10);    
}

/***************************************************************************************************************************************
*	�� �� ��: OV2640_ReadID
*
*	��������: ��ȡ OV2640 ������ID
*
*	˵    ��: ʵ�ʵ�����ID������ 0x2640 ���� 0x2642�����β�ͬID���ܻ᲻һ��		 
*
*****************************************************************************************************************************************/
uint16_t OV2640_ReadID(void)
{
   uint8_t PID_H,PID_L;     // ID����
	
	SCCB_WriteReg( OV2640_SEL_Registers, OV2640_SEL_SENSOR);   // ѡ�� SENSOR �Ĵ�����

   PID_H = SCCB_ReadReg(OV2640_SENSOR_PIDH); // ��ȡID���ֽ�
   PID_L = SCCB_ReadReg(OV2640_SENSOR_PIDL); // ��ȡID���ֽ�
	
	return(PID_H<<8)|PID_L; // ��������������ID
}

/***************************************************************************************************************************************
*	�� �� ��: OV2640_Config
*
*	��ڲ���:  (*ConfigData)[2] - Ҫ���õĲ�����������Ϊ OV2640_SVGA_Config �� OV2640_UXGA_Config
*
*	��������: ���� OV2640 ��������DSP����
*
*	˵    ��: 1. ������Ϊ SVGA ���� UXGAģʽ
*				 2. SVGA �ֱ���Ϊ800*600�����֧��30֡
*				 3. UXGA �ֱ���Ϊ1600*1200�����֧��15֡
*            4. ���������� dcmi_ov2640_cfg.h
*
*****************************************************************************************************************************************/
void OV2640_Config( const uint8_t (*ConfigData)[2] )
{
   uint32_t i; // ��������

	for( i=0;ConfigData[i][0] ; i++)
	{
		SCCB_WriteReg( ConfigData[i][0], ConfigData[i][1]);  // ���в�������   	
	} 
}

/***************************************************************************************************************************************
*	�� �� ��: OV2640_Set_Framesize
*
*	��ڲ���:  pixformat - ���ظ�ʽ����ѡ�� Pixformat_RGB565��Pixformat_JPEG
*
*	��������: ������������ظ�ʽ
*
*****************************************************************************************************************************************/
void OV2640_Set_Pixformat(uint8_t pixformat)
{
   const uint8_t (*ConfigData)[2];
   uint32_t i; // ��������

    switch (pixformat) 
    {
        case Pixformat_RGB565:
            ConfigData = OV2640_RGB565_Config;
            break;
        case Pixformat_JPEG:
            ConfigData = OV2640_JPEG_Config;
            break;
        default:  break;
    }

   for( i=0; ConfigData[i][0]; i++)
   {
      SCCB_WriteReg( ConfigData[i][0], ConfigData[i][1]);  // ���в�������   
   } 
}

/***************************************************************************************************************************************
*	�� �� ��: OV2640_Set_Framesize
*
*	��ڲ���:  width - ʵ�����ͼ��ĳ��ȣ�height - ʵ�����ͼ��Ŀ���
*
*	��������: ����ʵ�������ͼ���С
*
*	˵    ��: 1. OV2640����Ϊ SVGA��800*600�� ���� UXGA��1600*1200��ģʽ��ͼ���Сͨ����ʵ���õ���Ļ�ֱ��ʲ�һ����
*				    ��˿��Ե��ôκ���������ʵ�������ͼ���С
*				 2. ��Ҫע����ǣ�Ҫ���õ�ͼ�񳤡��������ܱ�4������
*            3. ���������������ͼ��ֱ���ԽС֡�ʾ�Խ�ߣ�֡��ֻ�����õ�ģʽ�йأ���������ΪSVGA���ֻ��֧��30֡
*
*****************************************************************************************************************************************/
int8_t OV2640_Set_Framesize(uint16_t width,uint16_t height)
{
	if( (width%4)||(height%4) )   // ���ͼ��Ĵ�Сһ��Ҫ�ܱ�4����
   {
       return OV2640_Error;  // ���ش����־
   }
	 
	SCCB_WriteReg(OV2640_SEL_Registers,OV2640_SEL_DSP);	// ѡ�� DSP�Ĵ�����

	SCCB_WriteReg(0X5A, width/4  &0XFF);		// ʵ��ͼ������Ŀ��ȣ�OUTW����7~0 bit���Ĵ�����ֵ����ʵ��ֵ/4
	SCCB_WriteReg(0X5B, height/4 &0XFF);		// ʵ��ͼ������ĸ߶ȣ�OUTH����7~0 bit���Ĵ�����ֵ����ʵ��ֵ/4
	SCCB_WriteReg(0X5C, (width/4>>8&0X03)|(height/4>>6&0x04) );	 // ����ZMHH��Bit[2:0]��Ҳ����OUTH �ĵ� 8 bit��OUTW �ĵ� 9~8 bit��

	SCCB_WriteReg(OV2640_DSP_RESET,0X00);	   // ��λ

	return OV2640_Success;  // �ɹ�
}

/***************************************************************************************************************************************
*	�� �� ��: OV2640_Set_Horizontal_Mirror
*
*	��ڲ���:  ConfigState - ��1ʱ��ͼ���ˮƽ������0ʱ�ָ�����
*
*	��������: �������������ͼ���Ƿ����ˮƽ����
*
*****************************************************************************************************************************************/
int8_t OV2640_Set_Horizontal_Mirror( int8_t ConfigState )
{
   uint8_t OV2640_Reg;  // �Ĵ�����ֵ

   SCCB_WriteReg(OV2640_SEL_Registers,OV2640_SEL_SENSOR);	// ѡ�� SENSOR �Ĵ�����
   OV2640_Reg = SCCB_ReadReg(OV2640_SENSOR_REG04);          // ��ȡ 0x04 �ļĴ���ֵ

// REG04,�Ĵ�����4���Ĵ�����ַΪ 0x04���üĴ�����Bit[7]����������ˮƽ�Ƿ���
   if ( ConfigState == OV2640_Enable )    // ���ʹ�ܾ���
   { 
      OV2640_Reg |= 0X80;  // Bit[7]��1����
   } 
   else                    // ȡ������
   {
      OV2640_Reg &= ~0X80; // Bit[7]��0��������ģʽ
   }
   return  SCCB_WriteReg(OV2640_SENSOR_REG04,OV2640_Reg);   // д��Ĵ���
}

/***************************************************************************************************************************************
*	�� �� ��: OV2640_Set_Vertical_Flip
*
*	��ڲ���:  ConfigState - ��1ʱ��ͼ��ᴹֱ��ת����0ʱ�ָ�����
*
*	��������: �������������ͼ���Ƿ���д�ֱ��ת
*
*****************************************************************************************************************************************/
int8_t OV2640_Set_Vertical_Flip( int8_t ConfigState )
{
   uint8_t OV2640_Reg;  // �Ĵ�����ֵ

   SCCB_WriteReg(OV2640_SEL_Registers,OV2640_SEL_SENSOR);	// ѡ�� SENSOR �Ĵ�����
   OV2640_Reg = SCCB_ReadReg(OV2640_SENSOR_REG04);          // ��ȡ 0x04 �ļĴ���ֵ

// REG04,�Ĵ�����4���Ĵ�����ַΪ 0x04���üĴ����ĵ�Bit[6]����������ˮƽ�Ǵ�ֱ��ת
   if ( ConfigState == OV2640_Enable )   
   { 
      // �˴����òο�OpenMV������
      // Bit[4]�����������ʲô�ֲ�û��˵�������ֱ��ת֮�󣬸�λ����1�Ļ�����ɫ�᲻��
      OV2640_Reg |= 0X40|0x10 ;     // Bit[6]��1ʱ��ͼ��ᴹֱ��ת
   } 
   else   // ȡ����ת
   {
      OV2640_Reg &= ~(0X40|0x10 ); // ��Bit[6]��Bit[4]��д0
   }
   return  SCCB_WriteReg(OV2640_SENSOR_REG04,OV2640_Reg);   // д��Ĵ���
}

/***************************************************************************************************************************************
*	�� �� ��: OV2640_Set_Saturation
*
*	��ڲ���:  Saturation - ���Ͷȣ�������Ϊ5���ȼ���2��1��0��-1��-2                   
*
*	˵    ��: 1. �ֲ���û��˵�������е���2���Ĵ������ʹ�ã��������ֱ��ʹ��OV2640����ֲ�����Ĵ���
*            2.���Ͷ�Խ�ߣ�ɫ�ʾ�Խ���ޣ�������Ӧ�������Ȼ��½��������
*
*****************************************************************************************************************************************/
void OV2640_Set_Saturation(int8_t Saturation)
{
	SCCB_WriteReg(OV2640_SEL_Registers,OV2640_SEL_DSP);	// ѡ�� DSP�Ĵ�����

   switch (Saturation)
   {
      case 2:      
         SCCB_WriteReg(OV2640_DSP_BPADDR,0x00);	   
         SCCB_WriteReg(OV2640_DSP_BPDATA,0x02);	
         SCCB_WriteReg(OV2640_DSP_BPADDR,0x03);	   
         SCCB_WriteReg(OV2640_DSP_BPDATA,0x68);	 
         SCCB_WriteReg(OV2640_DSP_BPDATA,0x68);	         
      break;
      
      case 1:    
         SCCB_WriteReg(OV2640_DSP_BPADDR,0x00);	
         SCCB_WriteReg(OV2640_DSP_BPDATA,0x02);	
         SCCB_WriteReg(OV2640_DSP_BPADDR,0x03);	 
         SCCB_WriteReg(OV2640_DSP_BPDATA,0x58);	 
         SCCB_WriteReg(OV2640_DSP_BPDATA,0x58);	          
      break;

      case 0:         
         SCCB_WriteReg(OV2640_DSP_BPADDR,0x00);	
         SCCB_WriteReg(OV2640_DSP_BPDATA,0x02);	
         SCCB_WriteReg(OV2640_DSP_BPADDR,0x03);	 
         SCCB_WriteReg(OV2640_DSP_BPDATA,0x48);	 
         SCCB_WriteReg(OV2640_DSP_BPDATA,0x48);	       
      break;

      case -1: 
         SCCB_WriteReg(OV2640_DSP_BPADDR,0x00);	
         SCCB_WriteReg(OV2640_DSP_BPDATA,0x02);	
         SCCB_WriteReg(OV2640_DSP_BPADDR,0x03);	 
         SCCB_WriteReg(OV2640_DSP_BPDATA,0x38);	 
         SCCB_WriteReg(OV2640_DSP_BPDATA,0x38);	       
      break;

      case -2: 
         SCCB_WriteReg(OV2640_DSP_BPADDR,0x00);	
         SCCB_WriteReg(OV2640_DSP_BPDATA,0x02);	
         SCCB_WriteReg(OV2640_DSP_BPADDR,0x03);	 
         SCCB_WriteReg(OV2640_DSP_BPDATA,0x28);	 
         SCCB_WriteReg(OV2640_DSP_BPDATA,0x28);	       
      break;

      default: break;  
   }
}

/***************************************************************************************************************************************
*	�� �� ��: OV2640_Set_Brightness
*
*	��ڲ���:  Brightness - ���ȣ�������Ϊ5���ȼ���2��1��0��-1��-2                   
*
*	˵    ��: 1. �ֲ���û��˵�������е���2���Ĵ������ʹ�ã��������ֱ��ʹ��OV2640����ֲ�����Ĵ���
*           2. ����Խ�ߣ������Խ���������ǻ��ģ��һЩ
*
*****************************************************************************************************************************************/
void OV2640_Set_Brightness(int8_t Brightness)
{
	SCCB_WriteReg(OV2640_SEL_Registers,OV2640_SEL_DSP);	// ѡ�� DSP�Ĵ�����

   switch (Brightness)
   {
      case 2:      
         SCCB_WriteReg(OV2640_DSP_BPADDR,0x00);	   
         SCCB_WriteReg(OV2640_DSP_BPDATA,0x04);	
         SCCB_WriteReg(OV2640_DSP_BPADDR,0x09);	   
         SCCB_WriteReg(OV2640_DSP_BPDATA,0x40);	 
         SCCB_WriteReg(OV2640_DSP_BPDATA,0x00);	         
      break;
      
      case 1:    
         SCCB_WriteReg(OV2640_DSP_BPADDR,0x00);	   
         SCCB_WriteReg(OV2640_DSP_BPDATA,0x04);	
         SCCB_WriteReg(OV2640_DSP_BPADDR,0x09);	   
         SCCB_WriteReg(OV2640_DSP_BPDATA,0x30);	 
         SCCB_WriteReg(OV2640_DSP_BPDATA,0x00);	            
      break;

      case 0:         
         SCCB_WriteReg(OV2640_DSP_BPADDR,0x00);	   
         SCCB_WriteReg(OV2640_DSP_BPDATA,0x04);	
         SCCB_WriteReg(OV2640_DSP_BPADDR,0x09);	   
         SCCB_WriteReg(OV2640_DSP_BPDATA,0x20);	 
         SCCB_WriteReg(OV2640_DSP_BPDATA,0x00);	       
      break;

      case -1: 
         SCCB_WriteReg(OV2640_DSP_BPADDR,0x00);	   
         SCCB_WriteReg(OV2640_DSP_BPDATA,0x04);	
         SCCB_WriteReg(OV2640_DSP_BPADDR,0x09);	   
         SCCB_WriteReg(OV2640_DSP_BPDATA,0x10);	 
         SCCB_WriteReg(OV2640_DSP_BPDATA,0x00);	          
      break;

      case -2: 
         SCCB_WriteReg(OV2640_DSP_BPADDR,0x00);	   
         SCCB_WriteReg(OV2640_DSP_BPDATA,0x04);	
         SCCB_WriteReg(OV2640_DSP_BPADDR,0x09);	   
         SCCB_WriteReg(OV2640_DSP_BPDATA,0x00);	 
         SCCB_WriteReg(OV2640_DSP_BPDATA,0x00);	      
      break;

      default: break;  
   }
}
/***************************************************************************************************************************************
*	�� �� ��: OV2640_Set_Contrast
*
*	��ڲ���: Contrast - �Աȶȣ�������Ϊ5���ȼ���2��1��0��-1��-2                   
*
*	˵    ��: 1. �ֲ���û��˵�������е���2���Ĵ������ʹ�ã��������ֱ��ʹ��OV2640����ֲ�����Ĵ���
*            2. �Աȶ�Խ�ߣ�����Խ�������ڰ�Խ�ӷ���
*
*****************************************************************************************************************************************/
void OV2640_Set_Contrast(int8_t Contrast)
{
	SCCB_WriteReg(OV2640_SEL_Registers,OV2640_SEL_DSP);	// ѡ�� DSP�Ĵ�����

   switch (Contrast)
   {
      case 2:      
         SCCB_WriteReg(OV2640_DSP_BPADDR,0x00);	   
         SCCB_WriteReg(OV2640_DSP_BPDATA,0x04);	
         SCCB_WriteReg(OV2640_DSP_BPADDR,0x07);	   
         SCCB_WriteReg(OV2640_DSP_BPDATA,0x20);         
         SCCB_WriteReg(OV2640_DSP_BPADDR,0x28);	   
         SCCB_WriteReg(OV2640_DSP_BPDATA,0x0c);	 
         SCCB_WriteReg(OV2640_DSP_BPDATA,0x06);	         
      break;
      
      case 1:    
         SCCB_WriteReg(OV2640_DSP_BPADDR,0x00);	   
         SCCB_WriteReg(OV2640_DSP_BPDATA,0x04);	
         SCCB_WriteReg(OV2640_DSP_BPADDR,0x07);	   
         SCCB_WriteReg(OV2640_DSP_BPDATA,0x20);         
         SCCB_WriteReg(OV2640_DSP_BPADDR,0x24);	   
         SCCB_WriteReg(OV2640_DSP_BPDATA,0x16);	 
         SCCB_WriteReg(OV2640_DSP_BPDATA,0x06);	             
      break;

      case 0:         
         SCCB_WriteReg(OV2640_DSP_BPADDR,0x00);	   
         SCCB_WriteReg(OV2640_DSP_BPDATA,0x04);	
         SCCB_WriteReg(OV2640_DSP_BPADDR,0x07);	   
         SCCB_WriteReg(OV2640_DSP_BPDATA,0x20);         
         SCCB_WriteReg(OV2640_DSP_BPADDR,0x20);	   
         SCCB_WriteReg(OV2640_DSP_BPDATA,0x20);	 
         SCCB_WriteReg(OV2640_DSP_BPDATA,0x06);	         
      break;

      case -1: 
         SCCB_WriteReg(OV2640_DSP_BPADDR,0x00);	   
         SCCB_WriteReg(OV2640_DSP_BPDATA,0x04);	
         SCCB_WriteReg(OV2640_DSP_BPADDR,0x07);	   
         SCCB_WriteReg(OV2640_DSP_BPDATA,0x20);         
         SCCB_WriteReg(OV2640_DSP_BPADDR,0x1c);	   
         SCCB_WriteReg(OV2640_DSP_BPDATA,0x2a);	 
         SCCB_WriteReg(OV2640_DSP_BPDATA,0x06);	              
      break;

      case -2: 
         SCCB_WriteReg(OV2640_DSP_BPADDR,0x00);	   
         SCCB_WriteReg(OV2640_DSP_BPDATA,0x04);	
         SCCB_WriteReg(OV2640_DSP_BPADDR,0x07);	   
         SCCB_WriteReg(OV2640_DSP_BPDATA,0x20);         
         SCCB_WriteReg(OV2640_DSP_BPADDR,0x18);	   
         SCCB_WriteReg(OV2640_DSP_BPDATA,0x34);	 
         SCCB_WriteReg(OV2640_DSP_BPDATA,0x06);	         
      break;

      default: break;  
   }
}
/***************************************************************************************************************************************
*	�� �� ��: OV2640_Set_Effect
*
*	��ڲ���:  effect_Mode - ��Чģʽ����ѡ����� OV2640_Effect_Normal��OV2640_Effect_Negative��
*                          OV2640_Effect_BW��OV2640_Effect_BW_Negative
*
*	��������: ��������OV2640����Ч����������Ƭ���ڰס��ڰ�+��Ƭ��ģʽ
*
*	˵    ��: �ֲ���û��˵�������е���2���Ĵ������ʹ�ã��������ֱ��ʹ��OV2640����ֲ�����Ĵ���
*
*****************************************************************************************************************************************/
void OV2640_Set_Effect(uint8_t effect_Mode)
{
	SCCB_WriteReg(OV2640_SEL_Registers,OV2640_SEL_DSP);	// ѡ�� DSP�Ĵ�����

   switch (effect_Mode)
   {
      case OV2640_Effect_Normal:       // ����ģʽ
         SCCB_WriteReg(OV2640_DSP_BPADDR,0x00);	
         SCCB_WriteReg(OV2640_DSP_BPDATA,0x00);	
         SCCB_WriteReg(OV2640_DSP_BPADDR,0x05);	   
         SCCB_WriteReg(OV2640_DSP_BPDATA,0x80);	 
         SCCB_WriteReg(OV2640_DSP_BPDATA,0x80);	         
      break;
      
      case OV2640_Effect_Negative:     // ��Ƭģʽ��Ҳ������ɫȫ��ȡ��
         SCCB_WriteReg(OV2640_DSP_BPADDR,0x00);	
         SCCB_WriteReg(OV2640_DSP_BPDATA,0x40);	
         SCCB_WriteReg(OV2640_DSP_BPADDR,0x05);	   
         SCCB_WriteReg(OV2640_DSP_BPDATA,0x80);	 
         SCCB_WriteReg(OV2640_DSP_BPDATA,0x80);	          
      break;

      case OV2640_Effect_BW:          // �ڰ�ģʽ
         SCCB_WriteReg(OV2640_DSP_BPADDR,0x00);	
         SCCB_WriteReg(OV2640_DSP_BPDATA,0x18);	
         SCCB_WriteReg(OV2640_DSP_BPADDR,0x05);	   
         SCCB_WriteReg(OV2640_DSP_BPDATA,0x80);	 
         SCCB_WriteReg(OV2640_DSP_BPDATA,0x80);	       
      break;

      case OV2640_Effect_BW_Negative: // �ڰ�+��Ƭģʽ
         SCCB_WriteReg(OV2640_DSP_BPADDR,0x00);	
         SCCB_WriteReg(OV2640_DSP_BPDATA,0x58);	
         SCCB_WriteReg(OV2640_DSP_BPADDR,0x05);	   
         SCCB_WriteReg(OV2640_DSP_BPDATA,0x80);	 
         SCCB_WriteReg(OV2640_DSP_BPDATA,0x80);	       
      break;

      default: break;  
   }
}

