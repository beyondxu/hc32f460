/**
 *******************************************************************************
 * @file  functional_safety/iec60730_class_b/source/test_impl_item/test_impl_wdt.c
 * @brief This file provides firmware functions to implement the watch test.
 @verbatim
   Change Logs:
   Date             Author          Notes
   2022-03-31       CDT             First version
   2024-11-08       CDT             Replace peripheral WDT with SWDT to avoid dependencies on the system clock
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
#include "hc32_ll.h"
#include "stl_utility.h"
#include "test_impl_wdt.h"

/**
 * @addtogroup STL_IEC60730_Application
 * @{
 */

/**
 * @defgroup Test_Implement_WDT Test Implement Watchdog
 * @{
 */

/*******************************************************************************
 * Local pre-processor symbols/macros ('#define')
 ******************************************************************************/
#define WDT_TIMEOUT                 (500000UL)

/*******************************************************************************
 * Global variable definitions (declared in header file with 'extern')
 ******************************************************************************/

/*******************************************************************************
 * Local type definitions ('typedef')
 ******************************************************************************/

/*******************************************************************************
 * Local function prototypes ('static')
 ******************************************************************************/

/*******************************************************************************
 * Local variable definitions ('static')
 ******************************************************************************/

/*******************************************************************************
 * Local pre-processor symbols/macros ('#define')
 ******************************************************************************/

/*******************************************************************************
 * Function implementation - global ('extern') and local ('static')
 ******************************************************************************/

/**
 * @defgroup Test_Implement_WDT_Global_Functions Test Implement Watchdog Global Functions
 * @{
 */

/**
 * @brief  Watchdog test in startup.
 * @param  None
 * @retval uint32_t:
 *           - STL_OK:          Test pass.
 *           - STL_ERR:         Test fail.
 */
uint32_t STL_WdtStartupTest(void)
{
    uint32_t i;
    uint32_t u32Ret;

    if (SET == RMU_GetStatus(RMU_FLAG_SWDT)) {
        RMU_ClearStatus();
        STL_WdtRuntimeFeed();
        u32Ret = STL_OK;
    } else {
        for (i = 0UL; i < WDT_TIMEOUT; i++) {
            STL_DelayMS(1UL);
        }
        u32Ret = STL_ERR;
    }

    return u32Ret;
}

/**
 * @brief  Watchdog test initialize in runtime.
 * @param  None
 * @retval uint32_t:
 *           - STL_OK:          Initialize successfully.
 *           - STL_ERR:         Initialize unsuccessfully.
 */
uint32_t STL_WdtRuntimeInit(void)
{
    /* SWDT only support hardware start, so don't need configure */
    STL_WdtRuntimeFeed();
    return STL_OK;
}

/**
 * @brief  Watchdog feed in runtime.
 * @param  None
 * @retval uint32_t:
 *           - STL_OK:          Test pass.
 */
uint32_t STL_WdtRuntimeFeed(void)
{
    SWDT_FeedDog();
    return STL_OK;
}

/**
 * @}
 */

/**
 * @}
 */

/**
 * @}
 */

/*******************************************************************************
 * EOF (not truncated)
 ******************************************************************************/
