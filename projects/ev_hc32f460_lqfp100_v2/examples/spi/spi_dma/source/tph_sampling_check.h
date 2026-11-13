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
#if TPH_SAMPLING_CHECK_ENABLE
#define TPH_SAMPLING_CHECK_MODE "CHECK_68NOP"
#define TPH_SAMPLING_SELECTED_NOPS 68U
static __attribute__((always_inline)) inline void TPH_Check_Sample_Delay(void) {
    __asm volatile (
        "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n "
        "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n "
        "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n "
        "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n "
        "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n "
        "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n "
        "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n "
        "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n "
        "nop \n nop \n nop \n nop \n"
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
