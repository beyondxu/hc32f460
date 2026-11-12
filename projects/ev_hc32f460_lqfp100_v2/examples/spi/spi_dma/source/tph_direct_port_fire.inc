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
