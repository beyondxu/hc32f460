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
/* Select the sampling check only after local helper definitions. */
#include "tph_sampling_check.h"
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

/* TPH_CONTINUOUS_CE_v1 -- M0333N continuous-CE protocol.
 * Requires the existing GPIO macros, LUTs and calibrated delay helpers above.
 * CE width is NOT tFire. Measure matching S falling edges in adjacent groups.
 * This code does not claim a calibrated 1.70 us address period.
 */
#include "tph_column_cooldown.h"
static uint32_t s_u32CooldownCycles, s_u32CooldownColumns;
static uint32_t s_u32CooldownMin, s_u32CooldownMax;
static bool s_bHeadPowered = false;
static bool s_bHaveStopTime = false;
static uint32_t s_u32StopCycle;
static uint32_t s_u32IdleCycles;

static __attribute__((always_inline)) inline void TPH_Clear_Address(void) {
    CM_GPIO->PORRB = ADDR_PORTB_MASK; /* Ax=0000000, NOT LUT index zero */
}

static __attribute__((always_inline)) inline void TPH_All_S_Low(void) {
    S1_LOW(); S2_LOW(); S3_LOW(); S4_LOW();
}

static __attribute__((always_inline)) inline void TPH_All_S_High(void) {
    S1_HIGH(); S2_HIGH(); S3_HIGH(); S4_HIGH();
}

/* Only call with CE HIGH and Ax=0000000, or with all heating already stopped.
 * Four zero-data samples implement the documented initialization/reset state.
 * There is deliberately no CE_LOW() here.
 */
static __attribute__((always_inline)) inline void TPH_Reset_Idle_CE_High(void) {
    TPH_Clear_Address();
    FAST_SET_DATA(0);
    S1_LOW(); delay_ts_400ns(); S1_HIGH();
    S2_LOW(); delay_ts_400ns(); S2_HIGH();
    S3_LOW(); delay_ts_400ns(); S3_HIGH();
    S4_LOW(); delay_ts_400ns(); S4_HIGH();
    __DSB();
}

/* DWT is used only while the head is idle, never inside the live address train.
 * Do not reset CYCCNT: another component may also use the free-running counter.
 */
static bool TPH_Start_Idle_Clock(void) {
    if (SystemCoreClock == 0U) return false;
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    __DSB(); __ISB();
    uint32_t before = DWT->CYCCNT;
    for (unsigned i = 0; i < 1000U; ++i) {
        if (DWT->CYCCNT != before) {
            /* Conservative 60 us IDLE gap after each completed column.
             * This is a diagnostic margin, not a per-address specification.
             */
            s_u32IdleCycles = (uint32_t)(((uint64_t)SystemCoreClock * 60U
                                         + 999999U) / 1000000U);
            s_u32CooldownCycles = (uint32_t)(((uint64_t)SystemCoreClock
                * TPH_COLUMN_COOLDOWN_US + 999999U) / 1000000U);
            return true;
        }
    }
    return false;
}

/* The zero stop/reset and caller IRQ mask are restored before this wait.
 * Extra idle is spent here before the HC32 main loop rearms SPI/READY, so
 * communication or motor waits cannot hide/absorb the added delay.
 * The previous 60 us minimum gap remains independent and unchanged.
 */
static void TPH_Column_Cooldown(void) {
#if TPH_COLUMN_COOLDOWN_US > 0
    uint32_t begin = DWT->CYCCNT;
    uint32_t elapsed;
    do {
        elapsed = (uint32_t)(DWT->CYCCNT - begin);
    } while (elapsed < s_u32CooldownCycles);
    if (elapsed < s_u32CooldownMin) s_u32CooldownMin = elapsed;
    if (elapsed > s_u32CooldownMax) s_u32CooldownMax = elapsed;
    ++s_u32CooldownColumns;
#endif
}

void TPH_M0333N_Power(en_functional_state_t enState) {
    if (ENABLE == enState) {
        if (s_bHeadPowered) return; /* Do not reset a live session. */
        if (!s_bLutInitialized) TPH_Build_Fast_LUT();
        CE_LOW();
        TPH_Clear_Address();
        FAST_SET_DATA(0);
        TPH_All_S_Low();
        GPIO_ResetPins(PIN_TPH_EN_PORT, PIN_TPH_EN_PIN);
        GPIO_ResetPins(PIN_VL_EN_PORT, PIN_VL_EN_PIN);
        if (!TPH_Start_Idle_Clock()) {
            printf("[TPH ERROR] idle clock unavailable; head remains OFF\r\n");
            return;
        }
        GPIO_SetPins(PIN_VL_EN_PORT, PIN_VL_EN_PIN);
        delay_10us(); /* VL -> VP: tpo >= 5 us */
        GPIO_SetPins(PIN_TPH_EN_PORT, PIN_TPH_EN_PIN);
        delay_10us(); /* VP -> S HIGH: tw >= 0.5 us */
        TPH_All_S_High();
        delay_10us(); /* S HIGH -> CE HIGH; conservative tn margin */
        CE_HIGH();
        TPH_Reset_Idle_CE_High();
        s_bHaveStopTime = false;
        s_u32CooldownColumns = 0;
        s_u32CooldownMin = UINT32_MAX;
        s_u32CooldownMax = 0;
        s_bHeadPowered = true;
        printf("[TPH Power] CONT_CE_v1 ON CE=HIGH idle=Ax0_Dx0; rails unmeasured\r\n");
        printf("[TPH COOLDOWN] v1 extra_idle_us=%lu after_zero_reset before_READY no_extra_fires\r\n",
               (unsigned long)TPH_COLUMN_COOLDOWN_US);
    } else {
        if (s_bHeadPowered) {
            /* The last column has already received its Add25 zero stop.
             * Reset at Ax=0 with CE still HIGH before the shutdown sequence.
             */
            TPH_Reset_Idle_CE_High();
            CE_LOW();
            delay_10us(); /* CE LOW -> S LOW */
        } else {
            CE_LOW();
            TPH_Clear_Address();
            FAST_SET_DATA(0);
        }
        TPH_All_S_Low();
        delay_10us(); /* S LOW -> VP OFF: tw >= 0.5 us */
        GPIO_ResetPins(PIN_TPH_EN_PORT, PIN_TPH_EN_PIN);
        delay_10us(); /* VP OFF -> VL OFF: tpf >= 5 us */
        GPIO_ResetPins(PIN_VL_EN_PORT, PIN_VL_EN_PIN);
        if (s_bHeadPowered) {
            printf("[TPH COOLDOWN_END] extra_idle_us=%lu columns=%lu min_wait_cycles=%lu max_wait_cycles=%lu core_hz=%lu\r\n",
                   (unsigned long)TPH_COLUMN_COOLDOWN_US,
                   (unsigned long)s_u32CooldownColumns,
                   (unsigned long)(s_u32CooldownColumns ? s_u32CooldownMin : 0U),
                   (unsigned long)s_u32CooldownMax, (unsigned long)SystemCoreClock);
        }
        s_bHeadPowered = false;
        s_bHaveStopTime = false;
        printf("[TPH Power] CONT_CE_v1 OFF CE=LOW\r\n");
    }
}

/* TPH_DIRECT_PORT_v2: halfword preparation and input-only preload barrier.
 * Removes redundant moves/truncations; S sampling helpers are unchanged.
 * Requires the continuous-CE power/stop protocol, never the old pulsed-CE one.
 * Re-measure all S widths and matching-S address periods after the local build.
 * No claim of 1.70 us is made by this source code.
 */
#ifndef TPH_DIRECT_PORT_PAD_NOPS
#define TPH_DIRECT_PORT_PAD_NOPS 0
#endif
#if TPH_DIRECT_PORT_PAD_NOPS < 0 || TPH_DIRECT_PORT_PAD_NOPS > 16 || (TPH_DIRECT_PORT_PAD_NOPS % 4) != 0
#error TPH_DIRECT_PORT_PAD_NOPS must be 0, 4, 8, 12 or 16
#endif

void TPH_FireOneColumn(const uint8_t *pColumn100B) {
    if (!s_bHeadPowered || pColumn100B == NULL) return;

    struct tph_group {
        uint16_t address;
        uint16_t data[4];
    } groups[26];
    /* Decode while no nozzle is active. Entry 26 stops Add25 with zero data. */
    for (unsigned add = 0; add < 26U; ++add) {
        unsigned index = add < 25U ? add : 24U;
        groups[add].address = s_AddrPortB_Set[index];
        for (unsigned sec = 0; sec < 4U; ++sec) {
            unsigned d = add < 25U ? (pColumn100B[add * 4U + sec] & 0x3FU) : 0U;
            groups[add].data[sec] = s_DataPortA_Set[d];
        }
    }
    if (s_bHaveStopTime) {
        while ((uint32_t)(DWT->CYCCNT - s_u32StopCycle) < s_u32IdleCycles) {
            /* Preserve the existing 60 us completed-column idle guard. */
        }
    }

    uint32_t saved_primask = __get_PRIMASK();
    __disable_irq();
    /* Snapshot OUTPUT latches, not input pins, after IRQ exclusion.
     * Preserve CE, VL/VP enables and unrelated GPIO outputs in every word.
     * No concurrent DMA or other core may write these same GPIO output latches.
     * Normal HC32 print firmware has one owner of these port outputs.
     */
    const uint32_t keep_a = CM_GPIO->PODRA & (uint16_t)~DATA_PORTA_MASK;
    const uint32_t keep_b = CM_GPIO->PODRB & (uint16_t)~ADDR_PORTB_MASK;
    for (unsigned add = 0; add < 26U; ++add) {
        groups[add].address |= keep_b;
        for (unsigned sec = 0; sec < 4U; ++sec) groups[add].data[sec] |= keep_a;
    }
    __DMB();
    const struct tph_group *group = groups;
    const struct tph_group *end = groups + 26U;
    do {
        uint16_t address = group->address;
        uint16_t w0 = group->data[0];
        uint16_t w1 = group->data[1];
        uint16_t w2 = group->data[2];
        uint16_t w3 = group->data[3];
        /* Read all five halfwords before any live GPIO update. Input-only
         * operands preserve their known 16-bit range: read/write operands
         * made GCC emit extra moves and UXTHs between sampling edges.
         * This is a compiler barrier, not a delay or an S-width guarantee.
         */
        __asm volatile ("" : : "r" (address), "r" (w0), "r" (w1),
                                "r" (w2), "r" (w3) : "memory");
        CM_GPIO->PODRB = (uint16_t)address;

        CM_GPIO->PODRA = (uint16_t)w0;
        S1_LOW(); delay_ts_400ns(); S1_HIGH();

        CM_GPIO->PODRA = (uint16_t)w1;
        S2_LOW(); delay_ts_400ns(); S2_HIGH();

        CM_GPIO->PODRA = (uint16_t)w2;
        S3_LOW(); delay_ts_400ns(); S3_HIGH();

        CM_GPIO->PODRA = (uint16_t)w3;
        S4_LOW(); delay_ts_400ns(); S4_HIGH();
        /* Optional calibrated address-period padding, never inside an S LOW.
         * These are counts of NOP instructions, not certified nanoseconds.
         */
#if TPH_DIRECT_PORT_PAD_NOPS >= 4
        __asm volatile ("nop\n nop\n nop\n nop\n" ::: "memory");
#endif
#if TPH_DIRECT_PORT_PAD_NOPS >= 8
        __asm volatile ("nop\n nop\n nop\n nop\n" ::: "memory");
#endif
#if TPH_DIRECT_PORT_PAD_NOPS >= 12
        __asm volatile ("nop\n nop\n nop\n nop\n" ::: "memory");
#endif
#if TPH_DIRECT_PORT_PAD_NOPS >= 16
        __asm volatile ("nop\n nop\n nop\n nop\n" ::: "memory");
#endif
        ++group;
    } while (group != end);

    TPH_Reset_Idle_CE_High();
    s_u32StopCycle = DWT->CYCCNT;
    s_bHaveStopTime = true;
    __DSB();
    __set_PRIMASK(saved_primask);
    TPH_Column_Cooldown();
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
    stcGpioInit.u16PinState = PIN_STAT_RST;
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

/* Compatibility entry points all use the same continuous-CE protocol. */
void TPH_FireOneColumn_fix(const uint8_t *unused) {
    uint8_t column[100];
    (void)unused;
    memset(column, 0x33, sizeof(column));
    TPH_FireOneColumn(column);
}
void TPH_FireOneColumnxx(const uint8_t *column) {
    TPH_FireOneColumn(column);
}
void HC32_Standalone_Hardware_Test(void) {
    uint8_t column[100] = {0};
    column[10U * 4U] = 0x15;
    for (unsigned i = 0; i < 600U; ++i) {
        TPH_FireOneColumn(column);
        DDL_DelayMS(2);
    }
}
