#include "tph_driver.h"

// ----------------------------------------------------------------------------
// M0333N 规格书 3.2 节 Table 地址映射编码 (Add1 ~ Add25)
// ----------------------------------------------------------------------------
static const uint8_t TPH_ADDR_CODE[25] = {
    0x07, 0x0B, 0x13, 0x0D, 0x15, 0x19, 0x0E, 0x16, 0x26, 0x1A,
    0x2A, 0x32, 0x1C, 0x2C, 0x4C, 0x34, 0x54, 0x64, 0x38, 0x58,
    0x68, 0x70, 0x23, 0x43, 0x25
};

// ----------------------------------------------------------------------------
// 微秒及纳秒级延时函数 (系统主频 200MHz: 1周期 = 5ns)
// ----------------------------------------------------------------------------
// 采样建立时间 ts >= 0.38us (低电平持续时间)
static inline void TPH_DelayTs(void) {
    for (volatile uint32_t i = 0UL; i < 20UL; i++) {
        __NOP();
    }
}

// 严苛打火发热时间 tFire: 2.0us ~ 2.2us
static inline void TPH_DelayTfire(void) {
    for (volatile uint32_t i = 0UL; i < 92UL; i++) {
        __NOP();
    }
}

// 通用微秒延时 (用于上下电时序)
static void TPH_DelayUS(uint32_t us) {
    for (volatile uint32_t i = 0UL; i < us * 40UL; i++) {
        __NOP();
    }
}

// ----------------------------------------------------------------------------
// 极速设置 A1 ~ A7 物理行地址 (全在 Port B)
// A1:PB2, A2:PB10, A3:PB1, A4:PB12, A5:PB14, A6:PB13, A7:PB0
// ----------------------------------------------------------------------------
static inline void TPH_SetAddressFast(uint8_t u8AddrCode) {
    uint16_t u16SetMask = 0U;
    uint16_t u16ClrMask = 0U;

    if (u8AddrCode & 0x01U) u16SetMask |= GPIO_PIN_02; else u16ClrMask |= GPIO_PIN_02; // A1
    if (u8AddrCode & 0x02U) u16SetMask |= GPIO_PIN_10; else u16ClrMask |= GPIO_PIN_10; // A2
    if (u8AddrCode & 0x04U) u16SetMask |= GPIO_PIN_01; else u16ClrMask |= GPIO_PIN_01; // A3
    if (u8AddrCode & 0x08U) u16SetMask |= GPIO_PIN_12; else u16ClrMask |= GPIO_PIN_12; // A4
    if (u8AddrCode & 0x10U) u16SetMask |= GPIO_PIN_14; else u16ClrMask |= GPIO_PIN_14; // A5
    if (u8AddrCode & 0x20U) u16SetMask |= GPIO_PIN_13; else u16ClrMask |= GPIO_PIN_13; // A6
    if (u8AddrCode & 0x40U) u16SetMask |= GPIO_PIN_00; else u16ClrMask |= GPIO_PIN_00; // A7

    GPIO_ResetPins(GPIO_PORT_B, u16ClrMask);
    GPIO_SetPins(GPIO_PORT_B, u16SetMask);
}

// ----------------------------------------------------------------------------
// 极速输出 D1 ~ D6 数据 (全在 Port A)
// D1:PA0, D2:PA2, D3:PA1, D4:PA10, D5:PA8, D6:PA3
// ----------------------------------------------------------------------------
static inline void TPH_SetDataFast(uint8_t u8Data6Bit) {
    uint16_t u16SetMask = 0U;
    uint16_t u16ClrMask = 0U;

    if (u8Data6Bit & 0x01U) u16SetMask |= GPIO_PIN_00; else u16ClrMask |= GPIO_PIN_00; // D1
    if (u8Data6Bit & 0x02U) u16SetMask |= GPIO_PIN_02; else u16ClrMask |= GPIO_PIN_02; // D2
    if (u8Data6Bit & 0x04U) u16SetMask |= GPIO_PIN_01; else u16ClrMask |= GPIO_PIN_01; // D3
    if (u8Data6Bit & 0x08U) u16SetMask |= GPIO_PIN_10; else u16ClrMask |= GPIO_PIN_10; // D4
    if (u8Data6Bit & 0x10U) u16SetMask |= GPIO_PIN_08; else u16ClrMask |= GPIO_PIN_08; // D5
    if (u8Data6Bit & 0x20U) u16SetMask |= GPIO_PIN_03; else u16ClrMask |= GPIO_PIN_03; // D6

    GPIO_ResetPins(GPIO_PORT_A, u16ClrMask);
    GPIO_SetPins(GPIO_PORT_A, u16SetMask);
}

// ----------------------------------------------------------------------------
// 喷头引脚初始化配置
// ----------------------------------------------------------------------------
void TPH_Init(void) {
    stc_gpio_init_t stcGpioInit;

    (void)GPIO_StructInit(&stcGpioInit);
    stcGpioInit.u16PinDir        = PIN_DIR_OUT;
    stcGpioInit.u16PinDrv        = PIN_HIGH_DRV; // 极速陡峭边沿，减小时钟抖动
    stcGpioInit.u16PinOutputType = PIN_OUT_TYPE_CMOS;
    stcGpioInit.u16PinState      = PIN_STAT_RST;

    // 1. 初始化电源使能及 CE 打火脚
    (void)GPIO_Init(TPH_PWR_VP_PORT, TPH_PWR_VP_PIN, &stcGpioInit);
    (void)GPIO_Init(TPH_PWR_VL_PORT, TPH_PWR_VL_PIN, &stcGpioInit);
    (void)GPIO_Init(TPH_CE_PORT,     TPH_CE_PIN,     &stcGpioInit);

    // 2. 初始化 A1~A7 地址引脚 (Port B)
    (void)GPIO_Init(GPIO_PORT_B, GPIO_PIN_00 | GPIO_PIN_01 | GPIO_PIN_02 | GPIO_PIN_10 |
                                 GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14, &stcGpioInit);

    // 3. 初始化 D1~D6 数据引脚 (Port A)
    (void)GPIO_Init(GPIO_PORT_A, GPIO_PIN_00 | GPIO_PIN_01 | GPIO_PIN_02 | GPIO_PIN_03 |
                                 GPIO_PIN_08 | GPIO_PIN_10, &stcGpioInit);

    // 4. 初始化 S1~S4 分段锁存引脚 (默认必须保持 HIGH 锁存态！)
    stcGpioInit.u16PinState = PIN_STAT_SET;
    (void)GPIO_Init(TPH_S1_PORT, TPH_S1_PIN, &stcGpioInit);
    (void)GPIO_Init(TPH_S2_PORT, TPH_S2_PIN, &stcGpioInit);
    (void)GPIO_Init(TPH_S3_PORT, TPH_S3_PIN, &stcGpioInit);
    (void)GPIO_Init(TPH_S4_PORT, TPH_S4_PIN, &stcGpioInit);

    // 5. 与 BK7258 的同步信号线
    // BK_1 (PC13): 输入，带上拉
    stcGpioInit.u16PinDir   = PIN_DIR_IN;
    stcGpioInit.u16PullUp   = PIN_PU_ON;
    (void)GPIO_Init(TPH_SYNC_IN_PORT, TPH_SYNC_IN_PIN, &stcGpioInit);

    // BK_2 (PH2): 输出，默认低电平
    stcGpioInit.u16PinDir   = PIN_DIR_OUT;
    stcGpioInit.u16PinState = PIN_STAT_RST;
    (void)GPIO_Init(TPH_READY_OUT_PORT, TPH_READY_OUT_PIN, &stcGpioInit);
}

// ----------------------------------------------------------------------------
// 严苛电源上下电顺序保护 (符合规格书 3.3 时序)
// ----------------------------------------------------------------------------
void TPH_PowerControl(en_functional_state_t enState) {
    if (ENABLE == enState) {
        // 上电顺序：先开启 VL(逻辑15V) -> 等待 tpo >= 5us -> 再开启 VP(加热15V)
        GPIO_SetPins(TPH_PWR_VL_PORT, TPH_PWR_VL_PIN);
        TPH_DelayUS(10U); // tpo >= 5us
        GPIO_SetPins(TPH_PWR_VP_PORT, TPH_PWR_VP_PIN);
        TPH_DelayUS(50U); // 稳压延时
    } else {
        // 下电顺序：CE 打火归零 -> 关断 VP -> 等待 tpf >= 5us -> 关断 VL
        GPIO_ResetPins(TPH_CE_PORT,     TPH_CE_PIN);
        GPIO_ResetPins(TPH_PWR_VP_PORT, TPH_PWR_VP_PIN);
        TPH_DelayUS(10U); // tpf >= 5us
        GPIO_ResetPins(TPH_PWR_VL_PORT, TPH_PWR_VL_PIN);
    }
}

// ----------------------------------------------------------------------------
// 核心打火函数：发射一列点阵 (25 Addr x 4 Section = 100 字节循环)
// ----------------------------------------------------------------------------
void TPH_FireOneColumnx(const uint8_t *pu8Data) {
    uint32_t u32Offset = 0UL;

    for (uint8_t add_idx = 0U; add_idx < TPH_ADDR_NUM; add_idx++) {
        // 1. 设置物理行地址 A1~A7
        TPH_SetAddressFast(TPH_ADDR_CODE[add_idx]);
        TPH_DelayTs();

        // 2. 依次加载锁存 S1 ~ S4 数据 (低电平采样，高电平锁存)
        // S1
        TPH_SetDataFast(pu8Data[u32Offset++]);
        GPIO_ResetPins(TPH_S1_PORT, TPH_S1_PIN); // S1 = 0 (采样)
        TPH_DelayTs();
        GPIO_SetPins(TPH_S1_PORT, TPH_S1_PIN);   // S1 = 1 (锁存)

        // S2
        TPH_SetDataFast(pu8Data[u32Offset++]);
        GPIO_ResetPins(TPH_S2_PORT, TPH_S2_PIN); // S2 = 0
        TPH_DelayTs();
        GPIO_SetPins(TPH_S2_PORT, TPH_S2_PIN);   // S2 = 1

        // S3
        TPH_SetDataFast(pu8Data[u32Offset++]);
        GPIO_ResetPins(TPH_S3_PORT, TPH_S3_PIN); // S3 = 0
        TPH_DelayTs();
        GPIO_SetPins(TPH_S3_PORT, TPH_S3_PIN);   // S3 = 1

        // S4
        TPH_SetDataFast(pu8Data[u32Offset++]);
        GPIO_ResetPins(TPH_S4_PORT, TPH_S4_PIN); // S4 = 0
        TPH_DelayTs();
        GPIO_SetPins(TPH_S4_PORT, TPH_S4_PIN);   // S4 = 1

        // 3. 核心打火使能脉冲 (发热时间严控在 2.0us ~ 2.2us)
        GPIO_SetPins(TPH_CE_PORT, TPH_CE_PIN);   // CE = 1
        TPH_DelayTfire();                        // 精确维持 2.0us
        GPIO_ResetPins(TPH_CE_PORT, TPH_CE_PIN); // CE = 0 (立即关断)

        // 墨水回填等待
        TPH_DelayTs();
    }
}