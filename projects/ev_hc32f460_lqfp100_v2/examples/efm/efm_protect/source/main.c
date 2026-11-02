/**
 *******************************************************************************
 * @file  efm/efm_protect/source/main.c
 * @brief Main program of EFM for the Device Driver Library.
 @verbatim
   Change Logs:
   Date             Author          Notes
   2023-01-15       CDT             First version
   2024-11-08       CDT             Disable cache while chip erase & set read only after chip erase
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
 * @addtogroup EFM_Base
 * @{
 */

/*******************************************************************************
 * Local type definitions ('typedef')
 ******************************************************************************/

/*******************************************************************************
 * Local pre-processor symbols/macros ('#define')
 ******************************************************************************/
#define SECURITY_LEN                (12U)

/*******************************************************************************
 * Global variable definitions (declared in header file with 'extern')
 ******************************************************************************/

/*******************************************************************************
 * Local function prototypes ('static')
 ******************************************************************************/

/*******************************************************************************
 * Local variable definitions ('static')
 ******************************************************************************/
static uint8_t u8SecurityCode[12] = {0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31};

/*******************************************************************************
 * Function implementation - global ('extern') and local ('static')
 ******************************************************************************/
/**
 * @brief  Release protect
 * @param  None
 * @retval None
 */
__NOINLINE __RAM_FUNC void Release_protect(void)
{
    /* EFM Protect Unlock. */
    WRITE_REG32(CM_EFM->FAPRT, EFM_REG_UNLOCK_KEY1);
    WRITE_REG32(CM_EFM->FAPRT, EFM_REG_UNLOCK_KEY2);

    /* EFM_FWMC write enable */
    SET_REG32_BIT(CM_EFM->FWMC, EFM_FWMC_PEMODE);

    /* disable Cache */
    CLR_REG32_BIT(CM_EFM->FRMC, EFM_CACHE_ALL);

    /* chip erase */
    MODIFY_REG32(CM_EFM->FWMC, EFM_FWMC_PEMOD, EFM_MD_ERASE_ALL_CHIP);
    RW_MEM32(0x1000) = 0UL;
    while (EFM_FLAG_RDY != READ_REG32_BIT(CM_EFM->FSR, EFM_FLAG_RDY));
    SET_REG32_BIT(CM_EFM->FSCLR, EFM_FLAG_OPTEND);
    /* read only */
    MODIFY_REG32(CM_EFM->FWMC, EFM_FWMC_PEMOD, EFM_MD_READONLY);

    /* Lock & protect */
    CLR_REG32_BIT(CM_EFM->FWMC, EFM_FWMC_PEMODE);
    WRITE_REG32(CM_EFM->FAPRT, EFM_REG_LOCK_KEY);
}

/**
 * @brief  Main function of EFM project
 * @param  None
 * @retval int32_t return value, if needed
 */
int32_t main(void)
{
    /* Register write enable for some required peripherals. */
    LL_PERIPH_WE(LL_PERIPH_GPIO | LL_PERIPH_PWC_CLK_RMU | LL_PERIPH_FCG | LL_PERIPH_EFM | LL_PERIPH_SRAM);

    /* System clock init */
    BSP_CLK_Init();
    /* LED init */
    BSP_LED_Init();
    /* System key init */
    BSP_KEY_Init();
    DDL_DelayMS(1000Ul);
    BSP_LED_On(LED_BLUE);
    DDL_DelayMS(1000Ul);
    BSP_LED_Off(LED_BLUE);
    DDL_DelayMS(1000Ul);
    BSP_LED_On(LED_BLUE);
    /* enable write fwmc register */
    EFM_FWMC_Cmd(ENABLE);
    /* Register write protected for some required peripherals. */
    LL_PERIPH_WP(LL_PERIPH_GPIO | LL_PERIPH_PWC_CLK_RMU | LL_PERIPH_FCG | LL_PERIPH_SRAM);

    for (;;) {
        if (SET == BSP_KEY_GetStatus(BSP_KEY_1)) {
            BSP_LED_Off(LED_BLUE);
            BSP_LED_On(LED_RED);
            EFM_WriteSecurityCode(u8SecurityCode, SECURITY_LEN);
            EFM_Protect_Enable(EFM_PROTECT_LEVEL1);
        } else if (SET == BSP_KEY_GetStatus(BSP_KEY_2)) {
            BSP_LED_Off(LED_BLUE);
            BSP_LED_On(LED_RED);
            EFM_Protect_Enable(EFM_PROTECT_LEVEL2);
        }

        if (SET == BSP_KEY_GetStatus(BSP_KEY_10)) {
            Release_protect();
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
