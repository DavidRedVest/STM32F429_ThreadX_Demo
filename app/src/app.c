#include "app.h"
#include "main.h"
#include "bsp_led.h"
#include "bsp_uart.h"
#include "rtthread.h"
#include "bsp_key.h"

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
    if(ucKeyCode != KEY_NONE)
    {
        switch(ucKeyCode)
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

    rt_kprintf("Hello RT-Thread:%ld \r\n", 123456789L);
    rt_kprintf("Test Float:%lf \r\n", 123456789.123456789);
}

void app_task(void)
{
    app_led_test();
 //   app_key_test();

}

