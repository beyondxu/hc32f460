#ifndef PRINTER_LINE_CACHE_H
#define PRINTER_LINE_CACHE_H
#include "printer_line_protocol.h"
typedef struct {uint8_t data[PL_ROW_BYTES];uint32_t token,period_us,expected_crc,crc;uint16_t received;volatile uint16_t fired;volatile uint8_t state,error,fire_profile;} printer_line_cache_t;
static inline void pl_fail(printer_line_cache_t*c,unsigned error){c->fire_profile=PL_FIRE_NORMAL;c->error=(uint8_t)error;c->state=PL_ERROR;}
/* Returns1 for buffered protocol packets; legacy PCTL/nozzle packets stay intact. */
static inline int pl_receive(printer_line_cache_t*c,const uint8_t*p){
 if(p[0]==PL_TAG){
  if(memcmp(p+2,PL_MAGIC,6)){c->state=PL_IDLE;c->error=PL_OK;return 0;}
  unsigned op=p[1];int valid=pl_get16(p+12)==PL_COLUMNS&&p[14]==0&&p[15]==0;
  for(unsigned i=24;i<PL_BYTES;i++)if(p[i])valid=0;
  uint32_t token=pl_get32(p+8),period=pl_get32(p+16),crc=pl_get32(p+20);
  /* Only explicit prespray BEGIN, or its ARM/CANCEL, may use fast periods. */
  unsigned fast=op==PL_BEGIN_PRESPRAY||((op==PL_ARM||op==PL_CANCEL)&&c->fire_profile==PL_FIRE_PRESPRAY);
  uint32_t min_period=fast?PL_PRESPRAY_PERIOD_US:PL_MIN_PERIOD_US;
  if(!valid||!token||period<min_period||period>PL_MAX_PERIOD_US){pl_fail(c,PL_BAD_COMMAND);return 1;}
  if(op==PL_BEGIN||op==PL_BEGIN_PRESPRAY){c->fire_profile=(uint8_t)(op==PL_BEGIN_PRESPRAY?PL_FIRE_PRESPRAY:PL_FIRE_NORMAL);c->token=token;c->period_us=period;c->expected_crc=crc;c->crc=0xFFFFFFFFU;c->received=c->fired=0;c->error=PL_OK;c->state=PL_LOADING;}
  else if(op==PL_ARM){if(c->state!=PL_READY||token!=c->token||period!=c->period_us||crc!=c->expected_crc)pl_fail(c,PL_BAD_STATE);else c->state=PL_ARMED;}
  else if(op==PL_CANCEL)pl_fail(c,PL_CANCELED);
  else pl_fail(c,PL_BAD_COMMAND);
  return 1;
 }
 if(c->state==PL_LOADING){
  if(c->received>=PL_COLUMNS){pl_fail(c,PL_BAD_STATE);return 1;}
  for(unsigned i=0;i<PL_BYTES;i++)if(p[i]&0xC0U){pl_fail(c,PL_BAD_DATA);return 1;}
  memcpy(c->data+(uint32_t)c->received*PL_BYTES,p,PL_BYTES);
  c->crc=pl_crc_update(c->crc,p,PL_BYTES);++c->received;
  if(c->received==PL_COLUMNS){if((c->crc^0xFFFFFFFFU)!=c->expected_crc)pl_fail(c,PL_BAD_CRC);else c->state=PL_READY;}
  return 1;
 }
 /* RX dummy bytes clocking a status reply must never turn into nozzle data. */
 return c->state!=PL_IDLE;
}
static inline void pl_status(const printer_line_cache_t*c,uint8_t*p){memset(p,0,PL_BYTES);p[0]=0x81U;memcpy(p+2,PL_MAGIC,6);p[8]=c->state;p[9]=c->error;p[10]=PL_CAP_PRESPRAY_FIRE|PL_CAP_PRESPRAY_FAST;p[11]=c->fire_profile;p[32]=PL_PRESPRAY_PAD_NOPS;pl_put16(p+33,PL_PRESPRAY_PERIOD_US);pl_put16(p+12,c->received);pl_put16(p+14,c->fired);pl_put32(p+16,c->token);pl_put32(p+20,c->period_us);pl_put32(p+24,c->crc^0xFFFFFFFFU);pl_put32(p+28,c->expected_crc);pl_put32(p+96,pl_crc(p,96));}
#endif
