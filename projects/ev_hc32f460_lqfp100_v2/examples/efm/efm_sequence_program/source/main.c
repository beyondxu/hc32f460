/**
 *******************************************************************************
 * @file  efm/efm_sequence_program/source/main.c
 * @brief Main program of EFM for the Device Driver Library.
 @verbatim
   Change Logs:
   Date             Author          Notes
   2022-03-31       CDT             First version
   2023-09-30       CDT             Re-structure
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
 * @addtogroup EFM_Sequence_Program
 * @{
 */

/*******************************************************************************
 * Local type definitions ('typedef')
 ******************************************************************************/

/*******************************************************************************
 * Local pre-processor symbols/macros ('#define')
 ******************************************************************************/
#define EFM_SECTOR30_NUM        (30U)
#define BUF_SIZE                (0x100U)

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
 * @brief  Main function of EFM project
 * @param  None
 * @retval int32_t return value, if needed
 */
int32_t main(void)
{
    uint32_t u32Addr;
    uint32_t i;
    uint8_t u8TestBuf[BUF_SIZE] = {0U};
    uint8_t u8ReadBuf[BUF_SIZE] = {0U};
    int32_t i32Ret;

    /* Register write enable for some required peripherals. */
    LL_PERIPH_WE(LL_PERIPH_GPIO | LL_PERIPH_PWC_CLK_RMU | LL_PERIPH_FCG | LL_PERIPH_EFM | LL_PERIPH_SRAM);

    /* System clock init */
    BSP_CLK_Init();
    /* LED init */
    BSP_LED_Init();
    /* KEY init */
    BSP_KEY_Init();

    /* Wait flash ready. */
    while (SET != EFM_GetStatus(EFM_FLAG_RDY)) {
        ;
    }

    /* EFM_FWMC write enable */
    EFM_FWMC_Cmd(ENABLE);
    /* Hold bus while erase & program */
    EFM_SetBusStatus(EFM_BUS_HOLD);

    /* Init buf */
    for (i = 0UL; i < BUF_SIZE; i++) {
        u8TestBuf[i] = (uint8_t)(i % 256U);
    }

    for (;;) {
        if (SET == BSP_KEY_GetStatus(BSP_KEY_10)) {
            u32Addr = EFM_SECTOR_ADDR(EFM_SECTOR30_NUM);
            /* Sector erase */
            (void)EFM_SectorErase(u32Addr);
            /* Sequence program */
            (void)EFM_SequenceProgram(u32Addr, u8TestBuf, BUF_SIZE);
            /* Read back */
            (void)EFM_ReadByte(u32Addr, u8ReadBuf, BUF_SIZE);
            /* Compare */
            i32Ret = memcmp(u8ReadBuf, u8TestBuf, BUF_SIZE);
            if (0 == i32Ret) {
                /* LED blue, as expected */
                BSP_LED_On(LED_BLUE);
            } else {
                /* LED red */
                BSP_LED_On(LED_RED);
            }
            /* Clear read buf */
            for (i = 0UL; i < BUF_SIZE; i++) {
                u8ReadBuf[i] = 0U;
            }
        }
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
