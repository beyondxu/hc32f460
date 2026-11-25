/* TPH_DIRECT_PORT_v2: halfword preparation and input-only preload barrier.
 * Removes redundant moves/truncations; S sampling helpers are unchanged.
 * Requires the continuous-CE power/stop protocol, never the old pulsed-CE one.
 * Re-measure all S widths and matching-S address periods after the local build.
 * No claim of 1.70 us is made by this source code.
 */
#ifndef TPH_DIRECT_PORT_PAD_NOPS
#define TPH_DIRECT_PORT_PAD_NOPS 0
#endif
#if TPH_DIRECT_PORT_PAD_NOPS < 0 || TPH_DIRECT_PORT_PAD_NOPS > 32 || (TPH_DIRECT_PORT_PAD_NOPS % 4) != 0
#error TPH_DIRECT_PORT_PAD_NOPS must be a multiple of4 from0 through32
#endif

#ifdef TPH_SHORT_PRESPRAY
bool TPH_FirePresprayShort(const uint8_t *pColumn100B, unsigned first, unsigned count,
                          uint32_t earliest, uint32_t latest, uint32_t *started) {
    if (!s_bHeadPowered || pColumn100B == NULL || !count ||
        count > PL_PRESPRAY_SHORT_ADDRESSES || first + count > 25U) return false;
#else
void TPH_FireOneColumn(const uint8_t *pColumn100B) {
    if (!s_bHeadPowered || pColumn100B == NULL) return;
    const unsigned first = 0U, count = 25U;
#endif

    struct tph_group {
        uint16_t address;
        uint16_t data[4];
#ifdef TPH_SHORT_PRESPRAY
    } groups[PL_PRESPRAY_SHORT_ADDRESSES + 1U];
#else
    } groups[26];
#endif
    /* Immediately follow the last active address with the same address at D=0. */
    for (unsigned add = 0; add <= count; ++add) {
        unsigned index = first + (add < count ? add : count - 1U);
        groups[add].address = s_AddrPortB_Set[index];
        for (unsigned sec = 0; sec < 4U; ++sec) {
            unsigned d = add < count ? (pColumn100B[index * 4U + sec] & 0x3FU) : 0U;
            groups[add].data[sec] = s_DataPortA_Set[d];
        }
    }
#ifndef TPH_SHORT_PRESPRAY
    if (s_bHaveStopTime) {
        while ((uint32_t)(DWT->CYCCNT - s_u32StopCycle) < s_u32IdleCycles) {
            /* Preserve the existing 60 us completed-column idle guard. */
        }
    }

#endif

    uint32_t saved_primask = __get_PRIMASK();
    __disable_irq();
    /* Snapshot OUTPUT latches, not input pins, after IRQ exclusion.
     * Preserve CE, VL/VP enables and unrelated GPIO outputs in every word.
     * No concurrent DMA or other core may write these same GPIO output latches.
     * Normal HC32 print firmware has one owner of these port outputs.
     */
    const uint32_t keep_a = CM_GPIO->PODRA & (uint16_t)~DATA_PORTA_MASK;
    const uint32_t keep_b = CM_GPIO->PODRB & (uint16_t)~ADDR_PORTB_MASK;
    for (unsigned add = 0; add <= count; ++add) {
        groups[add].address |= keep_b;
        for (unsigned sec = 0; sec < 4U; ++sec) groups[add].data[sec] |= keep_a;
    }
    __DMB();
    const struct tph_group *group = groups;
    const struct tph_group *end = groups + count + 1U;
#ifdef TPH_SHORT_PRESPRAY
    /* Timestamp the train after preparation, with IRQs masked. Identical
     * trains follow the same instruction path from here to each firing edge. */
    uint32_t now;
    do {
        now = DWT->CYCCNT;
    } while ((int32_t)(now - earliest) < 0);
    if ((int32_t)(now - latest) > 0) {
        __set_PRIMASK(saved_primask);
        return false;
    }
    *started = now;
#endif
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
#if TPH_DIRECT_PORT_PAD_NOPS >= 20
        __asm volatile ("nop\n nop\n nop\n nop\n" ::: "memory");
#endif
#if TPH_DIRECT_PORT_PAD_NOPS >= 24
        __asm volatile ("nop\n nop\n nop\n nop\n" ::: "memory");
#endif
#if TPH_DIRECT_PORT_PAD_NOPS >= 28
        __asm volatile ("nop\n nop\n nop\n nop\n" ::: "memory");
#endif
#if TPH_DIRECT_PORT_PAD_NOPS >= 32
        __asm volatile ("nop\n nop\n nop\n nop\n" ::: "memory");
#endif
        ++group;
    } while (group != end);

    TPH_Reset_Idle_CE_High();
    s_u32StopCycle = DWT->CYCCNT;
    s_bHaveStopTime = true;
    __DSB();
    __set_PRIMASK(saved_primask);
#ifdef TPH_SHORT_PRESPRAY
    return true;
#else
    TPH_Column_Cooldown();
#endif
}
