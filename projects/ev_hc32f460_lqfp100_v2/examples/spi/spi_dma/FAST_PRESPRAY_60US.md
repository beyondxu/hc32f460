# 60 µs stationary prespray trial

Build and flash HC32 first, then BK7258. Both sides must advertise/accept the
60 µs prespray period and 32-NOP profile. The ordinary 673 µs image line uses
the original 25-address train, zero stop and 60 µs completed-column idle guard.
The HC32 also retains its full-scan firing path for prespray at 300 µs and above.

The BK prespray generator divides the 25 addresses into groups of 13 and 12
for each Domain and section pair. It keeps the 10/100/1600 requests per nozzle,
at most eight consecutive requests, 40 rounds per cache row, and 5 ms
between batches with the head OFF. GPIO24 must remain LOW at the stationary
pad position during playback. BK releases SYNC if an optical poll detects
loss. The HC checks SYNC before short shots and still handles late slots,
cancellation, timeout and power-off.

The M0333N V1.2 specification lists 18 kHz (55.56 µs) maximum per nozzle,
a 0.38 µs minimum S LOW, a 2.2 µs maximum tFire, a zero-data stop immediately
after the final active address, and refill time between cycles.
The short path keeps the S sampling code, 32-NOP padding, final-address
zero stop and Ax/Dx reset. Its refill interval is controlled by the repeat
start schedule. It bypasses the old 60 µs idle wait after the full train,
which was an additional firmware margin. Overlapping consecutive columns
are accepted only as identical trains, so the same nozzle keeps its position
within the train. The DWT timestamp just before GPIO writes gates each repeat.
A repeat waits 12,000 cycles at 200 MHz from the previous train start and
aborts if it cannot meet the slot's phase limit. The 20 µs execution margin
and the VL/VP/CE shutdown sequencing remain in place.

## Bench identification and acceptance

- BK banner includes `V54_requires_HC_prespray_60us_pad32`. At the sponge,
  expect `WASTE_POSITION ... gpio24_LOW`, `PRESPRAY_BEGIN ... repeat_us=60
  ... between_rows_ms=5`, and `PRESPRAY_DONE`.
- HC `LINE_ARMED` shows `period_us=60 fire_profile=1 extra_pad_nops=32`.
  Expect `LINE_DONE ... state=5 error=0 fired=1180/1180`.
  `PRESPRAY_TRAIN` reports the range of repeat train-start intervals in
  CPU cycles. The following image row reports `period_us=673 fire_profile=0`.
- At the head connector, decode A, D and S together. Select one Address,
  Section and Domain with nonzero data. Measure successive matching S falling
  edges within an eight-shot burst, including channels near both ends of
  each active address range. Check for approximately 60 µs with no compressed
  intervals. Separately verify S LOW, tFire, final-address zero stop and
  idle reset before sustained trials. Register/timer logs give software
  timing. Electrical waveforms and ink delivery require bench measurements.
- BK boot and pre-page full-stroke preparation use a capped 11,315-step
  seek and a 11,315-step return. GPIO24 stops the seek as soon as the
  optical reference is found. The ordinary print stroke is 10,473 steps,
  and the final optical-reference-to-wiper return is 11,221 steps.

Both repositories retain the paired restore tag
`backup/before-prespray-60us-20261125` for the previous 300 µs build.
