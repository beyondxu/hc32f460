/**
 *******************************************************************************
 * @file  clk/clk_xtalstop_detect/source/main.c
 * @brief Main program of CLK for the Device Driver Library.
 @verbatim
   Change Logs:
   Date             Author          Notes
   2022-03-31       CDT             First version
   2022-10-31       CDT             Add configuration of XTAL IO as analog function
   2024-11-08       CDT             Use CLK_XtalStdInit() to replace XtalStopDetctInit()
                                    Modify XTAL_STOP_IrqCallback
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
 * @addtogroup CLK_Xtalstop_detect
 * @{
 */

/*******************************************************************************
 * Local type definitions ('typedef')
 ******************************************************************************/

/*******************************************************************************
 * Local pre-processor symbols/macros ('#define')
 ******************************************************************************/
#define XTALSTOP_INT_SRC    (INT_SRC_XTAL_STOP)
#define XTALSTOP_IRQn       (INT000_IRQn)
#define DLY_MS              (500UL)

#define XTALSTOP_OPS_INT    (0U)
#define XTALSTOP_OPS_RST    (1U)


#define XTALSTOP_OPS        XTALSTOP_OPS_INT

/*******************************************************************************
 * Global variable definitions (declared in header file with 'extern')
 ******************************************************************************/

/*******************************************************************************
 * Local function prototypes ('static')
 ******************************************************************************/
#if XTALSTOP_OPS == XTALSTOP_OPS_INT
static void XtalInit(void);
static void XtalStopIntInit(void);
static void XTAL_STOP_IrqCallback(void);
#endif

/*******************************************************************************
 * Local variable definitions ('static')
 ******************************************************************************/

/*******************************************************************************
 * Function implementation - global ('extern') and local ('static')
 ******************************************************************************/
#if XTALSTOP_OPS == XTALSTOP_OPS_INT
/**
 * @brief  XTAL stop detect IRQ callback function
 * @param  None
 * @retval None
 */
static void XTAL_STOP_IrqCallback(void)
{
    uint8_t u8Count = 2U;
    /* LED_BLUE Off */
    BSP_LED_Off(LED_BLUE);
    do {
        BSP_LED_Toggle(LED_RED);
        DDL_DelayMS(DLY_MS);
    } while ((--u8Count) != 0U);

    CLK_ClearXtalStdStatus();
}

/**
 * @brief  XTAL stop detect interrupt init
 * @param  None
 * @retval None
 */
static void XtalStopIntInit(void)
{
    stc_irq_signin_config_t stcIrqSignConfig;

    stcIrqSignConfig.enIntSrc   = XTALSTOP_INT_SRC;
    stcIrqSignConfig.enIRQn     = XTALSTOP_IRQn;
    stcIrqSignConfig.pfnCallback = &XTAL_STOP_IrqCallback;

    (void)INTC_IrqSignIn(&stcIrqSignConfig);

    CLK_ClearXtalStdStatus();

    /* NVIC setting */
    NVIC_ClearPendingIRQ(XTALSTOP_IRQn);
    NVIC_SetPriority(XTALSTOP_IRQn, DDL_IRQ_PRIO_DEFAULT);
    NVIC_EnableIRQ(XTALSTOP_IRQn);
}
#endif

/**
 * @brief  XTAL init
 * @param  None
 * @retval None
 */
static void XtalInit(void)
{
    stc_clock_xtal_init_t stcXtalInit;

    /* Config XTAL and Enable */
    GPIO_AnalogCmd(BSP_XTAL_PORT, BSP_XTAL_PIN, ENABLE);
    (void)CLK_XtalStructInit(&stcXtalInit);
    stcXtalInit.u8State = CLK_XTAL_ON;
    stcXtalInit.u8Mode = CLK_XTAL_MD_OSC;
    stcXtalInit.u8Drv = CLK_XTAL_DRV_ULOW;
    stcXtalInit.u8StableTime = CLK_XTAL_STB_2MS;

    (void)CLK_XtalInit(&stcXtalInit);
}

/**
 * @brief  Main function of XTAL stop detect project
 * @param  None
 * @retval int32_t return value, if needed
 */
int32_t main(void)
{
    /* Register write enable for some required peripherals. */
    LL_PERIPH_WE(LL_PERIPH_FCG | LL_PERIPH_GPIO | LL_PERIPH_PWC_CLK_RMU);
    /* BSP LED initialize */
    BSP_LED_Init();
#if XTALSTOP_OPS == XTALSTOP_OPS_RST
    if (SET == RMU_GetStatus(RMU_FLAG_XTAL_ERR)) {
        RMU_ClearStatus();
        BSP_LED_Toggle(LED_RED);
        DDL_DelayMS(DLY_MS);
        BSP_LED_Toggle(LED_RED);
        DDL_DelayMS(DLY_MS);
    }
#endif
    /* Xtal initialize */
    XtalInit();
#if XTALSTOP_OPS == XTALSTOP_OPS_RST
    /* Xtal stop detect initialize */
    (void)CLK_XtalStdInit(CLK_XTALSTD_ON, CLK_XTALSTD_EXP_TYPE_RST);
#elif XTALSTOP_OPS == XTALSTOP_OPS_INT
    /* Xtal stop detect initialize */
    (void)CLK_XtalStdInit(CLK_XTALSTD_ON, CLK_XTALSTD_EXP_TYPE_INT);
    /* Xtal stop detect interrupt initialize */
    XtalStopIntInit();
#endif
    /* Register write protected for some required peripherals. */
    LL_PERIPH_WP(LL_PERIPH_FCG);
    /* Turn off All LEDs */
    BSP_LED_Off(LED_ALL);
    for (;;) {
        BSP_LED_Toggle(LED_BLUE);
        DDL_DelayMS(DLY_MS);
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
