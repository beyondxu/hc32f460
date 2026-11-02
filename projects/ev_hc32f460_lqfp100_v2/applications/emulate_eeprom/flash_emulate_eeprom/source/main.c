/**
 *******************************************************************************
 * @file  flash_emulate_eeprom/source/main.c
 * @brief Main program of eeprom emulation for hc32xxx family.
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
 * @addtogroup HC32F460_DDL_Applications
 * @{
 */

/**
 * @addtogroup FLASH_EMULATE_EEPROM_TEST
 * @{
 */

/*******************************************************************************
 * Local type definitions ('typedef')
 ******************************************************************************/
typedef enum {
    HMI_CMD_START = 0U,
    HMI_CMD_ADDR,
    HMI_CMD_WR,
    HMI_CMD_DATA,
    HMI_CMD_END
} HMI_CMD;

typedef struct {
    uint16_t u16ReadIndex;
    uint16_t u16WriteIndex;
    uint16_t u16Size;
    uint8_t *pu8RxData;
} stc_hmi_data;
/*******************************************************************************
 * Local pre-processor symbols/macros ('#define')
 ******************************************************************************/
#define DLY_MS                          (100UL)

#define EE_TEST_BUF_SIZE                (128U)
#define EE_TEST_SIZE                    (1U)
#define EE_WRITE_SIZE                   (EE_CAPACITY % EE_TEST_SIZE) ? (EE_TEST_SIZE + 3U) / 4 * 4 : EE_TEST_SIZE

/* USART RX/TX pin definition */
#define USART_RX_PORT                   (GPIO_PORT_B)   /* PB9: USART4_RX */
#define USART_RX_PIN                    (GPIO_PIN_09)
#define USART_RX_GPIO_FUNC              (GPIO_FUNC_37)

#define USART_TX_PORT                   (GPIO_PORT_E)   /* PE6: USART4_TX */
#define USART_TX_PIN                    (GPIO_PIN_06)
#define USART_TX_GPIO_FUNC              (GPIO_FUNC_36)

/* USART unit definition */
#define USART_UNIT                      (CM_USART4)
#define USART_FCG_ENABLE()              (FCG_Fcg1PeriphClockCmd(FCG1_PERIPH_USART4, ENABLE))

/* USART interrupt definition */
#define USART_RX_ERR_IRQn               (INT000_IRQn)
#define USART_RX_ERR_INT_SRC            (INT_SRC_USART4_EI)

#define USART_RX_FULL_IRQn              (INT001_IRQn)
#define USART_RX_FULL_INT_SRC           (INT_SRC_USART4_RI)

#define TIMEOUT                         (3000UL)
/*******************************************************************************
 * Global variable definitions (declared in header file with 'extern')
 ******************************************************************************/

/*******************************************************************************
 * Local function prototypes ('static')
 ******************************************************************************/

/*******************************************************************************
 * Local variable definitions ('static')
 ******************************************************************************/
static uint8_t u8DataRxBuf[EE_CAPACITY];
static stc_hmi_data stcHmiData;
/*******************************************************************************
 * Function implementation - global ('extern') and local ('static')
 ******************************************************************************/
/**
 * @brief  USART RX IRQ callback
 * @param  None
 * @retval None
 */
static void USART_RxFull_IrqCallback(void)
{
    stcHmiData.pu8RxData[stcHmiData.u16WriteIndex] = (uint8_t)USART_ReadData(USART_UNIT);
    if (++stcHmiData.u16WriteIndex >= stcHmiData.u16Size) {
        stcHmiData.u16WriteIndex = 0U;
    }
}

/**
 * @brief  USART error IRQ callback.
 * @param  None
 * @retval None
 */
static void USART_RxError_IrqCallback(void)
{
    (void)USART_ReadData(USART_UNIT);

    USART_ClearStatus(USART_UNIT, (USART_FLAG_PARITY_ERR | USART_FLAG_FRAME_ERR | USART_FLAG_OVERRUN));
}

/**
 * @brief  Instal IRQ handler.
 * @param  [in] pstcConfig      Pointer to struct @ref stc_irq_signin_config_t
 * @param  [in] u32Priority     Interrupt priority
 * @retval None
 */
static void INTC_IrqInstalHandler(const stc_irq_signin_config_t *pstcConfig, uint32_t u32Priority)
{
    if (NULL != pstcConfig) {
        (void)INTC_IrqSignIn(pstcConfig);
        NVIC_ClearPendingIRQ(pstcConfig->enIRQn);
        NVIC_SetPriority(pstcConfig->enIRQn, u32Priority);
        NVIC_EnableIRQ(pstcConfig->enIRQn);
    }
}

static void UART_Init()
{
    stc_usart_uart_init_t stcUartInit;
    stc_irq_signin_config_t stcIrqSigninConfig;

    /* Configure USART RX/TX pin. */
    GPIO_SetFunc(USART_RX_PORT, USART_RX_PIN, USART_RX_GPIO_FUNC);
    GPIO_SetFunc(USART_TX_PORT, USART_TX_PIN, USART_TX_GPIO_FUNC);
    /* Enable peripheral clock */
    USART_FCG_ENABLE();

    /* Initialize UART. */
    (void)USART_UART_StructInit(&stcUartInit);
    stcUartInit.u32Baudrate = 115200UL;
    stcUartInit.u32OverSampleBit = USART_OVER_SAMPLE_8BIT;
    if (LL_OK != USART_UART_Init(USART_UNIT, &stcUartInit, NULL)) {
        BSP_LED_On(LED_RED);
        for (;;) {
        }
    }

    /* Register RX error IRQ handler && configure NVIC. */
    stcIrqSigninConfig.enIRQn = USART_RX_ERR_IRQn;
    stcIrqSigninConfig.enIntSrc = USART_RX_ERR_INT_SRC;
    stcIrqSigninConfig.pfnCallback = &USART_RxError_IrqCallback;
    INTC_IrqInstalHandler(&stcIrqSigninConfig, DDL_IRQ_PRIO_DEFAULT);

    /* Register RX full IRQ handler && configure NVIC. */
    stcIrqSigninConfig.enIRQn = USART_RX_FULL_IRQn;
    stcIrqSigninConfig.enIntSrc = USART_RX_FULL_INT_SRC;
    stcIrqSigninConfig.pfnCallback = &USART_RxFull_IrqCallback;
    INTC_IrqInstalHandler(&stcIrqSigninConfig, DDL_IRQ_PRIO_DEFAULT);

    /* Enable RX function */
    USART_FuncCmd(USART_UNIT, (USART_RX | USART_INT_RX | USART_TX), ENABLE);
}

static int32_t UART_BUFF_Init(stc_hmi_data *stcHmiData)
{
    int32_t i32Ret = LL_ERR_INVD_PARAM;

    if (stcHmiData != NULL) {
        stcHmiData->u16ReadIndex = 0UL;
        stcHmiData->u16WriteIndex = 0UL;
        stcHmiData->u16Size = EE_CAPACITY;
        stcHmiData->pu8RxData = u8DataRxBuf;
        i32Ret = LL_OK;
    }

    return i32Ret;
}

/**
 * @brief  String to Int.
 * @param  [in] pu8Str      Pointer to the string of will convert to int
 * @param  [out] pu32Data   Pointer to the convert result
 * @param  [in] u16Len      convert length
 * @retval None
 */
static uint8_t *StringToInt(uint8_t *pu8Str, uint32_t *pu32Data, uint16_t u16Len)
{
    uint8_t *pu8Position = NULL;
    uint32_t u32Data = 0;

    if (NULL == pu8Str) {
        return NULL;
    }
    while (u16Len > 0) {
        if ((*pu8Str >= '0') && (*pu8Str <= '9')) {
            u32Data = u32Data * 10UL + (*pu8Str - '0');
            pu8Str ++;
        } else {
            pu8Position = pu8Str;
            break;
        }
        u16Len --;
    }
    *pu32Data = u32Data;
    return pu8Position;
}

/**
 * @brief  Get data for uart.
 * @param  [in] pu8Data     Store read data addr
 * @retval uint32_t: The length of receive
 */
static uint32_t UART_Get_Data(uint8_t *pu8Data)
{
    uint32_t u32DataLen = 0UL;

    while (stcHmiData.u16ReadIndex != stcHmiData.u16WriteIndex) {
        *pu8Data = stcHmiData.pu8RxData[stcHmiData.u16ReadIndex];
        u32DataLen ++;
        pu8Data ++;
        if (++stcHmiData.u16ReadIndex >= stcHmiData.u16Size) {
            stcHmiData.u16ReadIndex = 0UL;
        }
        DDL_DelayMS(1U);
    }
    return u32DataLen;
}

/**
 * @brief  Human machine interface handle.
 * @param  None
 * @retval None
 */
static void UART_HMI_Handler(void)
{
    uint8_t *pu8Data;
    uint8_t u8Buf[10U] = {0U};
    uint16_t i;
    int32_t ret = -1;
    uint32_t u32Len, u32TxLen;
    static uint8_t u8EE_Cmd = 0U;
    static HMI_CMD hmi_cmd = HMI_CMD_START;
    static uint8_t u8TempBuf[EE_CAPACITY] = {0U};
    static uint32_t u32Addr;

    switch (hmi_cmd) {
        case HMI_CMD_START:
            hmi_cmd = HMI_CMD_ADDR;
            u32TxLen = sprintf((char *)u8TempBuf, "Input Operate EEPROM Virtual Address\r\n");
            USART_UART_Trans(USART_UNIT, u8TempBuf, u32TxLen, TIMEOUT);
            break;
        case HMI_CMD_ADDR:
            u32Len = UART_Get_Data(u8TempBuf);
            if (0UL != u32Len) {
                pu8Data = StringToInt(u8TempBuf, &u32Addr, u32Len);
                if ((u8TempBuf == pu8Data)) {
                    hmi_cmd = HMI_CMD_END;
                    u32TxLen = sprintf((char *)u8TempBuf, "Input EEPROM Virtual Address Format: ascii for number\r\n");
                    USART_UART_Trans(USART_UNIT, u8TempBuf, u32TxLen, TIMEOUT);
                    break;
                }
                if (u32Addr >= EE_CAPACITY) {
                    hmi_cmd = HMI_CMD_END;
                    u32TxLen = sprintf((char *)u8TempBuf, "Input EEPROM Virtual Address Invalid\r\n");
                    USART_UART_Trans(USART_UNIT, u8TempBuf, u32TxLen, TIMEOUT);
                } else {
                    hmi_cmd = HMI_CMD_WR;
                    u32TxLen = sprintf((char *)u8TempBuf, "Input EEPROM Condition:1-Write; 0:Read\r\n");
                    USART_UART_Trans(USART_UNIT, u8TempBuf, u32TxLen, TIMEOUT);
                }
            }
            break;
        case HMI_CMD_WR:
            if (0UL != UART_Get_Data(u8TempBuf)) {
                if (0U == memcmp((const void *)u8TempBuf, "1", 1U)) {
                    u8EE_Cmd = 1U;
                    hmi_cmd = HMI_CMD_DATA;
                    u32TxLen = sprintf((char *)u8TempBuf, "Write: Input Write EEPROM Data\r\n");
                    USART_UART_Trans(USART_UNIT, u8TempBuf, u32TxLen, TIMEOUT);
                    break;
                } else if (0U == memcmp((const void *)u8TempBuf, "0", 1U)) {
                    u8EE_Cmd = 2U;
                    hmi_cmd = HMI_CMD_DATA;
                    u32TxLen = sprintf((char *)u8TempBuf, "Read: Input Read EEPROM Length\r\n");
                    USART_UART_Trans(USART_UNIT, u8TempBuf, u32TxLen, TIMEOUT);
                } else {
                    u32TxLen = sprintf((char *)u8TempBuf, "Input Current Condition\r\n");
                    USART_UART_Trans(USART_UNIT, u8TempBuf, u32TxLen, TIMEOUT);
                    hmi_cmd = HMI_CMD_END;
                }
            }
            break;
        case HMI_CMD_DATA:
            u32Len = UART_Get_Data(u8TempBuf);
            if (0UL != u32Len) {
                hmi_cmd = HMI_CMD_END;
                if (1U == u8EE_Cmd) {
                    ret = EE_WriteNBytes(u32Addr, u8TempBuf, u32Len);
                    if (0UL != ret) {
                        u32TxLen = sprintf((char *)u8TempBuf, "Write EEPROM fail, error code: %02X\r\n", (unsigned int)ret);
                        USART_UART_Trans(USART_UNIT, u8TempBuf, u32TxLen, TIMEOUT);
                        break;
                    }
                    u32TxLen = sprintf((char *)u8TempBuf, "EEPROM Write Success.  Read back Data :");
                    USART_UART_Trans(USART_UNIT, u8TempBuf, u32TxLen, TIMEOUT);
                    EE_ReadNBytes(u32Addr, u8TempBuf, u32Len);
                    for (i = 0UL; i < u32Len; i++) {
                        if (0U == (i % 16UL)) {
                            u32TxLen = sprintf((char *)u8Buf, "\r\n%02X:", (unsigned int)(u32Addr + i));
                            USART_UART_Trans(USART_UNIT, u8Buf, u32TxLen, TIMEOUT);
                        }
                        u32TxLen = sprintf((char *)u8Buf, " %02X", u8TempBuf[i]);
                        USART_UART_Trans(USART_UNIT, u8Buf, u32TxLen, TIMEOUT);
                    }
                    USART_UART_Trans(USART_UNIT, "\r\n", 2U, TIMEOUT);
                    break;
                } else if (2U == u8EE_Cmd) {
                    pu8Data = StringToInt(u8TempBuf, &u32Len, u32Len);
                    if ((u8TempBuf == pu8Data)) {
                        hmi_cmd = HMI_CMD_END;
                        u32TxLen = sprintf((char *)u8TempBuf, "Input EEPROM Length Format: ascii for number\r\n");
                        USART_UART_Trans(USART_UNIT, u8TempBuf, u32TxLen, TIMEOUT);
                        break;
                    }
                    u32TxLen = sprintf((char *)u8TempBuf, "EEPROM Read Result: ");
                    USART_UART_Trans(USART_UNIT, u8TempBuf, u32TxLen, TIMEOUT);
                    ret = EE_ReadNBytes(u32Addr, u8TempBuf, u32Len);
                    if (0UL != ret) {
                        u32TxLen = sprintf((char *)u8TempBuf, "Read EEPROM fail, error code: %02X\r\n", (unsigned int)ret);
                        USART_UART_Trans(USART_UNIT, u8TempBuf, u32TxLen, TIMEOUT);
                        break;
                    }
                    for (i = 0UL; i < u32Len; i++) {
                        if (0UL == (i % 16UL)) {
                            u32TxLen = sprintf((char *)u8Buf, "\r\n%02X:", (unsigned int)(u32Addr + i));
                            USART_UART_Trans(USART_UNIT, u8Buf, u32TxLen, TIMEOUT);
                        }
                        u32TxLen = sprintf((char *)u8Buf, " %02X", u8TempBuf[i]);
                        USART_UART_Trans(USART_UNIT, u8Buf, u32TxLen, TIMEOUT);
                    }
                    USART_UART_Trans(USART_UNIT, "\r\n", 2U, TIMEOUT);
                    break;
                }
            }
            break;
        case HMI_CMD_END:
            hmi_cmd = HMI_CMD_START;
            break;
        default:
            break;
    }
}

/**
 * @brief  Main function of EEPROM emulation test
 * @param  None
 * @retval int32_t return value, if needed
 */
int32_t main(void)
{
    uint8_t data = 0xa5U, i, j;
    uint16_t addr, remain = EE_CAPACITY % EE_TEST_BUF_SIZE;
    static uint8_t write_buf[EE_TEST_BUF_SIZE];
    static uint8_t read_buf[EE_TEST_BUF_SIZE];

    /* Register write enable for some required peripherals. */
    LL_PERIPH_WE(LL_PERIPH_GPIO | LL_PERIPH_PWC_CLK_RMU | LL_PERIPH_FCG | LL_PERIPH_EFM | LL_PERIPH_INTC | LL_PERIPH_SRAM);
    /* Configure BSP */
    BSP_CLK_Init();
    /* Configure LED */
    BSP_LED_Init();
    /* Initialize ring buffer function. */
    UART_BUFF_Init(&stcHmiData);

    UART_Init();

    /* Register write protected for some required peripherals. */
    LL_PERIPH_WP(LL_PERIPH_GPIO | LL_PERIPH_PWC_CLK_RMU | LL_PERIPH_INTC);

    EE_FlashInit();

    DDL_DelayMS(DLY_MS);

    for (i = 0U; i < 5U; i ++) {
        /* Write data to each virtual address of the eeprom */
        for (j = 0U; j < EE_CAPACITY / EE_TEST_BUF_SIZE; j++) {
            for (addr = 0U; addr < EE_TEST_BUF_SIZE; addr ++) {
                write_buf[addr] = data ++;
            }
            /* Write/Read Specified size data */
            for (addr = 0U; addr < EE_TEST_BUF_SIZE; addr += EE_WRITE_SIZE) {
                EE_WriteNBytes(addr + j * EE_TEST_BUF_SIZE, write_buf + addr, EE_WRITE_SIZE);
                EE_ReadNBytes(addr + j * EE_TEST_BUF_SIZE, read_buf + addr, EE_WRITE_SIZE);
            }
            /* compare data */
            if (memcmp(write_buf, read_buf, EE_TEST_BUF_SIZE) != 0UL) {
                BSP_LED_On(LED_BLUE);
                for (; ;) {
                    ;
                }
            }

            memset(read_buf, 0U, EE_TEST_BUF_SIZE);
        }
        if (0U != (remain)) {
            for (addr = 0U; addr < remain; addr ++) {
                write_buf[addr] = data ++;
            }
            for (addr = 0U; addr < remain; addr += EE_WRITE_SIZE) {
                EE_WriteNBytes(addr + j * EE_TEST_BUF_SIZE, write_buf + addr, EE_WRITE_SIZE);
                EE_ReadNBytes(addr + j * EE_TEST_BUF_SIZE, read_buf + addr, EE_WRITE_SIZE);
            }

            if (memcmp(write_buf, read_buf, remain) != 0UL) {
                BSP_LED_On(LED_BLUE);
                for (; ;) {
                    ;
                }
            }
        }
    }

    for (; ;) {
        BSP_LED_Toggle(LED_BLUE);
        DDL_DelayMS(DLY_MS);
        UART_HMI_Handler();
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
