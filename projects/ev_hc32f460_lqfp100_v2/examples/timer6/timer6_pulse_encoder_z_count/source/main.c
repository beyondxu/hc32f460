/**
 *******************************************************************************
 * @file  timer6/timer6_pulse_encoder_z_count/source/main.c
 * @brief This example demonstrates Timer6 count for z Phase counting mode
 *        function.
 @verbatim
   Change Logs:
   Date             Author          Notes
   2023-09-30       CDT             First version
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
 * @addtogroup TIMER6_Pulse_Encoder_Z_Count
 * @{
 */

/*******************************************************************************
 * Local type definitions ('typedef')
 ******************************************************************************/

/*******************************************************************************
 * Local pre-processor symbols/macros ('#define')
 ******************************************************************************/
/* Position unit phase-difference count */
#define PHASE_DIFF_CNT_X1               (1U)
#define PHASE_DIFF_CNT_X2               (2U)
#define PHASE_DIFF_CNT_X4               (4U)

/* Select position unit phase-difference count. */
#define PHASE_DIFF_CNT                  (PHASE_DIFF_CNT_X1)

/**
 * Timer6 unit definitions for this example.
 *  ------------------|------------------|
 *  | TMR6_POS_UNIT   |     TMR6_Z_UNIT  |
 *  |-----------------|------------------|
 *  |  CM_TMR6_1      |      CM_TMR6_2   |
 *  |-----------------|------------------|
 */
#define TMR6_POS_UNIT                   (CM_TMR6_1)
#define TMR6_Z_UNIT                     (CM_TMR6_2)
#define TMR6_PERIPH_CLK                 (FCG2_PERIPH_TMR6_1 | FCG2_PERIPH_TMR6_2)

/* Select the pins for phase A and phase B inputting according to 'TMR6_POS_UNIT'. */
#define PHASE_A_PORT                    (GPIO_PORT_A)
#define PHASE_A_PIN                     (GPIO_PIN_08)
#define PHASE_A_PIN_FUNC                (GPIO_FUNC_3)
#define PHASE_B_PORT                    (GPIO_PORT_E)
#define PHASE_B_PIN                     (GPIO_PIN_08)
#define PHASE_B_PIN_FUNC                (GPIO_FUNC_3)
/* Select the pins for phase Z inputting according to 'TMR6_Z_UNIT'. */
#define PHASE_Z_PORT                    (GPIO_PORT_E)
#define PHASE_Z_PIN                     (GPIO_PIN_07)
#define PHASE_Z_PIN_FUNC                (GPIO_FUNC_3)

#if (PHASE_DIFF_CNT == PHASE_DIFF_CNT_X1)
#define TMR6_POS_UNIT_CNT_UP_COND       (TMR6_CNT_UP_COND_PWMA_HIGH_PWMB_RISING)
#define TMR6_POS_UNIT_CNT_DOWN_COND     (TMR6_CNT_DOWN_COND_PWMB_HIGH_PWMA_RISING)

#elif (PHASE_DIFF_CNT == PHASE_DIFF_CNT_X2)
#define TMR6_POS_UNIT_CNT_UP_COND       (TMR6_CNT_UP_COND_PWMA_HIGH_PWMB_RISING | \
                                         TMR6_CNT_UP_COND_PWMA_LOW_PWMB_FALLING)
#define TMR6_POS_UNIT_CNT_DOWN_COND     (TMR6_CNT_DOWN_COND_PWMB_HIGH_PWMA_RISING | \
                                         TMR6_CNT_DOWN_COND_PWMB_LOW_PWMA_FALLING)
#elif (PHASE_DIFF_CNT == PHASE_DIFF_CNT_X4)
#define TMR6_POS_UNIT_CNT_UP_COND       (TMR6_CNT_UP_COND_PWMA_HIGH_PWMB_RISING  | \
                                         TMR6_CNT_UP_COND_PWMA_LOW_PWMB_FALLING  | \
                                         TMR6_CNT_UP_COND_PWMB_HIGH_PWMA_FALLING | \
                                         TMR6_CNT_UP_COND_PWMB_LOW_PWMA_RISING)
#define TMR6_POS_UNIT_CNT_DOWN_COND     (TMR6_CNT_DOWN_COND_PWMB_HIGH_PWMA_RISING  | \
                                         TMR6_CNT_DOWN_COND_PWMB_LOW_PWMA_FALLING  | \
                                         TMR6_CNT_DOWN_COND_PWMA_HIGH_PWMB_FALLING | \
                                         TMR6_CNT_DOWN_COND_PWMA_LOW_PWMB_RISING)
#else
#error "Phase-difference count is NOT supported!!!"
#endif

/* Quadrature Encoder PPR(Pulse Per Round) */
#define QE_PPR                          (1000U)
/* Quadrature Encoder cycles per round. Different Quadrature Encoder different parameter. */
#define QE_CYCLE_PER_ROUND              (QE_PPR * PHASE_DIFF_CNT)

/* when phase Z rising edge the TMR6_Z_UNIT Counter plus one */
#define TMR6_Z_UNIT_CNT_UP_COND         (TMR6_CNT_UP_COND_TRIGA_RISING)

/* Timer0 for this example */
#define TMR0_UNIT                       (CM_TMR0_1)
#define TMR0_CH                         (TMR0_CH_B)
#define TMR0_PERIPH_CLK                 (FCG2_PERIPH_TMR0_1)
#define TMR0_INT_TYPE                   (TMR0_INT_CMP_B)
#define TMR0_INT_PRIO                   (DDL_IRQ_PRIO_03)
#define TMR0_INT_SRC                    (INT_SRC_TMR0_1_CMP_B)
#define TMR0_INT_IRQn                   (INT044_IRQn)
#define TMR0_INT_FLAG                   (TMR0_FLAG_CMP_B)

/* Period 100ms, TMR0_CMP_VAL = Timer0 clock / div / 10 - 1 */
#define TMR0_CMP_VAL                    (39062UL - 1UL)
#define TMR0_CLK_DIV                    (TMR0_CLK_DIV256)

/*******************************************************************************
 * Global variable definitions (declared in header file with 'extern')
 ******************************************************************************/

/*******************************************************************************
 * Local function prototypes ('static')
 ******************************************************************************/
static void Tmr6Config(void);

/* 1 second timer, for calculating rotation speed. */
static void Tmr0Config(void);
static void TMR0_IrqCallback(void);
/*******************************************************************************
 * Local variable definitions ('static')
 ******************************************************************************/
__IO static uint8_t m_u8SpeedUpd = 0U;

/*******************************************************************************
 * Function implementation - global ('extern') and local ('static')
 ******************************************************************************/

/**
 * @brief  Main function of TIMER6 pulse encoder z count mode project
 * @param  None
 * @retval int32_t return value, if needed
 */
int32_t main(void)
{
    static uint32_t u32CurrCycleCnt, u32LastCycleCnt;
    /* MCU Peripheral registers write unprotected. */
    LL_PERIPH_WE(LL_PERIPH_GPIO | LL_PERIPH_FCG | LL_PERIPH_PWC_CLK_RMU | \
                 LL_PERIPH_EFM | LL_PERIPH_SRAM);
    /* Configure BSP */
    BSP_CLK_Init();
    DDL_PrintfInit(BSP_PRINTF_DEVICE, BSP_PRINTF_BAUDRATE, BSP_PRINTF_Preinit);
    /* Configures Timer0. */
    Tmr0Config();
    /* Configures Timer6. */
    Tmr6Config();
    /* MCU Peripheral registers write protected. */
    LL_PERIPH_WP(LL_PERIPH_GPIO | LL_PERIPH_FCG | LL_PERIPH_PWC_CLK_RMU | \
                 LL_PERIPH_EFM | LL_PERIPH_SRAM);
    /* Starts Timer6. */
    TMR6_Start(TMR6_Z_UNIT);
    TMR6_Start(TMR6_POS_UNIT);
    /* Starts Timer0. */
    TMR0_Start(TMR0_UNIT, TMR0_CH);

    /***************** Configuration end, application start **************/

    for (;;) {
        if (m_u8SpeedUpd != 0U) {
            if (TMR6_GetCountDir(TMR6_POS_UNIT) == TMR6_CNT_DOWN) {
                DDL_Printf("Quadrature Encoder direction: Anticlockwise\r\n");
                u32CurrCycleCnt = (TMR6_GetCountValue(TMR6_Z_UNIT) * QE_CYCLE_PER_ROUND) + \
                                  (TMR6_GetPeriodValue(TMR6_Z_UNIT, TMR6_PERIOD_REG_A) - TMR6_GetCountValue(TMR6_POS_UNIT));
                DDL_Printf("Quadrature Encoder speed: -%u RPM\r\n", \
                           (unsigned int)(abs(u32CurrCycleCnt - u32LastCycleCnt) * 60UL / QE_CYCLE_PER_ROUND));
            } else {
                DDL_Printf("Quadrature Encoder direction: Clockwise\r\n");
                u32CurrCycleCnt = (TMR6_GetCountValue(TMR6_Z_UNIT) * QE_CYCLE_PER_ROUND) + \
                                  TMR6_GetCountValue(TMR6_POS_UNIT);
                DDL_Printf("Quadrature Encoder speed: %u RPM\r\n", \
                           (unsigned int)(abs(u32CurrCycleCnt - u32LastCycleCnt) * 60UL / QE_CYCLE_PER_ROUND));
            }
            u32LastCycleCnt = u32CurrCycleCnt;
            m_u8SpeedUpd = 0U;
        }
    }
}

/**
 * @brief  Timer6 configuration.
 * @param  None
 * @retval None
 */
static void Tmr6Config(void)
{
    stc_tmr6_init_t stcTmr6Init;

    /* 1. Configures the function of phase A, phase B and phase Z. */
    GPIO_SetFunc(PHASE_A_PORT, PHASE_A_PIN, PHASE_A_PIN_FUNC);
    GPIO_SetFunc(PHASE_B_PORT, PHASE_B_PIN, PHASE_B_PIN_FUNC);
    GPIO_SetFunc(PHASE_Z_PORT, PHASE_Z_PIN, PHASE_Z_PIN_FUNC);

    /* 2. Enable Timer6 peripheral clock. */
    FCG_Fcg2PeriphClockCmd(TMR6_PERIPH_CLK, ENABLE);

    (void)TMR6_StructInit(&stcTmr6Init);
    /* 3. Initializes position-count unit. */
    stcTmr6Init.u8CountSrc = TMR6_CNT_SRC_HW;
    stcTmr6Init.hw_count.u32CountUpCond   = TMR6_POS_UNIT_CNT_UP_COND;
    stcTmr6Init.hw_count.u32CountDownCond = TMR6_POS_UNIT_CNT_DOWN_COND;
    (void)TMR6_Init(TMR6_POS_UNIT, &stcTmr6Init);
    /* 3.1. Enable The TMR6_POS_UNIT counter reset when phase Z rising edge */
    TMR6_HWClearCondCmd(TMR6_POS_UNIT, TMR6_CLR_COND_TRIGA_RISING, ENABLE);
    TMR6_HWClearCmd(TMR6_POS_UNIT, ENABLE);

    /* 4. Initializes z-count unit. */
    stcTmr6Init.u8CountSrc = TMR6_CNT_SRC_HW;
    stcTmr6Init.hw_count.u32CountUpCond   = TMR6_Z_UNIT_CNT_UP_COND;
    stcTmr6Init.hw_count.u32CountDownCond = 0U;
    (void)TMR6_Init(TMR6_Z_UNIT, &stcTmr6Init);

    /* 5. Enable Filter if needed. */
    TMR6_SetFilterClockDiv(TMR6_POS_UNIT, TMR6_IO_PWMA, TMR6_FILTER_CLK_DIV64);
    TMR6_FilterCmd(TMR6_POS_UNIT, TMR6_IO_PWMA, ENABLE);
    TMR6_SetFilterClockDiv(TMR6_POS_UNIT, TMR6_IO_PWMB, TMR6_FILTER_CLK_DIV64);
    TMR6_FilterCmd(TMR6_POS_UNIT, TMR6_IO_PWMB, ENABLE);
    TMR6_SetFilterClockDiv(TMR6_Z_UNIT, TMR6_INPUT_TRIGA, TMR6_FILTER_CLK_DIV64);
    TMR6_FilterCmd(TMR6_Z_UNIT, TMR6_INPUT_TRIGA, ENABLE);
}

/**
 * @brief  Timer0 configuration.
 * @param  None
 * @retval None
 */
static void Tmr0Config(void)
{
    stc_tmr0_init_t stcTmr0Init;
    stc_irq_signin_config_t stcIrq;

    /* 1. Enable Timer0 peripheral clock. */
    FCG_Fcg2PeriphClockCmd(TMR0_PERIPH_CLK, ENABLE);

    /* 2. Set a default initialization value for stcTmr0Init. */
    (void)TMR0_StructInit(&stcTmr0Init);

    /* 3. Modifies the initialization values depends on the application. */
    stcTmr0Init.u32ClockSrc     = TMR0_CLK_SRC_INTERN_CLK;
    stcTmr0Init.u32ClockDiv     = TMR0_CLK_DIV;
    stcTmr0Init.u32Func         = TMR0_FUNC_CMP;
    stcTmr0Init.u16CompareValue = (uint16_t)TMR0_CMP_VAL;
    (void)TMR0_Init(TMR0_UNIT, TMR0_CH, &stcTmr0Init);

    /* 4. Configures IRQ if needed. */
    stcIrq.enIntSrc    = TMR0_INT_SRC;
    stcIrq.enIRQn      = TMR0_INT_IRQn;
    stcIrq.pfnCallback = &TMR0_IrqCallback;
    (void)INTC_IrqSignIn(&stcIrq);
    NVIC_ClearPendingIRQ(stcIrq.enIRQn);
    NVIC_SetPriority(stcIrq.enIRQn, TMR0_INT_PRIO);
    NVIC_EnableIRQ(stcIrq.enIRQn);
    /* Enable the specified interrupts of Timer0. */
    TMR0_IntCmd(TMR0_UNIT, TMR0_INT_TYPE, ENABLE);
}

/**
 * @brief  Timer0 interrupt callback function.
 * @param  None
 * @retval None
 */
static void TMR0_IrqCallback(void)
{
    static uint32_t u32TmrCnt = 0U;

    u32TmrCnt++;
    /* 100ms * 10 = 1s */
    if (u32TmrCnt >= 10U) {
        m_u8SpeedUpd = 1U;
        u32TmrCnt    = 0U;
    }
    TMR0_ClearStatus(TMR0_UNIT, TMR0_INT_FLAG);

    __DSB();  /* Arm Errata 838869 */
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
