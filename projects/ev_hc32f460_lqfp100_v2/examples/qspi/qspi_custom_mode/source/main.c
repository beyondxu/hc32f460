/**
 *******************************************************************************
 * @file  qspi/qspi_custom_mode/source/main.c
 * @brief Main program of QSPI custom mode for the Device Driver Library.
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
 * @addtogroup QSPI_Custom_Mode
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
                                         LL_PERIPH_PWC_CLK_RMU | LL_PERIPH_SRAM)
#define EXAMPLE_PERIPH_WP               (LL_PERIPH_EFM | LL_PERIPH_FCG | LL_PERIPH_SRAM)

/* Test data size */
#define TEST_DATA_SIZE                  (0x1000UL)

/*******************************************************************************
 * Global variable definitions (declared in header file with 'extern')
 ******************************************************************************/

/*******************************************************************************
 * Local function prototypes ('static')
 ******************************************************************************/

/*******************************************************************************
 * Local variable definitions ('static')
 ******************************************************************************/
static uint8_t m_au8WriteData[TEST_DATA_SIZE];
static uint8_t m_au8ReadData[TEST_DATA_SIZE];

/*******************************************************************************
 * Function implementation - global ('extern') and local ('static')
 ******************************************************************************/
/**
 * @brief  Init the test data.
 * @param  None
 * @retval None
 */
static void QSPI_TestDataInit(void)
{
    uint32_t i;

    for (i = 0UL; i < TEST_DATA_SIZE; i++) {
        m_au8WriteData[i] = (uint8_t)(i % 256U);
        m_au8ReadData[i]  = 0U;
    }
}

/**
 * @brief  Check the program result.
 * @param  None
 * @retval None
 */
static int32_t QSPI_CheckProgramResult(void)
{
    uint32_t i;
    int32_t i32Ret = LL_OK;

    for (i = 0UL; i < TEST_DATA_SIZE; i++) {
        if (m_au8ReadData[i] != m_au8WriteData[i]) {
            i32Ret = LL_ERR;
            break;
        }
    }

    return i32Ret;
}

/**
 * @brief  Main function of QSPI custom mode project
 * @param  None
 * @retval None
 */
int main(void)
{
    int32_t i32Ret;
    uint32_t u32FlashAddr = 0UL;
    qspi_direct_config_t qspi_direct_config;

    /* Peripheral registers write unprotected */
    LL_PERIPH_WE(EXAMPLE_PERIPH_WE);
    /* Configure BSP */
    BSP_CLK_Init();
    BSP_LED_Init();
    BSP_KEY_Init();
    /* Configure QSPI */
    QSPI_FLASH_Config();
    /* Peripheral registers write protected */
    LL_PERIPH_WP(EXAMPLE_PERIPH_WP);

    for (;;) {
        if (SET == BSP_KEY_GetStatus(BSP_KEY_2)) {
            BSP_LED_Off(LED_RED);
            BSP_LED_Off(LED_BLUE);
            QSPI_TestDataInit();
            /* Erase sector */
            qspi_direct_config.u8Instru     = W25Q64_SECTOR_ERASE;
            qspi_direct_config.u8InstruLine = QSPI_COMM_1_LINE;
            qspi_direct_config.u32Addr      = u32FlashAddr;
            qspi_direct_config.u32AddrLen   = 3U;
            qspi_direct_config.u8AddrLine   = QSPI_COMM_1_LINE;
            qspi_direct_config.u32Size      = 0U;
            i32Ret = QSPI_FLASH_WriteDirect(&qspi_direct_config);
            if (LL_OK == i32Ret) {
                /* Programming */
                qspi_direct_config.u8Instru     = W25Q64_QUAD_PROGRAM;
                qspi_direct_config.u8InstruLine = QSPI_COMM_1_LINE;
                qspi_direct_config.u32Addr      = u32FlashAddr;
                qspi_direct_config.u32AddrLen   = 3U;
                qspi_direct_config.u8AddrLine   = QSPI_COMM_1_LINE;
                qspi_direct_config.pu8Buf       = m_au8WriteData;
                qspi_direct_config.u32Size      = TEST_DATA_SIZE;
                qspi_direct_config.u8DataLine   = QSPI_COMM_4_LINE;
                i32Ret = QSPI_FLASH_WriteDirect(&qspi_direct_config);
                if (LL_OK == i32Ret) {
                    /* Read */
                    qspi_direct_config.u8Instru     = W25Q64_RD_DATA;
                    qspi_direct_config.u8InstruLine = QSPI_COMM_1_LINE;
                    qspi_direct_config.u32Addr      = u32FlashAddr;
                    qspi_direct_config.u32AddrLen   = 3U;
                    qspi_direct_config.u8AddrLine   = QSPI_COMM_1_LINE;
                    qspi_direct_config.pu8Buf       = m_au8ReadData;
                    qspi_direct_config.u32Size      = TEST_DATA_SIZE;
                    qspi_direct_config.u8DataLine   = QSPI_COMM_1_LINE;
                    QSPI_FLASH_ReadDirect(&qspi_direct_config);
                }
            }
            /* Compare */
            if (LL_OK == QSPI_CheckProgramResult()) {
                BSP_LED_On(LED_BLUE);
                BSP_LED_Off(LED_RED);
            } else {
                BSP_LED_On(LED_RED);
                BSP_LED_Off(LED_BLUE);
            }
            /* Flash address offset */
            u32FlashAddr += W25Q64_SECTOR_SIZE;
            if ((u32FlashAddr + TEST_DATA_SIZE) >= W25Q64_MAX_ADDR) {
                u32FlashAddr = 0U;
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
