#include "printer_row_audit.h"
#include "printer_control.h"
#include "main.h"
#include "tph_driver.h"
#include <stdio.h>
#include <string.h>
#include "tph_m0333n.h"

#define EXAMPLE_PERIPH_WE       (LL_PERIPH_GPIO | LL_PERIPH_EFM | LL_PERIPH_FCG | \
                                 LL_PERIPH_PWC_CLK_RMU | LL_PERIPH_SRAM)
#define EXAMPLE_PERIPH_WP       (LL_PERIPH_EFM | LL_PERIPH_FCG | LL_PERIPH_SRAM)

/* ★ 关键对齐：单列标准 100 字节 ★ */
#define TPH_COLUMN_BYTES        (100U)

/* ================= 1. J4 调试串口 ================= */
#define J4_UART_UNIT            (CM_USART2)
#define J4_UART_CLK             (FCG1_PERIPH_USART2)
#define J4_UART_TX_PORT         (GPIO_PORT_A)
#define J4_UART_TX_PIN          (GPIO_PIN_12)
#define J4_UART_TX_FUNC         (GPIO_FUNC_36)
#define J4_UART_RX_PORT         (GPIO_PORT_A)
#define J4_UART_RX_PIN          (GPIO_PIN_11)
#define J4_UART_RX_FUNC         (GPIO_FUNC_37)

/* ================= 2. 硬件互联握手引脚 ================= */
#define READY_PORT              (GPIO_PORT_H)
#define READY_PIN               (GPIO_PIN_02)   // PH2 (BK_2): 告诉 BK7258 就绪

#define SYNC_PORT               (GPIO_PORT_C)
#define SYNC_PIN                (GPIO_PIN_13)   // PC13 (BK_1): 接收打火物理同步脉冲

/* ================= 3. SPI1 引脚复用 ================= */
#define SPI_UNIT                (CM_SPI1)
#define SPI_CLK                 (FCG1_PERIPH_SPI1)
#define SPI_TX_EVT_SRC          (EVT_SRC_SPI1_SPTI)
#define SPI_RX_EVT_SRC          (EVT_SRC_SPI1_SPRI)

#define SPI_SS_PORT             (GPIO_PORT_A)
#define SPI_SS_PIN              (GPIO_PIN_04)
#define SPI_SS_FUNC             (GPIO_FUNC_42)

#define SPI_SCK_PORT            (GPIO_PORT_A)
#define SPI_SCK_PIN             (GPIO_PIN_05)
#define SPI_SCK_FUNC            (GPIO_FUNC_43)

#define SPI_MOSI_PORT           (GPIO_PORT_A)
#define SPI_MOSI_PIN            (GPIO_PIN_06)
#define SPI_MOSI_FUNC           (GPIO_FUNC_40)

#define SPI_MISO_PORT           (GPIO_PORT_A)
#define SPI_MISO_PIN            (GPIO_PIN_07)
#define SPI_MISO_FUNC           (GPIO_FUNC_41)

/* ================= 4. DMA 通道定义 ================= */
#define DMA_UNIT                (CM_DMA1)
#define DMA_CLK                 (FCG0_PERIPH_DMA1 | FCG0_PERIPH_AOS)

#define DMA_TX_CH               (DMA_CH0)
#define DMA_TX_TRIG_CH          (AOS_DMA1_0)

#define DMA_RX_CH               (DMA_CH1)
#define DMA_RX_INT_CH           (DMA_INT_TC_CH1)
#define DMA_RX_TRIG_CH          (AOS_DMA1_1)
#define DMA_RX_INT_SRC          (INT_SRC_DMA1_TC1)
#define DMA_RX_IRQ_NUM          (INT006_IRQn)

/* 缓存定义 */
static uint8_t u8SelfTestRxBuf[TPH_COLUMN_BYTES];
static uint8_t u8SelfTestTxBuf[TPH_COLUMN_BYTES];

static uint8_t u8PrintBuffer[2][TPH_COLUMN_BYTES]; // 打印 Ping-Pong 双缓存
static volatile uint8_t u8CurrentBufIdx = 0U;
static volatile en_flag_status_t enDmaRxDone = RESET;

static int32_t J4_UART_PreInit(void *vpDevice, uint32_t u32Baudrate) {
    stc_usart_uart_init_t stcUartInit;
    stc_gpio_init_t stcGpioInit;
    CM_USART_TypeDef *USARTx = (CM_USART_TypeDef *)vpDevice;

    FCG_Fcg1PeriphClockCmd(FCG1_PERIPH_USBFS, DISABLE);
    FCG_Fcg1PeriphClockCmd(J4_UART_CLK, ENABLE);

    (void)GPIO_StructInit(&stcGpioInit);
    stcGpioInit.u16PinDrv        = PIN_HIGH_DRV;
    stcGpioInit.u16PinDir        = PIN_DIR_OUT;
    stcGpioInit.u16PinOutputType = PIN_OUT_TYPE_CMOS;
    (void)GPIO_Init(J4_UART_TX_PORT, J4_UART_TX_PIN, &stcGpioInit);

    stcGpioInit.u16PinDrv        = PIN_LOW_DRV;
    stcGpioInit.u16PinDir        = PIN_DIR_IN;
    stcGpioInit.u16PullUp        = PIN_PU_ON;
    (void)GPIO_Init(J4_UART_RX_PORT, J4_UART_RX_PIN, &stcGpioInit);

    GPIO_SetFunc(J4_UART_TX_PORT, J4_UART_TX_PIN, J4_UART_TX_FUNC);
    GPIO_SetFunc(J4_UART_RX_PORT, J4_UART_RX_PIN, J4_UART_RX_FUNC);

    (void)USART_UART_StructInit(&stcUartInit);
    stcUartInit.u32ClockSrc          = USART_CLK_SRC_INTERNCLK;
    stcUartInit.u32ClockDiv          = USART_CLK_DIV1;
    stcUartInit.u32Baudrate          = u32Baudrate;
    stcUartInit.u32DataWidth         = USART_DATA_WIDTH_8BIT;
    stcUartInit.u32StopBit           = USART_STOPBIT_1BIT;
    stcUartInit.u32Parity            = USART_PARITY_NONE;
    stcUartInit.u32OverSampleBit     = USART_OVER_SAMPLE_16BIT;
    stcUartInit.u32FirstBit          = USART_FIRST_BIT_LSB;
    stcUartInit.u32HWFlowControl     = USART_HW_FLOWCTRL_RTS;
    (void)USART_UART_Init(USARTx, &stcUartInit, NULL);

    USART_FuncCmd(USARTx, (USART_TX | USART_RX), ENABLE);
    return LL_OK;
}

static void DMA_RxComplete_Callback(void) {
    enDmaRxDone = SET;
    DMA_ClearTransCompleteStatus(DMA_UNIT, DMA_RX_INT_CH);
}

static void SystemClock_200MHz_Config(void) {
    stc_clock_pll_init_t stcPLLInit;

    PWC_HighSpeedToHighPerformance();
    EFM_SetWaitCycle(EFM_WAIT_CYCLE5);
    SRAM_SetWaitCycle(SRAM_SRAMH, SRAM_WAIT_CYCLE0, SRAM_WAIT_CYCLE0);
    SRAM_SetWaitCycle((SRAM_SRAM12 | SRAM_SRAM3 | SRAM_SRAMR), SRAM_WAIT_CYCLE1, SRAM_WAIT_CYCLE1);
    GPIO_SetReadWaitCycle(GPIO_RD_WAIT3);

    MODIFY_REG32(CM_CMU->SCFGR,
                 (CMU_SCFGR_HCLKS | CMU_SCFGR_EXCKS | CMU_SCFGR_PCLK0S |
                  CMU_SCFGR_PCLK1S | CMU_SCFGR_PCLK2S | CMU_SCFGR_PCLK3S | CMU_SCFGR_PCLK4S),
                 (CLK_HCLK_DIV1  | CLK_EXCLK_DIV2 | CLK_PCLK0_DIV1 |
                  CLK_PCLK1_DIV2 | CLK_PCLK2_DIV4 | CLK_PCLK3_DIV4 | CLK_PCLK4_DIV4));

    (void)CLK_HrcCmd(ENABLE);
    (void)CLK_PLLStructInit(&stcPLLInit);
    stcPLLInit.u8PLLState = CLK_PLL_ON;
    stcPLLInit.PLLCFGR_f.PLLSRC = CLK_PLL_SRC_HRC;
    stcPLLInit.PLLCFGR_f.PLLM   = 0U;
    stcPLLInit.PLLCFGR_f.PLLN   = 24U;
    stcPLLInit.PLLCFGR_f.PLLP   = 1U;
    (void)CLK_PLLInit(&stcPLLInit);

    while ((CM_CMU->OSCSTBSR & (1UL << 5)) == 0UL) {}
    CLK_SetSysClockSrc(CLK_SYSCLK_SRC_PLL);
    SystemCoreClockUpdate();
}

static void Hardware_GPIO_Init(void) {
    stc_gpio_init_t stcGpioInit;
    (void)GPIO_StructInit(&stcGpioInit);

    // PH2 (BK_2) 输出就绪引脚，默认拉低
    stcGpioInit.u16PinDir        = PIN_DIR_OUT;
    stcGpioInit.u16PinDrv        = PIN_HIGH_DRV;
    stcGpioInit.u16PinOutputType = PIN_OUT_TYPE_CMOS;
    stcGpioInit.u16PinState      = PIN_STAT_RST; 
    GPIO_Init(READY_PORT, READY_PIN, &stcGpioInit);

    // PC13 (BK_1) 接收打火/同步脉冲，配置为输入
    stcGpioInit.u16PinDir = PIN_DIR_IN;
    stcGpioInit.u16PullUp = PIN_PU_OFF;
    GPIO_Init(SYNC_PORT, SYNC_PIN, &stcGpioInit);
}

static void SPI1_Slave_DMA_Base_Init(void) {
    stc_spi_init_t stcSpiInit;
    stc_dma_init_t stcDmaInit;
    stc_gpio_init_t stcGpioInit;
    stc_irq_signin_config_t stcIrqSignConfig;

    // 1. SPI GPIO
    (void)GPIO_StructInit(&stcGpioInit);
    stcGpioInit.u16PinDrv = PIN_HIGH_DRV;
    (void)GPIO_Init(SPI_SS_PORT,   SPI_SS_PIN,   &stcGpioInit);
    (void)GPIO_Init(SPI_SCK_PORT,  SPI_SCK_PIN,  &stcGpioInit);
    (void)GPIO_Init(SPI_MOSI_PORT, SPI_MOSI_PIN, &stcGpioInit);
    (void)GPIO_Init(SPI_MISO_PORT, SPI_MISO_PIN, &stcGpioInit);

    GPIO_SetFunc(SPI_SS_PORT,   SPI_SS_PIN,   SPI_SS_FUNC);
    GPIO_SetFunc(SPI_SCK_PORT,  SPI_SCK_PIN,  SPI_SCK_FUNC);
    GPIO_SetFunc(SPI_MOSI_PORT, SPI_MOSI_PIN, SPI_MOSI_FUNC);
    GPIO_SetFunc(SPI_MISO_PORT, SPI_MISO_PIN, SPI_MISO_FUNC);

    // 2. SPI Slave (Mode 3, 全双工)
    FCG_Fcg1PeriphClockCmd(SPI_CLK, ENABLE);
    SPI_StructInit(&stcSpiInit);
    stcSpiInit.u32WireMode          = SPI_4_WIRE;
    stcSpiInit.u32TransMode         = SPI_FULL_DUPLEX;
    stcSpiInit.u32MasterSlave       = SPI_SLAVE;
    stcSpiInit.u32Parity            = SPI_PARITY_INVD;
    stcSpiInit.u32SpiMode           = SPI_MD_3; // 与 BK7258 Mode 3 严格一致
    stcSpiInit.u32DataBits          = SPI_DATA_SIZE_8BIT;
    stcSpiInit.u32FirstBit          = SPI_FIRST_MSB;
    stcSpiInit.u32FrameLevel        = SPI_1_FRAME;
    (void)SPI_Init(SPI_UNIT, &stcSpiInit);

    // 3. DMA 模块时钟与基础配置
    FCG_Fcg0PeriphClockCmd(DMA_CLK, ENABLE);
    (void)DMA_StructInit(&stcDmaInit);
    stcDmaInit.u32BlockSize   = 1UL;
    stcDmaInit.u32TransCount  = TPH_COLUMN_BYTES;
    stcDmaInit.u32DataWidth   = DMA_DATAWIDTH_8BIT;

    // ★ 关键：同时配置 DMA TX 通道，防止全双工发生欠载死锁 ★
    stcDmaInit.u32IntEn       = DMA_INT_DISABLE;
    stcDmaInit.u32SrcAddrInc  = DMA_SRC_ADDR_INC;
    stcDmaInit.u32DestAddrInc = DMA_DEST_ADDR_FIX;
    stcDmaInit.u32SrcAddr     = (uint32_t)(&u8SelfTestTxBuf[0]);
    stcDmaInit.u32DestAddr    = (uint32_t)(&SPI_UNIT->DR);
    (void)DMA_Init(DMA_UNIT, DMA_TX_CH, &stcDmaInit);
    AOS_SetTriggerEventSrc(DMA_TX_TRIG_CH, SPI_TX_EVT_SRC);

    // DMA RX 通道配置
    stcDmaInit.u32IntEn       = DMA_INT_ENABLE;
    stcDmaInit.u32SrcAddrInc  = DMA_SRC_ADDR_FIX;
    stcDmaInit.u32DestAddrInc = DMA_DEST_ADDR_INC;
    stcDmaInit.u32SrcAddr     = (uint32_t)(&SPI_UNIT->DR);
    stcDmaInit.u32DestAddr    = (uint32_t)(&u8SelfTestRxBuf[0]);
    (void)DMA_Init(DMA_UNIT, DMA_RX_CH, &stcDmaInit);
    AOS_SetTriggerEventSrc(DMA_RX_TRIG_CH, SPI_RX_EVT_SRC);

    // 注册 DMA RX 完成中断
    stcIrqSignConfig.enIntSrc    = DMA_RX_INT_SRC;
    stcIrqSignConfig.enIRQn      = DMA_RX_IRQ_NUM;
    stcIrqSignConfig.pfnCallback = &DMA_RxComplete_Callback;
    (void)INTC_IrqSignIn(&stcIrqSignConfig);
    NVIC_ClearPendingIRQ(stcIrqSignConfig.enIRQn);
    NVIC_SetPriority(stcIrqSignConfig.enIRQn, DDL_IRQ_PRIO_02);
    NVIC_EnableIRQ(stcIrqSignConfig.enIRQn);

    DMA_Cmd(DMA_UNIT, ENABLE);
}


// 辅助函数：崭新装载 DMA 与 SPI 从机
static void Slave_Arm_SPI_DMA(uint8_t *pTxBuf, uint8_t *pRxBuf, uint16_t len) {
    stc_dma_init_t stcDmaInit;

    // 1. 关闭以彻底复位
    SPI_Cmd(SPI_UNIT, DISABLE);
    DMA_ChCmd(DMA_UNIT, DMA_TX_CH, DISABLE);
    DMA_ChCmd(DMA_UNIT, DMA_RX_CH, DISABLE);

    // 2. 装载 TX 通道
    (void)DMA_StructInit(&stcDmaInit);
    stcDmaInit.u32BlockSize   = 1UL;
    stcDmaInit.u32TransCount  = len;
    stcDmaInit.u32DataWidth   = DMA_DATAWIDTH_8BIT;
    stcDmaInit.u32IntEn       = DMA_INT_DISABLE;
    stcDmaInit.u32SrcAddrInc  = DMA_SRC_ADDR_INC;
    stcDmaInit.u32DestAddrInc = DMA_DEST_ADDR_FIX;
    stcDmaInit.u32SrcAddr     = (uint32_t)pTxBuf;
    stcDmaInit.u32DestAddr    = (uint32_t)(&SPI_UNIT->DR);
    (void)DMA_Init(DMA_UNIT, DMA_TX_CH, &stcDmaInit);
    AOS_SetTriggerEventSrc(DMA_TX_TRIG_CH, SPI_TX_EVT_SRC);

    // 3. 装载 RX 通道
    stcDmaInit.u32IntEn       = DMA_INT_ENABLE;
    stcDmaInit.u32SrcAddrInc  = DMA_SRC_ADDR_FIX;
    stcDmaInit.u32DestAddrInc = DMA_DEST_ADDR_INC;
    stcDmaInit.u32SrcAddr     = (uint32_t)(&SPI_UNIT->DR);
    stcDmaInit.u32DestAddr    = (uint32_t)pRxBuf;
    (void)DMA_Init(DMA_UNIT, DMA_RX_CH, &stcDmaInit);
    AOS_SetTriggerEventSrc(DMA_RX_TRIG_CH, SPI_RX_EVT_SRC);

    // 4. 清除状态标志
    uint32_t u32Err = READ_REG32_BIT(SPI_UNIT->SR, SPI_FLAG_CLR_ALL);
    if (u32Err != 0UL) {
        SPI_ClearStatus(SPI_UNIT, u32Err);
    }
    DMA_ClearTransCompleteStatus(DMA_UNIT, DMA_RX_INT_CH);

    enDmaRxDone = RESET;
    DMA_ChCmd(DMA_UNIT, DMA_TX_CH, ENABLE);
    DMA_ChCmd(DMA_UNIT, DMA_RX_CH, ENABLE);
    SPI_Cmd(SPI_UNIT, ENABLE);
}

void HC32_PowerOn_SelfTest_100B(void) {
    printf("\r\n[HC32 SelfTest] Waiting for BK7258 Active-LOW Request (PC13 LOW)...\r\n");

    // 默认回显数据 (FF FE FD FC...)
    for (uint32_t i = 0; i < TPH_COLUMN_BYTES; i++) {
        u8SelfTestTxBuf[i] = (uint8_t)(~i & 0xFF);
    }

    // BK_2 默认保持高电平 (空闲状态，配合 R61 上拉)
    GPIO_SetPins(READY_PORT, READY_PIN);

    // ====================================================
    // ★ 阶段 1：响应 BK7258 的写入测试包 (低电平有效)
    // ====================================================
    // 1. 等待 BK7258 拉低 PC13 (BK_1) 发起请求：只要还是默认的 3.3V 高电平，就一直等待
    while (PIN_SET == GPIO_ReadInputPins(SYNC_PORT, SYNC_PIN)) {
    }

    printf("[HC32 SelfTest] PC13 LOW detected! BK7258 Triggered Step 1!\r\n");

    // 2. 收到拉低请求！立即启动 SPI1 和 DMA
    Slave_Arm_SPI_DMA(u8SelfTestTxBuf, u8SelfTestRxBuf, TPH_COLUMN_BYTES);

    // 3. 回应 ACK：将 PH2 (BK_2) 拉低 (0V，通知 BK7258 从机就绪)
    GPIO_ResetPins(READY_PORT, READY_PIN);

    // 4. 等待 100 字节接收完成
    while (RESET == enDmaRxDone) {
    }
    enDmaRxDone = RESET;

    // 5. 传输完成：关闭 SPI，释放 PH2 恢复高电平
    SPI_Cmd(SPI_UNIT, DISABLE);
    /* Echo the actual received bytes, not an unrelated canned pattern. */
    for (uint32_t i = 0; i < TPH_COLUMN_BYTES; ++i) {
        u8SelfTestTxBuf[i] = (uint8_t)~u8SelfTestRxBuf[i];
    }

    GPIO_SetPins(READY_PORT, READY_PIN);

    // 6. 等待 BK7258 释放 PC13 恢复为高电平
    while (PIN_RESET == GPIO_ReadInputPins(SYNC_PORT, SYNC_PIN)) {
    }

    printf("[HC32 SelfTest] Step 1 Finished. Waiting for Read Request (PC13 LOW)...\r\n");

    // ====================================================
    // ★ 阶段 2：响应 BK7258 的读回显请求 (低电平有效)
    // ====================================================
    // 1. 等待 BK7258 再次拉低 PC13 (BK_1) 发起读请求
    while (PIN_SET == GPIO_ReadInputPins(SYNC_PORT, SYNC_PIN)) {
    }

    printf("[HC32 SelfTest] PC13 LOW detected! BK7258 Triggered Step 2!\r\n");

    // 2. 崭新装载 SPI1 和 DMA (装入取反数据)
    Slave_Arm_SPI_DMA(u8SelfTestTxBuf, u8SelfTestRxBuf, TPH_COLUMN_BYTES);

    // 3. 回应 ACK：将 PH2 (BK_2) 拉低 (0V)
    GPIO_ResetPins(READY_PORT, READY_PIN);

    // 4. 等待 BK7258 时钟读走 100 字节
    while (RESET == enDmaRxDone) {
    }
    enDmaRxDone = RESET;

    // 5. 自检完成：彻底关闭，恢复高电平
    SPI_Cmd(SPI_UNIT, DISABLE);
    GPIO_SetPins(READY_PORT, READY_PIN);

    // 等待 PC13 释放恢复高电平
    while (PIN_RESET == GPIO_ReadInputPins(SYNC_PORT, SYNC_PIN)) {
    }

    printf("======================================================\r\n");
    printf("[HC32 SelfTest] >>> SelfTest Finished! Link 100%% VERIFIED! <<<\r\n");
    printf("======================================================\r\n");
}



/* Per-column handshake: READY high = RX armed; READY low = DMA accepted,
 * firing or rearming. SYNC must remain low until READY acknowledges it.
 */
void HC32_Realtime_Print_Engine(void) {
    TPH_M0333N_Power(DISABLE);
    uint8_t head_powered = 0U;
    printer_row_audit_t row_audit = {0};
    printf("[HC32 CONTROL] PCTLv1 ROW_AUDIT_v1 ready power=OFF\r\n");
    u8CurrentBufIdx = 0U;
    Slave_Arm_SPI_DMA(u8SelfTestTxBuf, u8PrintBuffer[0], TPH_COLUMN_BYTES);
    GPIO_SetPins(READY_PORT, READY_PIN);

    for (;;) {
        if (PIN_RESET != GPIO_ReadInputPins(SYNC_PORT, SYNC_PIN)) continue;

        /* Never use a partially received column. If the master cancels its
         * request after a timeout, discard the partial DMA and rearm it.
         */
        while (RESET == enDmaRxDone) {
            if (PIN_SET == GPIO_ReadInputPins(SYNC_PORT, SYNC_PIN)) break;
        }
        GPIO_ResetPins(READY_PORT, READY_PIN);
        if (SET == enDmaRxDone) {
            /* DMA owns no further writes to this buffer after completion. */
            __DMB();
            const uint8_t *column = u8PrintBuffer[u8CurrentBufIdx];
            if (column[0] == PRINTER_CONTROL_TAG) {
                unsigned valid = column[1] == PRINTER_CONTROL_ON || column[1] == PRINTER_CONTROL_OFF;
                for (unsigned i = 0; i < PRINTER_CONTROL_MAGIC_LEN; ++i)
                    if (column[i + 2] != (uint8_t)PRINTER_CONTROL_MAGIC[i]) valid = 0;
                for (unsigned i = 2 + PRINTER_CONTROL_MAGIC_LEN; i < TPH_COLUMN_BYTES; ++i)
                    if (column[i]) valid = 0;
                if (valid) {
                    head_powered = column[1] == PRINTER_CONTROL_ON;
                    TPH_M0333N_Power(head_powered ? ENABLE : DISABLE);
                    if (head_powered) printer_row_audit_begin(&row_audit);
                    else printer_row_audit_end(&row_audit);
                    printf("[HC32 CONTROL] PCTLv1 power=%s\r\n", head_powered ? "ON" : "OFF");
                } else {
                    head_powered = 0;
                    TPH_M0333N_Power(DISABLE);
                    printer_row_audit_end(&row_audit);
                    printf("[HC32 CONTROL] invalid command power=OFF\r\n");
                }
            } else {
                unsigned valid = 1, first_slot = TPH_COLUMN_BYTES, active_bits = 0;
                static uint8_t reported_nonzero = 0;
                for (unsigned i = 0; i < TPH_COLUMN_BYTES; ++i) {
                    if (column[i] & 0xC0U) valid = 0;
                    if (column[i] && first_slot == TPH_COLUMN_BYTES) first_slot = i;
                    for (unsigned bit = 0; bit < 6; ++bit) active_bits += (column[i] >> bit) & 1U;
                }
                if (valid && head_powered) {
                    TPH_FireOneColumn(column);
                    if (!reported_nonzero && first_slot < TPH_COLUMN_BYTES) {
                        reported_nonzero = 1;
                        printf("[HC32 COL] slot=%u data=0x%02X addr=%u section=%u active_bits=%u fire_call_returned=1\r\n",
                               first_slot, (unsigned)column[first_slot], first_slot / 4U + 1U,
                               first_slot % 4U + 1U, active_bits);
                    }
                }
                if (head_powered)
                    printer_row_audit_column(&row_audit, column, active_bits, valid);
            }
            u8CurrentBufIdx ^= 1U;
        } else if (row_audit.active) {
            ++row_audit.canceled;
        }
        while (PIN_RESET == GPIO_ReadInputPins(SYNC_PORT, SYNC_PIN)) {}
        Slave_Arm_SPI_DMA(u8SelfTestTxBuf,
                         u8PrintBuffer[u8CurrentBufIdx], TPH_COLUMN_BYTES);
        GPIO_SetPins(READY_PORT, READY_PIN);
    }
}
void HC32_Standalone_Hardware_Test(void);
int32_t main(void) {
    LL_PERIPH_WE(EXAMPLE_PERIPH_WE);

    // 1. 系统主频升至 200MHz
    SystemClock_200MHz_Config();

    // 2. 初始化 J4 调试串口
    (void)LL_PrintfInit(J4_UART_UNIT, 115200UL, J4_UART_PreInit);
    printf("\r\n==============================================\r\n");
    printf("   HC32F460 Coprocessor Firmware Starting...   \r\n");
    printf("==============================================\r\n");

    // 3. 初始化握手 GPIO 与 SPI1 + DMA 从机
    Hardware_GPIO_Init();
    SPI1_Slave_DMA_Base_Init();

    // 4. 初始化喷墨头 GPIO 与引脚状态
    TPH_M0333N_Init();

    LL_PERIPH_WP(EXAMPLE_PERIPH_WP);

    // 5. 响应 BK7258 开机自检握手 (100 字节回显测试)
    HC32_PowerOn_SelfTest_100B();

    // 6. 自检通过，使能 15V (先 VL 后 VP)
    TPH_M0333N_Power(DISABLE);

    /* Standalone firing must be requested explicitly, not run on startup. */
    // 7. 进入实时喷墨流水线引擎 (等待 BK7258 发送数据并拉低 PC13 打火)
    HC32_Realtime_Print_Engine();

    return 0;
}
int32_t mainxx(void) {
    LL_PERIPH_WE(EXAMPLE_PERIPH_WE);

    // 1. 系统升频至 200MHz
    SystemClock_200MHz_Config();

    // 2. 初始化 J4 调试串口并绑定 printf
    (void)LL_PrintfInit(J4_UART_UNIT, 115200UL, J4_UART_PreInit);
    printf("\r\n==============================================\r\n");
    printf("   HC32F460 Coprocessor Firmware Starting...   \r\n");
    printf("==============================================\r\n");

    // 3. 初始化握手 GPIO 与 SPI1 + DMA 外设
    Hardware_GPIO_Init();
    SPI1_Slave_DMA_Base_Init();

    // 4. 初始化喷墨头 IO 与高压供电
    TPH_Init();
    TPH_PowerControl(ENABLE);

    LL_PERIPH_WP(EXAMPLE_PERIPH_WP);

    // ★ 步骤一：专门执行 100 字节自检，闭环响应 BK7258 的 bk_spi_master_selftest_100b ★
    HC32_PowerOn_SelfTest_100B();

    // ★ 步骤二：自检通过，无缝进入正式喷墨打印流水线引擎 ★
    HC32_Realtime_Print_Engine();
 // 1. 初始化喷头引脚与 DWT 定时器
    TPH_M0333N_Init();

    // 2. 规范上电：先 VL (10us) -> 后 VP
    TPH_M0333N_Power(ENABLE);

    LL_PERIPH_WP(EXAMPLE_PERIPH_WP);

    // 循环测试三色独立打火 (间隔 200ms)
    for (;;) {
        // ★ 测试 1：仅打火青色 (Cyan, D1 & D2) ★
        TPH_Test_SingleColor_Fire(COLOR_TEST_CYAN);
        DDL_DelayMS(200);

        // ★ 测试 2：仅打火品红 (Magenta, D3 & D4) ★
        TPH_Test_SingleColor_Fire(COLOR_TEST_MAGENTA);
        DDL_DelayMS(200);

        // ★ 测试 3：仅打火黄色 (Yellow, D5 & D6) ★
        TPH_Test_SingleColor_Fire(COLOR_TEST_YELLOW);
        DDL_DelayMS(500);
    }
    return 0;
}