/*FILE main.c*/
#include "main.h"
#include "tph_driver.h"

/* 统一的外设寄存器写保护与解锁定义 */
#define EXAMPLE_PERIPH_WE       (LL_PERIPH_GPIO | LL_PERIPH_EFM | LL_PERIPH_FCG | \
                                 LL_PERIPH_PWC_CLK_RMU | LL_PERIPH_SRAM)
#define EXAMPLE_PERIPH_WP       (LL_PERIPH_EFM | LL_PERIPH_FCG | LL_PERIPH_SRAM)


/* ================= 1. J4 接口 (USART2: PA12=RX, PA11=TX) ================= */
#define J4_UART_UNIT            (CM_USART2)
#define J4_UART_CLK             (FCG1_PERIPH_USART2)

#define J4_UART_TX_PORT         (GPIO_PORT_A)
#define J4_UART_TX_PIN          (GPIO_PIN_12)   // J4 Pin 3 (USB2_P)
#define J4_UART_TX_FUNC         (GPIO_FUNC_36)  // USART2_TX

#define J4_UART_RX_PORT         (GPIO_PORT_A)
#define J4_UART_RX_PIN          (GPIO_PIN_11)   // J4 Pin 2 (USB2_N)
#define J4_UART_RX_FUNC         (GPIO_FUNC_37)  // USART2_RX

static int32_t J4_UART_PreInit(void *vpDevice, uint32_t u32Baudrate) {
    stc_usart_uart_init_t stcUartInit;
    stc_gpio_init_t stcGpioInit;
    CM_USART_TypeDef *USARTx = (CM_USART_TypeDef *)vpDevice;

    // 1. 关闭 USBFS，释放 PA11 / PA12 物理管脚控制权
    FCG_Fcg1PeriphClockCmd(FCG1_PERIPH_USBFS, DISABLE);

    // 2. 开启 USART2 外设时钟
    FCG_Fcg1PeriphClockCmd(J4_UART_CLK, ENABLE);

    // 3. 配置引脚
    (void)GPIO_StructInit(&stcGpioInit);
    
    // 3.1 PA12 (TX) 配置
    stcGpioInit.u16PinDrv        = PIN_HIGH_DRV;
    stcGpioInit.u16PinDir        = PIN_DIR_OUT;
    stcGpioInit.u16PinOutputType = PIN_OUT_TYPE_CMOS;
    (void)GPIO_Init(J4_UART_TX_PORT, J4_UART_TX_PIN, &stcGpioInit);

    // 3.2 PA11 (RX) 配置
    stcGpioInit.u16PinDrv        = PIN_LOW_DRV;
    stcGpioInit.u16PinDir        = PIN_DIR_IN;
    stcGpioInit.u16PullUp        = PIN_PU_ON;
    (void)GPIO_Init(J4_UART_RX_PORT, J4_UART_RX_PIN, &stcGpioInit);

    // 4. 引脚功能复用 (FUNC_36 = USART2_TX, FUNC_37 = USART2_RX)
    GPIO_SetFunc(J4_UART_TX_PORT, J4_UART_TX_PIN, J4_UART_TX_FUNC);
    GPIO_SetFunc(J4_UART_RX_PORT, J4_UART_RX_PIN, J4_UART_RX_FUNC);

    // 5. 初始化串口参数
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
    
    if (LL_OK != USART_UART_Init(USARTx, &stcUartInit, NULL)) {
        return LL_ERR;
    }

    // 6. 开启发送和接收
    USART_FuncCmd(USARTx, (USART_TX | USART_RX), ENABLE);

    return LL_OK;
}
/* SPI1 引脚与外设定义 (连接主控 BK7258) */
#define SPI_UNIT                (CM_SPI1)
#define SPI_CLK                 (FCG1_PERIPH_SPI1)
#define SPI_RX_EVT_SRC          (EVT_SRC_SPI1_SPRI)

// PA4(NSS), PA5(SCK), PA6(MOSI), PA7(MISO)
#define SPI_NSS_PORT            (GPIO_PORT_A)
#define SPI_NSS_PIN             (GPIO_PIN_04)
#define SPI_NSS_FUNC            (GPIO_FUNC_40)

#define SPI_SCK_PORT            (GPIO_PORT_A)
#define SPI_SCK_PIN             (GPIO_PIN_05)
#define SPI_SCK_FUNC            (GPIO_FUNC_41)

#define SPI_MOSI_PORT           (GPIO_PORT_A)
#define SPI_MOSI_PIN            (GPIO_PIN_06)
#define SPI_MOSI_FUNC           (GPIO_FUNC_42)

#define SPI_MISO_PORT           (GPIO_PORT_A)
#define SPI_MISO_PIN            (GPIO_PIN_07)
#define SPI_MISO_FUNC           (GPIO_FUNC_43)

/* DMA 通信定义 */
#define DMA_UNIT                (CM_DMA1)
#define DMA_CLK                 (FCG0_PERIPH_DMA1 | FCG0_PERIPH_AOS)
#define DMA_RX_CH               (DMA_CH0)
#define DMA_RX_INT_CH           (DMA_INT_TC_CH0)
#define DMA_RX_TRIG_CH          (AOS_DMA1_0)
#define DMA_RX_INT_SRC          (INT_SRC_DMA1_TC0)
#define DMA_RX_IRQ_NUM          (INT000_IRQn)



/* 点阵双缓冲区定义 */
static uint8_t u8PrintBuffer[2][TPH_COLUMN_BYTES];
static volatile uint8_t u8CurrentBufferIndex = 0U;
static volatile en_flag_status_t enColumnDataReady = RESET;

/**
 * @brief  DMA 传输完成中断回调 (接收完一整列 100 字节点阵后触发)
 */
static void DMA_TransCompleteCallback(void) {
    enColumnDataReady = SET;
    DMA_ClearTransCompleteStatus(DMA_UNIT, DMA_RX_INT_CH);

    // 自动重装下一次 DMA 接收至另一块 Ping-Pong 缓冲区
    uint8_t u8NextIndex = u8CurrentBufferIndex ^ 1U;
    DMA_SetDestAddr(DMA_UNIT, DMA_RX_CH, (uint32_t)&u8PrintBuffer[u8NextIndex][0]);
    DMA_SetTransCount(DMA_UNIT, DMA_RX_CH, TPH_COLUMN_BYTES);
    DMA_ChCmd(DMA_UNIT, DMA_RX_CH, ENABLE);

    __DSB(); // Arm Errata 838869
}

/**
 * @brief  配置 SPI1 从机以及 DMA1 自动接收
 */
static void SPI_Slave_DMA_Config(void) {
    stc_spi_init_t stcSpiInit;
    stc_dma_init_t stcDmaInit;
    stc_irq_signin_config_t stcIrqSignConfig;
    stc_gpio_init_t stcGpioInit;

    // 1. 引脚模式配置
    (void)GPIO_StructInit(&stcGpioInit);
    stcGpioInit.u16PinDrv = PIN_HIGH_DRV;
    (void)GPIO_Init(SPI_NSS_PORT,  SPI_NSS_PIN,  &stcGpioInit);
    (void)GPIO_Init(SPI_SCK_PORT,  SPI_SCK_PIN,  &stcGpioInit);
    (void)GPIO_Init(SPI_MOSI_PORT, SPI_MOSI_PIN, &stcGpioInit);
    (void)GPIO_Init(SPI_MISO_PORT, SPI_MISO_PIN, &stcGpioInit);

    // 2. 引脚功能映射
    GPIO_SetFunc(SPI_NSS_PORT,  SPI_NSS_PIN,  SPI_NSS_FUNC);
    GPIO_SetFunc(SPI_SCK_PORT,  SPI_SCK_PIN,  SPI_SCK_FUNC);
    GPIO_SetFunc(SPI_MOSI_PORT, SPI_MOSI_PIN, SPI_MOSI_FUNC);
    GPIO_SetFunc(SPI_MISO_PORT, SPI_MISO_PIN, SPI_MISO_FUNC);

    // 3. 配置 SPI1 从机模式
    FCG_Fcg1PeriphClockCmd(SPI_CLK, ENABLE);
    (void)SPI_StructInit(&stcSpiInit);
    stcSpiInit.u32WireMode    = SPI_4_WIRE;
    stcSpiInit.u32TransMode   = SPI_FULL_DUPLEX;
    stcSpiInit.u32MasterSlave = SPI_SLAVE;
    stcSpiInit.u32Parity      = SPI_PARITY_INVD;
    stcSpiInit.u32SpiMode     = SPI_MD_0; // 模式0: CPOL=0, CPHA=0
    stcSpiInit.u32DataBits    = SPI_DATA_SIZE_8BIT;
    stcSpiInit.u32FirstBit    = SPI_FIRST_MSB;
    stcSpiInit.u32FrameLevel  = SPI_1_FRAME;
    (void)SPI_Init(SPI_UNIT, &stcSpiInit);

    // 4. 配置 DMA1 通道 0 接收
    FCG_Fcg0PeriphClockCmd(DMA_CLK, ENABLE);
    (void)DMA_StructInit(&stcDmaInit);
    stcDmaInit.u32IntEn       = DMA_INT_ENABLE;
    stcDmaInit.u32BlockSize   = 1UL;
    stcDmaInit.u32TransCount  = TPH_COLUMN_BYTES;
    stcDmaInit.u32DataWidth   = DMA_DATAWIDTH_8BIT;
    stcDmaInit.u32SrcAddrInc  = DMA_SRC_ADDR_FIX;
    stcDmaInit.u32DestAddrInc = DMA_DEST_ADDR_INC;
    stcDmaInit.u32SrcAddr     = (uint32_t)(&SPI_UNIT->DR);
    stcDmaInit.u32DestAddr    = (uint32_t)(&u8PrintBuffer[0][0]);
    (void)DMA_Init(DMA_UNIT, DMA_RX_CH, &stcDmaInit);

    // 5. 绑定 SPI1 RX 满触发源至 DMA
    AOS_SetTriggerEventSrc(DMA_RX_TRIG_CH, SPI_RX_EVT_SRC);

    // 6. 中断注册
    stcIrqSignConfig.enIntSrc    = DMA_RX_INT_SRC;
    stcIrqSignConfig.enIRQn      = DMA_RX_IRQ_NUM;
    stcIrqSignConfig.pfnCallback = &DMA_TransCompleteCallback;
    (void)INTC_IrqSignIn(&stcIrqSignConfig);
    NVIC_ClearPendingIRQ(stcIrqSignConfig.enIRQn);
    NVIC_SetPriority(stcIrqSignConfig.enIRQn, DDL_IRQ_PRIO_02);
    NVIC_EnableIRQ(stcIrqSignConfig.enIRQn);

    // 7. 启动外设
    DMA_Cmd(DMA_UNIT, ENABLE);
    DMA_ChCmd(DMA_UNIT, DMA_RX_CH, ENABLE);
    SPI_Cmd(SPI_UNIT, ENABLE);
}
static void SystemClock_200MHz_Config(void) {
    stc_clock_pll_init_t stcPLLInit;

    // 1. 设置内部电源电压调节模式以适配 200MHz 高性能运行
    PWC_HighSpeedToHighPerformance();

    // 2. Flash 等待周期：200MHz 下需 5 个 Wait 周期
    EFM_SetWaitCycle(EFM_WAIT_CYCLE5);

    // 3. 配置各总线时钟分频寄存器 CMU->SCFGR
    // 目标配置 (SYSCLK=200MHz):
    // HCLK  = 200MHz (DIV1)
    // EXCLK = 100MHz (DIV2)
    // PCLK0 = 200MHz (DIV1)
    // PCLK1 = 100MHz (DIV2) -> USART 所在外设总线, 必须 <= 100MHz!
    // PCLK2 = 50MHz  (DIV4)
    // PCLK3 = 50MHz  (DIV4)
    // PCLK4 = 50MHz  (DIV4)
    MODIFY_REG32(CM_CMU->SCFGR,
                 (CMU_SCFGR_HCLKS | CMU_SCFGR_EXCKS | CMU_SCFGR_PCLK0S |
                  CMU_SCFGR_PCLK1S | CMU_SCFGR_PCLK2S | CMU_SCFGR_PCLK3S | CMU_SCFGR_PCLK4S),
                 (CLK_HCLK_DIV1  | CLK_EXCLK_DIV2 | CLK_PCLK0_DIV1 |
                  CLK_PCLK1_DIV2 | CLK_PCLK2_DIV4 | CLK_PCLK3_DIV4 | CLK_PCLK4_DIV4));

    // 4. 开启内部 16MHz 高速时钟 (HRC)
    (void)CLK_HrcCmd(ENABLE);

    // 5. PLL 配置: 16MHz(HRC) / 1(M) * 25(N) / 2(P) = 200MHz
    (void)CLK_PLLStructInit(&stcPLLInit);
    stcPLLInit.u8PLLState = CLK_PLL_ON;
    stcPLLInit.PLLCFGR_f.PLLSRC = CLK_PLL_SRC_HRC;
    stcPLLInit.PLLCFGR_f.PLLM   = 0U;   // 1 分频 (0: div 1)
    stcPLLInit.PLLCFGR_f.PLLN   = 24U;  // 25 倍频 (24: mul 25)
    stcPLLInit.PLLCFGR_f.PLLP   = 1U;   // 2 分频 (1: div 2) -> 200MHz
    (void)CLK_PLLInit(&stcPLLInit);

    // 6. 等待主 PLL 硬件稳定标志 (bit 5 为 MPLLSTBF)
    while ((CM_CMU->OSCSTBSR & (1UL << 5)) == 0UL) {
    }

    // 7. 将系统主时钟切换至 PLL
    CLK_SetSysClockSrc(CLK_SYSCLK_SRC_PLL);

    // 8. 必须更新 CMSIS 全局时钟变量，保证 USART 波特率计算准确
    SystemCoreClockUpdate();
}

void HC32_SPI_Slave_Run_Loop(void);
/**
 * @brief  主程序入口
 */
int32_t main(void) {
    /* 1. 开启寄存器写保护，进行全局初始化 */
    LL_PERIPH_WE(EXAMPLE_PERIPH_WE);

    // 提升时钟至 200MHz
    SystemClock_200MHz_Config();

// 2. ★ 核心改动：使用官方标准接口将 printf 绑定到 J4(USART2) ★
    (void)LL_PrintfInit(J4_UART_UNIT, 115200UL, J4_UART_PreInit);
		// 4. 直接使用 printf 验证 J4 串口
    printf("\r\n========================================\r\n");
    printf("   HC32F460 J4 Port (PA11/PA12) Ready!  \r\n");
    printf("   Official LL_PrintfInit registered.   \r\n");
    printf("========================================\r\n");
    // 初始化喷头所有控制 IO
    TPH_Init();

    // 初始化 SPI 从机及 DMA 接收服务
    SPI_Slave_DMA_Config();

    // 喷头规范上电 (先开 VL，再开 VP)
    TPH_PowerControl(ENABLE);

    /* 2. 闭合写保护寄存器 */
    LL_PERIPH_WP(EXAMPLE_PERIPH_WP);

    // 拉高 BK_2 (PH2)，通知主控 BK7258 打火协处理器已就绪
    GPIO_SetPins(TPH_READY_OUT_PORT, TPH_READY_OUT_PIN);
     HC32_SPI_Slave_Run_Loop();
    /* 3. 打印主事件循环 */
    for (;;) {
	     //	printf("HC32 Alive Heartbeat...\r\n");
        // 检测 BK7258 发来的硬件同步触发脉冲 (PC13 / BK_1 为高)
        if (PIN_SET == GPIO_ReadInputPins(TPH_SYNC_IN_PORT, TPH_SYNC_IN_PIN)) {
            
            // 校验 DMA 是否已接收完当前列点阵
            if (SET == enColumnDataReady) {
                // 立即进入微秒级关断打火流程
                TPH_FireOneColumn(&u8PrintBuffer[u8CurrentBufferIndex][0]);

                // 切换至下一列缓冲区，并清除完成标志
                u8CurrentBufferIndex ^= 1U;
                enColumnDataReady = RESET;
            }

            // 等待硬件同步脉冲下降沿释放，防止单步内重复打火
            while (PIN_SET == GPIO_ReadInputPins(TPH_SYNC_IN_PORT, TPH_SYNC_IN_PIN)) {
                // 等待释放
            }
        }
    }
}
#include "hc32_ll.h"
#include <stdio.h>
#include <string.h>

#define TEST_BUF_LEN       (16U)
#define SPI_TEST_UNIT       (CM_SPI1)

// 握手就绪引脚 PH2 (BK_2)
#define READY_PORT          (GPIO_PORT_H)
#define READY_PIN           (GPIO_PIN_02)

void HC32_SPI_Slave_Test_Init(void) {
    stc_spi_init_t stcSpiInit;
    stc_gpio_init_t stcGpioInit;

    /* 1. 外设时钟与引脚解锁 */
    LL_PERIPH_WE(LL_PERIPH_GPIO | LL_PERIPH_FCG);
    FCG_Fcg1PeriphClockCmd(FCG1_PERIPH_SPI1, ENABLE);

    /* 2. 引脚配置: PA4(NSS), PA5(SCK), PA6(MOSI), PA7(MISO) */
    GPIO_StructInit(&stcGpioInit);
    stcGpioInit.u16PinDrv = PIN_HIGH_DRV;
    GPIO_Init(GPIO_PORT_A, GPIO_PIN_04 | GPIO_PIN_05 | GPIO_PIN_06 | GPIO_PIN_07, &stcGpioInit);

    GPIO_SetFunc(GPIO_PORT_A, GPIO_PIN_04, GPIO_FUNC_40); // NSS
    GPIO_SetFunc(GPIO_PORT_A, GPIO_PIN_05, GPIO_FUNC_41); // SCK
    GPIO_SetFunc(GPIO_PORT_A, GPIO_PIN_06, GPIO_FUNC_42); // MOSI
    GPIO_SetFunc(GPIO_PORT_A, GPIO_PIN_07, GPIO_FUNC_43); // MISO

    /* 3. 就绪引脚 PH2 (BK_2) 初始化为输出 */
    stcGpioInit.u16PinDir   = PIN_DIR_OUT;
    stcGpioInit.u16PinState = PIN_STAT_RST;
    GPIO_Init(READY_PORT, READY_PIN, &stcGpioInit);

    /* 4. SPI 从机配置: 模式0 (CPOL=0, CPHA=0), 8Bit, MSB */
    SPI_StructInit(&stcSpiInit);
    stcSpiInit.u32WireMode    = SPI_4_WIRE;
    stcSpiInit.u32TransMode   = SPI_FULL_DUPLEX;
    stcSpiInit.u32MasterSlave = SPI_SLAVE;
    stcSpiInit.u32Parity      = SPI_PARITY_INVD;
    stcSpiInit.u32SpiMode     = SPI_MD_0; // 必须与主机完全一致
    stcSpiInit.u32DataBits    = SPI_DATA_SIZE_8BIT;
    stcSpiInit.u32FirstBit    = SPI_FIRST_MSB;
    stcSpiInit.u32FrameLevel  = SPI_1_FRAME;
    SPI_Init(SPI_TEST_UNIT, &stcSpiInit);

    SPI_Cmd(SPI_TEST_UNIT, ENABLE);
    LL_PERIPH_WP(LL_PERIPH_GPIO | LL_PERIPH_FCG);
}

/**
 * @brief HC32 从机回环测试主循环 (放入 main() 中测试)
 */
void HC32_SPI_Slave_Run_Loop(void) {
    uint8_t rx_buf[TEST_BUF_LEN];
    uint8_t tx_buf[TEST_BUF_LEN];

    HC32_SPI_Slave_Test_Init();

    while (1) {
        memset(rx_buf, 0, sizeof(rx_buf));
        memset(tx_buf, 0, sizeof(tx_buf));
  	 
        // 步骤 1: 拉低 BK_2，准备接收主机发来的数据
        GPIO_ResetPins(READY_PORT, READY_PIN);

        // 步骤 2: 等待接收主机的 16 字节测试包 (超时时间设大一些)
        if (LL_OK == SPI_Receive(SPI_TEST_UNIT, rx_buf, TEST_BUF_LEN, 0xFFFFFFUL)) {
               printf("HC32 SPI_Receive OK...\r\n");
            // 步骤 3: 接收成功，将数据按位取反，作为应答包
            for (int i = 0; i < TEST_BUF_LEN; i++) {
                tx_buf[i] = ~rx_buf[i];
            }

            // 步骤 4: 拉高 BK_2，通知主机：“应答数据已备好，请发起下一次读取传输”
            GPIO_SetPins(READY_PORT, READY_PIN);

            // 步骤 5: 将应答数据送入发送缓冲区，等待主机时钟将其读走
            SPI_Trans(SPI_TEST_UNIT, tx_buf, TEST_BUF_LEN, 0xFFFFFFUL);

            // 测试完成一次，拉低就绪标志
            GPIO_ResetPins(READY_PORT, READY_PIN);
        }
    }
}