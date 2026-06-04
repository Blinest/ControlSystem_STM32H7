#ifndef __DCMI_OV2640_H
#define __DCMI_OV2640_H

#include "stm32h7xx_hal.h"
#include "dcmi.h"
#include "sccb.h"
#include "usart.h"
#include "lcd_spi_154.h"

 // DCMI状态标志位，当完整一帧数据被抓取时，会被 HAL_DCMI_FrameEventCallback() 中断回调函数置 1
// DCMI_FrameState 在 dcmi.h / dcmi.c 中定义
#define OV2640_FPS  DCMI_FPS  // 映射到 dcmi.c 中的公共变量

#define  OV2640_Success   0            // ͨѶ�ɹ���־
#define  OV2640_Error     -1           // ͨѶ����

#define  OV2640_Enable    1
#define  OV2640_Disable   0

// ������������ĸ�ʽ���� OV2640_Set_Pixformat() ����
#define Pixformat_RGB565   0
#define Pixformat_JPEG     1

// OV2640����Чģʽ���� OV2640_Set_Effect() ����
#define  OV2640_Effect_Normal       0  // ����ģʽ
#define  OV2640_Effect_Negative     1  // ��Ƭģʽ��Ҳ������ɫȫ��ȡ��
#define  OV2640_Effect_BW           2  // �ڰ�ģʽ
#define  OV2640_Effect_BW_Negative  3  // �ڰ�ģʽ+��Ƭģʽ

// 1. ����OV2640ʵ�������ͼ���С�����Ը���ʵ�ʵ�Ӧ�û�����ʾ�����е�����ͬʱҲҪ�޸����ò������ʱ�ӷ�Ƶ��
// 2. ��������������Ӱ��֡�ʣ��Ҳ��ܳ�����Ӧģʽ�����ߴ�
// 3. SVGAģʽ�£����ͼ�����ֱ���Ϊ 800*600,  ���֡��30֡
// 4. UXGAģʽ�£����ͼ�����ֱ���Ϊ 1600*1200,���֡��15֡
// 5. Ҫ���õ�ͼ�񳤡��������ܱ�4������
// 6. Ҫ���õ�ͼ�񳤡����ȱ�������4:3����Ȼ����ᱻ�������
#define OV2640_Width          400   // ͼ�񳤶� 
#define OV2640_Height         300  // ͼ�����

// 1. ����Ҫ��ʾ�Ļ����С����ֵһ��Ҫ�ܱ�4��������
// 2. RGB565��ʽ�£����ջ���DCMI��OV2640�����4:3ͼ��ü�Ϊ��Ӧ��Ļ�ı���
// 3. �˴��ķֱ��ʲ��ܳ��� OV2640_Width �� OV2640_Height
// 4. �ֱ���̫��ʱ����Ҫ�޸�PCLK��ʱ���ٶȣ���ϸ����˵���ɲο� dcmi_ov2640_cfg.h ��� 0xd3 �Ĵ�������
#define	Display_Width			 LCD_Width
#define	Display_Height			 LCD_Height

// 1.RGB565ģʽ�£���Ҫ ͼ��ֱ���*2 �Ĵ�С
// 2.JPGģʽ�£���Ҫ�Ļ�������С�����ǹ̶��ģ����� 640*480�ֱ��ʣ�JPGͼ����Ҫռ30K��
//   ������Ԥ��2�����Ҵ�С���ɣ��û��ɸ���ʵ�����ȥ����,
#define 	OV2640_BufferSize     Display_Width * Display_Height*2 /4   // DMA�������ݴ�С��32λ����
//#define 	OV2640_BufferSize     	100*1024/4   // DMA�������ݴ�С��32λ����

#define  OV2640_SEL_Registers       0xFF	// �Ĵ�����ѡ��Ĵ���
#define  OV2640_SEL_DSP             0x00	// ����Ϊ0x00ʱ��ѡ��  DSP    �Ĵ�����
#define  OV2640_SEL_SENSOR          0x01	// ����Ϊ0x01ʱ��ѡ��  SENSOR �Ĵ�����


// DSP �Ĵ����� (0xFF = 0x00) 
#define 	OV2640_DSP_RESET           0xE0	// ��ѡ��λ ��������SCCB��Ԫ��JPEG��Ԫ��DVP�ӿڵ�Ԫ��
#define 	OV2640_DSP_BPADDR          0x7C	// ��ӼĴ�������:��ַ
#define 	OV2640_DSP_BPDATA          0x7D	// ��ӼĴ�������:����

// SENSOR �Ĵ����� (0xFF = 0x01) 
#define 	OV2640_SENSOR_COM7         0x12	// ��������,ϵͳ��λ������ͷ�ֱ���ѡ������ģʽ����ɫ�������� 
#define 	OV2640_SENSOR_REG04        0x04	// �Ĵ�����4,����������ͷɨ�跽���
#define  OV2640_SENSOR_PIDH         0x0a	// ID���ֽ�
#define  OV2640_SENSOR_PIDL         0x0b	// ID���ֽ�

/*------------------------------------------------------------ �������� ------------------------------------------------*/

int8_t   DCMI_OV2640_Init(void);	// ��ʼSCCB��DCMI��DMA�Լ�����OV2640

void     OV2640_DMA_Transmit_Continuous(uint32_t DMA_Buffer,uint32_t DMA_BufferSize);	// ����DMA���䣬����ģʽ
void     OV2640_DMA_Transmit_Snapshot(uint32_t DMA_Buffer,uint32_t DMA_BufferSize);		//  ����DMA���䣬����ģʽ������һ֡ͼ���ֹͣ
void     OV2640_DCMI_Suspend(void);		// ����DCMI��ֹͣ��������
void     OV2640_DCMI_Resume(void);		// �ָ�DCMI����ʼ��������
void     OV2640_DCMI_Stop(void);			// ��ֹDCMI��DMA����ֹͣDCMI���񣬽�ֹDCMI����
int8_t 	OV2640_DCMI_Crop(uint16_t Displey_XSize,uint16_t Displey_YSize,uint16_t Sensor_XSize,uint16_t Sensor_YSize );	// �ü�����

void     OV2640_Reset(void);				//	ִ��������λ		
uint16_t OV2640_ReadID(void);				// ��ȡ����ID
void     OV2640_Config( const uint8_t (*ConfigData)[2] );		// ���ø������
void     OV2640_Set_Pixformat(uint8_t pixformat);					// ����ͼ�������ʽ
int8_t   OV2640_Set_Framesize(uint16_t width,uint16_t height);	// ����ʵ�������ͼ���С
int8_t   OV2640_Set_Horizontal_Mirror( int8_t ConfigState );	// �������������ͼ���Ƿ����ˮƽ����
int8_t   OV2640_Set_Vertical_Flip( int8_t ConfigState );			//	�������������ͼ���Ƿ���д�ֱ��ת 
void     OV2640_Set_Saturation(int8_t Saturation);					// ���ñ��Ͷ�
void     OV2640_Set_Brightness(int8_t Brightness);					// ��������
void     OV2640_Set_Contrast(int8_t Contrast);						// ���öԱȶ�
void     OV2640_Set_Effect(uint8_t effect_Mode );					// ����������Ч����������Ƭ���ڰס��ڰ�+��Ƭ��ģʽ

/*-------------------------------------------------------------- �������ú� ---------------------------------------------*/

#define OV2640_PWDN_PIN            			 GPIO_PIN_13        				 	// PWDN ����      
#define OV2640_PWDN_PORT           			 GPIOF                 			 	// PWDN GPIO�˿�     
#define GPIO_OV2640_PWDN_CLK_ENABLE    	__HAL_RCC_GPIOF_CLK_ENABLE() 		// PWDN GPIO�˿�ʱ��

// �͵�ƽ������������ģʽ������ͷ��������
#define	OV2640_PWDN_OFF	HAL_GPIO_WritePin(OV2640_PWDN_PORT, OV2640_PWDN_PIN, GPIO_PIN_RESET)	

// �ߵ�ƽ���������ģʽ������ͷֹͣ��������ʱ���Ľ������
#define 	OV2640_PWDN_ON		HAL_GPIO_WritePin(OV2640_PWDN_PORT, OV2640_PWDN_PIN, GPIO_PIN_SET)	
  
 
 
#endif //__DCMI_OV2640_H




