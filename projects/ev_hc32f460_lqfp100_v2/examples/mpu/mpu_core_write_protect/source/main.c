/**
 *******************************************************************************
 * @file  mpu/mpu_core_write_protect/source/main.c
 * @brief Main program of MPU write protect for the Device Driver Library.
 @verbatim
   Change Logs:
   Date             Author          Notes
   2022-03-31       CDT             First version
   2023-09-30       CDT             MemManage_Handler add __DSB for Arm Errata 838869
                                    Modify trigger condition for RTC protection
                                    Optimize RTC init sequence
   2024-11-08       CDT             Modify protect region from RTC to SRAM
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
 * @addtogroup MPU_Core_Write_Protect
 * @{
 */

/*******************************************************************************
 * Local type definitions ('typedef')
 ******************************************************************************/

/*******************************************************************************
 * Local pre-processor symbols/macros ('#define')
 ******************************************************************************/
/* unlock/lock peripheral */
#define EXAMPLE_PERIPH_WE               (LL_PERIPH_GPIO | LL_PERIPH_EFM | LL_PERIPH_FCG | \
                                         LL_PERIPH_PWC_CLK_RMU | LL_PERIPH_SRAM | LL_PERIPH_MPU)
#define EXAMPLE_PERIPH_WP               (LL_PERIPH_EFM | LL_PERIPH_FCG | LL_PERIPH_SRAM)

/* SRAM test address */
#define SRAM_SIZE                       (188 * 1024UL)
#define SRAM_TEST_ADDR                  (SRAM_BASE + SRAM_SIZE - 1024U)
#define SRAM_TEST_DATA                  (0xA5A5A5A5UL)

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
 * @brief  Memory management fault callback
 * @param  None
 * @retval None
 */
void MemManage_Handler(void)
{
    BSP_LED_On(LED_RED);
    BSP_LED_Off(LED_BLUE);

    __DSB();  /* Arm Errata 838869 */
}

/**
 * @brief  MPU configuration.
 * @param  None
 * @retval None
 */
static void MPU_Config(void)
{
    stc_core_mpu_config_t stcMpuConfig;
    stc_core_mpu_region_init_t stcRegionInit;

    CORE_MPU_Cmd(DISABLE);
    stcMpuConfig.u32BackgroundPermission = CORE_MPU_BKGRD_PRIV_ACCESS_ENABLE;
    stcMpuConfig.u32NmiFaultPermission   = CORE_MPU_NMI_FAULT_DISABLE_MPU;
    (void)CORE_MPU_Config(&stcMpuConfig);

    CORE_MPU_RegionStructInit(&stcRegionInit);
    stcRegionInit.u32BaseAddr           = SRAM_TEST_ADDR;
    stcRegionInit.u32Size               = CORE_MPU_REGION_SIZE_1KBYTE;
    stcRegionInit.u32InstruAccess       = CORE_MPU_INSTRU_ACCESS_ENABLE;
    stcRegionInit.u32AccessPermission   = CORE_MPU_REGION_PRIV_RO;
    stcRegionInit.u32TypeExtend         = CORE_MPU_TYPE_EXTEND_LVL0;
    stcRegionInit.u32SubRegion          = 0x00UL;
    CORE_MPU_RegionInit(CORE_MPU_REGION_NUM2, &stcRegionInit);
    CORE_MPU_RegionCmd(CORE_MPU_REGION_NUM2, ENABLE);
    CORE_MPU_Cmd(ENABLE);
}

/**
 * @brief  Main function of MPU write protect.
 * @param  None
 * @retval int32_t return value, if needed
 */
int32_t main(void)
{
    /* Peripheral registers write unprotected */
    LL_PERIPH_WE(EXAMPLE_PERIPH_WE);
    /* Configure BSP */
    BSP_CLK_Init();
    BSP_LED_Init();
    BSP_KEY_Init();
    /* Configure UART */
    DDL_PrintfInit(BSP_PRINTF_DEVICE, BSP_PRINTF_BAUDRATE, BSP_PRINTF_Preinit);
    /* Init the value of test address */
    RW_MEM32(SRAM_TEST_ADDR) = SRAM_TEST_DATA;
    /* Configure MPU */
    MPU_Config();
    /* Peripheral registers write protected */
    LL_PERIPH_WP(EXAMPLE_PERIPH_WP);

    for (;;) {
        if (SET == BSP_KEY_GetStatus(BSP_KEY_2)) {
            if (SRAM_TEST_DATA == RW_MEM32(SRAM_TEST_ADDR)) {
                BSP_LED_Off(LED_RED);
                BSP_LED_On(LED_BLUE);
                DDL_Printf("The read value is equal to the init value!\r\n");
            } else {
                DDL_Printf("The read value isn't equal to the init value!\r\n");
            }
        }
        if (SET == BSP_KEY_GetStatus(BSP_KEY_3)) {
            /* Write value to test address */
            RW_MEM32(SRAM_TEST_ADDR) = 0UL;
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
