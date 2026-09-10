/**
 ******************************************************************************
 * @file    stm32f4xx_hal_timebase_tim.c
 * @brief   HAL time base source override: TIM6 instead of SysTick.
 *
 * SysTick is what ThreadX's own Cortex-M4 port normally claims for its RTOS
 * tick once middlewares/threadx grows real sources, which would collide with
 * HAL's default use of SysTick for HAL_GetTick()/HAL_Delay(). Overriding the
 * weak HAL_InitTick()/HAL_SuspendTick()/HAL_ResumeTick() here to drive uwTick
 * from TIM6 (a basic timer, otherwise unused in this project) instead keeps
 * HAL_Delay() working without competing with the RTOS for SysTick.
 ******************************************************************************
 */

#include "main.h"

TIM_HandleTypeDef htim6;

/**
 * @brief  This function configures the TIM6 as a time base source.
 *         The time source is configured to have 1ms time base with a
 *         dedicated Tick interrupt priority.
 * @note   This function is called automatically at the beginning of program
 *         after reset by HAL_Init() or at any time when clock is reconfigured
 *         by HAL_RCC_ClockConfig().
 * @param  TickPriority Tick interrupt priority.
 * @retval HAL status
 */
HAL_StatusTypeDef HAL_InitTick(uint32_t TickPriority)
{
  RCC_ClkInitTypeDef clkconfig;
  uint32_t uwTimclock, uwAPB1Prescaler;
  uint32_t uwPrescalerValue;
  uint32_t pFLatency;
  HAL_StatusTypeDef status;

  __HAL_RCC_TIM6_CLK_ENABLE();

  HAL_RCC_GetClockConfig(&clkconfig, &pFLatency);
  uwAPB1Prescaler = clkconfig.APB1CLKDivider;

  /* TIM6 is on APB1; APB1 timer clock doubles when the APB1 prescaler != 1 */
  if (uwAPB1Prescaler == RCC_HCLK_DIV1)
  {
    uwTimclock = HAL_RCC_GetPCLK1Freq();
  }
  else
  {
    uwTimclock = 2UL * HAL_RCC_GetPCLK1Freq();
  }

  /* Prescale TIM6's counter clock down to 1MHz, then count 1000 ticks for 1ms */
  uwPrescalerValue = (uint32_t)((uwTimclock / 1000000U) - 1U);

  htim6.Instance = TIM6;
  htim6.Init.Period = (1000000U / 1000U) - 1U;
  htim6.Init.Prescaler = uwPrescalerValue;
  htim6.Init.ClockDivision = 0;
  htim6.Init.CounterMode = TIM_COUNTERMODE_UP;

  status = HAL_TIM_Base_Init(&htim6);
  if (status == HAL_OK)
  {
    status = HAL_TIM_Base_Start_IT(&htim6);
    if (status == HAL_OK)
    {
      if (TickPriority < (1UL << __NVIC_PRIO_BITS))
      {
        HAL_NVIC_SetPriority(TIM6_DAC_IRQn, TickPriority, 0U);
        HAL_NVIC_EnableIRQ(TIM6_DAC_IRQn);
        uwTickPrio = TickPriority;
      }
      else
      {
        status = HAL_ERROR;
      }
    }
  }

  return status;
}

/**
 * @brief  Suspends the time base source interrupt.
 */
void HAL_SuspendTick(void)
{
  __HAL_TIM_DISABLE_IT(&htim6, TIM_IT_UPDATE);
}

/**
 * @brief  Resumes the time base source interrupt.
 */
void HAL_ResumeTick(void)
{
  __HAL_TIM_ENABLE_IT(&htim6, TIM_IT_UPDATE);
}

/**
 * @brief  TIM6 global interrupt handler.
 */
void TIM6_DAC_IRQHandler(void)
{
  HAL_TIM_IRQHandler(&htim6);
}

/**
 * @brief  Period elapsed callback in non-blocking mode. Advances uwTick for
 *         every TIM6 update event, taking over from SysTick_Handler.
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM6)
  {
    HAL_IncTick();
  }
}
