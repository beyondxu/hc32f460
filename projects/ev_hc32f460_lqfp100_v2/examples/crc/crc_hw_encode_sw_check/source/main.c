/**
 *******************************************************************************
 * @file  crc/crc_hw_encode_sw_check/source/main.c
 * @brief This example demonstrates CRC compare with software.
 @verbatim
   Change Logs:
   Date             Author          Notes
   2022-03-31       CDT             First version
   2023-09-30       CDT             Functional reconstruction
   2024-11-08       CDT             Fix cppcheck warning
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
 * @addtogroup CRC_HW_Encode_SW_Check
 * @{
 */

/*******************************************************************************
 * Local type definitions ('typedef')
 ******************************************************************************/
typedef struct {
    uint64_t Poly;              /*!< Specifies the CRC Poly.
                                     This parameter can be a value of @ref CRC_POLY */
    uint64_t InitValue;
    uint64_t XorOut;
    uint8_t RefIn;
    uint8_t RefOut;
} stc_swcrc_init_t;

/*******************************************************************************
 * Local pre-processor symbols/macros ('#define')
 ******************************************************************************/
#define M_True                          (1U)
#define M_False                         (0U)

#define CRC16_INIT_VALUE                (0xFFFFUL)
#define CRC32_INIT_VALUE                (0xFFFFFFFFUL)

/* Hardware Algorithm types supported by CRC16 */
#define CRC16_X25                       (0U)
#define CRC16_XMODEM                    (1U)
#define CRC16_CCITT                     (2U)
#define CRC16_CCITT_FALSE               (3U)
/* Hardware Algorithm types supported by CRC32 */
#define CRC32                           (0U)
#define CRC32_MPEG2                     (1U)

#define CRC16_ALGORITHM_SELECT          (CRC16_X25)
#define CRC32_ALGORITHM_SELECT          (CRC32)

/* Peripheral register WE/WP selection */
#define LL_PERIPH_SEL                   (LL_PERIPH_GPIO | LL_PERIPH_FCG | LL_PERIPH_PWC_CLK_RMU)

/**
 * @defgroup CRC_DATA_Bit_Width CRC Data Bit Width
 * @{
 */
#define SW_CRC_DATA_WIDTH_8BIT          (1U)
#define SW_CRC_DATA_WIDTH_16BIT         (2U)
#define SW_CRC_DATA_WIDTH_32BIT         (4U)
/**
 * @}
 */

/**
 * @defgroup CRC_POLY CRC Poly
 * @{
 */
#define CRC16_X25_POLY                  (0x1021U)
#define CRC16_XMODE_POLY                (0x1021U)
#define CRC16_CCITT_POLY                (0x1021U)
#define CRC16_CCITT_FALSE_POLY          (0x1021U)
#define CRC32_POLY                      (0x04C11DB7UL)
#define CRC32_MPEG2_POLY                (0x04C11DB7UL)
/**
 * @}
 */

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
 * @brief InvertUint8.
 * @param [out] dBuf          The data after the Invert.
 * @param [in]  srcBuf        Data that is used to Invert.
 * @retval none
 */
static void InvertUint8(uint8_t *dBuf, uint8_t *srcBuf)
{
    uint32_t i;
    uint8_t Temp = 0;
    for (i = 0; i < 8U; i++) {
        if ((*srcBuf) & (1U << i)) {
            Temp |= 1U << (7U - i);
        }
    }
    *dBuf = Temp;
}

/**
 * @brief InvertUint16.
 * @param [out] dBuf          The data after the Invert.
 * @param [in]  srcBuf        Data that is used to Invert.
 * @retval none
 */
static void InvertUint16(uint16_t *dBuf, uint16_t *srcBuf)
{
    uint32_t i;
    uint16_t Temp = 0;
    for (i = 0; i < 16U; i++) {
        if ((*srcBuf) & (1U << i)) {
            Temp |= 1U << (15U - i);
        }
    }
    *dBuf = Temp;
}

/**
 * @brief CRC-16 calculation.
 * @param [in] swCrcInit             Pointer the CRC Init struct.
 * @param [in] u8DataWidth           Width of the data.
*         This parameter can be one of the macros group @ref CRC_DATA_Bit_Width
 * @param [in] pu8Data               Pointer to the buffer containing the data to be computed.
 * @param [in] u32Len                The length of the data to be computed.
 * @retval CRC-16 calculation result
 */
static uint16_t CRC16_SW_Calculate(stc_swcrc_init_t *swCrcInit, uint8_t u8DataWidth, const uint8_t *pu8Data, uint32_t u32Len)
{
    uint8_t i, j;
    uint8_t wChar;
    uint16_t wCRCin = swCrcInit->InitValue;
    while (u32Len--) {
        j = u8DataWidth;
        while (j--) {
            wChar = *(pu8Data++);
            if (swCrcInit->RefIn) {
                InvertUint8(&wChar, &wChar);
            }
            wCRCin ^= (wChar << 8U);
            for (i = 0; i < 8U; i++) {
                if (wCRCin & 0x8000UL) {
                    wCRCin = (wCRCin << 1U) ^ swCrcInit->Poly;
                } else {
                    wCRCin = wCRCin << 1U;
                }
            }
        }
    }
    if (swCrcInit->RefOut) {
        InvertUint16(&wCRCin, &wCRCin);
    }

    return (wCRCin ^ (swCrcInit->XorOut));
}

/**
 * @brief InvertUint32.
 * @param [out] dBuf          The data after the Invert.
 * @param [in]  srcBuf        Data that is used to Invert.
 * @retval none
 */
static void InvertUint32(uint32_t *dBuf, uint32_t *srcBuf)
{
    uint32_t i;
    uint32_t Temp = 0;
    for (i = 0; i < 32U; i++) {
        if ((*srcBuf) & (1U << i))
            Temp |= 1U << (31U - i);
    }
    *dBuf = Temp;
}

/**
 * @brief CRC-32 calculation.
 * @param [in] swCrcInit             Pointer the CRC Init struct.
 * @param [in] u8DataWidth           Width of the data.
 *         This parameter can be one of the macros group @ref CRC_DATA_Bit_Width
 * @param [in] pu8Data               Pointer to the buffer containing the data to be computed.
 * @param [in] u32Len                The length of the data to be computed.
 * @retval CRC-32 calculation result
 */
static uint32_t CRC32_SW_Calculate(stc_swcrc_init_t *swCrcInit, uint8_t u8DataWidth, const uint8_t *pu8Data, uint32_t u32Len)
{
    uint8_t i, j;
    uint32_t wChar;
    uint32_t wCRCin = swCrcInit->InitValue;
    while (u32Len--) {
        j = u8DataWidth;
        while (j--) {
            wChar = *(pu8Data++);
            if (swCrcInit->RefIn) {
                InvertUint8((uint8_t *)&wChar, (uint8_t *)&wChar);
            }
            wCRCin ^= (wChar << 24U);
            for (i = 0; i < 8U; i++) {
                if (wCRCin & 0x80000000UL) {
                    wCRCin = (wCRCin << 1U) ^ swCrcInit->Poly;
                } else {
                    wCRCin = wCRCin << 1U;
                }
            }
        }
    }
    if (swCrcInit->RefOut) {
        InvertUint32(&wCRCin, &wCRCin);
    }

    return (wCRCin ^ (swCrcInit->XorOut));
}

/**
 * @brief  Main function of CRC software project
 * @param  None
 * @retval int32_t return value, if needed
 */
int32_t main(void)
{
    const uint8_t  au8SrcData [3U] = {0x12U, 0x21U, 0U};
    const uint16_t au16SrcData[3U] = {0x1234U, 0x4321U, 0U};
    const uint32_t au32SrcData[3U] = {0x12345678UL, 0x87654321UL, 0UL};
    uint16_t u16SwCrcValue;
    uint16_t u16HwCrcValue;
    uint32_t u32SwCrcValue;
    uint32_t u32HwCrcValue;
    uint32_t u32CrcErrorCount = 0UL;
    stc_crc_init_t stcCrcInit;
    stc_swcrc_init_t stcSwCrcInit;

    /* MCU Peripheral registers write unprotected */
    LL_PERIPH_WE(LL_PERIPH_SEL);

    /* Initialize BSP LED. */
    BSP_LED_Init();

    /* Enable CRC module clock. */
    FCG_Fcg0PeriphClockCmd(FCG0_PERIPH_CRC, ENABLE);

    /* MCU Peripheral registers write protected */
    LL_PERIPH_WP(LL_PERIPH_SEL);

#if (CRC16_ALGORITHM_SELECT == CRC16_X25)
    /******** Initialize CRC16_X25. *******************************************/
    stcCrcInit.u32Protocol = CRC_CRC16;
    stcCrcInit.u32InitValue = CRC16_INIT_VALUE;
    stcCrcInit.u32RefIn = CRC_REFIN_ENABLE;
    stcCrcInit.u32RefOut = CRC_REFOUT_ENABLE;
    stcCrcInit.u32XorOut = CRC_XOROUT_ENABLE;
    (void)CRC_Init(&stcCrcInit);

    /******** Initialize Software CRC16_X25. **********************************/
    stcSwCrcInit.Poly = CRC16_X25_POLY;
    stcSwCrcInit.InitValue = CRC16_INIT_VALUE;
    stcSwCrcInit.RefIn = M_True;
    stcSwCrcInit.RefOut = M_True;
    stcSwCrcInit.XorOut = 0xFFFFU;
#elif (CRC16_ALGORITHM_SELECT == CRC16_XMODEM)
    /******** Initialize CRC16_XMODEM. ****************************************/
    stcCrcInit.u32Protocol = CRC_CRC16;
    stcCrcInit.u32InitValue = 0x0000U;
    stcCrcInit.u32RefIn = CRC_REFIN_DISABLE;
    stcCrcInit.u32RefOut = CRC_REFOUT_DISABLE;
    stcCrcInit.u32XorOut = CRC_XOROUT_DISABLE;
    (void)CRC_Init(&stcCrcInit);

    /******** Initialize Software CRC16_XMODEM. *******************************/
    stcSwCrcInit.Poly = CRC16_XMODE_POLY;
    stcSwCrcInit.InitValue = 0x0000U;
    stcSwCrcInit.RefIn = M_False;
    stcSwCrcInit.RefOut = M_False;
    stcSwCrcInit.XorOut = 0x0000U;
#elif (CRC16_ALGORITHM_SELECT == CRC16_CCITT)
    /******** Initialize CRC16_CCITT. *****************************************/
    stcCrcInit.u32Protocol = CRC_CRC16;
    stcCrcInit.u32InitValue = 0x0000U;
    stcCrcInit.u32RefIn = CRC_REFIN_ENABLE;
    stcCrcInit.u32RefOut = CRC_REFOUT_ENABLE;
    stcCrcInit.u32XorOut = CRC_XOROUT_DISABLE;
    (void)CRC_Init(&stcCrcInit);

    /******** Initialize Software CRC16_CCITT. ********************************/
    stcSwCrcInit.Poly = CRC16_CCITT_POLY;
    stcSwCrcInit.InitValue = 0x0000U;
    stcSwCrcInit.RefIn = M_True;
    stcSwCrcInit.RefOut = M_True;
    stcSwCrcInit.XorOut = 0x0000U;
#else
    /******** Initialize CRC16_CCITT_FALSE. ***********************************/
    stcCrcInit.u32Protocol = CRC_CRC16;
    stcCrcInit.u32InitValue = CRC16_INIT_VALUE;
    stcCrcInit.u32RefIn = CRC_REFIN_DISABLE;
    stcCrcInit.u32RefOut = CRC_REFOUT_DISABLE;
    stcCrcInit.u32XorOut = CRC_XOROUT_DISABLE;
    (void)CRC_Init(&stcCrcInit);

    /******** Initialize Software CRC16_CCITT_FALSE. **************************/
    stcSwCrcInit.Poly = CRC16_CCITT_FALSE_POLY;
    stcSwCrcInit.InitValue = CRC16_INIT_VALUE;
    stcSwCrcInit.RefIn = M_False;
    stcSwCrcInit.RefOut = M_False;
    stcSwCrcInit.XorOut = 0x0000U;
#endif

    /******** Calculates CRC16 checksum by writing data in byte. **************/
    u16SwCrcValue = CRC16_SW_Calculate(&stcSwCrcInit, SW_CRC_DATA_WIDTH_8BIT, au8SrcData, ARRAY_SZ(au8SrcData));
    (void)CRC_CRC16_AccumulateData(CRC_DATA_WIDTH_8BIT, au8SrcData, ARRAY_SZ(au8SrcData), &u16HwCrcValue);
    if (u16SwCrcValue != u16HwCrcValue) {
        u32CrcErrorCount++;
    }

    /******** Calculates CRC16 checksum by writing data in half-word. *********/
    u16SwCrcValue = CRC16_SW_Calculate(&stcSwCrcInit, SW_CRC_DATA_WIDTH_16BIT, (const uint8_t *)au16SrcData, ARRAY_SZ(au16SrcData));
    (void)CRC_CRC16_Calculate(stcCrcInit.u32InitValue, CRC_DATA_WIDTH_16BIT, au16SrcData, ARRAY_SZ(au16SrcData), &u16HwCrcValue);
    if (u16SwCrcValue != u16HwCrcValue) {
        u32CrcErrorCount++;
    }

    /******** Calculates CRC16 checksum by writing data in word. **************/
    u16SwCrcValue = CRC16_SW_Calculate(&stcSwCrcInit, SW_CRC_DATA_WIDTH_32BIT, (const uint8_t *)au32SrcData, ARRAY_SZ(au32SrcData));
    (void)CRC_CRC16_Calculate(stcCrcInit.u32InitValue, CRC_DATA_WIDTH_32BIT, au32SrcData, ARRAY_SZ(au32SrcData), &u16HwCrcValue);
    if (u16SwCrcValue != u16HwCrcValue) {
        u32CrcErrorCount++;
    }

#if (CRC32_ALGORITHM_SELECT == CRC32)
    /******** Initialize CRC32. ***********************************************/
    stcCrcInit.u32Protocol = CRC_CRC32;
    stcCrcInit.u32InitValue = CRC32_INIT_VALUE;
    stcCrcInit.u32RefIn = CRC_REFIN_ENABLE;
    stcCrcInit.u32RefOut = CRC_REFOUT_ENABLE;
    stcCrcInit.u32XorOut = CRC_XOROUT_ENABLE;
    (void)CRC_Init(&stcCrcInit);

    /******** Initialize Software CRC32. **************************************/
    stcSwCrcInit.Poly = CRC32_POLY;
    stcSwCrcInit.InitValue = CRC32_INIT_VALUE;
    stcSwCrcInit.RefIn = M_True;
    stcSwCrcInit.RefOut = M_True;
    stcSwCrcInit.XorOut = 0xFFFFFFFFUL;
#else
    /******** Initialize CRC32_MPEG2. *****************************************/
    stcCrcInit.u32Protocol = CRC_CRC32;
    stcCrcInit.u32InitValue = CRC32_INIT_VALUE;
    stcCrcInit.u32RefIn = CRC_REFIN_DISABLE;
    stcCrcInit.u32RefOut = CRC_REFOUT_DISABLE;
    stcCrcInit.u32XorOut = CRC_XOROUT_DISABLE;
    (void)CRC_Init(&stcCrcInit);

    /******** Initialize Software CRC32_MPEG2. ********************************/
    stcSwCrcInit.Poly = CRC32_MPEG2_POLY;
    stcSwCrcInit.InitValue = CRC32_INIT_VALUE;
    stcSwCrcInit.RefIn = M_False;
    stcSwCrcInit.RefOut = M_False;
    stcSwCrcInit.XorOut = 0x00000000UL;
#endif

    /******** Calculates CRC32 checksum by writing data in byte. **************/
    u32SwCrcValue = CRC32_SW_Calculate(&stcSwCrcInit, SW_CRC_DATA_WIDTH_8BIT, au8SrcData, ARRAY_SZ(au8SrcData));
    (void)CRC_CRC32_AccumulateData(CRC_DATA_WIDTH_8BIT, au8SrcData, ARRAY_SZ(au8SrcData), &u32HwCrcValue);
    if (u32SwCrcValue != u32HwCrcValue) {
        u32CrcErrorCount++;
    }

    /******** Calculates CRC32 checksum by writing data in half-word. *********/
    u32SwCrcValue = CRC32_SW_Calculate(&stcSwCrcInit, SW_CRC_DATA_WIDTH_16BIT, (const uint8_t *)au16SrcData, ARRAY_SZ(au16SrcData));
    (void)CRC_CRC32_Calculate(stcCrcInit.u32InitValue, CRC_DATA_WIDTH_16BIT, au16SrcData, ARRAY_SZ(au16SrcData), &u32HwCrcValue);
    if (u32SwCrcValue != u32HwCrcValue) {
        u32CrcErrorCount++;
    }

    /******** Calculates CRC32 checksum by writing data in word. **************/
    u32SwCrcValue = CRC32_SW_Calculate(&stcSwCrcInit, SW_CRC_DATA_WIDTH_32BIT, (const uint8_t *)au32SrcData, ARRAY_SZ(au32SrcData));
    (void)CRC_CRC32_Calculate(stcCrcInit.u32InitValue, CRC_DATA_WIDTH_32BIT, au32SrcData, ARRAY_SZ(au32SrcData), &u32HwCrcValue);
    if (u32SwCrcValue != u32HwCrcValue) {
        u32CrcErrorCount++;
    }

    /* Check test result */
    if (0UL == u32CrcErrorCount) {
        BSP_LED_On(LED_BLUE);   /* Test result meets the expected. */
    } else {
        BSP_LED_On(LED_RED);    /* Test result doesn't meet the expected. */
    }

    for (;;) {
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
