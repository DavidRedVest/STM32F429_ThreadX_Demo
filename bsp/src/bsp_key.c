#include "bsp_key.h"

/*
 * 正点原子STM32F429IGT6 阿波罗开发板
 * KEY0 按键		:	PH3	低电平有效
 * KEY1 按键		:	PH2 低电平有效
 * KEY2 按键		:	PC13 低电平有效
 * WK_UP 按键		:	PA0 高电平有效
*/

#define HARD_KEY_NUM    (4)             /*.实体按键个数 */
#define KEY_COUNT       (HARD_KEY_NUM + 0)      /*.4个独立按键+ 组合按键 */

/* 使能GPIO时钟 */
#define ALL_KEY_GPIO_CLK_ENABLE() {\
    __HAL_RCC_GPIOA_CLK_ENABLE();	\
    __HAL_RCC_GPIOC_CLK_ENABLE();	\
	__HAL_RCC_GPIOF_CLK_ENABLE();	\
	__HAL_RCC_GPIOG_CLK_ENABLE();	\
	__HAL_RCC_GPIOH_CLK_ENABLE();	\
	__HAL_RCC_GPIOI_CLK_ENABLE();	\
};

/* 依次定义GPIO */
typedef struct
{
	GPIO_TypeDef *gpio;
	uint16_t pin;
	uint8_t  ActiveLevel;	/* 激活电平 */
}X_GPIO_T;

/* GPIO和PIN定义 */
static const X_GPIO_T s_gpio_list[HARD_KEY_NUM] = {
	{GPIOH, GPIO_PIN_3, 0},		/*.KEY0 */
	{GPIOH, GPIO_PIN_2, 0},		/*.KEY1 */
	{GPIOC, GPIO_PIN_13, 0},	/*.KEY2 */
	{GPIOA, GPIO_PIN_0, 1},		/*.WK_UP */
};

/* 定义一个宏函数简化后续代码 
	判断GPIO引脚是否有效按下
*/
static KEY_T s_tBtn[KEY_COUNT] = {0};
static KEY_FIFO_T s_tKey;	/* 按键FIFO变量,结构体 */

/* 独立的按键扫描定时器(TIM7,基本定时器,项目里未被 core/ 的 HAL 时基(TIM6)占用),
   10ms 周期中断里直接调用 bsp_KeyScan10ms()。之所以不复用 TIM6:
   TIM6 从 HAL_Init() 里就开始跑(core/src/stm32f4xx_hal_timebase_tim.c),
   比 app_init() 里 bsp_InitKey() 的调用早得多,若挂在 TIM6 上会在按键变量/
   GPIO 还没初始化完成前就抢先扫描。改用独立的 TIM7,只在 bsp_InitKey() 之后
   才由 bsp_KeyTimerInit() 显式使能,天然保证了先初始化、后扫描的顺序。 */
static TIM_HandleTypeDef s_htim7;

static void bsp_InitKeyVar(void);
static void bsp_InitKeyHard(void);
static void bsp_DetectKey(uint8_t i);

/*
*********************************************************************************************************
*	函 数 名: KeyPinActive
*	功能说明: 判断按键是否按下
*	形    参: 无
*	返 回 值: 返回值1 表示按下(导通），0表示未按下（释放）
*********************************************************************************************************
*/
static uint8_t KeyPinActive(uint8_t _id)
{
	uint8_t level;
	if( 0 == (s_gpio_list[_id].gpio->IDR & s_gpio_list[_id].pin) ) {
		level = 0;
	}
	else {
		level = 1;
	}
	if(level == s_gpio_list[_id].ActiveLevel) {
		return 1;
	} else {
		return 0;
	}
}

/*
*********************************************************************************************************
*	函 数 名: IsKeyDownFunc
*	功能说明: 判断按键是否按下。单键和组合键区分。单键事件不允许有其他键按下。
*	形    参: 无
*	返 回 值: 返回值1 表示按下(导通），0表示未按下（释放）
*********************************************************************************************************
*/
static uint8_t IsKeyDownFunc(uint8_t _id)
{
	/* 实体按键 */
	if(_id< HARD_KEY_NUM) {
		uint8_t i;
		uint8_t count = 0;
		uint8_t save = 255;

		/* 判断有几个按键按下 */
		for(i=0;i<HARD_KEY_NUM; i++) {
			if(KeyPinActive(i)) {
				count++;
				save = i;
			}
		}
		if(1 == count && save == _id) {
			return 1;	/* 只有一个按键按下时才有效 */
		}
		return 0;
	}

	/* 组合按键 这里没有,省略 */

	return 0;
}

/*
*********************************************************************************************************
*	函 数 名: bsp_InitKey
*	功能说明: 初始化按键. 该函数被 bsp_Init() 调用。
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitKey(void)
{
	bsp_InitKeyVar();		/* 初始化按键变量 */
	bsp_InitKeyHard();		/* 初始化按键硬件 */
}

/*
*********************************************************************************************************
*	函 数 名: bsp_KeyTimerInit
*	功能说明: 配置 TIM7 为 10ms 周期中断,驱动 bsp_KeyScan10ms()。必须在 bsp_InitKey() 之后调用,
*			  否则中断可能在按键变量/GPIO 初始化完成前就先跑起来。
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
void bsp_KeyTimerInit(void)
{
	RCC_ClkInitTypeDef clk_cfg;
	uint32_t apb1_clk, apb1_prescaler, prescaler_value;
	uint32_t flash_latency;

	__HAL_RCC_TIM7_CLK_ENABLE();

	/* TIM7 挂在 APB1 上;APB1 分频不为1时,定时器时钟会翻倍 */
	HAL_RCC_GetClockConfig(&clk_cfg, &flash_latency);
	apb1_prescaler = clk_cfg.APB1CLKDivider;
	apb1_clk = (apb1_prescaler == RCC_HCLK_DIV1) ? HAL_RCC_GetPCLK1Freq() : (2UL * HAL_RCC_GetPCLK1Freq());

	/* 先把计数时钟分频到1MHz(计1us),再数10000个tick凑够10ms */
	prescaler_value = (apb1_clk / 1000000U) - 1U;

	s_htim7.Instance = TIM7;
	s_htim7.Init.Prescaler = prescaler_value;
	s_htim7.Init.CounterMode = TIM_COUNTERMODE_UP;
	s_htim7.Init.Period = (10U * 1000U) - 1U;
	s_htim7.Init.ClockDivision = 0;
	HAL_TIM_Base_Init(&s_htim7);

	HAL_NVIC_SetPriority(TIM7_IRQn, 5, 0);
	HAL_NVIC_EnableIRQ(TIM7_IRQn);

	HAL_TIM_Base_Start_IT(&s_htim7);
}

/*
*********************************************************************************************************
*	函 数 名: TIM7_IRQHandler
*	功能说明: TIM7 中断服务函数。不走 HAL_TIM_IRQHandler()/HAL_TIM_PeriodElapsedCallback(),
*			  因为那个回调是全局共享的弱符号,core/ 的 TIM6 时基已经用它来驱动 uwTick 了 ——
*			  两边都实现同一个强符号会在 OBJECT 库链接时报重复定义。这里直接读写更新事件
*			  标志位,和 TIM6 互不干扰。
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
void TIM7_IRQHandler(void)
{
	if ((__HAL_TIM_GET_FLAG(&s_htim7, TIM_FLAG_UPDATE) != RESET) &&
	    (__HAL_TIM_GET_IT_SOURCE(&s_htim7, TIM_IT_UPDATE) != RESET))
	{
		__HAL_TIM_CLEAR_IT(&s_htim7, TIM_IT_UPDATE);
		bsp_KeyScan10ms();
	}
}

/*
*********************************************************************************************************
*	函 数 名: bsp_InitKeyHard
*	功能说明: 配置按键对应的GPIO
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
static void bsp_InitKeyHard(void)
{
	GPIO_InitTypeDef gpio_init;
	uint8_t i;

	/*.打开GIPIO时钟 */
	ALL_KEY_GPIO_CLK_ENABLE();

	/* 配置所有的按键GPIO为输入模式。低电平有效的键(KEY0/KEY1/KEY2)配内部上拉,
	   空闲时读到高电平;高电平有效的键(WK_UP)配内部下拉,空闲时读到低电平——
	   不能像之前那样统一用 GPIO_NOPULL,否则空闲时是浮空输入,读到的电平不确定,
	   会被 IsKeyDownFunc() 误判成"同时有多个键按下"而永远无法识别单键按下。 */
	gpio_init.Mode = GPIO_MODE_INPUT;		/* 设置输入 */
	gpio_init.Speed = GPIO_SPEED_FREQ_VERY_HIGH;	/* GPIO速度等级 */

	for(i=0; i<HARD_KEY_NUM; i++) {
		gpio_init.Pin = s_gpio_list[i].pin;
		gpio_init.Pull = (0 == s_gpio_list[i].ActiveLevel) ? GPIO_PULLUP : GPIO_PULLDOWN;
		HAL_GPIO_Init(s_gpio_list[i].gpio, &gpio_init);
	}

}

/*
*********************************************************************************************************
*	函 数 名: bsp_InitKeyVar
*	功能说明: 初始化按键变量
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
static void bsp_InitKeyVar(void)
{
	uint8_t i;

	/* 对按键FIFO读写指针清零 */
	s_tKey.Read= 0;
	s_tKey.Write = 0;
	s_tKey.Read2 = 0;

	/* 给每一个按键结构体成员变量赋一组缺省值 */
	for(i=0; i<KEY_COUNT; i++) {
		s_tBtn[i].LongTime = KEY_LONG_TIME;			/*.长按时间0表示不检测长按事件 */
		s_tBtn[i].Count= KEY_FILERT_TIME/2;			/*.计数器设置为滤波时间的一半 */
		s_tBtn[i].State = 0;						/* 按键缺省状态,0表示未按下 */
		s_tBtn[i].RepeatSpeed= 0;					/* 按键连发的速度,0表示不支持连发 */
		s_tBtn[i].RepeatCount= 0;					/*.连发计数器 */
	}

	/* 如果需要单独更改某个按键的参数，可以在此单独重新赋值 */

}

/*
*********************************************************************************************************
*	函 数 名: bsp_PutKey
*	功能说明: 将1个键值压入按键FIFO缓冲区。可用于模拟一个按键。
*	形    参:  _KeyCode : 按键代码
*	返 回 值: 无
*********************************************************************************************************
*/
void bsp_PutKey(uint8_t _KeyCode)
{
	s_tKey.Buf[s_tKey.Write] = _KeyCode;
	if(++s_tKey.Write >= KEY_FIFO_SIZE) {
		s_tKey.Write = 0;
	}
}

/*
*********************************************************************************************************
*	函 数 名: bsp_GetKey
*	功能说明: 从按键FIFO缓冲区读取一个键值。
*	形    参: 无
*	返 回 值: 按键代码
*********************************************************************************************************
*/
uint8_t bsp_GetKey(void)
{
	uint8_t ret;

	if(s_tKey.Read == s_tKey.Write) {
		return KEY_NONE;
	} else {
		ret = s_tKey.Buf[s_tKey.Read];

		if(++s_tKey.Read >= KEY_FIFO_SIZE) {
			s_tKey.Read = 0;
		}
		return ret;
	}

}
/*
*********************************************************************************************************
*	函 数 名: bsp_GetKey2
*	功能说明: 从按键FIFO缓冲区读取一个键值。独立的读指针。
*	形    参:  无
*	返 回 值: 按键代码
*********************************************************************************************************
*/
uint8_t bsp_GetKey2(void)
{
	uint8_t ret;

	if (s_tKey.Read2 == s_tKey.Write)
	{
		return KEY_NONE;
	}
	else
	{
		ret = s_tKey.Buf[s_tKey.Read2];

		if (++s_tKey.Read2 >= KEY_FIFO_SIZE)
		{
			s_tKey.Read2 = 0;
		}
		return ret;
	}
}
/*
*********************************************************************************************************
*	函 数 名: bsp_GetKeyState
*	功能说明: 读取按键的状态
*	形    参:  _ucKeyID : 按键ID，从0开始
*	返 回 值: 1 表示按下， 0 表示未按下
*********************************************************************************************************
*/
uint8_t bsp_GetKeyState(KEY_ID_E _ucKeyID)
{
	return s_tBtn[_ucKeyID].State;
}
/*
*********************************************************************************************************
*	函 数 名: bsp_SetKeyParam
*	功能说明: 设置按键参数
*	形    参：_ucKeyID : 按键ID，从0开始
*			_LongTime : 长按事件时间
*			 _RepeatSpeed : 连发速度
*	返 回 值: 无
*********************************************************************************************************
*/
void bsp_SetKeyParam(uint8_t _ucKeyID, uint16_t _LongTime, uint8_t  _RepeatSpeed)
{
	s_tBtn[_ucKeyID].LongTime = _LongTime;			/* 长按时间 0 表示不检测长按键事件 */
	s_tBtn[_ucKeyID].RepeatSpeed = _RepeatSpeed;			/* 按键连发的速度，0表示不支持连发 */
	s_tBtn[_ucKeyID].RepeatCount = 0;						/* 连发计数器 */
}

/*
*********************************************************************************************************
*	函 数 名: bsp_ClearKey
*	功能说明: 清空按键FIFO缓冲区
*	形    参：无
*	返 回 值: 按键代码
*********************************************************************************************************
*/
void bsp_ClearKey(void)
{
	s_tKey.Read = s_tKey.Write;
}

/*
*********************************************************************************************************
*	函 数 名: bsp_DetectKey
*	功能说明: 检测一个按键。非阻塞状态，必须被周期性的调用。
*	形    参: IO的id， 从0开始编码
*	返 回 值: 无
*********************************************************************************************************
*/
static void bsp_DetectKey(uint8_t i)
{
	KEY_T *pBtn;

	pBtn = &s_tBtn[i];
	if(IsKeyDownFunc(i)) {
		if(pBtn->Count < KEY_FILERT_TIME) {
			pBtn->Count = KEY_FILERT_TIME;
		} else if(pBtn->Count < 2 * KEY_FILERT_TIME) {
			pBtn->Count++;
		} else {
			if(0 == pBtn->State) {
				pBtn->State = 1;

				/*.发送按键按下的消息 */
				bsp_PutKey((uint8_t)(3*i + 1));
			}
			if(pBtn->LongTime > 0) {
				if(pBtn->LongCount < pBtn->LongTime) {
					/*.发送按键持续按下的消息 */
					if(++pBtn->LongCount== pBtn->LongTime) {
						/*.键值放入按键FIFO */
						bsp_PutKey((uint8_t)(3*i + 3));
					}
				} else {
					if(pBtn->RepeatSpeed > 0) {
						if(++pBtn->RepeatCount >= pBtn->RepeatSpeed) {
							pBtn->RepeatCount = 0;
							/*.长按后,没间隔10ms发送一个按键 */
							bsp_PutKey((uint8_t)(3*i + 1));
						}
					}
				}

			}
		}
	}
	else {
		if(pBtn->Count  > KEY_FILERT_TIME) {
			pBtn->Count = KEY_FILERT_TIME;
		} else if(pBtn->Count != 0) {
			pBtn->Count--;
		}
		else {
			if(1 == pBtn->State) {
				pBtn->State = 0;
				/*.发送按键弹起的消息 */
				bsp_PutKey((uint8_t)(3*i + 2));
			}
		}
		pBtn->LongCount = 0;
		pBtn->RepeatCount = 0;
	}
}

/*
*********************************************************************************************************
*	函 数 名: bsp_DetectFastIO
*	功能说明: 检测高速的输入IO. 1ms刷新一次
*	形    参: IO的id， 从0开始编码
*	返 回 值: 无
*********************************************************************************************************
*/
static void bsp_DetectFastIO(uint8_t i)
{
	KEY_T *pBtn;

	pBtn = &s_tBtn[i];
	if(IsKeyDownFunc(i)) {
		if(0 == pBtn->State) {
			pBtn->State = 1;

			/*.发送按键按下的消息 */
			bsp_PutKey((uint8_t)(3*i + i));
		}
		if(pBtn->LongTime > 0) {
			if(pBtn->LongCount < pBtn->LongTime) {
				/*.发送按键持续按下的消息 */
				if(++pBtn->LongCount==pBtn->LongTime) {
					/*.按键键值放入按键FIFO */
					bsp_PutKey((uint8_t)(3*i + 3));
				}
			} else {
				if(pBtn->RepeatSpeed > 0) {
					if(++pBtn->RepeatCount >= pBtn->RepeatSpeed) {
						pBtn->RepeatCount = 0;
						/*.长按键后,每间隔10ms发送1个按键 */
						bsp_PutKey((uint8_t)(3*i + 1));
					}
				}
			}
		}
	}
	else {
		if(1 == pBtn->State) {
			pBtn->State = 0;
			/*.发送按键弹起的消息 */
			bsp_PutKey((uint8_t)(3*i + 2));
		}
		pBtn->LongCount = 0;
		pBtn->RepeatCount = 0;
	}

}

/*
*********************************************************************************************************
*	函 数 名: bsp_KeyScan10ms
*	功能说明: 扫描所有按键。非阻塞，被systick中断周期性的调用，10ms一次
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void bsp_KeyScan10ms(void)
{
	uint8_t i = 0;
	for(i = 0; i < KEY_COUNT; i++) {
		bsp_DetectKey(i);
	}
}

/*
*********************************************************************************************************
*	函 数 名: bsp_KeyScan1ms
*	功能说明: 扫描所有按键。非阻塞，被systick中断周期性的调用，1ms一次.
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void bsp_KeyScan1ms(void)
{
	uint8_t i;
	for(i=0;i< KEY_COUNT; i++)
	{
		bsp_DetectFastIO(i);
	}
}