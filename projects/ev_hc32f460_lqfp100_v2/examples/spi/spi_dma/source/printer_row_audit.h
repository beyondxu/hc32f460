#ifndef PRINTER_ROW_AUDIT_H
#define PRINTER_ROW_AUDIT_H
#include <stdint.h>
#include <stdio.h>
#include <string.h>
/* Observational only. Fixed reference: BK CMY_ROWS, pitch=3000, direction=-1,
 * row C95/M135/Y175. Never changes data, waveform, READY or power decisions.
 */
typedef struct {
    uint32_t columns, fired, invalid, canceled, other_bits;
    uint32_t dots[3], bars[3][3], outside[3], first[3], last[3];
    uint8_t active;
    uint32_t rx_crc, rx_domains, rx_addresses, rx_sections, domain_dots[6];
} printer_row_audit_t;
/* Fast table CRC over the received 100-byte frames, including zero columns.
 * Commands are excluded by the existing engine. No GPIO or firing calls here. */
static uint32_t printer_rx_crc_lut[256];
static uint8_t printer_rx_crc_ready;
static void printer_rx_crc_prepare(void) {
    if (printer_rx_crc_ready) return;
    for (unsigned i = 0; i < 256; ++i) {
        uint32_t c = i;
        for (unsigned bit = 0; bit < 8; ++bit)
            c = (c >> 1) ^ ((0U - (c & 1U)) & 0xEDB88320U);
        printer_rx_crc_lut[i] = c;
    }
    printer_rx_crc_ready = 1;
}
static void printer_row_audit_begin(printer_row_audit_t *a) {
    memset(a, 0, sizeof(*a));
    a->active = 1;
    printer_rx_crc_prepare();
    a->rx_crc = 0xFFFFFFFFU;
    for (unsigned ch = 0; ch < 3; ++ch) a->first[ch] = UINT32_MAX;
}
static void printer_row_audit_column(printer_row_audit_t *a, const uint8_t *p,
                                     unsigned active_bits, unsigned valid) {
    static const uint8_t slots[3] = {73, 46, 15};
    static const uint8_t masks[3] = {0x02, 0x08, 0x20};
    static const uint8_t delays[3] = {59, 30, 0};
    if (!a->active) return;
    uint32_t x = a->columns++;
    /* Runs after the existing TPH_FireOneColumn has returned, before READY.
     * Adds bounded bookkeeping time between columns, never inside CE high. */
    for (unsigned slot = 0; slot < 100; ++slot) {
        uint8_t b = p[slot];
        a->rx_crc = (a->rx_crc >> 8) ^ printer_rx_crc_lut[(a->rx_crc ^ b) & 0xFFU];
        b &= 0x3FU;
        if (!b) continue;
        a->rx_domains |= b;
        a->rx_addresses |= 1UL << (slot / 4U);
        a->rx_sections |= 1UL << (slot % 4U);
        for (unsigned bit = 0; bit < 6; ++bit) a->domain_dots[bit] += (b >> bit) & 1U;
    }
    if (valid) ++a->fired; else ++a->invalid;
    unsigned selected_bits = 0;
    for (unsigned ch = 0; ch < 3; ++ch) {
        if (!(p[slots[ch]] & masks[ch])) continue;
        ++selected_bits;
        ++a->dots[ch];
        if (a->first[ch] == UINT32_MAX) a->first[ch] = x;
        a->last[ch] = x;
        unsigned matched = 0;
        for (unsigned bar = 0; bar < 3; ++bar) {
            uint32_t start = 300U + 200U * bar + delays[ch];
            if (x >= start && x < start + 150U) { ++a->bars[ch][bar]; matched = 1; }
        }
        if (!matched) ++a->outside[ch];
    }
    if (active_bits > selected_bits) a->other_bits += active_bits - selected_bits;
}
static void printer_row_audit_end(printer_row_audit_t *a) {
    if (!a->active) return;
    a->active = 0;
    printf("[HC32 ROW_AUDIT] columns=%lu fired=%lu invalid=%lu canceled=%lu other_bits=%lu\r\n",
           (unsigned long)a->columns, (unsigned long)a->fired, (unsigned long)a->invalid,
           (unsigned long)a->canceled, (unsigned long)a->other_bits);
    for (unsigned ch = 0; ch < 3; ++ch) {
        printf("[HC32 ROW_%c] dots=%lu bars=%lu,%lu,%lu outside=%lu first=%ld last=%ld\r\n",
               "CMY"[ch], (unsigned long)a->dots[ch],
               (unsigned long)a->bars[ch][0], (unsigned long)a->bars[ch][1], (unsigned long)a->bars[ch][2],
               (unsigned long)a->outside[ch], a->dots[ch] ? (long)a->first[ch] : -1L,
               a->dots[ch] ? (long)a->last[ch] : -1L);
    }
    printf("[HC32 RX_AUDIT] bytes=%lu crc32=%08lX D=%02lX A=%08lX S=%02lX dots_D1_D6=%lu,%lu,%lu,%lu,%lu,%lu\r\n",
           (unsigned long)(a->columns * 100U), (unsigned long)(a->rx_crc ^ 0xFFFFFFFFU),
           (unsigned long)a->rx_domains, (unsigned long)a->rx_addresses, (unsigned long)a->rx_sections,
           (unsigned long)a->domain_dots[0], (unsigned long)a->domain_dots[1],
           (unsigned long)a->domain_dots[2], (unsigned long)a->domain_dots[3],
           (unsigned long)a->domain_dots[4], (unsigned long)a->domain_dots[5]);
}
#endif
