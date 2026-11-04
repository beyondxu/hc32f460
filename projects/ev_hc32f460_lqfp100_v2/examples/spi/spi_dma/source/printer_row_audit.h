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
} printer_row_audit_t;
static void printer_row_audit_begin(printer_row_audit_t *a) {
    memset(a, 0, sizeof(*a));
    a->active = 1;
    for (unsigned ch = 0; ch < 3; ++ch) a->first[ch] = UINT32_MAX;
}
static void printer_row_audit_column(printer_row_audit_t *a, const uint8_t *p,
                                     unsigned active_bits, unsigned valid) {
    static const uint8_t slots[3] = {73, 46, 15};
    static const uint8_t masks[3] = {0x02, 0x08, 0x20};
    static const uint8_t delays[3] = {59, 30, 0};
    if (!a->active) return;
    uint32_t x = a->columns++;
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
}
#endif
