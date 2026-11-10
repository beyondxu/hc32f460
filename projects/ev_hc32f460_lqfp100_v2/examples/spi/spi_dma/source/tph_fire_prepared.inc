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
