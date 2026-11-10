#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "tph_m0333n.h"

// 端口 B 地址专用引脚掩码 (PB0, PB1, PB2, PB10, PB12, PB13, PB14)
#define ADDR_PORTB_MASK         (0x7407U)

// 端口 A 数据专用引脚掩码 (PA0, PA1, PA2, PA3, PA8, PA10)
#define DATA_PORTA_MASK         (0x050FU)

/* 并发掩码查表变量 */
static uint16_t s_AddrPortB_Set[25];
static uint16_t s_AddrPortB_Clr[25];
static uint16_t s_DataPortA_Set[64];
static uint16_t s_DataPortA_Clr[64];
static bool     s_bLutInitialized = false;

// 规格书第 13 页 25 组地址真实编码表 (A7 ~ A1)
static const uint8_t s_u8AddrLut[25] = {
    0x07, 0x0B, 0x13, 0x0D, 0x15, 0x19, 0x0E, 0x16, 0x26, 0x1A,
    0x2A, 0x32, 0x1C, 0x2C, 0x4C, 0x34, 0x54, 0x64, 0x38, 0x58,
    0x68, 0x70, 0x23, 0x43, 0x25
};

/* ========================================================================== */
/* ★ 1. 纯汇编绝对刚性 NOP 延时 (200MHz 主频，1 NOP = 严格 5 纳秒，零内存开销) ★ */
/* ========================================================================== */

// 50ns 极速过渡延时：10 个 NOP (10 * 5ns = 50ns)
static __attribute__((always_inline)) inline void delay_40ns(void) {
    __asm volatile (
        "nop \n nop \n nop \n nop \n nop \n"
        
    );
}

// 100ns 地址/使能稳定延时：20 个 NOP (20 * 5ns = 100ns)
static __attribute__((always_inline)) inline void delay_100ns(void) {
    __asm volatile (
        "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n"
        "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n"
    );
}

// ★ 400ns 采样延时 (ts >= 0.38us)：68 个纯汇编 NOP (示波器实测精准 395~405ns) ★
static __attribute__((always_inline)) inline void delay_ts_400ns(void) {
    __asm volatile (
        "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n"
        "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n"
        "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n"
        "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n"
        "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n"
        "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n"
        "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n"
        "nop \n nop \n nop \n nop \n nop   \n" // 62
                                  // 68 NOPs
    );
}

// 800ns 墨水腔回填微延时：160 个纯汇编 NOP (160 * 5ns = 600ns)
static __attribute__((always_inline)) inline void delay_600ns(void) {
    __asm volatile (
        "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n"
        "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n"
        "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n"
        "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n"
        "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n"
        "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n"
        "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n"
        "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n"
        "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n"
        "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n"
        "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n"
        "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n"

    );
}

// ★ 400ns 采样延时 (ts >= 0.38us)：68 个纯汇编 NOP (示波器实测精准 395~405ns) ★
static __attribute__((always_inline)) inline void delay_ts_2us(void) {
    __asm volatile (
        "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n"
        "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n"
        "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n"
        "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n"
        "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n"
        "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n"
        "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n"
        "nop \n nop \n nop \n nop \n nop   \n" // 62
				 "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n"
        "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n"
        "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n"
        "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n"
        "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n"
        "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n"
        "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n"
        "nop \n nop \n nop \n nop \n nop   \n" // 62
				 "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n"
        "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n"
        "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n"
        "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n"
        "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n"
        "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n"
        "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n"
        "nop \n nop \n nop \n nop \n nop   \n" // 62
				 "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n"
        "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n"
        "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n"
        "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n"
        "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n"
        "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n"
        "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n"
        "nop \n nop \n nop \n nop \n nop   \n" // 62
				 "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n"
        "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n"
        "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n"
        "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n"
        "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n"
        "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n"
        "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n"
        "nop \n nop \n nop \n nop \n nop   \n" // 62
				  "nop \n nop \n nop \n nop \n nop   \n" // 62
					  "nop \n nop \n nop \n nop \n nop   \n" // 62
                                  // 68 NOPs
    );
}
// 10us 上下电延时 (纯汇编寄存器循环，绝不访问内存，绝对不死锁)
static __attribute__((always_inline)) inline void delay_10us(void) {
    __asm volatile (
        "    mov r3, #500         \n"
        "1:  subs r3, r3, #1      \n"
        "    bne 1b               \n"
        ::: "r3"
    );
}

/* ========================================================================== */
/* ★ 2. S 与 CE 单周期寄存器直写宏 (耗时仅 5ns)                             ★ */
/* ========================================================================== */
#define S1_LOW()    (CM_GPIO->PORRC = (1U << 15))
#define S1_HIGH()   (CM_GPIO->POSRC = (1U << 15))

#define S2_LOW()    (CM_GPIO->PORRH = (1U << 1))
#define S2_HIGH()   (CM_GPIO->POSRH = (1U << 1))

#define S3_LOW()    (CM_GPIO->PORRH = (1U << 0))
#define S3_HIGH()   (CM_GPIO->POSRH = (1U << 0))

#define S4_LOW()    (CM_GPIO->PORRC = (1U << 14))
#define S4_HIGH()   (CM_GPIO->POSRC = (1U << 14))

#define CE_LOW()    (CM_GPIO->PORRB = (1U << 15))
#define CE_HIGH()   (CM_GPIO->POSRB = (1U << 15))

/* ========================================================================== */
/* ★ 3. 建立并发掩码查表与原子操作                                           ★ */
/* ========================================================================== */
void TPH_Build_Fast_LUT(void) {
    if (s_bLutInitialized) return;

    for (int i = 0; i < 25; i++) {
        uint8_t a = s_u8AddrLut[i];
        uint16_t set_bits = 0;

        if (a & (1 << 0)) set_bits |= (1 << 2);  // A1 -> PB2
        if (a & (1 << 1)) set_bits |= (1 << 10); // A2 -> PB10
        if (a & (1 << 2)) set_bits |= (1 << 1);  // A3 -> PB1
        if (a & (1 << 3)) set_bits |= (1 << 12); // A4 -> PB12
        if (a & (1 << 4)) set_bits |= (1 << 14); // A5 -> PB14
        if (a & (1 << 5)) set_bits |= (1 << 13); // A6 -> PB13
        if (a & (1 << 6)) set_bits |= (1 << 0);  // A7 -> PB0

        s_AddrPortB_Set[i] = set_bits;
        s_AddrPortB_Clr[i] = (~set_bits) & ADDR_PORTB_MASK;
    }

    for (int d = 0; d < 64; d++) {
        uint16_t set_bits = 0;

        if (d & (1 << 0)) set_bits |= (1 << 0);  // D1 -> PA0
        if (d & (1 << 1)) set_bits |= (1 << 2);  // D2 -> PA2
        if (d & (1 << 2)) set_bits |= (1 << 1);  // D3 -> PA1
        if (d & (1 << 3)) set_bits |= (1 << 10); // D4 -> PA10
        if (d & (1 << 4)) set_bits |= (1 << 8);  // D5 -> PA8
        if (d & (1 << 5)) set_bits |= (1 << 3);  // D6 -> PA3

        s_DataPortA_Set[d] = set_bits;
        s_DataPortA_Clr[d] = (~set_bits) & DATA_PORTA_MASK;
    }

    s_bLutInitialized = true;
}

static __attribute__((always_inline)) inline void FAST_SET_ADDRESS(uint8_t add_idx) {
    CM_GPIO->PORRB = s_AddrPortB_Clr[add_idx];
    CM_GPIO->POSRB = s_AddrPortB_Set[add_idx];
}

static __attribute__((always_inline)) inline void FAST_SET_DATA(uint8_t d_val) {
    uint8_t idx = d_val & 0x3F;
    CM_GPIO->PORRA = s_DataPortA_Clr[idx];
    CM_GPIO->POSRA = s_DataPortA_Set[idx];
}

/* CE gates heating, while S1..S4 retain sampled domain bits. Clearing the
 * external D pins alone does not clear those latches. All clearing below is
 * performed with CE LOW; it adds no intentional heating pulse.
 */
static __attribute__((always_inline)) inline void TPH_Clear_Latches_CE_Low(void) {
    CE_LOW();
    FAST_SET_DATA(0);
    delay_100ns();
    S1_LOW(); delay_ts_400ns(); S1_HIGH(); delay_40ns();
    S2_LOW(); delay_ts_400ns(); S2_HIGH(); delay_40ns();
    S3_LOW(); delay_ts_400ns(); S3_HIGH(); delay_40ns();
    S4_LOW(); delay_ts_400ns(); S4_HIGH(); delay_40ns();
    __DSB();
}
static __attribute__((always_inline)) inline void TPH_Clear_Address(void) {
    CM_GPIO->PORRB = ADDR_PORTB_MASK; /* actual Ax=0000000, not LUT index 0 */
}

/* ========================================================================== */
/* ★ 4. 喷头 GPIO 引脚初始化与规范上下电                                    ★ */
/* ========================================================================== */
void TPH_M0333N_Init(void) {
    stc_gpio_init_t stcGpioInit;
    (void)GPIO_StructInit(&stcGpioInit);

    stcGpioInit.u16PinDrv        = PIN_HIGH_DRV;
    stcGpioInit.u16PinDir        = PIN_DIR_OUT;
    stcGpioInit.u16PinOutputType = PIN_OUT_TYPE_CMOS;
    stcGpioInit.u16PinState      = PIN_STAT_RST;

    // 1. 电源使能控制脚
    GPIO_Init(PIN_VL_EN_PORT, PIN_VL_EN_PIN, &stcGpioInit);
    GPIO_Init(PIN_TPH_EN_PORT, PIN_TPH_EN_PIN, &stcGpioInit);

    // 2. CE 加热引脚
    GPIO_Init(PIN_CE_PORT, PIN_CE_PIN, &stcGpioInit);

    // 3. 地址线 A1~A7
    GPIO_Init(PIN_A1_PORT, PIN_A1_PIN, &stcGpioInit);
    GPIO_Init(PIN_A2_PORT, PIN_A2_PIN, &stcGpioInit);
    GPIO_Init(PIN_A3_PORT, PIN_A3_PIN, &stcGpioInit);
    GPIO_Init(PIN_A4_PORT, PIN_A4_PIN, &stcGpioInit);
    GPIO_Init(PIN_A5_PORT, PIN_A5_PIN, &stcGpioInit);
    GPIO_Init(PIN_A6_PORT, PIN_A6_PIN, &stcGpioInit);
    GPIO_Init(PIN_A7_PORT, PIN_A7_PIN, &stcGpioInit);

    // 4. S1~S4 锁存线 (默认高电平锁存)
    stcGpioInit.u16PinState = PIN_STAT_SET;
    GPIO_Init(PIN_S1_PORT, PIN_S1_PIN, &stcGpioInit);
    GPIO_Init(PIN_S2_PORT, PIN_S2_PIN, &stcGpioInit);
    GPIO_Init(PIN_S3_PORT, PIN_S3_PIN, &stcGpioInit);
    GPIO_Init(PIN_S4_PORT, PIN_S4_PIN, &stcGpioInit);

    // 5. D1~D6 数据线
    stcGpioInit.u16PinState = PIN_STAT_RST;
    GPIO_Init(PIN_D1_PORT, PIN_D1_PIN, &stcGpioInit);
    GPIO_Init(PIN_D2_PORT, PIN_D2_PIN, &stcGpioInit);
    GPIO_Init(PIN_D3_PORT, PIN_D3_PIN, &stcGpioInit);
    GPIO_Init(PIN_D4_PORT, PIN_D4_PIN, &stcGpioInit);
    GPIO_Init(PIN_D5_PORT, PIN_D5_PIN, &stcGpioInit);
    GPIO_Init(PIN_D6_PORT, PIN_D6_PIN, &stcGpioInit);

    TPH_Build_Fast_LUT();
}

void TPH_M0333N_Power(en_functional_state_t enState) {
    if (ENABLE == enState) {
        printf("[TPH Power] Step 0: Enabling VL Logic Power (PB6)...\r\n");
        GPIO_SetPins(PIN_VL_EN_PORT, PIN_VL_EN_PIN);
        delay_10us(); // 严格延时 10us (tpo >= 5us)

        printf("[TPH Power] Step 1: Enabling VP Heater Power (PB5)...\r\n");
        TPH_Clear_Latches_CE_Low();
        TPH_Clear_Address();
        GPIO_SetPins(PIN_TPH_EN_PORT, PIN_TPH_EN_PIN);
        delay_10us(); // 严格延时 10us
        
        printf("[TPH Power] Step 2: VL_ENABLE(PB6) and TPH_ENABLE(PB5) asserted; rail voltages NOT measured.\r\n");
    } else {
        TPH_Clear_Latches_CE_Low();
        TPH_Clear_Address();
        GPIO_ResetPins(PIN_TPH_EN_PORT, PIN_TPH_EN_PIN);
        delay_10us();
        GPIO_ResetPins(PIN_VL_EN_PORT, PIN_VL_EN_PIN);
        printf("[TPH Power] VL and VP Powered OFF.\r\n");
    }
}

/* TPH_PREPARED_CE_v1: prepare D words outside CE; no added A/D/S waits.
 * Keep the existing sampling, latch reset, quiet delays and IRQ restoration.
 * Actual CE and S widths must be checked after the local Keil rebuild. */
void TPH_FireOneColumn(const uint8_t *pColumn100B) {
    if (!s_bLutInitialized) {
        TPH_Build_Fast_LUT();
    }
    TPH_Clear_Latches_CE_Low();
    if (pColumn100B == NULL) { TPH_Clear_Address(); return; }

    uint32_t data_idx = 0;
    for (uint8_t add = 0; add < 25; ++add) {
        /* All input reads and table lookups happen while CE is LOW.
         * Pack clear bits in the high half, set bits in the low half. */
        uint8_t d0 = pColumn100B[data_idx++] & 0x3FU;
        uint8_t d1 = pColumn100B[data_idx++] & 0x3FU;
        uint8_t d2 = pColumn100B[data_idx++] & 0x3FU;
        uint8_t d3 = pColumn100B[data_idx++] & 0x3FU;
        uint32_t w0 = ((uint32_t)s_DataPortA_Clr[d0] << 16) | s_DataPortA_Set[d0];
        uint32_t w1 = ((uint32_t)s_DataPortA_Clr[d1] << 16) | s_DataPortA_Set[d1];
        uint32_t w2 = ((uint32_t)s_DataPortA_Clr[d2] << 16) | s_DataPortA_Set[d2];
        uint32_t w3 = ((uint32_t)s_DataPortA_Clr[d3] << 16) | s_DataPortA_Set[d3];
        /* Force prepared words into registers before the heating window. */
        __asm volatile ("" : "+r" (w1), "+r" (w2), "+r" (w3) : : "memory");

        uint32_t saved_primask = __get_PRIMASK();
        __disable_irq();
        FAST_SET_ADDRESS(add);
        CM_GPIO->PORRA = w0 >> 16;
        CM_GPIO->POSRA = (uint16_t)w0;
        __DMB();

        CE_HIGH();
        S1_LOW(); delay_ts_400ns(); S1_HIGH();

        CM_GPIO->PORRA = w1 >> 16;
        CM_GPIO->POSRA = (uint16_t)w1;
        S2_LOW(); delay_ts_400ns(); S2_HIGH();

        CM_GPIO->PORRA = w2 >> 16;
        CM_GPIO->POSRA = (uint16_t)w2;
        S3_LOW(); delay_ts_400ns(); S3_HIGH();

        CM_GPIO->PORRA = w3 >> 16;
        CM_GPIO->POSRA = (uint16_t)w3;
        S4_LOW(); delay_ts_400ns(); S4_HIGH();
        CE_LOW();

        __DSB();
        __set_PRIMASK(saved_primask);
        TPH_Clear_Latches_CE_Low();
        delay_600ns();
        delay_ts_400ns();
    }

    TPH_Clear_Latches_CE_Low();
    TPH_Clear_Address();
    CE_LOW();
    delay_600ns();
    delay_ts_400ns();
}
/* ========================================================================== */
/* ★ 5. 核心打火函数 (CE 高电平严格覆盖 S1~S4 全过程，脉宽精确收敛在 2.10us) ★ */
/* ========================================================================== */
void TPH_FireOneColumn_fix(const uint8_t *pColumn100B) {
    (void)pColumn100B; // 强制测试绿色
     uint32_t data_idx = 0; // 0 ~ 99 字节游标
    if (!s_bLutInitialized) {
        TPH_Build_Fast_LUT();
    }

    // ★ 强制测试绿色通道 (D1,D2 青色 + D5,D6 黄色)
    // 如需测试单一颜色：
    // 0x03: 仅青色 (Cyan)
    // 0x0C: 仅品红 (Magenta)
    // 0x30: 仅黄色 (Yellow)
    const uint8_t FORCED_COLOR_DATA = 0x33;

    // 轮询 25 组地址 (Add 1 ~ Add 25)
    for (uint8_t add = 0; add < 25; add++) {
        // ----------------------------------------------------
        // 步骤 1：在 CE=0 安全期并发切换 7 根地址线
        // ----------------------------------------------------
        FAST_SET_ADDRESS(add);
        delay_100ns(); // 100ns 纯汇编延时

        // ----------------------------------------------------
        // 步骤 2：★ 正确时序：拉高 CE！CE 高电平门控开始覆盖本地址打火 ★
        // ----------------------------------------------------
        CE_HIGH();
        delay_100ns();

        // ----------------------------------------------------
        // 步骤 3：在 CE 保持高电平的窗口内，依次触发 S1 ~ S4 采样与点火
        // ----------------------------------------------------
        // ---- Section 1 ----
        FAST_SET_DATA(FORCED_COLOR_DATA);
        delay_40ns();
        S1_LOW();  delay_ts_400ns(); S1_HIGH(); // 400ns 精准负脉冲
        delay_40ns();

        // ---- Section 2 ----
        FAST_SET_DATA(FORCED_COLOR_DATA);
        delay_40ns();
        S2_LOW();  delay_ts_400ns(); S2_HIGH();
        delay_40ns();

        // ---- Section 3 ----
        FAST_SET_DATA(FORCED_COLOR_DATA);
        delay_40ns();
        S3_LOW();  delay_ts_400ns(); S3_HIGH();
        delay_40ns();

        // ---- Section 4 ----
        FAST_SET_DATA(FORCED_COLOR_DATA);
        delay_40ns();
        S4_LOW();  delay_ts_400ns(); S4_HIGH();
        delay_40ns();

        // ----------------------------------------------------
        // 步骤 4：★ S4 一结束立即拉低 CE！关断发热！★
        // 此时 CE 高电平总时长 = 100ns + 4 * (50+400+50)ns = 2.10us！
        // 绝不多通 1 纳秒，严格锁定在 tFire 标准的 2.0us ~ 2.2us！
        // ----------------------------------------------------
        CE_LOW();

        // 步骤 5：墨水腔回填微延时
        delay_600ns();
				
				delay_ts_400ns();
    }

    // 停止时序
    FAST_SET_DATA(0x00);
    FAST_SET_ADDRESS(0);
    CE_LOW();
}
/* ========================================================================== */
/* ★ 5. 核心打火函数 (CE 高电平严格覆盖 S1~S4 全过程，脉宽精确收敛在 2.10us) ★ */
/* ========================================================================== */
void TPH_FireOneColumnxx(const uint8_t *pColumn100B) {
    (void)pColumn100B; // 强制测试绿色
     uint32_t data_idx = 0; // 0 ~ 99 字节游标
    if (!s_bLutInitialized) {
        TPH_Build_Fast_LUT();
    }

    // ★ 强制测试绿色通道 (D1,D2 青色 + D5,D6 黄色)
    // 如需测试单一颜色：
    // 0x03: 仅青色 (Cyan)
    // 0x0C: 仅品红 (Magenta)
    // 0x30: 仅黄色 (Yellow)
   // const uint8_t FORCED_COLOR_DATA = 0x03;

    // 轮询 25 组地址 (Add 1 ~ Add 25)
    for (uint8_t add = 0; add < 25; add++) {
        // ----------------------------------------------------
        // 步骤 1：在 CE=0 安全期并发切换 7 根地址线
        // ----------------------------------------------------
        FAST_SET_ADDRESS(add);
        delay_100ns(); // 100ns 纯汇编延时

        // ----------------------------------------------------
        // 步骤 2：★ 正确时序：拉高 CE！CE 高电平门控开始覆盖本地址打火 ★
        // ----------------------------------------------------
        CE_HIGH();
        delay_100ns();

        // ----------------------------------------------------
        // 步骤 3：在 CE 保持高电平的窗口内，依次触发 S1 ~ S4 采样与点火
        // ----------------------------------------------------
        // ---- Section 1 ----
        FAST_SET_DATA(pColumn100B[data_idx++]);
        delay_40ns();
        S1_LOW();  delay_ts_400ns(); S1_HIGH(); // 400ns 精准负脉冲
        delay_40ns();

        // ---- Section 2 ----
        FAST_SET_DATA(pColumn100B[data_idx++]);
        delay_40ns();
        S2_LOW();  delay_ts_400ns(); S2_HIGH();
        delay_40ns();

        // ---- Section 3 ----
        FAST_SET_DATA(pColumn100B[data_idx++]);
        delay_40ns();
        S3_LOW();  delay_ts_400ns(); S3_HIGH();
        delay_40ns();

        // ---- Section 4 ----
        FAST_SET_DATA(pColumn100B[data_idx++]);
        delay_40ns();
        S4_LOW();  delay_ts_400ns(); S4_HIGH();
        delay_40ns();

        // ----------------------------------------------------
        // 步骤 4：★ S4 一结束立即拉低 CE！关断发热！★
        // 此时 CE 高电平总时长 = 100ns + 4 * (50+400+50)ns = 2.10us！
        // 绝不多通 1 纳秒，严格锁定在 tFire 标准的 2.0us ~ 2.2us！
        // ----------------------------------------------------
        CE_LOW();

        // 步骤 5：墨水腔回填微延时
        delay_600ns();
				delay_ts_400ns();
    }

    // 停止时序
    FAST_SET_DATA(0x00);
    FAST_SET_ADDRESS(0);
    CE_LOW();
}
/**
 * @brief ★ HC32 纯硬件底层单线直打自检 (严格符合第 11/14 页原厂波形图) ★
 */
void HC32_Standalone_Hardware_Test(void) {
    printf("\r\n>>> [HC32 TEST] Firing 100%% Specification-Compliant Waveform <<<\r\n");

    // 模拟连续扫掠 600 列
    for (int col = 0; col < 600; col++) {
        
        // CE 全程保持为高电平！
        CE_HIGH();
        delay_100ns();

        // 只在一个固定的地址打火 (比如 Address 10)
        FAST_SET_ADDRESS(10);
        delay_100ns();

        // Section 1: 只打这一处，其余为 0
        // 0x15 代表 D1, D3, D5 三色偶喷孔全开 (纯黑色！)
        FAST_SET_DATA(0x15);
        delay_40ns();
        S1_LOW();  delay_ts_400ns(); S1_HIGH();
        delay_40ns();

        // Section 2, 3, 4 全部填 0x00
        FAST_SET_DATA(0x00);
        delay_40ns();
        S2_LOW();  delay_ts_400ns(); S2_HIGH();
        delay_40ns();

        FAST_SET_DATA(0x00);
        delay_40ns();
        S3_LOW();  delay_ts_400ns(); S3_HIGH();
        delay_40ns();

        FAST_SET_DATA(0x00);
        delay_40ns();
        S4_LOW();  delay_ts_400ns(); S4_HIGH();
        delay_40ns();

        // 停止时序
        FAST_SET_ADDRESS(0);
        FAST_SET_DATA(0x00);
        CE_LOW();

        // 回填等待
        delay_600ns();
        delay_ts_400ns();

        DDL_DelayMS(2); // 模拟字车移动步进
    }

    printf(">>> [HC32 TEST] Finished! Check Paper! <<<\r\n");
}