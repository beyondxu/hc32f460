/* TPH_CONTINUOUS_CE_v1 -- M0333N continuous-CE protocol.
 * Requires the existing GPIO macros, LUTs and calibrated delay helpers above.
 * CE width is NOT tFire. Measure matching S falling edges in adjacent groups.
 * This code does not claim a calibrated 1.70 us address period.
 */
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
            return true;
        }
    }
    return false;
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
        s_bHeadPowered = true;
        printf("[TPH Power] CONT_CE_v1 ON CE=HIGH idle=Ax0_Dx0; rails unmeasured\r\n");
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
        s_bHeadPowered = false;
        s_bHaveStopTime = false;
        printf("[TPH Power] CONT_CE_v1 OFF CE=LOW\r\n");
    }
}

void TPH_FireOneColumn(const uint8_t *pColumn100B) {
    if (!s_bHeadPowered || pColumn100B == NULL) return;

    /* Predecode the complete column PLUS one zero stop group while idle.
     * The 26th entry uses Add25 again, not Add1 and not an out-of-range LUT.
     * All entries follow the same live loop path, including the stop entry.
     * 26 * 5 * 4 = 520 bytes of automatic storage; no heap allocation.
     */
    struct tph_group {
        uint32_t address;
        uint32_t data[4];
    } groups[26];
    for (unsigned add = 0; add < 26U; ++add) {
        unsigned index = add < 25U ? add : 24U;
        groups[add].address = ((uint32_t)s_AddrPortB_Clr[index] << 16)
                           | s_AddrPortB_Set[index];
        for (unsigned sec = 0; sec < 4U; ++sec) {
            unsigned d = add < 25U ? (pColumn100B[add * 4U + sec] & 0x3FU) : 0U;
            groups[add].data[sec] = ((uint32_t)s_DataPortA_Clr[d] << 16)
                                | s_DataPortA_Set[d];
        }
    }
    if (s_bHaveStopTime) {
        while ((uint32_t)(DWT->CYCCNT - s_u32StopCycle) < s_u32IdleCycles) {
            /* CE HIGH, Ax=0, all latches zero: no active heating here. */
        }
    }

    uint32_t saved_primask = __get_PRIMASK();
    __disable_irq();
    __DMB();
    const struct tph_group *group = groups;
    const struct tph_group *end = groups + 26U;
    do {
        /* No source buffer reads, LUT lookups, logging, timers or IRQ service
         * in the live train. Only prepared-word loads and GPIO/S operations.
         * Group overhead IS part of tFire; there is no extra 1 us wait.
         */
        uint32_t address = group->address;
        uint32_t w0 = group->data[0];
        uint32_t w1 = group->data[1];
        uint32_t w2 = group->data[2];
        uint32_t w3 = group->data[3];
        __asm volatile ("" : "+r" (address), "+r" (w0), "+r" (w1),
                              "+r" (w2), "+r" (w3) : : "memory");
        CM_GPIO->PORRB = address >> 16;
        CM_GPIO->POSRB = (uint16_t)address;

        CM_GPIO->PORRA = w0 >> 16;
        CM_GPIO->POSRA = (uint16_t)w0;
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
        ++group;
    } while (group != end);

    /* All four sections have now sampled ZERO at Add25, at the normal cadence.
     * Only now enter global reset/idle. CE remains HIGH until power OFF.
     */
    TPH_Reset_Idle_CE_High();
    s_u32StopCycle = DWT->CYCCNT;
    s_bHaveStopTime = true;
    __DSB();
    __set_PRIMASK(saved_primask);
}
