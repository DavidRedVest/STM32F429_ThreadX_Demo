#include "bsp_uart.h"


#include "rtthread.h"

UART_HandleTypeDef huart1;


T_UART_Handle g_tUART1_Handle = {0};



void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    //GPIO端口设置
	GPIO_InitTypeDef GPIO_Initure;
	
	if(huart->Instance==USART1)//如果是串口1，进行串口1 MSP初始化
	{
		__HAL_RCC_GPIOA_CLK_ENABLE();			//使能GPIOA时钟
		__HAL_RCC_USART1_CLK_ENABLE();			//使能USART1时钟
	
		GPIO_Initure.Pin=GPIO_PIN_9;			//PA9
		GPIO_Initure.Mode=GPIO_MODE_AF_PP;		//复用推挽输出
		GPIO_Initure.Pull=GPIO_PULLUP;			//上拉
		GPIO_Initure.Speed=GPIO_SPEED_FREQ_HIGH;		//高速
		GPIO_Initure.Alternate=GPIO_AF7_USART1;	//复用为USART1
		HAL_GPIO_Init(GPIOA,&GPIO_Initure);	   	//初始化PA9

		GPIO_Initure.Pin=GPIO_PIN_10;			//PA10
		HAL_GPIO_Init(GPIOA,&GPIO_Initure);	   	//初始化PA10
		
#if EN_USART1_RX
		HAL_NVIC_EnableIRQ(USART1_IRQn);		//使能USART1中断通道
		HAL_NVIC_SetPriority(USART1_IRQn,3,3);	//抢占优先级3，子优先级3
#endif	
	}

}



int myputchar(const char ch)
{
	while((USART1->SR&0X40)==0);//循环发送,直到发送完毕   
	USART1->DR = (u8) ch;      
	return ch;	
}

void myputstr(const char *str)
{
	while (*str)
	{
		myputchar(*str);
		str++;
	}
}

void my_uart_init(u32 bound)
{
//GPIO端口设置
	GPIO_InitTypeDef GPIO_Initure;

	__HAL_RCC_GPIOA_CLK_ENABLE();			//使能GPIOA时钟
	__HAL_RCC_USART1_CLK_ENABLE();			//使能USART1时钟
	
	GPIO_Initure.Pin=GPIO_PIN_9;			//PA9
	GPIO_Initure.Mode=GPIO_MODE_AF_PP;		//复用推挽输出
	GPIO_Initure.Pull=GPIO_PULLUP;			//上拉
	GPIO_Initure.Speed=GPIO_SPEED_FREQ_HIGH;		//高速
	GPIO_Initure.Alternate=GPIO_AF7_USART1;	//复用为USART1
	HAL_GPIO_Init(GPIOA,&GPIO_Initure);	   	//初始化PA9

	GPIO_Initure.Pin=GPIO_PIN_10;			//PA10
	HAL_GPIO_Init(GPIOA,&GPIO_Initure);	   	//初始化PA10	

#if 1
	HAL_NVIC_EnableIRQ(USART1_IRQn);		//使能USART1中断通道
	HAL_NVIC_SetPriority(USART1_IRQn,3,3);	//抢占优先级3，子优先级3
#endif

	//UART 初始化设置
	huart1.Instance=USART1;					    //USART1
	huart1.Init.BaudRate=bound;				    //波特率
	huart1.Init.WordLength=UART_WORDLENGTH_8B;   //字长为8位数据格式
	huart1.Init.StopBits=UART_STOPBITS_1;	    //一个停止位
	huart1.Init.Parity=UART_PARITY_NONE;		    //无奇偶校验位
	huart1.Init.HwFlowCtl=UART_HWCONTROL_NONE;   //无硬件流控
	huart1.Init.Mode=UART_MODE_TX_RX;		    //收发模式
	HAL_UART_Init(&huart1);					    //HAL_UART_Init()会使能UART1

//	HAL_UART_Receive_IT(&huart1, (u8 *)aRxBuffer, RXBUFFERSIZE);//该函数会开启接收中断：标志位UART_IT_RXNE，并且设置接收缓冲以及接收缓冲接收最大数据量
//	不用HAL_UART_Receive_IT():它会把 huart1 切到 HAL_UART_STATE_BUSY_RX 并接管一套
//	自己的 pRxBuffPtr/RxXferCount 缓冲逻辑，跟 USART1_IRQHandler 里直接读DR、手写
//	到 g_tUART1_Handle 的方式冲突。只开 RXNE 中断位,中断处理仍然全部走下面手写的
//	USART1_IRQHandler。之前一直没开这个位,USART1 收不到任何数据,list/led_set等
//	串口指令全都没反应。
	__HAL_UART_ENABLE_IT(&huart1, UART_IT_RXNE);

}


/* 中断处理函数 */
void USART1_IRQHandler(void)                	
{
	uint8_t u8UartData;
	if( ( __HAL_UART_GET_FLAG(&huart1, UART_FLAG_RXNE) != RESET ) && ( __HAL_UART_GET_IT_SOURCE(&huart1, UART_IT_RXNE) != RESET ) )
	{
		u8UartData = (uint8_t)(huart1.Instance->DR & 0xFF);	//直接读DR，读DR会自动清RXNE，不用阻塞等HAL_UART_Receive
		if( 0 == (g_tUART1_Handle.u16Uart_Rx_Sta & 0x8000) )	//接收未完成
		{
			if( g_tUART1_Handle.u16Uart_Rx_Sta & 0x4000 )	//接收到了0x0d
			{
				if( 0x0a != u8UartData )
				{
					g_tUART1_Handle.u16Uart_Rx_Sta = 0;	/* 接收错误，重新开始 */
				}
				else
				{
					g_tUART1_Handle.u16Uart_Rx_Sta |= 0x8000;	//接收完成了
				}
			}
			else	//还没接收到0x0d
			{
				if( 0x0d == u8UartData )
				{
					g_tUART1_Handle.u16Uart_Rx_Sta |= 0x4000;
				}
				else
				{
					g_tUART1_Handle.u8Uart_Hex_Buffer[g_tUART1_Handle.u16Uart_Rx_Sta & 0x3FFF] = u8UartData;
					g_tUART1_Handle.u16Uart_Rx_Sta ++;
					if( g_tUART1_Handle.u16Uart_Rx_Sta > (UART_MAX_REC_LEN -1) )
					{
						g_tUART1_Handle.u16Uart_Rx_Sta = 0;	/* 接收错误，重新开始 */
					}

				}
			}
		}
	}	
	HAL_UART_IRQHandler(&huart1);
}