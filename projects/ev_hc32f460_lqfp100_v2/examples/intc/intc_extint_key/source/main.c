/**
 *******************************************************************************
 * @file  intc/intc_extint_key/source/main.c
 * @brief Main program EXTINT_KEY for the Device Driver Library.
 @verbatim
   Change Logs:
   Date             Author          Notes
   2022-03-31       CDT             First version
   2024-11-08       CDT             Integrate global, group, share interrupt in one project
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
 * @addtogroup EXTINT_KEY
 * @{
 */

/*******************************************************************************
 * Local type definitions ('typedef')
 ******************************************************************************/

/*******************************************************************************
 * Local pre-processor symbols/macros ('#define')
 ******************************************************************************/
#define KEY10_PORT              (GPIO_PORT_B)
#define KEY10_PIN               (GPIO_PIN_01)
#define KEY10_EXTINT_CH         (EXTINT_CH01)
#define KEY10_INT_SRC           (INT_SRC_PORT_EIRQ1)
#define KEY10_GLB_INT_IRQn      (INT001_IRQn)
#define KEY10_GRP_INT_IRQn      (INT033_IRQn)
#define KEY10_SHARE_INT_IRQn    (INT128_IRQn)
#define KEY10_INT_PRIO          (DDL_IRQ_PRIO_DEFAULT)

/*******************************************************************************
 * Global variable definitions (declared in header file with 'extern')
 ******************************************************************************/

/*******************************************************************************
 * Local function prototypes ('static')
 ******************************************************************************/
static void EXTINT_KEY10_Global_IrqCallback(void);
static void EXTINT_KEY10_Group_IrqCallback(void);
static void KEY10_Global_Init(void);
static void KEY10_Group_Init(void);
static void KEY10_Share_Init(void);

/*******************************************************************************
 * Local variable definitions ('static')
 ******************************************************************************/

/*******************************************************************************
 * Function implementation - global ('extern') and local ('static')
 ******************************************************************************/
/**
 * @brief  KEY10(K10) External interrupt Ch.1 callback function
 *         IRQ No.1 in Global IRQ entry No.0~31 is used for EXTINT1
 * @param  None
 * @retval None
 */
static void EXTINT_KEY10_Global_IrqCallback(void)
{
    if (SET == EXTINT_GetExtIntStatus(KEY10_EXTINT_CH)) {
        BSP_LED_Toggle(LED_BLUE);
        while (PIN_RESET == GPIO_ReadInputPins(KEY10_PORT, KEY10_PIN)) {
            ;
        }
        EXTINT_ClearExtIntStatus(KEY10_EXTINT_CH);
        NVIC_DisableIRQ(KEY10_GLB_INT_IRQn);
        NVIC_EnableIRQ(KEY10_GRP_INT_IRQn);
    }
}

/**
 * @brief  KEY10 global IT initialize
 * @param  None
 * @retval None
 */
static void KEY10_Global_Init(void)
{
    stc_extint_init_t stcExtIntInit;
    stc_irq_signin_config_t stcIrqSignConfig;
    stc_gpio_init_t stcGpioInit;

    /* GPIO config */
    (void)GPIO_StructInit(&stcGpioInit);
    stcGpioInit.u16ExtInt = PIN_EXTINT_ON;
    stcGpioInit.u16PullUp = PIN_PU_ON;
    (void)GPIO_Init(KEY10_PORT, KEY10_PIN, &stcGpioInit);

    /* ExtInt config */
    (void)EXTINT_StructInit(&stcExtIntInit);
    stcExtIntInit.u32Filter      = EXTINT_FILTER_ON;
    stcExtIntInit.u32FilterClock = EXTINT_FCLK_DIV8;
    stcExtIntInit.u32Edge = EXTINT_TRIG_FALLING;
    (void)EXTINT_Init(KEY10_EXTINT_CH, &stcExtIntInit);

    /* IRQ sign-in */
    stcIrqSignConfig.enIntSrc = KEY10_INT_SRC;
    stcIrqSignConfig.enIRQn   = KEY10_GLB_INT_IRQn;
    stcIrqSignConfig.pfnCallback = &EXTINT_KEY10_Global_IrqCallback;
    (void)INTC_IrqSignIn(&stcIrqSignConfig);

    /* NVIC config */
    NVIC_ClearPendingIRQ(stcIrqSignConfig.enIRQn);
    NVIC_SetPriority(stcIrqSignConfig.enIRQn, KEY10_INT_PRIO);
    NVIC_EnableIRQ(stcIrqSignConfig.enIRQn);
}

/**
 * @brief  KEY10(K10) External interrupt Ch.1 callback function
 *         IRQ No.33 in Group IRQ entry No.32~37 is used for EXTINT0
 * @param  None
 * @retval None
 */
static void EXTINT_KEY10_Group_IrqCallback(void)
{
    if (SET == EXTINT_GetExtIntStatus(KEY10_EXTINT_CH)) {
        BSP_LED_Toggle(LED_YELLOW);
        while (PIN_RESET == GPIO_ReadInputPins(KEY10_PORT, KEY10_PIN)) {
            ;
        }
        EXTINT_ClearExtIntStatus(KEY10_EXTINT_CH);
        NVIC_DisableIRQ(KEY10_GRP_INT_IRQn);
        NVIC_EnableIRQ(KEY10_SHARE_INT_IRQn);
    }
}

/**
 * @brief  KEY10 group IT initialize
 * @param  None
 * @retval None
 */
static void KEY10_Group_Init(void)
{
    stc_extint_init_t stcExtIntInit;
    stc_irq_signin_config_t stcIrqSignConfig;
    stc_gpio_init_t stcGpioInit;

    /* GPIO config */
    (void)GPIO_StructInit(&stcGpioInit);
    stcGpioInit.u16ExtInt = PIN_EXTINT_ON;
    stcGpioInit.u16PullUp = PIN_PU_ON;
    (void)GPIO_Init(KEY10_PORT, KEY10_PIN, &stcGpioInit);

    /* ExtInt config */
    (void)EXTINT_StructInit(&stcExtIntInit);
    stcExtIntInit.u32Filter      = EXTINT_FILTER_ON;
    stcExtIntInit.u32FilterClock = EXTINT_FCLK_DIV8;
    stcExtIntInit.u32Edge = EXTINT_TRIG_FALLING;
    (void)EXTINT_Init(KEY10_EXTINT_CH, &stcExtIntInit);

    /* IRQ sign-in */
    stcIrqSignConfig.enIntSrc = KEY10_INT_SRC;
    stcIrqSignConfig.enIRQn   = KEY10_GRP_INT_IRQn;
    stcIrqSignConfig.pfnCallback = &EXTINT_KEY10_Group_IrqCallback;
    (void)INTC_IrqSignIn(&stcIrqSignConfig);

    /* NVIC config */
    NVIC_ClearPendingIRQ(KEY10_GRP_INT_IRQn);
    NVIC_SetPriority(KEY10_GRP_INT_IRQn, KEY10_INT_PRIO);
    NVIC_EnableIRQ(KEY10_GRP_INT_IRQn);
}

/**
  * @brief  KEY10(K10) External interrupt Ch.1 ISR
  *         Share IRQ entry No.128 is used for EXTINT0
  * @param  None
  * @retval None
  */
void EXTINT01_IrqHandler(void)
{
    if (SET == EXTINT_GetExtIntStatus(KEY10_EXTINT_CH)) {
        BSP_LED_Toggle(LED_RED);
        while (PIN_RESET == GPIO_ReadInputPins(KEY10_PORT, KEY10_PIN)) {
            ;
        }
        EXTINT_ClearExtIntStatus(KEY10_EXTINT_CH);
        NVIC_DisableIRQ(KEY10_SHARE_INT_IRQn);
        NVIC_EnableIRQ(KEY10_GLB_INT_IRQn);
    }
}

/**
 * @brief  KEY10 share IT initialize
 * @param  None
 * @retval None
 */
static void KEY10_Share_Init(void)
{
    stc_extint_init_t stcExtIntInit;
    stc_gpio_init_t stcGpioInit;

    /* GPIO config */
    (void)GPIO_StructInit(&stcGpioInit);
    stcGpioInit.u16ExtInt = PIN_EXTINT_ON;
    stcGpioInit.u16PullUp = PIN_PU_ON;
    (void)GPIO_Init(KEY10_PORT, KEY10_PIN, &stcGpioInit);

    /* ExtInt config */
    (void)EXTINT_StructInit(&stcExtIntInit);
    stcExtIntInit.u32Filter      = EXTINT_FILTER_ON;
    stcExtIntInit.u32FilterClock = EXTINT_FCLK_DIV8;
    stcExtIntInit.u32Edge = EXTINT_TRIG_FALLING;
    (void)EXTINT_Init(KEY10_EXTINT_CH, &stcExtIntInit);

    (void)INTC_ShareIrqCmd(KEY10_INT_SRC, ENABLE);

    /* NVIC config */
    NVIC_ClearPendingIRQ(KEY10_SHARE_INT_IRQn);
    NVIC_SetPriority(KEY10_SHARE_INT_IRQn, KEY10_INT_PRIO);
    NVIC_EnableIRQ(KEY10_SHARE_INT_IRQn);
}

/**
 * @brief  Main function of EXTINT project
 * @param  None
 * @retval int32_t return value, if needed
 */
int32_t main(void)
{
    /* Register write enable for some required peripherals. */
    LL_PERIPH_WE(LL_PERIPH_EFM | LL_PERIPH_FCG | LL_PERIPH_GPIO | LL_PERIPH_PWC_CLK_RMU | LL_PERIPH_SRAM);
    /* BSP Clock initialize */
    BSP_CLK_Init();
    /* BSP LED initialize */
    BSP_LED_Init();
    /* KEY10 initialize */
    KEY10_Global_Init();
    KEY10_Group_Init();
    KEY10_Share_Init();
    /* Register write protected for some required peripherals. */
    LL_PERIPH_WP(LL_PERIPH_EFM | LL_PERIPH_FCG | LL_PERIPH_GPIO | LL_PERIPH_PWC_CLK_RMU | LL_PERIPH_SRAM);
    /* wait KEY10 pressed */
    for (;;) {
        ;
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
