# V29 paired speed trial: lower allowed column period to500us

BK V29 requests542us columns and66us carriage STEP cruise, approximately12.1% faster than V28. The previous600us minimum was a software validation bound. Both peers now accept500..3000us; the on-wire LBUFv1 layout, row length and CRC remain unchanged.

No change is made to electrical A/D/S/CE firing routines, per-address timings,60us inter-column idle guard or extra-column cooldown. The existing SysTick playback logic remains unchanged: over20us deadline lateness or an execution duration consuming all but20us of the column period causes PL_TIMING and stops the line without catch-up firing. Head power is disabled after leaving playback. At542us, the checked execution duration must remain below522us (104400 cycles at200MHz), and start lateness must be <=4000 cycles.

Source inspection establishes the presence of these runtime guards, not actual worst-case hardware execution time. The current line log already reports max_fire_cycles and max_late_cycles. Review those on the first same-image hardware trial. These host tests do not measure the target's GPIO/peripheral execution cost or verify ink output.

Validation:
- Cache rejects499us and accepts500us and542us, preserving existing data/CRC/state validation.
- Actual playback code completes1180 simulated columns at542us and500us.
- At542us, a synthetic521us execution passes while522us causes PL_TIMING after one column, powers OFF and never continues firing.
- Existing cancel, late IRQ, busy clock and timeout tests still pass.
- Full HC GCC compile/link succeeds; this is not a substitute for the user's Keil build/hardware timing verification.

Update/build/flash HC first on codex/printer-cached-line-timer, then BK on codex/printer-startup-load-prep. Old HC rejects the542us request, so updating only BK produces a preparation/printing failure instead of the intended faster scan. HC startup marker includes speed_trial_v3 min_period_us=500 deadline_guard_us=20.
