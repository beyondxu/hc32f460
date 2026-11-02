/**
 *******************************************************************************
 * @file  can/can_classical/source/main.c
 * @brief Main program of CAN classical for the Device Driver Library.
 @verbatim
   Change Logs:
   Date             Author          Notes
   2022-03-31       CDT             First version
   2023-01-15       CDT             CAN_IrqCallback() fixed
   2023-09-30       CDT             SysTick_Handler add __DSB for Arm Errata 838869
                                    Modified the prescaler of CAN communication clock from 1 to 2.
   2024-11-08       CDT             Example optimized.
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
 * @addtogroup CAN_Classical
 * @{
 */

/*******************************************************************************
 * Local type definitions ('typedef')
 ******************************************************************************/

/*******************************************************************************
 * Local pre-processor symbols/macros ('#define')
 ******************************************************************************/
/* CAN unit definitions. */
#define CAN_UNIT                        (CM_CAN)
#define CAN_PERIPH_CLK                  (FCG1_PERIPH_CAN)

#define CAN_TX_PORT                     (GPIO_PORT_B)
#define CAN_TX_PIN                      (GPIO_PIN_07)
#define CAN_TX_PIN_FUNC                 (GPIO_FUNC_50)

#define CAN_RX_PORT                     (GPIO_PORT_B)
#define CAN_RX_PIN                      (GPIO_PIN_06)
#define CAN_RX_PIN_FUNC                 (GPIO_FUNC_51)

#define CAN_INT_PRIO                    (DDL_IRQ_PRIO_03)
#define CAN_INT_SRC                     (INT_SRC_CAN_INT)
#define CAN_INT_IRQn                    (INT122_IRQn)

/* CAN interrupt type selection. */
#define CAN_INT_SEL                     (CAN_INT_PTB_TX      | \
                                         CAN_INT_RX          | \
                                         CAN_INT_ERR_INT)

/* CAN PHY standby pin. */
#define CAN_PHY_STBY_PORT               (GPIO_PORT_D)
#define CAN_PHY_STBY_PIN                (GPIO_PIN_15)

/* Acceptance filter. */
#define CAN_FILTER_SEL                  (CAN_FILTER1 | CAN_FILTER2 | CAN_FILTER3)
#define CAN_FILTER_NUM                  (3U)

#define CAN_FILTER1_ID                  (0x701UL)
#define CAN_FILTER1_ID_MASK             (0x0UL)
#define CAN_FILTER1_ID_TYPE             (CAN_ID_STD)        /*!< Only accept frames with standard ID 0x701. */

#define CAN_FILTER2_ID                  (0x12131415UL)
#define CAN_FILTER2_ID_MASK             (0x000000F0UL)
#define CAN_FILTER2_ID_TYPE             (CAN_ID_EXT)        /*!< Accept frames with extended ID 0x121314x5. */

#define CAN_FILTER3_ID                  (0x1A1B1C1DUL)
#define CAN_FILTER3_ID_MASK             (0x0000000FUL)
#define CAN_FILTER3_ID_TYPE             (CAN_ID_STD_EXT)    /*!< Accept frames with extended ID 0x1A1B1C1x, \
                                                                 standard ID 0x41x(0x1A1B1C1D & 0x7FF(standard ID mask)). */
/* Timeout value */
#define CAN_TX_TIMEOUT_MS               (100U)

/* Number of RX frame */
#define CAN_RX_FRAME_NUM                (10U)

/*******************************************************************************
 * Global variable definitions (declared in header file with 'extern')
 ******************************************************************************/

/*******************************************************************************
 * Local function prototypes ('static')
 ******************************************************************************/
static void CanPinConfig(void);
static void CanInitConfig(void);
static void CanIrqConfig(void);
static void CanPhyEnable(void);

static void CanRxTx(void);

static void CAN_IrqCallback(void);

/*******************************************************************************
 * Local variable definitions ('static')
 ******************************************************************************/
#if (LL_PRINT_ENABLE == DDL_ON)
const static char *m_s8IDTypeStr[] = {
    "standard",
    "extended",
};

const static char *m_s8ErrorTypeStr[] = {
    "NO error.",
    "Bit Error.",
    "Form Error.",
    "Stuff Error.",
    "ACK Error.",
    "CRC Error.",
    "Other Error.",
    "Error type is NOT defined.",
};
#endif

static stc_can_rx_frame_t m_astRxFrame[CAN_RX_FRAME_NUM];

static __IO uint8_t m_u8RxFlag    = 0U;
static __IO uint8_t m_u8PTBTxFlag = 0U;
static __IO uint8_t m_u8TxStart   = 0U;
static __IO uint8_t m_u8Busoff    = 0U;
static __IO uint32_t m_u32TxTick  = 0UL;

/*******************************************************************************
 * Function implementation - global ('extern') and local ('static')
 ******************************************************************************/
/**
 * @brief  Main function of can_classical project
 * @param  None
 * @retval int32_t return value, if needed
 */
int32_t main(void)
{
    /* MCU Peripheral registers write unprotected. */
    LL_PERIPH_WE(LL_PERIPH_GPIO | LL_PERIPH_FCG | LL_PERIPH_PWC_CLK_RMU | \
                 LL_PERIPH_EFM | LL_PERIPH_SRAM);
    /* Configures the system clock to 200MHz. */
    BSP_CLK_Init();
    /* BSP keys are used as TX triggers. */
    BSP_KEY_Init();
    /* Initializes UART for debug printing. Baudrate is 115200. */
    DDL_PrintfInit(BSP_PRINTF_DEVICE, BSP_PRINTF_BAUDRATE, BSP_PRINTF_Preinit);
    /* Configures CAN. */
    CanPinConfig();
    CanInitConfig();
    CanIrqConfig();
    CanPhyEnable();
    /* For CAN TX timeout. */
    (void)SysTick_Init(1000U);
    /* MCU Peripheral registers write protected. */
    LL_PERIPH_WP(LL_PERIPH_GPIO | LL_PERIPH_FCG | LL_PERIPH_PWC_CLK_RMU | \
                 LL_PERIPH_EFM | LL_PERIPH_SRAM);

    /***************** Configuration end, application start **************/
    for (;;) {
        CanRxTx();
    }
}

/**
 * @brief  Specifies pin function for TXD and RXD.
 * @param  None
 * @retval None
 */
static void CanPinConfig(void)
{
    GPIO_SetFunc(CAN_TX_PORT, CAN_TX_PIN, CAN_TX_PIN_FUNC);
    GPIO_SetFunc(CAN_RX_PORT, CAN_RX_PIN, CAN_RX_PIN_FUNC);
}

/**
 * @brief  CAN initial configuration.
 * @param  None
 * @retval None
 */
static void CanInitConfig(void)
{
    stc_can_init_t stcCanInit;
    stc_can_filter_config_t astcFilter[CAN_FILTER_NUM] = {
        {CAN_FILTER1_ID, CAN_FILTER1_ID_MASK, CAN_FILTER1_ID_TYPE},
        {CAN_FILTER2_ID, CAN_FILTER2_ID_MASK, CAN_FILTER2_ID_TYPE},
        {CAN_FILTER3_ID, CAN_FILTER3_ID_MASK, CAN_FILTER3_ID_TYPE},
    };

    /* Initializes CAN. */
    (void)CAN_StructInit(&stcCanInit);
    stcCanInit.stcBitCfg.u32Prescaler = 2U;
    stcCanInit.stcBitCfg.u32TimeSeg1  = 6U;
    stcCanInit.stcBitCfg.u32TimeSeg2  = 2U;
    stcCanInit.stcBitCfg.u32SJW       = 2U;
    stcCanInit.pstcFilter             = astcFilter;
    stcCanInit.u16FilterSelect        = CAN_FILTER_SEL;
    stcCanInit.u8WorkMode             = CAN_WORK_MD_NORMAL;

    /* Enable peripheral clock of CAN. */
    FCG_Fcg1PeriphClockCmd(CAN_PERIPH_CLK, ENABLE);
    (void)CAN_Init(CAN_UNIT, &stcCanInit);
    /* Enable the interrupts, the status flags can be read. */
    CAN_IntCmd(CAN_UNIT, CAN_INT_ALL, DISABLE);
    /* Enable the interrupts that needed. */
    CAN_IntCmd(CAN_UNIT, CAN_INT_SEL, ENABLE);
}

/**
 * @brief  CAN interrupt configuration.
 * @param  None
 * @retval None
 */
static void CanIrqConfig(void)
{
    stc_irq_signin_config_t stcIrq;

    stcIrq.enIntSrc    = CAN_INT_SRC;
    stcIrq.enIRQn      = CAN_INT_IRQn;
    stcIrq.pfnCallback = &CAN_IrqCallback;
    (void)INTC_IrqSignIn(&stcIrq);
    NVIC_ClearPendingIRQ(stcIrq.enIRQn);
    NVIC_SetPriority(stcIrq.enIRQn, CAN_INT_PRIO);
    NVIC_EnableIRQ(stcIrq.enIRQn);
}

/**
 * @brief  Set CAN PHY STB pin as low.
 * @param  None
 * @retval None
 */
static void CanPhyEnable(void)
{
    /* Set PHY STB pin as low. */
    GPIO_ResetPins(CAN_PHY_STBY_PORT, CAN_PHY_STBY_PIN);
    GPIO_OutputCmd(CAN_PHY_STBY_PORT, CAN_PHY_STBY_PIN, ENABLE);
}

/**
 * @brief  CAN receives and transmits data.
 * @param  None
 * @retval None
 */
static void CanRxTx(void)
{
    uint8_t i;
    uint8_t j;
    uint8_t u8RxFrameNum = 0U;
    int32_t i32Ret;

    if (m_u8RxFlag == 0U) {
        return;
    }
    m_u8RxFlag = 0U;

    /* Read frames here or in CAN_IrqCallback */
    /* Get all received frames. */
    do {
        i32Ret = CAN_GetRxFrame(CAN_UNIT, &m_astRxFrame[u8RxFrameNum]);
        if (i32Ret == LL_OK) {
            u8RxFrameNum++;
        }
    } while (i32Ret == LL_OK);

    /* Handle received frames. */
    for (i = 0U; i < u8RxFrameNum; i++) {
        DDL_Printf("CAN received frame with %s ID %.8x:\r\n", \
                   m_s8IDTypeStr[m_astRxFrame[i].IDE],    \
                   (unsigned int)m_astRxFrame[i].u32ID);
        for (j = 0; j < (uint8_t)m_astRxFrame[i].DLC; j++) {
            DDL_Printf(" %.2x.", m_astRxFrame[i].au8Data[j]);
        }
        DDL_Printf("\r\n");
    }

    /* Send back one frame for simple test */
    if (m_u8TxStart == 0U) {
        /* Transmit one frame via PTB is recommended */
        (void)CAN_FillTxFrame(CAN_UNIT, CAN_TX_BUF_PTB, (stc_can_tx_frame_t *)&m_astRxFrame[0]);
        CAN_StartTx(CAN_UNIT, CAN_TX_REQ_PTB);

        m_u32TxTick = CAN_TX_TIMEOUT_MS;
        m_u8TxStart = 1U;
    } else {
        DDL_Printf("CAN is transmitting.\r\n");
    }
}

/**
 * @brief  CAN interrupt callback.
 * @param  None
 * @retval None
 */
static void CAN_IrqCallback(void)
{
    uint32_t u32Status;
    stc_can_error_info_t stcErr;

    u32Status = CAN_GetStatusValue(CAN_UNIT);
    if (u32Status != 0U) {
        CAN_ClearStatus(CAN_UNIT, u32Status);
    }

    if ((u32Status & CAN_FLAG_PTB_TX) != 0U) {
        DDL_Printf("PTB transmitted.\r\n");
        m_u8PTBTxFlag = 1U;
    }

    if ((u32Status & CAN_FLAG_RX) != 0U) {
        /* Received frame can be read here. */
        DDL_Printf("Received a frame.\r\n");
        m_u8RxFlag = 1U;
    }

    if ((u32Status & CAN_FLAG_ERR_INT) != 0U) {
        if ((u32Status & CAN_FLAG_BUS_OFF) != 0U) {
            CAN_ExitLocalReset(CAN_UNIT);
            m_u8Busoff = 1U;
            DDL_Printf("---> CAN BUS OFF\r\n");
        } else {
            (void)CAN_GetErrorInfo(CAN_UNIT, &stcErr);
            DDL_Printf("---> CAN error type: %u, %s\r\n", stcErr.u8ErrorType, m_s8ErrorTypeStr[stcErr.u8ErrorType]);
        }
    }
}

/**
 * @brief  SysTick interrupt handler function.
 * @param  None
 * @retval None
 */
void SysTick_Handler(void)
{
    if (m_u8TxStart != 0U) {
        if ((m_u8PTBTxFlag != 0U) || (m_u8Busoff != 0U)) {
            m_u8TxStart   = 0U;
            m_u8PTBTxFlag = 0U;
            m_u8Busoff    = 0U;
            m_u32TxTick   = 0UL;
        }

        if (m_u32TxTick > 0U) {
            m_u32TxTick--;
            if (m_u32TxTick == 0UL) {
                FCG_Fcg1PeriphClockCmd(CAN_PERIPH_CLK, DISABLE);
                CanInitConfig();
                m_u8TxStart   = 0U;
                m_u8PTBTxFlag = 0U;
                DDL_Printf("CAN TX timeout!\r\n");
            }
        }
    }

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
