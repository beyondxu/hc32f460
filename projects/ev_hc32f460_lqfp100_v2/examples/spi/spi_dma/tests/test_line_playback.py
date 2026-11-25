"""Execute actual SysTick line engine: 1180 fixed deadlines, cancel, lateness, timeout."""
from pathlib import Path
import subprocess,tempfile,os
S=Path(__file__).resolve().parents[1]/'source'
h=r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define PL_TEST 1
#define __DMB() ((void)0)
#include "printer_line_cache.h"
#define SET 1
#define PIN_RESET 0
#define PIN_SET 1
#define ENABLE 1
#define DISABLE 0
#define SYNC_PORT 0
#define SYNC_PIN 0
#define READY_PORT 0
#define READY_PIN 0
#define DMA_UNIT 0
#define DMA_RX_CH 1
#define DMA_TX_CH 0
#define SysTick_CTRL_ENABLE_Msk 1U
#define SysTick_CTRL_TICKINT_Msk 2U
#define SysTick_CTRL_CLKSOURCE_Msk 4U
#define SCB_ICSR_PENDSTCLR_Msk (1U<<25)
#define SysTick_IRQn 0
static struct{volatile uint32_t CTRL,LOAD,VAL;}tick;
static struct{volatile uint32_t ICSR;}scb;
static struct{volatile uint32_t CYCCNT,CTRL;}dwt;
static struct{volatile uint32_t DEMCR;}debug;
#define CoreDebug (&debug)
#define CoreDebug_DEMCR_TRCENA_Msk (1U<<24)
#define DWT_CTRL_CYCCNTENA_Msk 1U
#define __DSB() ((void)0)
#define __ISB() ((void)0)
#define SysTick (&tick)
#define SCB (&scb)
#define DWT (&dwt)
#define __NOP() do{if(mode!=7 && (debug.DEMCR&CoreDebug_DEMCR_TRCENA_Msk) && (dwt.CTRL&DWT_CTRL_CYCCNTENA_Msk))++dwt.CYCCNT;}while(0)
static uint32_t SystemCoreClock=200000000U;
static unsigned enDmaRxDone,head,actual_fires,mode,normal_calls,prespray_calls;
static uint32_t trial_period,train_starts[8];
static unsigned short_calls, short_cancel, short_primed, in_tick;
typedef struct{unsigned columns;}printer_row_audit_t;
static void printer_row_audit_begin(printer_row_audit_t*a){a->columns=0;}
static void printer_row_audit_end(printer_row_audit_t*a){(void)a;}
static void printer_row_audit_column(printer_row_audit_t*a,const uint8_t*p,unsigned b,unsigned v){(void)p;(void)b;assert(v);++a->columns;}
static unsigned GPIO_ReadInputPins(unsigned,unsigned);
static void GPIO_ResetPins(unsigned p,unsigned pin){(void)p;(void)pin;}
static void DMA_ChCmd(unsigned a,unsigned b,unsigned c){(void)a;(void)b;assert(!c);}
static void NVIC_SetPriority(unsigned a,unsigned b){(void)a;assert(!b);}
static void TPH_M0333N_Power(unsigned on){head=on;}
static void TPH_FireOneColumn(const uint8_t*p){if(!prespray_calls)++normal_calls;assert(head);for(unsigned i=0;i<100;i++)assert(p[i]==((actual_fires+i)&63U));++actual_fires;DWT->CYCCNT+=mode==10?(trial_period-20U)*200U:mode==11?(trial_period-21U)*200U:30000U;}
static void TPH_FireOneColumn_Prespray(const uint8_t*p){++prespray_calls;TPH_FireOneColumn(p);}
static int TPH_FirePresprayShort(const uint8_t*p,unsigned first,unsigned count,
                                uint32_t earliest,uint32_t latest,uint32_t*started){
 assert(head&&first==0U&&count==13U&&p[0]==1U&&p[48]==1U);
 uint32_t ready=DWT->CYCCNT+200U;
 if(short_calls==1U&&mode==20)ready+=1800U;
 if(short_calls==0U&&mode==19)ready+=1000U;
 if((int32_t)(ready-earliest)<0)ready=earliest;
 if((int32_t)(ready-latest)>0)return 0;
 if(mode<23U)train_starts[short_calls]=ready;
 ++short_calls;
 *started=ready;
 DWT->CYCCNT=ready+(mode==22?8000U:6000U);
 return 1;
}
#include "printer_line_playback.h"
static unsigned GPIO_ReadInputPins(unsigned p,unsigned pin){
 (void)p;(void)pin;
 if(s_line.state==PL_ARMED){if(mode==3){DWT->CYCCNT+=200000000U;return PIN_SET;}return PIN_RESET;}
 if(mode>=19&&mode<=22)return short_cancel?PIN_SET:PIN_RESET;
 if(mode==23U&&s_line.state==PL_RUNNING){
  if(in_tick||!short_primed){short_primed=1U;return PIN_RESET;}
  DWT->CYCCNT=s_line_epoch+(uint32_t)s_line.fired*s_line_period_cycles;
  in_tick=1U;SysTick_Handler();in_tick=0U;return PIN_RESET;
 }
 if(s_line.state==PL_RUNNING){
  if(mode==1&&actual_fires==10)return PIN_SET;
  DWT->CYCCNT=s_line_epoch+(uint32_t)s_line.fired*s_line_period_cycles;
  if(mode==2&&actual_fires==10)DWT->CYCCNT+=5000U;
  SysTick_Handler();return PIN_RESET;
 }
 return PIN_SET;
}
int main(int argc,char**argv){
 assert(argc==2);unsigned requested=atoi(argv[1]);
 if(requested==23U){
  mode=requested;
  uint8_t p[100],old_head=0;
  printer_row_audit_t legacy={0};
  uint8_t *row=calloc(PL_ROW_BYTES,1U);assert(row);
  for(unsigned col=40;col<1000U;col+=16U)
   for(unsigned shot=0;shot<8U;++shot){
    uint8_t *data=row+(col+shot)*PL_BYTES;
    data[0]=data[48]=1U;
   }
  uint32_t crc=pl_crc(row,PL_ROW_BYTES);
  pl_command(p,PL_BEGIN_PRESPRAY,1U,58U,crc);
  assert(HC32_LinePacket(p,&old_head,&legacy));
  for(unsigned col=0;col<PL_COLUMNS;++col)
   assert(HC32_LinePacket(row+col*PL_BYTES,&old_head,&legacy));
  assert(s_line.state==PL_READY&&!head&&!short_calls);
  pl_command(p,PL_ARM,1U,58U,crc);assert(HC32_LinePacket(p,&old_head,&legacy));
  assert(s_line.state==PL_ARMED&&head&&!short_calls);
  HC32_RunBufferedLine();
  assert(!head&&!SysTick->CTRL&&s_line.state==PL_DONE&&s_line.error==PL_OK);
  assert(s_line.fired==PL_COLUMNS&&s_line_audit.columns==PL_COLUMNS&&short_calls==480U);
  assert(s_short_min_repeat==11600U&&s_short_max_repeat==11600U&&!normal_calls&&!prespray_calls);
  free(row);
  puts("PASS complete short-scan upload, ARM/SYNC, blank-slot gating, repeat timing and power OFF");
  return 0;
 }
 if(requested>=19){
  mode=requested;head=1;s_line.state=PL_RUNNING;
  s_line.fire_profile=PL_FIRE_PRESPRAY;s_line.period_us=58U;
  s_line_period_cycles=11600U;s_line_epoch=UINT32_MAX-30000U;
  for(unsigned j=0;j<8U;++j){
   uint8_t *p=s_line.data+j*PL_BYTES;
   p[0]=p[48]=1;s_line.received=j;assert(pl_short_column(&s_line,p));
  }
  for(unsigned j=0;j<8U&&s_line.state==PL_RUNNING;++j){
   short_cancel=j==1U&&mode==21;
   DWT->CYCCNT=s_line_epoch+j*11600U;
   SysTick_Handler();
  }
  if(mode==19){
   assert(short_calls==8U&&s_line.fired==8U);
   for(unsigned j=1;j<8U;++j)assert((uint32_t)(train_starts[j]-train_starts[j-1])==11600U);
  }else{
   assert(short_calls==1U&&s_line.state==PL_ERROR);
   assert(s_line.error==(mode==21?PL_CANCELED:PL_TIMING));
   if(mode==20)assert(!strcmp(s_short_timing_fault,"train_start"));
   if(mode==22)assert(!strcmp(s_short_timing_fault,"slot_overrun"));
  }
  return 0;
 }
 unsigned fast=requested>=13;mode=fast?(requested==13?12U:requested==14?10U:requested==15?11U:requested==16?2U:requested==17?1U:5U):requested;trial_period=mode>=8?(mode==9?500U:542U):720U;if(fast)trial_period=300U;if(mode==5)DWT->CYCCNT=UINT32_MAX-100000U;uint8_t p[100];uint8_t*row=malloc(PL_ROW_BYTES);
 for(unsigned c=0;c<1180;c++)for(unsigned i=0;i<100;i++)row[c*100+i]=(c+i)&63U;
 if(mode==6)SysTick->CTRL=SysTick_CTRL_ENABLE_Msk;
 uint8_t old_head=0;printer_row_audit_t legacy={0};uint32_t crc=pl_crc(row,PL_ROW_BYTES);
 pl_command(p,(fast||mode>=12)?PL_BEGIN_PRESPRAY:PL_BEGIN,1,trial_period,crc);assert(HC32_LinePacket(p,&old_head,&legacy));
 for(unsigned c=0;c<1180;c++)assert(HC32_LinePacket(row+c*100,&old_head,&legacy));
 if(mode==6||mode==7){assert(s_line.state==PL_ERROR&&s_line.error==PL_TIMING&&!actual_fires&&!head);assert(SysTick->CTRL==(mode==6?1U:0U));free(row);puts("PASS busy SysTick/dead DWT rejected before ARM or firing");return 0;}
 assert(s_line.state==PL_READY&&!actual_fires&&!head);
 assert((debug.DEMCR&CoreDebug_DEMCR_TRCENA_Msk)&&(dwt.CTRL&DWT_CTRL_CYCCNTENA_Msk));
 /* Fresh boot starts disabled. Readiness must enable, not reset, CYCCNT. */
 assert(DWT->CYCCNT==(mode==5?UINT32_MAX-99999U:1U));
 pl_command(p,PL_ARM,1,trial_period,crc);assert(HC32_LinePacket(p,&old_head,&legacy));assert(s_line.state==PL_ARMED&&head&&!actual_fires);
 if(mode==4)enDmaRxDone=SET;
 HC32_RunBufferedLine();assert(!head&&!SysTick->CTRL);
 if(!mode||mode==5||mode==8||mode==9||mode==11||mode>=12)assert(s_line.state==PL_DONE&&s_line.fired==1180&&actual_fires==1180&&s_line_audit.columns==1180);
 if(mode==1)assert(s_line.state==PL_ERROR&&s_line.error==PL_CANCELED&&actual_fires==10);
 if(mode==2)assert(s_line.state==PL_ERROR&&s_line.error==PL_TIMING&&actual_fires==10);
 if(mode==3)assert(s_line.state==PL_ERROR&&s_line.error==PL_TIMEOUT&&!actual_fires);
 if(mode==4)assert(s_line.state==PL_ERROR&&s_line.error==PL_CANCELED&&!actual_fires);
 if(mode==10)assert(s_line.state==PL_ERROR&&s_line.error==PL_TIMING&&actual_fires==1);
 if(mode>=12){
  assert(prespray_calls==1180&&!normal_calls&&s_line.fire_profile==PL_FIRE_PRESPRAY);
  if(fast)trial_period=673U;
  pl_command(p,PL_BEGIN,2,trial_period,crc);HC32_LinePacket(p,&old_head,&legacy);
  assert(s_line.fire_profile==PL_FIRE_NORMAL);
  for(unsigned c=0;c<1180;++c)HC32_LinePacket(row+c*100,&old_head,&legacy);
  pl_command(p,PL_ARM,2,trial_period,crc);HC32_LinePacket(p,&old_head,&legacy);
  actual_fires=0;prespray_calls=0;HC32_RunBufferedLine();
  assert(normal_calls==1180&&!prespray_calls&&s_line.state==PL_DONE&&!head);
 }
 unsigned old=actual_fires;SysTick_Handler();assert(old==actual_fires);free(row);
 puts("PASS real SysTick playback, fixed deadlines, no pre-start fires, cancellation/lateness/timeout, bounded count and power OFF");
}
'''
with tempfile.TemporaryDirectory() as t:
 p=Path(t);(p/'test.c').write_text(h)
 subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(S),str(p/'test.c'),'-o',str(p/'test')],check=True)
 for mode in range(24):subprocess.run([str(p/'test'),str(mode)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
