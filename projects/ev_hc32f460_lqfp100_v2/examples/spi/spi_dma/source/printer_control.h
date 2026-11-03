#ifndef PRINTER_CONTROL_H
#define PRINTER_CONTROL_H
/* Reserved upper bits are forbidden in nozzle columns. Both MCUs must use v1. */
#define PRINTER_CONTROL_TAG 0x80U
#define PRINTER_CONTROL_ON 1U
#define PRINTER_CONTROL_OFF 2U
#define PRINTER_CONTROL_MAGIC "PCTLv1"
#define PRINTER_CONTROL_MAGIC_LEN 6U
#endif
