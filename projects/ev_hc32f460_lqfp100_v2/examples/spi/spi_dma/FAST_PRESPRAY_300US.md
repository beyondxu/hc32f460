# V52: dedicated300us prespray

Update HC32 first, then BK7258, on the existing branches. Requires BOTH updates.
Normal image and manual waste-clean rows retain673us (normal protocol minimum
500us). Only automatic prespray uses300us, about3.33kHz within each8-shot burst.
Shots/nozzle remain10/100/800 for <3min/3..<15min/long-or-unknown idle respectively.
40 rounds/cache, paired Sections in a single Domain,32-NOP flash padding,
S LOW width, VP, stroke, feed and image mapping are unchanged. No preheat or TSR.
Batch head-OFF waits are5ms, previously20ms; transfers and power-on waits remain.

| Dose/nozzle | Cache rows | Nominal playback + OFF pauses |
| ---: | ---: | ---: |
|10|1|0.354s|
|100|3|1.072s|
|800|20|7.175s|

Formula: rows*1180*300us +(rows-1)*5ms. Transfer, positioning and return add time.
No claim of measured temperature, actual tFire or overnight recovery is made.
The existing25+zero stop, Ax/Dx reset and60us completed-column idle guard stay.
The20us SysTick deadline/overrun margin stays: at300us, execution reaching280us
stops with PL_TIMING; no silent column skipping or automatic retry is added.
Inspect HC LINE_DONE max_fire_cycles and max_late_cycles at actual200MHz;
these report runtime timing, not independently measured S pulse width.

## Pair negotiation

Status byte10 now has PL_CAP_PRESPRAY_FAST (bit1) as well as the existing FIRE
capability (bit0). Bytes33..34 report the300us prespray minimum, little-endian;
byte32 still reports32 NOPs. Existing CRC covers all fields. BK verifies both
capabilities, minimum and padding before upload/ARM. An older HC with pad32 but
without fast capability is rejected before firing. Old BK can still use673us
with new HC. Normal BEGIN below500us is rejected, even after a fast prespray.
Only prespray BEGIN, and ARM/CANCEL belonging to it, permit300us; lower periods
are rejected. ARM period/token/CRC must exactly match the loaded row.

BK uses the selected period for both prepare and SYNC-held completion wait,
including stalled-RTC iteration bounds. Stationary optical retention remains
polled every1ms; faster firing permits up to about4 additional columns between
polls. Motor remains held over OC3_RIGHT; no carriage/paper steps during firing.

## Validation and identification

Tests exercise full fast upload/ARM/cancel,299us rejection, normal300us rejection
after a fast role, mismatched ARM period, CRC/capability rejection, actual
SysTick300us deadlines and280us overrun edge, cancellation/lateness/wrap, fast
prespray followed by normal673us, stationary300us completion wait and optical
release. Existing normal-motion and boot/job tests remain. BK SDK ARM objects
compile; actual HC spi_dma compiles and links. Hardware measurements are pending.

BK banner: APP_SERVICE_CMY_PAD_FAST_PRESPRAY_V52_requires_HC_prespray_300us_pad32.
PRESPRAY_BEGIN: repeat_us=300 between_rows_ms=5. Prespray LINE_BUFFER_READY and
HC LINE_ARMED/LINE_DONE should show period_us=300 fire_profile=1; subsequent
image rows should show period_us=673 fire_profile=0. Require PRESPRAY_DONE;
a reset or STOP is not a completed recovery trial.

Restore tag in BOTH repositories: backup/before-prespray-300us-20261124.
It retains the previous673us/20ms behavior (BK db1b659, HC972fa1d).
