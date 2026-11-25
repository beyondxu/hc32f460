#ifndef PRINTER_LINE_PROTOCOL_H
#define PRINTER_LINE_PROTOCOL_H
#include <stdint.h>
#include <string.h>
#define PL_BYTES 100U
#define PL_COLUMNS 1180U
#define PL_ROW_BYTES (PL_COLUMNS*PL_BYTES)
#define PL_TAG 0x80U
#define PL_BEGIN 0x10U
#define PL_ARM 0x11U
#define PL_CANCEL 0x12U
#define PL_BEGIN_PRESPRAY 0x13U
#define PL_FIRE_NORMAL 0U
#define PL_FIRE_PRESPRAY 1U
#define PL_CAP_PRESPRAY_FIRE 1U
#define PL_CAP_PRESPRAY_FAST 2U
#define PL_PRESPRAY_PERIOD_US 60U
#define PL_PRESPRAY_SHORT_ADDRESSES 13U
#define PL_PRESPRAY_BURST_MAX 8U
#define PL_PRESPRAY_PAUSE_MS 5U
#define PL_PRESPRAY_PAD_NOPS 32U
/* V29 speed trial: retain the playback20us deadline/overrun guard. */
#define PL_MIN_PERIOD_US 500U
#define PL_MAX_PERIOD_US 3000U
#define PL_MAGIC "LBUFv1"
enum {PL_IDLE,PL_LOADING,PL_READY,PL_ARMED,PL_RUNNING,PL_DONE,PL_ERROR};
enum {PL_OK,PL_BAD_COMMAND,PL_BAD_DATA,PL_BAD_CRC,PL_BAD_STATE,PL_CANCELED,PL_TIMEOUT,PL_TIMING};
static inline uint32_t pl_get32(const uint8_t*p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static inline uint32_t pl_get16(const uint8_t*p){return (uint32_t)p[0]|(uint32_t)p[1]<<8;}
static inline void pl_put32(uint8_t*p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
static inline void pl_put16(uint8_t*p,uint32_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
static inline uint32_t pl_crc_update(uint32_t crc,const uint8_t*p,uint32_t n){while(n--){crc^=*p++;for(unsigned b=0;b<8;b++)crc=(crc>>1)^(0xEDB88320U&(0U-(crc&1U)));}return crc;}
static inline uint32_t pl_crc(const uint8_t*p,uint32_t n){return pl_crc_update(0xFFFFFFFFU,p,n)^0xFFFFFFFFU;}
static inline void pl_command(uint8_t*p,unsigned op,uint32_t token,uint32_t period,uint32_t crc){memset(p,0,PL_BYTES);p[0]=PL_TAG;p[1]=(uint8_t)op;memcpy(p+2,PL_MAGIC,6);pl_put32(p+8,token);pl_put16(p+12,PL_COLUMNS);pl_put32(p+16,period);pl_put32(p+20,crc);}
static inline int pl_status_valid(const uint8_t*p){return p[0]==0x81U&&!memcmp(p+2,PL_MAGIC,6)&&pl_get32(p+96)==pl_crc(p,96);}
#endif
