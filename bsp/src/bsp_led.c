#include "bsp_led.h"

void led_init(void)
{
    GPIO_InitTypeDef GPIO_Initure;
    __HAL_RCC_GPIOB_CLK_ENABLE(); // 开启GPIOB时钟

    GPIO_Initure.Pin = GPIO_PIN_0 | GPIO_PIN_1; // PB1,0
    GPIO_Initure.Mode = GPIO_MODE_OUTPUT_PP;    // 推挽输出
    GPIO_Initure.Pull = GPIO_PULLUP;            // 上拉
    GPIO_Initure.Speed = GPIO_SPEED_FREQ_HIGH;  // 高速
    HAL_GPIO_Init(GPIOB, &GPIO_Initure);

    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET); // PB0置1
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET); // PB1置1
}
void led1_on(void)
{
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1,GPIO_PIN_RESET);
}
void led1_off(void)
{
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1,GPIO_PIN_SET);
}
void led1_toggle(void)
{
    HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_1);
}
void led2_on(void)
{
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0,GPIO_PIN_RESET);
}
void led2_off(void)
{
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0,GPIO_PIN_SET);
}
void led2_toggle(void)
{
    HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_0);
}
