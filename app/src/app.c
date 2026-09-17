#include "app.h"
#include "main.h"
#include "bsp_led.h"
#include "bsp_uart.h"
#include "rtthread.h"
#include "bsp_key.h"
#include "usmart.h"
#include "tx_api.h" //threadx header file

/*
*********************************************************************************************************
*                                 任务优先级，数值越小优先级越高
*********************************************************************************************************
*/
#define APP_CFG_TASK_START_PRIO (2u)
#define APP_CFG_TASK_LED_PRIO (3u)
#define APP_CFG_TASK_KEY_PRIO (3u)
#define APP_CFG_TASK_STAT_PRIO (30u)
#define APP_CFG_TASK_IDLE_PRIO (31U)
/*
*********************************************************************************************************
*                                    任务栈大小，单位字节
*********************************************************************************************************
*/
#define APP_CFG_TASK_START_STK_SIZE (4096u)
#define APP_CFG_TASK_LED_STK_SIZE (4096u)
#define APP_CFG_TASK_KEY_STK_SIZE (4096u)
#define APP_CFG_TASK_IDLE_STK_SIZE (1024u)
#define APP_CFG_TASK_STAT_STK_SIZE (1024u)
/*
*********************************************************************************************************
*                                       静态全局变量
*********************************************************************************************************
*/
static TX_THREAD AppTaskStartTCB;
static uint64_t AppTaskStartStk[APP_CFG_TASK_START_STK_SIZE / 8];

static TX_THREAD AppTaskLedTCB;
static uint64_t AppTaskLedStk[APP_CFG_TASK_LED_STK_SIZE / 8];

static TX_THREAD AppTaskKeyTCB;
static uint64_t AppTaskKeyStk[APP_CFG_TASK_KEY_STK_SIZE / 8];

static TX_THREAD AppTaskIdleTCB;
static uint64_t AppTaskIdleStk[APP_CFG_TASK_IDLE_STK_SIZE / 8];

static TX_THREAD AppTaskStatTCB;
static uint64_t AppTaskStatStk[APP_CFG_TASK_STAT_STK_SIZE / 8];

/*
*********************************************************************************************************
*                                      函数声明
*********************************************************************************************************
*/
static void AppTaskStart(ULONG thread_input);
static void AppTaskLed(ULONG thread_input);
static void AppTaskKey(ULONG thread_input);
static void AppObjCreate(void);
static void App_Printf(const char *fmt, ...);
static void AppTaskStat(ULONG thread_input);
static void AppTaskIDLE(ULONG thread_input);
static void DispTaskInfo(void);
void OSStatInit(void);
static void AppTaskCreate(void);
/*
*******************************************************************************************************
*                               变量
*******************************************************************************************************
*/
static TX_MUTEX AppPrintfSemp; /* 用于printf互斥 */

/* 统计任务使用 */
__IO uint8_t OSStatRdy;  /* 统计任务就绪标志 */
__IO uint32_t OSIdleCtr; /* 空闲任务计数 */
__IO float OSCPUUsage;   /* CPU百分比 */
uint32_t OSIdleCtrMax;   /* 1秒内最大的空闲计数 */
uint32_t OSIdleCtrRun;   /* 1秒内空闲任务当前计数 */

void led_set(uint8_t sta)
{
    /*.如果0表示关闭LED1,1表示点亮LED1 */
    switch (sta)
    {
    case 0:
        led1_off();
        break;
    case 1:
        led1_on();
        break;
    case 2:
        led2_off();
        break;
    case 3:
        led2_on();
        break;
    case 4:
        led1_toggle();
        break;
    case 5:
        led2_toggle();
        break;
    default:
        break;
    }
}

void app_led_test(void)
{
    led1_toggle();
    led2_toggle();
    HAL_Delay(500);
}

void app_key_test(void)
{
    static uint8_t ucKeyCode = 0; /*.按键代码 */
    ucKeyCode = bsp_GetKey();
    if (ucKeyCode != KEY_NONE)
    {
        switch (ucKeyCode)
        {
        case KEY_DOWN_K0:
            rt_kprintf("K0 按键按下\r\n");
            break;
        case KEY_DOWN_K1:
            rt_kprintf("K1 按键按下\r\n");
            break;
        case KEY_DOWN_K2:
            rt_kprintf("K2 按键按下\r\n");
            break;
        case KEY_DOWN_WKUP:
            rt_kprintf("WKUP 按键按下\r\n");
            break;
        default:
            break;
        }
    }
    HAL_Delay(5);
}

void app_init(void)
{
    led_init();
    my_uart_init(115200);
    bsp_InitKey();       /* 先初始化按键变量/GPIO */
    bsp_KeyTimerInit();  /* 再启动 TIM7,10ms 一次调用 bsp_KeyScan10ms() */
    usmart_dev.init(84); /* 传的是 TIM4(APB1)的实际时钟:168MHz主频/APB1分频4,
                             分频不为1按HAL规则再翻倍=84MHz,不是168 */

    rt_kprintf("Hello RT-Thread:%ld \r\n", 123456789L);
    rt_kprintf("Test Float:%lf \r\n", 123456789.123456789);

    /* enter threadx kernel */
    tx_kernel_enter();
}

void app_task(void)
{
    //   app_led_test();
    app_key_test();
}

/*
*********************************************************************************************************
*	函 数 名: tx_application_define
*	功能说明: ThreadX专用的任务创建，通信组件创建函数
*	形    参: first_unused_memory  未使用的地址空间
*	返 回 值: 无
*********************************************************************************************************
*/
void tx_application_define(void *first_unused_memory)
{
    (void)first_unused_memory;
    /*
       如果实现任务CPU利用率统计的话，此函数仅用于实现启动任务，统计任务和空闲任务，其它任务在函数
       AppTaskCreate里面创建。
    */
    /**************创建启动任务*********************/
    tx_thread_create(&AppTaskStartTCB,            /* 任务控制块地址 */
                     "App Task Start",            /* 任务名 */
                     AppTaskStart,                /* 启动任务函数地址 */
                     0,                           /* 传递给任务的参数 */
                     &AppTaskStartStk[0],         /* 堆栈基地址 */
                     APP_CFG_TASK_START_STK_SIZE, /* 堆栈空间大小 */
                     APP_CFG_TASK_START_PRIO,     /* 任务优先级 */
                     APP_CFG_TASK_START_PRIO,     /* 任务抢占阈值 */
                     TX_NO_TIME_SLICE,            /* 不开启时间片 */
                     TX_AUTO_START);              /* 创建后立即启动 */
    /**************创建统计任务*********************/
    tx_thread_create(&AppTaskStatTCB,            /* 任务控制块地址 */
                     "App Task STAT",            /* 任务名 */
                     AppTaskStat,                /* 启动任务函数地址 */
                     0,                          /* 传递给任务的参数 */
                     &AppTaskStatStk[0],         /* 堆栈基地址 */
                     APP_CFG_TASK_STAT_STK_SIZE, /* 堆栈空间大小 */
                     APP_CFG_TASK_STAT_PRIO,     /* 任务优先级 */
                     APP_CFG_TASK_STAT_PRIO,     /* 任务抢占阈值 */
                     TX_NO_TIME_SLICE,           /* 不开启时间片 */
                     TX_AUTO_START);             /* 创建后立即启动 */

    /**************创建空闲任务*********************/
    tx_thread_create(&AppTaskIdleTCB, /* 任务控制块地址 */
                     "App Task IDLE", /* 任务名 */
                     AppTaskIDLE,     /*启动任务函数地址*/
                     0,
                     &AppTaskIdleStk[0],         /* 堆栈基地址 */
                     APP_CFG_TASK_IDLE_STK_SIZE, /* 堆栈空间大小 */
                     APP_CFG_TASK_IDLE_PRIO,     /* 任务优先级 */
                     APP_CFG_TASK_IDLE_PRIO,     /* 任务抢占阈值 */
                     TX_NO_TIME_SLICE,           /* 不开启时间片 */
                     TX_AUTO_START);             /* 创建后立即启动 */
}

/*
*********************************************************************************************************
*	函 数 名: AppTaskStart
*	功能说明: 启动任务。
*	形    参: thread_input 是在创建该任务时传递的形参
*	返 回 值: 无
    优 先 级: 2
*********************************************************************************************************
*/
static void AppTaskStart(ULONG thread_input)
{
    (void)thread_input;

    /* 优先执行任务统计 */
    OSStatInit();

    /* 创建任务间通信机制：必须先于 AppTaskCreate()，否则新建的任务一旦被调度就可能
     * 用到还没创建的 AppPrintfSemp（正确性不应该依赖任务优先级/调度顺序） */
    AppObjCreate();

    /* 创建任务 */
    AppTaskCreate();

    while (1)
    {
        tx_thread_sleep(1);
    }
}

/*
*********************************************************************************************************
*	函 数 名: AppTaskStatistic
*	功能说明: 统计任务，用于实现CPU利用率的统计。为了测试更加准确，可以开启注释调用的全局中断开关
*	形    参: thread_input 创建该任务时传递的形参
*	返 回 值: 无
*   优 先 级: 30
*********************************************************************************************************
*/
void OSStatInit(void)
{
    OSStatRdy = FALSE;

    tx_thread_sleep(2u); /* 时钟同步 */

    //__disable_irq();
    OSIdleCtr = 0uL; /* 清空闲计数 */
                     //__enable_irq();

    tx_thread_sleep(100); /* 统计100ms内，最大空闲计数 */

    //__disable_irq();
    OSIdleCtrMax = OSIdleCtr; /* 保存最大空闲计数 */
    OSStatRdy = TRUE;
    //__enable_irq();
}

static void AppTaskStat(ULONG thread_input)
{
    (void)thread_input;

    while (OSStatRdy == FALSE)
    {
        tx_thread_sleep(200); /* 等待统计任务就绪 */
    }

    OSIdleCtrMax /= 100uL;
    if (OSIdleCtrMax == 0uL)
    {
        OSCPUUsage = 0u;
    }

    //__disable_irq();
    OSIdleCtr = OSIdleCtrMax * 100uL; /* 设置初始CPU利用率 0% */
                                      //__enable_irq();

    for (;;)
    {
        // __disable_irq();
        OSIdleCtrRun = OSIdleCtr; /* 获得100ms内空闲计数 */
        OSIdleCtr = 0uL;          /* 复位空闲计数 */
                                  //	__enable_irq();            /* 计算100ms内的CPU利用率 */
        OSCPUUsage = (100uL - (float)OSIdleCtrRun / OSIdleCtrMax);
        tx_thread_sleep(100); /* 每100ms统计一次 */
    }
}

/*
*********************************************************************************************************
*	函 数 名: AppTaskIDLE
*	功能说明: 空闲任务
*	形    参: thread_input 创建该任务时传递的形参
*	返 回 值: 无
    优 先 级: 31
*********************************************************************************************************
*/
static void AppTaskIDLE(ULONG thread_input)
{
    TX_INTERRUPT_SAVE_AREA

        (void)
    thread_input;

    while (1)
    {
        TX_DISABLE
        OSIdleCtr++;
        TX_RESTORE
    }
}

/*
*********************************************************************************************************
*	函 数 名: AppTaskCreate
*	功能说明: 创建应用任务
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
static void AppTaskCreate(void)
{
    /**************创建LED任务*********************/
    tx_thread_create(&AppTaskLedTCB,            /* 任务控制块地址 */
                     "App Led Task",            /* 任务名 */
                     AppTaskLed,                /* 任务函数地址 */
                     0,                         /* 传递给任务的参数 */
                     &AppTaskLedStk[0],         /* 堆栈基地址 */
                     APP_CFG_TASK_LED_STK_SIZE, /* 堆栈空间大小 */
                     APP_CFG_TASK_LED_PRIO,     /* 任务优先级 */
                     APP_CFG_TASK_LED_PRIO,     /* 任务抢占阈值 */
                     TX_NO_TIME_SLICE,          /* 不开启时间片 */
                     TX_AUTO_START);            /* 创建后立即启动 */
    /**************创建KEY任务*********************/
    tx_thread_create(&AppTaskKeyTCB,            /* 任务控制块地址 */
                     "App Key Task",            /* 任务名 */
                     AppTaskKey,                /* 任务函数地址 */
                     0,                         /* 传递给任务的参数 */
                     &AppTaskKeyStk[0],         /* 堆栈基地址 */
                     APP_CFG_TASK_KEY_STK_SIZE, /* 堆栈空间大小 */
                     APP_CFG_TASK_KEY_PRIO,     /* 任务优先级 */
                     APP_CFG_TASK_KEY_PRIO,     /* 任务抢占阈值 */
                     TX_NO_TIME_SLICE,          /* 不开启时间片 */
                     TX_AUTO_START);            /* 创建后立即启动 */
}

/*
*********************************************************************************************************
*	函 数 名: AppObjCreate
*	功能说明: 创建任务通讯
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
static void AppObjCreate(void)
{
    /* 创建互斥信号量 */
    tx_mutex_create(&AppPrintfSemp, "AppPrintfStmp", TX_INHERIT);
}
/*
*********************************************************************************************************
*	函 数 名: App_Printf
*	功能说明: 线程安全的printf方式
*	形    参: 同printf的参数。
*             在C中，当无法列出传递函数的所有实参的类型和数目时,可以用省略号指定参数表
*	返 回 值: 无
*********************************************************************************************************
*/
static void App_Printf(const char *fmt, ...)
{
    char buf_str[200 + 1]; /* 特别注意，如果printf的变量较多，注意此局部变量的大小是否够用 */
    va_list v_args;

    /* 用 rt_vsnprintf（bsp/src/rt_vsnprintf.c，不依赖 <stdio.h>）代替标准库的 vsnprintf，
     * 先把格式化结果拼进局部缓冲区，再整串交给 rt_kprintf 输出。 */
    va_start(v_args, fmt);
    (void)rt_vsnprintf((char *)&buf_str[0],
                        (rt_size_t)sizeof(buf_str),
                        (char const *)fmt,
                        v_args);
    va_end(v_args);

    /* 互斥操作 */
    tx_mutex_get(&AppPrintfSemp, TX_WAIT_FOREVER);

    rt_kprintf("%s", buf_str);

    tx_mutex_put(&AppPrintfSemp);
}
/*
*********************************************************************************************************
*	函 数 名: AppTaskLed
*	功能说明: Led任务，这里用来点灯
*	形    参: thread_input 是在创建该任务时传递的形参
*	返 回 值: 无
    优 先 级: 3
*********************************************************************************************************
*/
static void AppTaskLed(ULONG thread_input)
{
    (void)thread_input;
    while (1)
    {
        led1_toggle();
        led2_toggle();
        tx_thread_sleep(500);
    }
}

/*
*********************************************************************************************************
*	函 数 名: AppTaskKey
*	功能说明: KEY任务，这里用来检测按键任务
*	形    参: thread_input 是在创建该任务时传递的形参
*	返 回 值: 无
    优 先 级: 3
*********************************************************************************************************
*/
static void AppTaskKey(ULONG thread_input)
{
    (void)thread_input;
    uint8_t ucKeyCode = 0; /*.按键代码 */

    while (1)
    {
        ucKeyCode = bsp_GetKey();
        if (ucKeyCode != KEY_NONE)
        {
            switch (ucKeyCode)
            {
            case KEY_DOWN_K0:
                App_Printf("K0 按键按下\r\n");
                break;
            case KEY_DOWN_K1:
                App_Printf("K1 按键按下\r\n");
                break;
            case KEY_DOWN_K2:
                App_Printf("K2 按键按下\r\n");
                break;
            case KEY_DOWN_WKUP:
                App_Printf("WKUP 按键按下\r\n");
                DispTaskInfo();
                break;
            default:
                break;
            }
        }
        tx_thread_sleep(5);
    }
}


/*
*********************************************************************************************************
*	函 数 名: DispTaskInfo
*	功能说明: 将uCOS-III任务信息通过串口打印出来
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/
static void DispTaskInfo(void)
{
	TX_THREAD      *p_tcb;	        /* 定义一个任务控制块指针 */

    p_tcb = &AppTaskStartTCB;
	
	/* 打印标题 */
	App_Printf("===============================================================\r\n");
	App_Printf("OS CPU Usage = %5.2f%%\r\n", OSCPUUsage);
	App_Printf("===============================================================\r\n");
	App_Printf(" 任务优先级 任务栈大小 当前使用栈  最大栈使用   任务名\r\n");
	App_Printf("   Prio     StackSize   CurStack    MaxStack   Taskname\r\n");

	/* 遍历任务控制块列?TCB list)，打印所有的任务的优先级和名?*/
	while (p_tcb != (TX_THREAD *)0) 
	{
		
		App_Printf("   %2d        %5d      %5d       %5d      %s\r\n", 
                    p_tcb->tx_thread_priority,
                    p_tcb->tx_thread_stack_size,
                    (int)p_tcb->tx_thread_stack_end - (int)p_tcb->tx_thread_stack_ptr,
                    (int)p_tcb->tx_thread_stack_end - (int)p_tcb->tx_thread_stack_highest_ptr,
                    p_tcb->tx_thread_name);


        p_tcb = p_tcb->tx_thread_created_next;

        if(p_tcb == &AppTaskStartTCB) break;
	}
}
