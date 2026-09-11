#include "app.h"
#include "main.h"
#include "bsp_led.h"
#include "bsp_uart.h"
#include "rtthread.h"
#include "bsp_key.h"
#include "usmart.h"

void led_set(uint8_t sta)
{
    /*.如果0表示关闭LED1,1表示点亮LED1 */
   switch(sta)
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
    usmart_dev.init(84);  /* 传的是 TIM4(APB1)的实际时钟:168MHz主频/APB1分频4,
                              分频不为1按HAL规则再翻倍=84MHz,不是168 */

    rt_kprintf("Hello RT-Thread:%ld \r\n", 123456789L);
    rt_kprintf("Test Float:%lf \r\n", 123456789.123456789);
}

void app_task(void)
{
 //   app_led_test();
    app_key_test();

}

