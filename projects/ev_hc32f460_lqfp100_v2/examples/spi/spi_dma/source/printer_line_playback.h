#ifndef PRINTER_LINE_PLAYBACK_H
#define PRINTER_LINE_PLAYBACK_H
#include "printer_line_cache.h"
static printer_line_cache_t s_line;
static printer_row_audit_t s_line_audit;
static volatile uint32_t s_line_epoch,s_line_period_cycles,s_line_max_late,s_line_max_fire;
static void HC32_LineStop(unsigned error){SysTick->CTRL=0U;SCB->ICSR=SCB_ICSR_PENDSTCLR_Msk;if(error)pl_fail(&s_line,error);else s_line.state=PL_DONE;}
static void HC32_LineFire(void){
 if(s_line.state!=PL_RUNNING)return;
 uint32_t start=DWT->CYCCNT;
 uint32_t expected=(uint32_t)s_line.fired*s_line_period_cycles;
 if((uint32_t)(start-s_line_epoch)>expected+(SystemCoreClock/1000000U)*20U){HC32_LineStop(PL_TIMING);return;}
 uint32_t late=(uint32_t)(start-s_line_epoch)-expected;
 if(late>s_line_max_late)s_line_max_late=late;
 const uint8_t*p=s_line.data+(uint32_t)s_line.fired*PL_BYTES;
 TPH_FireOneColumn(p);
 unsigned bits=0;for(unsigned i=0;i<PL_BYTES;i++)for(unsigned b=0;b<6;b++)bits+=(p[i]>>b)&1U;
 printer_row_audit_column(&s_line_audit,p,bits,1U);
 uint32_t duration=(uint32_t)(DWT->CYCCNT-start);
 if(duration>s_line_max_fire)s_line_max_fire=duration;
 ++s_line.fired;
 if(s_line.fired==PL_COLUMNS)HC32_LineStop(PL_OK);
 else if((uint32_t)(DWT->CYCCNT-start)>=s_line_period_cycles-(SystemCoreClock/1000000U)*20U)HC32_LineStop(PL_TIMING);
}
void SysTick_Handler(void){HC32_LineFire();}
static int HC32_LineClockReady(void){
 if(SystemCoreClock!=200000000U){printf("[HC32 LINE_CLOCK] HCLK_INVALID\r\n");return 0;}
 if(SysTick->CTRL&SysTick_CTRL_ENABLE_Msk){printf("[HC32 LINE_CLOCK] SYSTICK_BUSY\r\n");return 0;}
 /* READY is checked before ARM/Power(ENABLE). Start the cycle clock here;
  * never reset CYCCNT, which is also used by the head idle-time guard. */
 CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
 DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
 __DSB();__ISB();
 uint32_t before=DWT->CYCCNT;
 for(unsigned i=0;i<1000U;i++){__NOP();if(DWT->CYCCNT!=before)return 1;}
 printf("[HC32 LINE_CLOCK] DWT_STOPPED\r\n");return 0;
}
static int HC32_LinePacket(const uint8_t*p,uint8_t*head,printer_row_audit_t*legacy){
 if(!pl_receive(&s_line,p))return 0;
 if(*head){TPH_M0333N_Power(DISABLE);printer_row_audit_end(legacy);*head=0U;}
 if(s_line.state==PL_READY && !HC32_LineClockReady())pl_fail(&s_line,PL_TIMING);
 if(s_line.state==PL_ARMED){
  s_line_max_late=0;s_line_max_fire=0;
  TPH_M0333N_Power(ENABLE); /* settle and printf BEFORE the start edge */
  printer_row_audit_begin(&s_line_audit);
  printf("[HC32 LINE_ARMED] token=%lu columns=%u period_us=%lu crc32=%08lX no_fires_until_SYNC\r\n",(unsigned long)s_line.token,PL_COLUMNS,(unsigned long)s_line.period_us,(unsigned long)s_line.expected_crc);
 }
 return 1;
}
/* Called with a100-byte cancel RX already armed. READY remains LOW during the
 * whole run and until SYNC is released, then the caller rearms status RX/TX. */
static void HC32_RunBufferedLine(void){
 uint32_t began=DWT->CYCCNT;
 while(s_line.state==PL_ARMED){
  if(enDmaRxDone==SET){pl_fail(&s_line,PL_CANCELED);break;} /* cancel before start, no ink */
  if(GPIO_ReadInputPins(SYNC_PORT,SYNC_PIN)==PIN_RESET)break;
  if((uint32_t)(DWT->CYCCNT-began)>SystemCoreClock*5U){pl_fail(&s_line,PL_TIMEOUT);break;}
 }
 GPIO_ResetPins(READY_PORT,READY_PIN);
 DMA_ChCmd(DMA_UNIT,DMA_RX_CH,DISABLE);DMA_ChCmd(DMA_UNIT,DMA_TX_CH,DISABLE);
 if(s_line.state==PL_ARMED){
  if(SystemCoreClock!=200000000U || (SysTick->CTRL&SysTick_CTRL_ENABLE_Msk))pl_fail(&s_line,PL_TIMING);
  else {
   s_line_period_cycles=(SystemCoreClock/1000000U)*s_line.period_us;
   s_line.fired=0;s_line_max_late=0;s_line_max_fire=0;__DMB();s_line.state=PL_RUNNING;
   SysTick->CTRL=0;SysTick->LOAD=s_line_period_cycles-1U;SysTick->VAL=0;
   SCB->ICSR=SCB_ICSR_PENDSTCLR_Msk;
   NVIC_SetPriority(SysTick_IRQn,0U);
   s_line_epoch=DWT->CYCCNT;
   SysTick->CTRL=SysTick_CTRL_CLKSOURCE_Msk|SysTick_CTRL_TICKINT_Msk|SysTick_CTRL_ENABLE_Msk;
   HC32_LineFire(); /* column0; subsequent columns from hardware SysTick */
   while(s_line.state==PL_RUNNING){
    if(GPIO_ReadInputPins(SYNC_PORT,SYNC_PIN)==PIN_SET){HC32_LineStop(PL_CANCELED);break;}
    if((uint32_t)(DWT->CYCCNT-s_line_epoch)>(PL_COLUMNS+1U)*s_line_period_cycles){HC32_LineStop(PL_TIMEOUT);break;}
   }
  }
 }
 SysTick->CTRL=0;SCB->ICSR=SCB_ICSR_PENDSTCLR_Msk;
 TPH_M0333N_Power(DISABLE);printer_row_audit_end(&s_line_audit);
 printf("[HC32 LINE_DONE] token=%lu state=%u error=%u fired=%u/%u period_us=%lu crc32=%08lX max_late_cycles=%lu max_fire_cycles=%lu\r\n",(unsigned long)s_line.token,s_line.state,s_line.error,s_line.fired,PL_COLUMNS,(unsigned long)s_line.period_us,(unsigned long)s_line.expected_crc,(unsigned long)s_line_max_late,(unsigned long)s_line_max_fire);
 began=DWT->CYCCNT;
 while(GPIO_ReadInputPins(SYNC_PORT,SYNC_PIN)==PIN_RESET){if((uint32_t)(DWT->CYCCNT-began)>SystemCoreClock*5U){pl_fail(&s_line,PL_TIMEOUT);break;}}
}
#endif
