#include "app.h"
#include "main.h"
#include "bsp_led.h"
#include "bsp_uart.h"
#include "rtthread.h"

void app_init(void)
{
    led_init();
    my_uart_init(115200);

    rt_kprintf("%x\r\n", 0x1234);
    rt_kprintf("Hello RT-Thread:%ld \r\n", 123456789L);
    rt_kprintf("Test Float:%lf \r\n", 123456789.123456789);
}

void app_task(void)
{
    HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_0);
    HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_1);
    HAL_Delay(500);
}
