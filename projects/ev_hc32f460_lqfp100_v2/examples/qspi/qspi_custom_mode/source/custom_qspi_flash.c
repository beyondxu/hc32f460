/**
 *******************************************************************************
 * @file  qspi/qspi_custom_mode/source/custom_qspi_flash.c
 * @brief This file provides firmware functions to the QSPI accesses flash.
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
#include "custom_qspi_flash.h"

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
 * @defgroup QSPI_FLASH_Global_Functions QSPI_FLASH Global Functions
 * @{
 */
/**
 * @brief  Convert word to bytes.
 * @param  [in] u32Word                 The word value.
 * @param  [in] pu8Byte                 Pointer to the byte buffer.
 * @retval None
 */
static void QSPI_FLASH_WordToByte(uint32_t u32Word, uint8_t *pu8Byte)
{
    uint32_t u32ByteNum;
    uint8_t u8Count = 0U;

    u32ByteNum = QSPI_FLASH_ADDR_WIDTH;
    do {
        pu8Byte[u8Count++] = (uint8_t)(u32Word >> (u32ByteNum * 8U)) & 0xFFU;
    } while ((u32ByteNum--) != 0UL);
}

/**
 * @brief  Write data by direct mode.
 * @param  [in] u32SelLine              Select line @ref QSPI_Comm_Line_Select.
 * @param  [in] pu8WriteBuf             Pointer to the byte buffer.
 * @param  [in] u32Size                 Size of byte buffer.
 * @retval None
 */
static void Qspi_WriteDirectMode(uint32_t u32SelLine, uint8_t *pu8WriteBuf, uint32_t u32Size)
{
    uint32_t u32Count;
    stc_qspi_custom_mode_t stcQspiCustomMode;

    if ((NULL != pu8WriteBuf) && (u32Size != 0U)) {
        stcQspiCustomMode.u32InstrProtocol = u32SelLine << QSPI_CR_IPRSL_POS;
        stcQspiCustomMode.u32AddrProtocol  = u32SelLine << QSPI_CR_APRSL_POS;;
        stcQspiCustomMode.u32DataProtocol  = u32SelLine << QSPI_CR_DPRSL_POS;;
        stcQspiCustomMode.u8InstrCode = 0U;
        (void)QSPI_CustomReadConfig(&stcQspiCustomMode);

        for (u32Count = 0UL; u32Count < u32Size; u32Count++) {
            QSPI_WriteDirectCommValue(pu8WriteBuf[u32Count]);
        }
        while (SET == QSPI_GetStatus(QSPI_FLAG_DIRECT_COMM_BUSY));
    }
}

/**
 * @brief  Read data by direct mode.
 * @param  [in] u32SelLine              Select line @ref QSPI_Comm_Line_Select.
 * @param  [in] pu8ReadBuf              Pointer to the byte buffer.
 * @param  [in] u32Size                 Size of byte buffer.
 * @retval None
 */
static void Qspi_ReadDirectMode(uint32_t u32SelLine, uint8_t *pu8ReadBuf, uint32_t u32Size)
{
    uint32_t u32Count;
    stc_qspi_custom_mode_t stcQspiCustomMode;

    if ((NULL != pu8ReadBuf) && (u32Size != 0U)) {
        stcQspiCustomMode.u32InstrProtocol = u32SelLine << QSPI_CR_IPRSL_POS;
        stcQspiCustomMode.u32AddrProtocol  = u32SelLine << QSPI_CR_APRSL_POS;;
        stcQspiCustomMode.u32DataProtocol  = u32SelLine << QSPI_CR_DPRSL_POS;;
        stcQspiCustomMode.u8InstrCode = 0U;
        (void)QSPI_CustomReadConfig(&stcQspiCustomMode);

        for (u32Count = 0UL; u32Count < u32Size; u32Count++) {
            pu8ReadBuf[u32Count] = QSPI_ReadDirectCommValue();
        }
        while (SET == QSPI_GetStatus(QSPI_FLAG_DIRECT_COMM_BUSY));
    }
}

/**
 * @brief  Wait for operation done.
 * @param  [in] u32Timeout              Wait time (u32Timeout * 400us)
 * @retval int32_t:
 *           - LL_OK: No errors occurred.
 *           - LL_ERR_TIMEOUT: Works timeout.
 */
static int32_t QSPI_FLASH_WaitForDone(uint32_t u32Timeout)
{
    uint8_t u8Status;
    int32_t i32Ret = LL_ERR_TIMEOUT;
    qspi_direct_config_t qspi_direct_config;

    /* Read status register */
    qspi_direct_config.u8Instru     = W25Q64_RD_STATUS_REG1;
    qspi_direct_config.u8InstruLine = QSPI_COMM_1_LINE;
    qspi_direct_config.u32AddrLen   = 0U;
    qspi_direct_config.pu8Buf       = &u8Status;
    qspi_direct_config.u32Size      = 1U;
    qspi_direct_config.u8DataLine   = QSPI_COMM_1_LINE;

    do {
        DDL_DelayUS(400U);
        QSPI_FLASH_ReadDirect(&qspi_direct_config);
        if (0U == (u8Status & W25Q64_FLAG_BUSY)) {
            i32Ret = LL_OK;
            break;
        }
    } while ((u32Timeout--) != 0UL);

    return i32Ret;
}

/**
 * @brief  Qspi configuration.
 * @param  None
 * @retval None
 */
void QSPI_FLASH_Config(void)
{
    stc_gpio_init_t stcGpioInit;
    stc_qspi_init_t stcQspiInit;

    (void)GPIO_StructInit(&stcGpioInit);
    stcGpioInit.u16PinDrv = PIN_HIGH_DRV;
    (void)GPIO_Init(QSPI_FLASH_SCK_PORT, QSPI_FLASH_SCK_PIN, &stcGpioInit);
    (void)GPIO_Init(QSPI_FLASH_IO0_PORT, QSPI_FLASH_IO0_PIN, &stcGpioInit);
    (void)GPIO_Init(QSPI_FLASH_IO1_PORT, QSPI_FLASH_IO1_PIN, &stcGpioInit);
    (void)GPIO_Init(QSPI_FLASH_IO2_PORT, QSPI_FLASH_IO2_PIN, &stcGpioInit);
    (void)GPIO_Init(QSPI_FLASH_IO3_PORT, QSPI_FLASH_IO3_PIN, &stcGpioInit);
    stcGpioInit.u16PinDir   = PIN_DIR_OUT;
    stcGpioInit.u16PinState = PIN_STAT_SET;
    (void)GPIO_Init(QSPI_FLASH_CS_PORT, QSPI_FLASH_CS_PIN,  &stcGpioInit);

    GPIO_SetFunc(QSPI_FLASH_SCK_PORT, QSPI_FLASH_SCK_PIN, QSPI_FLASH_SCK_FUNC);
    GPIO_SetFunc(QSPI_FLASH_IO0_PORT, QSPI_FLASH_IO0_PIN, QSPI_FLASH_IO0_FUNC);
    GPIO_SetFunc(QSPI_FLASH_IO1_PORT, QSPI_FLASH_IO1_PIN, QSPI_FLASH_IO1_FUNC);
    GPIO_SetFunc(QSPI_FLASH_IO2_PORT, QSPI_FLASH_IO2_PIN, QSPI_FLASH_IO2_FUNC);
    GPIO_SetFunc(QSPI_FLASH_IO3_PORT, QSPI_FLASH_IO3_PIN, QSPI_FLASH_IO3_FUNC);

    FCG_Fcg1PeriphClockCmd(QSPI_FLASH_CLK, ENABLE);
    (void)QSPI_StructInit(&stcQspiInit);
    stcQspiInit.u32ClockDiv       = QSPI_CLK_DIV32;
    stcQspiInit.u32ReadMode       = QSPI_FLASH_RD_MD;
    stcQspiInit.u32PrefetchMode   = QSPI_PREFETCH_MD_EDGE_STOP;
    stcQspiInit.u32DummyCycle     = QSPI_FLASH_RD_DUMMY_CYCLE;
    stcQspiInit.u32AddrWidth      = QSPI_FLASH_ADDR_WIDTH;
    stcQspiInit.u32SetupTime      = QSPI_QSSN_SETUP_ADVANCE_QSCK1P5;
    stcQspiInit.u32ReleaseTime    = QSPI_QSSN_RELEASE_DELAY_QSCK1P5;
    stcQspiInit.u32IntervalTime   = QSPI_QSSN_INTERVAL_QSCK2;
    (void)QSPI_Init(&stcQspiInit);
}

/**
 * @brief  Write data into flash by direct mode.
 * @param  [in] qspi_direct_config      Struct config for qspi.
 * @retval int32_t:
 *           - LL_OK: No errors occurred.
 *           - LL_ERR_TIMEOUT: Works timeout.
 */
int32_t QSPI_FLASH_WriteDirect(qspi_direct_config_t *qspi_direct_config)
{
    uint8_t u8AddrBuf[4U];
    uint8_t  u8Cmd = W25Q64_WR_ENABLE;
    uint32_t u32TempSize;
    int32_t i32Ret = LL_OK;

    if (qspi_direct_config != NULL) {
        do {
            if (qspi_direct_config->u32Size >= W25Q64_PAGE_SIZE) {
                u32TempSize = W25Q64_PAGE_SIZE;
            } else {
                u32TempSize = qspi_direct_config->u32Size;
            }
            QSPI_FLASH_WordToByte(qspi_direct_config->u32Addr, u8AddrBuf);
            /* Enter direct communication mode */
            SET_REG32_BIT(CM_QSPI->CR, QSPI_CR_DCOME);
            /* Write enable */
            GPIO_ResetPins(QSPI_FLASH_CS_PORT, QSPI_FLASH_CS_PIN);
            Qspi_WriteDirectMode(QSPI_COMM_1_LINE, &u8Cmd, 1U);
            GPIO_SetPins(QSPI_FLASH_CS_PORT, QSPI_FLASH_CS_PIN);
            DDL_DelayUS(1U);
            /* Programming */
            GPIO_ResetPins(QSPI_FLASH_CS_PORT, QSPI_FLASH_CS_PIN);
            Qspi_WriteDirectMode(qspi_direct_config->u8InstruLine, &qspi_direct_config->u8Instru, 1U);
            Qspi_WriteDirectMode(qspi_direct_config->u8AddrLine, u8AddrBuf, qspi_direct_config->u32AddrLen);
            Qspi_WriteDirectMode(qspi_direct_config->u8DataLine, qspi_direct_config->pu8Buf, u32TempSize);
            GPIO_SetPins(QSPI_FLASH_CS_PORT, QSPI_FLASH_CS_PIN);
            /* Exit direct communication mode */
            CLR_REG32_BIT(CM_QSPI->CR, QSPI_CR_DCOME);

            i32Ret = QSPI_FLASH_WaitForDone(1000U);
            if (LL_OK != i32Ret) {
                break;
            }
            qspi_direct_config->u32Addr += u32TempSize;
            qspi_direct_config->pu8Buf  += u32TempSize;
            qspi_direct_config->u32Size -= u32TempSize;
        } while (0U != qspi_direct_config->u32Size);
    }

    return i32Ret;
}

/**
 * @brief  Read data from flash by direct mode.
 * @param  [in] qspi_direct_config      Struct config for qspi.
 * @retval None
 */
void QSPI_FLASH_ReadDirect(qspi_direct_config_t *qspi_direct_config)
{
    uint8_t u8AddrBuf[4U];

    if (qspi_direct_config != NULL) {
        QSPI_FLASH_WordToByte(qspi_direct_config->u32Addr, u8AddrBuf);
        /* Enter direct communication mode */
        SET_REG32_BIT(CM_QSPI->CR, QSPI_CR_DCOME);
        /* Read data */
        GPIO_ResetPins(QSPI_FLASH_CS_PORT, QSPI_FLASH_CS_PIN);
        Qspi_WriteDirectMode(qspi_direct_config->u8InstruLine, &qspi_direct_config->u8Instru, 1U);
        Qspi_WriteDirectMode(qspi_direct_config->u8AddrLine, u8AddrBuf, qspi_direct_config->u32AddrLen);
        Qspi_ReadDirectMode(qspi_direct_config->u8DataLine, qspi_direct_config->pu8Buf, qspi_direct_config->u32Size);
        GPIO_SetPins(QSPI_FLASH_CS_PORT, QSPI_FLASH_CS_PIN);
        /* Exit direct communication mode */
        CLR_REG32_BIT(CM_QSPI->CR, QSPI_CR_DCOME);
    }
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

/******************************************************************************
 * EOF (not truncated)
 *****************************************************************************/
