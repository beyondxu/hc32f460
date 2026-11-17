# LBUFv1 cached-line playback

Requires the BK branch `codex/printer-cached-line-timer`. Rebuild the existing
MDK/spi_dma.uvprojx project; new logic is included by main.c, so no extra C source
entry or wiring change is needed. Flash HC32 first, then BK. HC32 boot prints
LBUFv1 clock_init_v2 capacity=118000; BK prints APP_SERVICE_CMY_BUFFERED_V10. Old BK per-column
PCTL remains supported, but new BK refuses an old HC32 peer before motion.

## Stationary transfer

The shared printer_line_protocol.h must match byte-for-byte between repositories.
A row is1180*100 bytes, all nozzle bytes must be0..63. Single global cache; no
second row bank and no dynamic allocation. Commands are100 bytes, first0x80,
opcode0x10 BEGIN/0x11 ARM/0x12 CANCEL, magic LBUFv1 at2..7, nonzero little-endian
token at8, columns16 at12=1180, zero14..15, period32 at16=600..3000us, CRC32 at20,
all other bytes zero. Current BK requests720us, linked to six120us motor steps.

BEGIN clears receive/fired counts and CRC. Each subsequent nozzle packet copies
exactly100 bytes without firing. Only after1180 packets and matching reflected
CRC32 does state becomeREADY. Incomplete, invalid data/CRC/token/period/reserved
bytes cannot ARM. PCTL packets cancel buffered state and retain legacy semantics.

Status is100 bytes transmitted on MISO, marker0x81, magic2..7, state8,error9,
received16 at12, fired16 at14, token32 at16, period32 at20, received CRC32 at24,
expected CRC32 at28, and status CRC32 over bytes0..95 at96. BK clocks it with a
100-byte read and the existing LOW-ack-HIGH packet handshake. Dummy read data in
READY/DONE/ERROR is ignored. Initial IDLE dummy bytes follow the legacy unpowered
path, so status probing never enables the head. SPI transmit+read API in this SDK
is sequential, not full-duplex: the implementation deliberately uses a separate
status read instead of pretending transmit returns the same100 clocks.

## ARM and single start

At READY check200MHz SystemCoreClock and unused SysTick, explicitly enable trace
and DWT CYCCNT without resetting the shared counter, then verify it advances.
This must happen before ARM: head power-on initializes DWT too late for READY.
Clock failures log HCLK_INVALID, SYSTICK_BUSY or DWT_STOPPED and reject before firing. ARM powers
and resets the head to idle and completes all console output BEFORE reporting
ready. No nozzle fires before the start edge. After the ARM packet's release,
receive DMA is armed only for pre-start cancellation. A complete DMA packet has
priority over an empty SYNC start; cancellation never fires column0.

The next SYNC LOW with no DMA packet starts playback. Disable SPI RX/TX DMA,
drive READY LOW, timestamp DWT and start core SysTick. Column0 fires immediately;
columns1..1179 fire every requested period. No UART/SPI operations in the live
column callback. Existing TPH_FireOneColumn, zero stop/reset, pulse widths and
minimum refill behavior remain unchanged. Column audit includes the original
100-byte chronological row and CRC. Timing guard stops on>20us start lateness
or a callback consuming the period. No catch-up or repeated fires.

At completion or error disable SysTick, power OFF, emit audits and LINE_DONE,
then wait for SYNC HIGH before rearming100-byte status RX/TX and raising READY.
BK holds SYNC LOW through its full bounded traverse and releases after stopping;
it verifies DONE token/CRC/1180 fires before any further feed. An HC32 playback
error stops ink immediately and is reported after this bounded motion; no new
page movement is allowed after failure. Premature release cancels remaining
columns. ARM/release waits are bounded5s. Cached startup is blank until signaled.

## Resources and evidence

Cache data118000 bytes, one bank. main object BSS approximately119.6KB. The full
project GCC verification link, including8KiB stack and8KiB heap, uses about139KB
main SRAM versus188KiB available. The supplied MDK linker remains unchanged;
Keil target build/hardware waveform verification is still required. SysTick's
startup handler is weak; main supplies the strong cached-line handler. Do not
configure utility SysTick while using buffered playback; busy/dead clocks are
rejected at READY before ARM.

BK and HC32 do not share a clock. Their crystal error and IRQ latency still need
row-start/end registration checks. READY/fired counters prove software calls,
not physical ink. Timing/CRC failures do not silently fall back to legacy firing.

Tests run the real state machine and playback callback under ASan/UBSan: exact
row/CRC, incomplete arm, bad token/period/data/reserved bytes, pre-start cancel,
1180 deadlines, early cancel, late IRQ, timeout, DWT wrap, busy SysTick and stopped
DWT. The legacy scripted DMA handshake and original zero-stop/audit tests pass.
BK tests also simulate a stalled foreground worker while both row clocks continue.
