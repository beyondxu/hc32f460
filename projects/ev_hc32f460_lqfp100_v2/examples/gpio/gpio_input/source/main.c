/**
 *******************************************************************************
 * @file  gpio/gpio_input/source/main.c
 * @brief Main program of GPIO for the Device Driver Library.
 @verbatim
   Change Logs:
   Date             Author          Notes
   2024-11-08       CDT             First version
 @endverbatim
 *******************************************************************************
 * Copyright (C) 2022-2025, Xiaohua Semiconductor Co., Ltd. All rights reserved.
 *
 * This software component is licensed by XHSC under BSD 3-Clause license
 * (the "License"); You may not use this file except in compliance with the
 * License. You may obtain a copy of the License at:
 *                    opensource.org/licenses/BSD-3-Clause
 *
 *******************************************************************************
 */

/*******************************************************************************
 * Include files
 ******************************************************************************/
#include "main.h"

/**
 * @addtogroup HC32F460_DDL_Examples
 * @{
 */

/**
 * @addtogroup GPIO_INPUT
 * @{
 */

/*******************************************************************************
 * Local type definitions ('typedef')
 ******************************************************************************/

/*******************************************************************************
 * Local pre-processor symbols/macros ('#define')
 ******************************************************************************/
/* KEY Port/Pin definition */
#define KEY10_PORT          (GPIO_PORT_B)
#define KEY10_PIN           (GPIO_PIN_01)
/* KEY Dithering Elimination */
#define KEY_DELAY_MS        (10UL)

/* LED Port/Pin definition */
#define LED_G_PORT          (GPIO_PORT_D)
#define LED_G_PIN           (GPIO_PIN_04)
/* LED ON/OFF definition */
#define LED_G_ON()          (GPIO_SetPins(LED_G_PORT, LED_G_PIN))
#define LED_G_OFF()         (GPIO_ResetPins(LED_G_PORT, LED_G_PIN))

/*******************************************************************************
 * Global variable definitions (declared in header file with 'extern')
 ******************************************************************************/

/*******************************************************************************
 * Local function prototypes ('static')
 ******************************************************************************/

/*******************************************************************************
 * Local variable definitions ('static')
 ******************************************************************************/

/*******************************************************************************
 * Function implementation - global ('extern') and local ('static')
 ******************************************************************************/
/**
 * @brief  KEY Init
 * @param  None
 * @retval None
 */
static void KEY_Init(void)
{
    stc_gpio_init_t stcGpioInit;

    (void)GPIO_StructInit(&stcGpioInit);
    stcGpioInit.u16PullUp   = PIN_PU_ON;
    stcGpioInit.u16PinDir   = PIN_DIR_IN;
    (void)GPIO_Init(KEY10_PORT, KEY10_PIN, &stcGpioInit);
}

/**
 * @brief  LED Init
 * @param  None
 * @retval None
 */
static void LED_Init(void)
{
    stc_gpio_init_t stcGpioInit;

    (void)GPIO_StructInit(&stcGpioInit);
    stcGpioInit.u16PinState = PIN_STAT_RST;
    stcGpioInit.u16PinDir   = PIN_DIR_OUT;
    (void)GPIO_Init(LED_G_PORT, LED_G_PIN, &stcGpioInit);
}

/**
 * @brief  Main function of GPIO project
 * @param  None
 * @retval int32_t return value, if needed
 */
int32_t main(void)
{
    /* Register write enable for some required peripherals. */
    LL_PERIPH_WE(LL_PERIPH_GPIO);
    /* KEY initialize */
    KEY_Init();
    /* LED initialize */
    LED_Init();
    /* Register write protected for some required peripherals. */
    LL_PERIPH_WP(LL_PERIPH_GPIO);
    for (;;) {
        if (PIN_RESET == GPIO_ReadInputPins(KEY10_PORT, KEY10_PIN)) {
            /* Key filter */
            DDL_DelayMS(KEY_DELAY_MS);
            if (PIN_RESET == GPIO_ReadInputPins(KEY10_PORT, KEY10_PIN)) {
                LED_G_ON();
            } else {
                LED_G_OFF();
            }
        }
        /* De-init port if necessary */
        // GPIO_DeInit();
    }
}

/**
 * @}
 */

/**
 * @}
 */

/*******************************************************************************
 * EOF (not truncated)
 ******************************************************************************/
