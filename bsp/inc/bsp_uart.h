#ifndef BSP_UART_H_
#define BSP_UART_H_

#include "main.h"

#define USART_REC_LEN  			200  	//定义最大接收字节数 200
#define EN_USART1_RX 			1		//使能（1）/禁止（0）串口1接收
//定义串口最大接收字节数
#define UART_MAX_REC_LEN	(200)

//接收状态 uint16_t u16Uart_Rx_Sta;
//bit15，	接收完成标志
//bit14，	接收到0x0d
//bit13~0，	接收到的有效字节数目
typedef struct _uart_handle {
	union {
		uint8_t u8Uart_Hex_Buffer[UART_MAX_REC_LEN];
		char	s8Uart_Str_Buffer[UART_MAX_REC_LEN];
	};
	uint16_t u16Uart_Rx_Sta;
	
}T_UART_Handle,*PT_UART_Handle;

extern T_UART_Handle g_tUART1_Handle;
extern UART_HandleTypeDef huart1;

//extern u8  USART_RX_BUF[USART_REC_LEN]; //接收缓冲,最大USART_REC_LEN个字节.末字节为换行符 
//extern u16 USART_RX_STA;         		//接收状态标记	
//extern UART_HandleTypeDef UART1_Handler; //UART句柄

//#define RXBUFFERSIZE   1 //缓存大小


void uart_init(u32 bound);
void HAL_UART_MspInit(UART_HandleTypeDef *huart);


int myputchar(const char ch);
void myputstr(const char *str);

void my_uart_init(u32 bound);


#endif
