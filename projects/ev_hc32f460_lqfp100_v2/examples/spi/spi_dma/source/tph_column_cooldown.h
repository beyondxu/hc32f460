/* Diagnostic extra idle AFTER every complete column and zero reset.
 * Does not set S width, tFire, voltage or the number of nozzle commands.
 * Fixed-S2 per-color check defaults to 0 extra idle; keep the 60us guard.
 */
#ifndef TPH_COLUMN_COOLDOWN_H
#define TPH_COLUMN_COOLDOWN_H
#ifndef TPH_COLUMN_COOLDOWN_US
#define TPH_COLUMN_COOLDOWN_US 0U
#endif
#if TPH_COLUMN_COOLDOWN_US < 0 || TPH_COLUMN_COOLDOWN_US > 5000U
#error TPH_COLUMN_COOLDOWN_US must be between 0 and 5000
#endif
#endif
