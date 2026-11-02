/**
 *******************************************************************************
 * @file  sdioc/sdioc_sdio/source/main.c
 * @brief Main program of SDIO card for the Device Driver Library.
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
 * @addtogroup SDIOC_SDIO_Card
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

/* SDIO transfer mode */
#define SDIO_TRANS_BLOCK_MD_POLLING     (0U)
#define SDIO_TRANS_BLOCK_MD_INT         (1U)
#define SDIO_TRANS_BLOCK_MD_DMA         (2U)
/* Populate the following macro with an value, reference "SDIO transfer mode" */
#define SDIO_TRANS_BLOCK_MD             (SDIO_TRANS_BLOCK_MD_POLLING)

/* SDIOC DMA configuration define */
#define SDIOC_DMA_UNIT                  (CM_DMA1)
#define SDIOC_DMA_CLK                   (FCG0_PERIPH_DMA1 | FCG0_PERIPH_AOS)
#define SDIOC_DMA_TX_CH                 (DMA_CH0)
#define SDIOC_DMA_RX_CH                 (DMA_CH1)
#define SDIOC_DMA_TX_TRIG_CH            (AOS_DMA1_0)
#define SDIOC_DMA_RX_TRIG_CH            (AOS_DMA1_1)
#define SDIOC_DMA_TX_TRIG_SRC           (EVT_SRC_SDIOC1_DMAW)
#define SDIOC_DMA_RX_TRIG_SRC           (EVT_SRC_SDIOC1_DMAR)

/* SDIOC configuration define */
#define SDIOC_SDIO_UINT                 (CM_SDIOC1)
#define SDIOC_SDIO_CLK                  (FCG1_PERIPH_SDIOC1)
#define SIDOC_SDIO_INT_SRC              (INT_SRC_SDIOC1_SD)
#define SIDOC_SDIO_IRQ                  (INT006_IRQn)
/* CK = PC12 */
#define SDIOC_CK_PORT                   (GPIO_PORT_C)
#define SDIOC_CK_PIN                    (GPIO_PIN_12)
/* CMD = PD02 */
#define SDIOC_CMD_PORT                  (GPIO_PORT_D)
#define SDIOC_CMD_PIN                   (GPIO_PIN_02)
/* D0 = PC08 */
#define SDIOC_D0_PORT                   (GPIO_PORT_C)
#define SDIOC_D0_PIN                    (GPIO_PIN_08)
/* D1 = PC09 */
#define SDIOC_D1_PORT                   (GPIO_PORT_C)
#define SDIOC_D1_PIN                    (GPIO_PIN_09)
/* D2 = PC10 */
#define SDIOC_D2_PORT                   (GPIO_PORT_C)
#define SDIOC_D2_PIN                    (GPIO_PIN_10)
/* D3 = PC11 */
#define SDIOC_D3_PORT                   (GPIO_PORT_C)
#define SDIOC_D3_PIN                    (GPIO_PIN_11)

/* PDN = PE14 */
#define WIFI_CARD_PDN_PORT              (GPIO_PORT_E)
#define WIFI_CARD_PDN_PIN               (GPIO_PIN_14)

/* SDIO block numbers */
#define SDIO_TRANS_BLOCK_SIZE           (256U)
#define SDIO_TRANS_BLOCK_NUM            (5U)

/*******************************************************************************
 * Global variable definitions (declared in header file with 'extern')
 ******************************************************************************/

/*******************************************************************************
 * Local function prototypes ('static')
 ******************************************************************************/

/*******************************************************************************
 * Local variable definitions ('static')
 ******************************************************************************/
static stc_sdio_host_t m_stcSdioHost;

static uint8_t m_au8RwDirectBuf[SDIO_TRANS_BLOCK_SIZE * SDIO_TRANS_BLOCK_NUM];
static uint8_t m_au8RwExtendBuf[SDIO_TRANS_BLOCK_SIZE * SDIO_TRANS_BLOCK_NUM];

#if SDIO_TRANS_BLOCK_MD != SDIO_TRANS_BLOCK_MD_POLLING
static __IO uint8_t m_u8TxRxErrorFlag = 0U;
static __IO uint8_t m_u8TransCompleteFlag = 0U;
#endif

/*******************************************************************************
 * Function implementation - global ('extern') and local ('static')
 ******************************************************************************/
#if SDIO_TRANS_BLOCK_MD != SDIO_TRANS_BLOCK_MD_POLLING
/**
 * @brief  SDIOC transfer complete interrupt callback function.
 * @param  None
 * @retval None
 */
static void SdioCard_TransCompleteIrqCallback(void)
{
    SDIO_IRQHandler(&m_stcSdioHost);
}

/**
 * @brief  SDIOC TX complete callback function.
 * @param  None
 * @retval None
 */
void SDIO_TxCompleteCallback(stc_sdio_host_t *pstcHost)
{
    m_u8TransCompleteFlag = 1U;
}

/**
 * @brief  SDIOC RX complete callback function.
 * @param  None
 * @retval None
 */
void SDIO_RxCompleteCallback(stc_sdio_host_t *pstcHost)
{
    m_u8TransCompleteFlag = 1U;
}

/**
 * @brief  SDIOC error callback function.
 * @param  None
 * @retval None
 */
void SDIO_ErrorCallback(stc_sdio_host_t *pstcHost)
{
    m_u8TxRxErrorFlag = 1U;
}
#endif

#if SDIO_TRANS_BLOCK_MD == SDIO_TRANS_BLOCK_MD_DMA
/**
 * @brief  Initializes the SDIO DMA.
 * @param  None
 * @retval None
 */
static void SdioCard_DMAInit(void)
{
    stc_dma_init_t stcDmaInit;

    /* Enable DMA and AOS clock */
    FCG_Fcg0PeriphClockCmd(SDIOC_DMA_CLK, ENABLE);

    (void)DMA_StructInit(&stcDmaInit);
    /* Configure SDIO DMA Transfer */
    stcDmaInit.u32IntEn        = DMA_INT_DISABLE;
    stcDmaInit.u32DataWidth    = DMA_DATAWIDTH_32BIT;
    stcDmaInit.u32SrcAddrInc   = DMA_SRC_ADDR_INC;
    stcDmaInit.u32DestAddrInc  = DMA_DEST_ADDR_FIX;
    if (LL_OK != DMA_Init(SDIOC_DMA_UNIT, SDIOC_DMA_TX_CH, &stcDmaInit)) {
        for (;;) {
        }
    }
    /* Configure SDIO DMA Receive */
    stcDmaInit.u32SrcAddrInc   = DMA_SRC_ADDR_FIX;
    stcDmaInit.u32DestAddrInc  = DMA_DEST_ADDR_INC;
    if (LL_OK != DMA_Init(SDIOC_DMA_UNIT, SDIOC_DMA_RX_CH, &stcDmaInit)) {
        for (;;) {
        }
    }

    AOS_SetTriggerEventSrc(SDIOC_DMA_TX_TRIG_CH, SDIOC_DMA_TX_TRIG_SRC);
    AOS_SetTriggerEventSrc(SDIOC_DMA_RX_TRIG_CH, SDIOC_DMA_RX_TRIG_SRC);
    /* Enable DMA */
    DMA_Cmd(SDIOC_DMA_UNIT, ENABLE);
}
#endif

/**
 * @brief  SDIO card configuration.
 * @param  None
 * @retval int32_t:
 *           - LL_OK: SDIO configure successfully
 *           - LL_ERR: SDIO configure unsuccessfully
 */
static int32_t SdioCard_Config(stc_sdio_host_t *pstcHost)
{
    int32_t i32Ret = LL_ERR;
    stc_gpio_init_t stcGpioInit;

#if SDIO_TRANS_BLOCK_MD != SDIO_TRANS_BLOCK_MD_POLLING
    stc_irq_signin_config_t stcIrqSignConfig;

    /* Interrupt configuration */
    stcIrqSignConfig.enIntSrc    = SIDOC_SDIO_INT_SRC;
    stcIrqSignConfig.enIRQn      = SIDOC_SDIO_IRQ;
    stcIrqSignConfig.pfnCallback = &SdioCard_TransCompleteIrqCallback;
    (void)INTC_IrqSignIn(&stcIrqSignConfig);
    NVIC_ClearPendingIRQ(stcIrqSignConfig.enIRQn);
    NVIC_SetPriority(stcIrqSignConfig.enIRQn, DDL_IRQ_PRIO_DEFAULT);
    NVIC_EnableIRQ(stcIrqSignConfig.enIRQn);
#endif

    (void)GPIO_StructInit(&stcGpioInit);
    stcGpioInit.u16PinState = PIN_STAT_SET;
    stcGpioInit.u16PinDir = PIN_DIR_OUT;
    (void)GPIO_Init(WIFI_CARD_PDN_PORT, WIFI_CARD_PDN_PIN, &stcGpioInit);
    DDL_DelayMS(50UL);
    GPIO_ResetPins(WIFI_CARD_PDN_PORT, WIFI_CARD_PDN_PIN);
    DDL_DelayMS(50UL);
    GPIO_SetPins(WIFI_CARD_PDN_PORT, WIFI_CARD_PDN_PIN);
    DDL_DelayMS(50UL);

    /* SDIOC pins configuration */
    GPIO_SetFunc(SDIOC_CK_PORT,  SDIOC_CK_PIN,  GPIO_FUNC_9);
    GPIO_SetFunc(SDIOC_CMD_PORT, SDIOC_CMD_PIN, GPIO_FUNC_9);
    GPIO_SetFunc(SDIOC_D0_PORT,  SDIOC_D0_PIN,  GPIO_FUNC_9);
    GPIO_SetFunc(SDIOC_D1_PORT,  SDIOC_D1_PIN,  GPIO_FUNC_9);
    GPIO_SetFunc(SDIOC_D2_PORT,  SDIOC_D2_PIN,  GPIO_FUNC_9);
    GPIO_SetFunc(SDIOC_D3_PORT,  SDIOC_D3_PIN,  GPIO_FUNC_9);

    /* Enable SDIOC clock */
    FCG_Fcg1PeriphClockCmd(SDIOC_SDIO_CLK, ENABLE);

    /* Configure structure initialization */
    pstcHost->SDIOCx                     = SDIOC_SDIO_UINT;
    pstcHost->stcSdiocInit.u32Mode       = SDIOC_MD_SD;
    pstcHost->stcSdiocInit.u8CardDetect  = SDIOC_CARD_DETECT_CD_PIN_LVL;
    pstcHost->stcSdiocInit.u8SpeedMode   = SDIOC_SPEED_MD_HIGH;
    pstcHost->stcSdiocInit.u8BusWidth    = SDIOC_BUS_WIDTH_4BIT;
    pstcHost->stcSdiocInit.u16ClockDiv   = SDIOC_CLK_DIV2;
#if SDIO_TRANS_BLOCK_MD == SDIO_TRANS_BLOCK_MD_DMA
    SdioCard_DMAInit();
    pstcHost->DMAx      = SDIOC_DMA_UNIT;
    pstcHost->u8DmaTxCh = SDIOC_DMA_TX_CH;
    pstcHost->u8DmaRxCh = SDIOC_DMA_RX_CH;
#else
    pstcHost->DMAx      = NULL;
#endif

    if (LL_OK == SDIOC_SWReset(pstcHost->SDIOCx, SDIOC_SW_RST_ALL)) {
        if (LL_OK == SDIO_Init(pstcHost)) {
            i32Ret = SDIO_SetBlockSize(pstcHost->pstcCard->apstcSdioFunc[0], SDIO_TRANS_BLOCK_SIZE);
            if (LL_OK != i32Ret) {
                DDL_Printf("SDIO card set block size failed!\r\n");
            }
        } else {
            DDL_Printf("SDIO card initialize failed!\r\n");
        }
    } else {
        DDL_Printf("Reset SDIOC failed!\r\n");
    }

    return i32Ret;
}

/**
 * @brief  SDIO card multi-block read.
 * @param  None
 * @retval int32_t:
 *           - LL_OK: operate successfully
 *           - LL_ERR: operate unsuccessfully
 */
static int32_t SdioCard_RdMultiBlock(stc_sdio_host_t *pstcHost)
{
    uint32_t i;
    uint32_t u32TransSize;
    int32_t i32Ret = LL_ERR;

    if (NULL != pstcHost) {
        u32TransSize = (uint32_t)SDIO_TRANS_BLOCK_SIZE * (uint32_t)SDIO_TRANS_BLOCK_NUM;
        (void)memset(m_au8RwDirectBuf, 0xFF, u32TransSize);
        (void)memset(m_au8RwExtendBuf, 0xFF, u32TransSize);

        for (i = 0UL; i < u32TransSize; i++) {
            (void)SDIO_IOReadByte(m_stcSdioHost.pstcCard->apstcSdioFunc[0], i, &m_au8RwDirectBuf[i]);
        }

#if SDIO_TRANS_BLOCK_MD == SDIO_TRANS_BLOCK_MD_POLLING
        (void)SDIO_IORwExtended(m_stcSdioHost.pstcCard->apstcSdioFunc[0], SDIO_CMD53_ARG_RD, 0UL,
                                SDIO_CMD53_ARG_OP_CODE_ADDR_INC, m_au8RwExtendBuf, SDIO_TRANS_BLOCK_NUM, SDIO_TRANS_BLOCK_SIZE);
#elif SDIO_TRANS_BLOCK_MD == SDIO_TRANS_BLOCK_MD_INT
        (void)SDIO_IORwExtended_INT(m_stcSdioHost.pstcCard->apstcSdioFunc[0], SDIO_CMD53_ARG_RD, 0UL,
                                    SDIO_CMD53_ARG_OP_CODE_ADDR_INC, m_au8RwExtendBuf, SDIO_TRANS_BLOCK_NUM, SDIO_TRANS_BLOCK_SIZE);
        /* Wait for transfer completed */
        while (0U == m_u8TransCompleteFlag) {
            DDL_Printf("Waiting for transmission to complete \r\n");
        }
#else
        (void)SDIO_IORwExtended_DMA(m_stcSdioHost.pstcCard->apstcSdioFunc[0], SDIO_CMD53_ARG_RD, 0UL,
                                    SDIO_CMD53_ARG_OP_CODE_ADDR_INC, m_au8RwExtendBuf, SDIO_TRANS_BLOCK_NUM, SDIO_TRANS_BLOCK_SIZE);
        /* Wait for transfer completed */
        while (0U == m_u8TransCompleteFlag) {
            DDL_Printf("Waiting for transmission to complete \r\n");
        }
#endif

        /* Check data value */
        if (0 == memcmp(m_au8RwDirectBuf, m_au8RwExtendBuf, u32TransSize)) {
            i32Ret = LL_OK;
        }
    }

    return i32Ret;
}

/**
 * @brief  Main function of SDIOC SDIO card.
 * @param  None
 * @retval int32_t return value, if needed
 */
int32_t main(void)
{
    int32_t i32Ret;

    /* Peripheral registers write unprotected */
    LL_PERIPH_WE(EXAMPLE_PERIPH_WE);

    /* Configure BSP */
    BSP_CLK_Init();
    BSP_LED_Init();

    /* Configure UART */
    DDL_PrintfInit(BSP_PRINTF_DEVICE, BSP_PRINTF_BAUDRATE, BSP_PRINTF_Preinit);

    /* Configure SDIO card */
    i32Ret = SdioCard_Config(&m_stcSdioHost);
    if (LL_OK == i32Ret) {
        /* SDIO card transfer data */
        i32Ret = SdioCard_RdMultiBlock(&m_stcSdioHost);
    }

    if (LL_OK != i32Ret) {
        /* Test failed */
        BSP_LED_On(LED_RED);
        BSP_LED_Off(LED_BLUE);
    } else {
        /* Test success */
        BSP_LED_On(LED_BLUE);
        BSP_LED_Off(LED_RED);
    }

    /* Peripheral registers write protected */
    LL_PERIPH_WP(EXAMPLE_PERIPH_WP);

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
