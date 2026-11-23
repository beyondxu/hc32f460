# Dedicated prespray firing profile: first calibration step

This revision changes BOTH BK and HC32. Update/build/flash HC32 first, then BK.
The automatic boot/per-page prespray selects a dedicated firing function. Image
printing and legacy manual cleaning keep the original normal function. Image
routing, motion,5/50/200 doses, single-Domain/Section pattern and pauses stay as
V46. The earlier22-second gap between pre-page spit and first nonblank image
is not changed by this timing-only comparison.

## Actual change, and what is not claimed

HC32 retains the ordinary TPH_FireOneColumn source unchanged. A separately
compiled TPH_FireOneColumn_Prespray reuses the same implementation with16 NOPs
of address-group padding. At nominal200MHz this corresponds to80ns of extra
instructions, NOT an independently measured80ns edge shift or a certified2.2us
tFire. Actual sampling and address periods must be measured with the user's
compiler and at the head connector. If the baseline were exactly2.0us, a nominal
addition of0.08us would be around2.08us, not2.2us. Do not label it a2.2us mode.

Padding is outside all S LOW sample windows. The normal address train, selected
57-NOP sampling helper, continuous CE,25+zero-data stop, Ax/Dx reset, completed-
column idle guard and SysTick deadlines are retained. No TSR, preheat, CE pulse
heater, voltage increase or shortening of ts is implemented. This code cannot
certify a safe non-ejecting preheat temperature or enforce a measured2.2us maximum.

## Protocol selection and fail-closed behavior

PL_BEGIN_PRESPRAY=0x13 selects the profile for one cached row. The ordinary
PL_BEGIN=0x10 resets selection to normal. Status byte10 advertises support,
byte11 echoes selection and byte32 identifies the16-NOP profile; the existing
status CRC protects these fields. BK checks support/padding before loading a
prespray row and checks the selected profile before ARM. Missing capability,
wrong selection/padding or peer errors reject ARM/firing. The carriage may have
already moved to the waste-pad position before this check; it then aborts.

No persistent global heater-mode switch is used. Selection belongs to a cached
row, dispatches outside the live address train, and a new normal BEGIN selects
the normal function. Error/cancel clears selection. Head power shuts OFF after
playback and on existing abort paths. Old BK can use normal rows with the new
HC; new BK's prespray rejects old HC instead of silently using the wrong mode.

## First hardware check

Confirm HC boot log FIRE_PROFILE cap_v1 prespray_pad_nops=16. BK/HC LINE_ARMED
or LINE_BUFFER_READY logs must show fire_profile=1 extra_pad_nops=16 for prespray
and fire_profile=0 extra_pad_nops=0 for the following image rows. BK banner is
APP_SERVICE_CMY_PAD_FIRE_PROFILE_V47_requires_HC_prespray_pad16.

Before sustained recovery trials, measure matching S falling-edge spacing in
adjacent active address groups for both profiles, including the last-address
zero stop; verify actual tFire does not exceed the datasheet2.2us maximum and
S LOW remains>=0.38us. The nominal NOP count is not a replacement for this check.
Use the same ink/head/test image for the comparison, without simultaneously
changing wetting, supply, dose or motion.

## Validation

Checks cover actual M4 -O2/-Os objects (normal and dedicated functions), five
halfword preloads/13 writes per group, no calls in live windows, unchanged
sampling blocks and16-NOP dedicated padding; cache selection/cancel/reset;
actual SysTick normal/prespray/normal dispatch and power-OFF; BK SPI negotiation,
CRC, old/mismatched peer rejection before ARM, stationary zero-U4/U5 playback;
ordinary motion regression. HC32 spi_dma builds and links with the GCC sample
linker script; BK actual ARM objects compile. These checks do not measure real
edges, temperature, voltage or ink delivery.

## Restore pair

Both repositories have backup/before-prespray-fire-profile-20261123: BK104bd8d
and HC79ba462. Preserve local work and create a separate branch from the tag to
rebuild a baseline. Restore both for a complete return to the earlier pair.
