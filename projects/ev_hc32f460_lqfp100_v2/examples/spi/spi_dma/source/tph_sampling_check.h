/* Diagnostic sampling selection. In the driver, include only AFTER the
 * original delay helper definitions and BEFORE protocol call sites. Never
 * include from tph_m0333n.h: that would rewrite the original definition.
 * NOP count is not a certified time; measure at the head connector.
 */
#ifndef TPH_SAMPLING_CHECK_H
#define TPH_SAMPLING_CHECK_H
#ifndef TPH_SAMPLING_CHECK_ENABLE
#define TPH_SAMPLING_CHECK_ENABLE 1
#endif
#if TPH_SAMPLING_CHECK_ENABLE != 0 && TPH_SAMPLING_CHECK_ENABLE != 1
#error TPH_SAMPLING_CHECK_ENABLE must be 0 or 1
#endif
/* Candidate derived from local 68-NOP/445ns and reference 390ns readings.
 * Eleven fewer NOPs estimate 55ns at 200MHz; hardware must confirm. */
#ifndef TPH_SAMPLING_CHECK_NOPS
#define TPH_SAMPLING_CHECK_NOPS 57U
#endif
#if TPH_SAMPLING_CHECK_NOPS != 57U && TPH_SAMPLING_CHECK_NOPS != 68U
#error TPH_SAMPLING_CHECK_NOPS must be 57 or 68
#endif
#if TPH_SAMPLING_CHECK_ENABLE
#if TPH_SAMPLING_CHECK_NOPS == 57U
#define TPH_SAMPLING_CHECK_MODE "CHECK_57NOP"
#else
#define TPH_SAMPLING_CHECK_MODE "CHECK_68NOP"
#endif
#define TPH_SAMPLING_SELECTED_NOPS TPH_SAMPLING_CHECK_NOPS
static __attribute__((always_inline)) inline void TPH_Check_Sample_Delay(void) {
    __asm volatile (
        "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n "
        "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n "
        "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n "
        "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n "
        "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n "
        "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n "
        "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n "
        "nop \n"
#if TPH_SAMPLING_CHECK_NOPS == 68U
        "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n "
        "nop \n nop \n nop \n"
#endif
        ::: "memory"
    );
}
/* Only calls below this include are redirected; original local calibration
 * remains intact and is used again when the diagnostic is disabled.
 */
#define delay_ts_400ns() TPH_Check_Sample_Delay()
#else
#define TPH_SAMPLING_CHECK_MODE "LOCAL_HELPER"
#define TPH_SAMPLING_SELECTED_NOPS 0U
#endif
#endif
