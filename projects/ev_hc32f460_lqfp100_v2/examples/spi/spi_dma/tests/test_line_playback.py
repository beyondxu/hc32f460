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
static struct{volatile uint32_t CYCCNT;}dwt;
#define SysTick (&tick)
#define SCB (&scb)
#define DWT (&dwt)
#define __NOP() do{if(mode!=7)++dwt.CYCCNT;}while(0)
static uint32_t SystemCoreClock=200000000U;
static unsigned enDmaRxDone,head,actual_fires,mode;
typedef struct{unsigned columns;}printer_row_audit_t;
static void printer_row_audit_begin(printer_row_audit_t*a){a->columns=0;}
static void printer_row_audit_end(printer_row_audit_t*a){(void)a;}
static void printer_row_audit_column(printer_row_audit_t*a,const uint8_t*p,unsigned b,unsigned v){(void)p;(void)b;assert(v);++a->columns;}
static unsigned GPIO_ReadInputPins(unsigned,unsigned);
static void GPIO_ResetPins(unsigned p,unsigned pin){(void)p;(void)pin;}
static void DMA_ChCmd(unsigned a,unsigned b,unsigned c){(void)a;(void)b;assert(!c);}
static void NVIC_SetPriority(unsigned a,unsigned b){(void)a;assert(!b);}
static void TPH_M0333N_Power(unsigned on){head=on;}
static void TPH_FireOneColumn(const uint8_t*p){assert(head);for(unsigned i=0;i<100;i++)assert(p[i]==((actual_fires+i)&63U));++actual_fires;DWT->CYCCNT+=30000U;}
#include "printer_line_playback.h"
static unsigned GPIO_ReadInputPins(unsigned p,unsigned pin){
 (void)p;(void)pin;
 if(s_line.state==PL_ARMED){if(mode==3){DWT->CYCCNT+=200000000U;return PIN_SET;}return PIN_RESET;}
 if(s_line.state==PL_RUNNING){
  if(mode==1&&actual_fires==10)return PIN_SET;
  DWT->CYCCNT=s_line_epoch+(uint32_t)s_line.fired*s_line_period_cycles;
  if(mode==2&&actual_fires==10)DWT->CYCCNT+=5000U;
  SysTick_Handler();return PIN_RESET;
 }
 return PIN_SET;
}
int main(int argc,char**argv){
 assert(argc==2);mode=atoi(argv[1]);if(mode==5)DWT->CYCCNT=UINT32_MAX-100000U;uint8_t p[100];uint8_t*row=malloc(PL_ROW_BYTES);
 for(unsigned c=0;c<1180;c++)for(unsigned i=0;i<100;i++)row[c*100+i]=(c+i)&63U;
 if(mode==6)SysTick->CTRL=SysTick_CTRL_ENABLE_Msk;
 uint8_t old_head=0;printer_row_audit_t legacy={0};uint32_t crc=pl_crc(row,PL_ROW_BYTES);
 pl_command(p,PL_BEGIN,1,720,crc);assert(HC32_LinePacket(p,&old_head,&legacy));
 for(unsigned c=0;c<1180;c++)assert(HC32_LinePacket(row+c*100,&old_head,&legacy));
 if(mode>=6){assert(s_line.state==PL_ERROR&&s_line.error==PL_TIMING&&!actual_fires&&!head);assert(SysTick->CTRL==(mode==6?1U:0U));free(row);puts("PASS busy SysTick/dead DWT rejected before ARM or firing");return 0;}
 assert(s_line.state==PL_READY&&!actual_fires&&!head);
 pl_command(p,PL_ARM,1,720,crc);assert(HC32_LinePacket(p,&old_head,&legacy));assert(s_line.state==PL_ARMED&&head&&!actual_fires);
 if(mode==4)enDmaRxDone=SET;
 HC32_RunBufferedLine();assert(!head&&!SysTick->CTRL);
 if(!mode||mode==5)assert(s_line.state==PL_DONE&&s_line.fired==1180&&actual_fires==1180&&s_line_audit.columns==1180);
 if(mode==1)assert(s_line.state==PL_ERROR&&s_line.error==PL_CANCELED&&actual_fires==10);
 if(mode==2)assert(s_line.state==PL_ERROR&&s_line.error==PL_TIMING&&actual_fires==10);
 if(mode==3)assert(s_line.state==PL_ERROR&&s_line.error==PL_TIMEOUT&&!actual_fires);
 if(mode==4)assert(s_line.state==PL_ERROR&&s_line.error==PL_CANCELED&&!actual_fires);
 unsigned old=actual_fires;SysTick_Handler();assert(old==actual_fires);free(row);
 puts("PASS real SysTick playback, fixed deadlines, no pre-start fires, cancellation/lateness/timeout, bounded count and power OFF");
}
'''
with tempfile.TemporaryDirectory() as t:
 p=Path(t);(p/'test.c').write_text(h)
 subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(S),str(p/'test.c'),'-o',str(p/'test')],check=True)
 for mode in range(8):subprocess.run([str(p/'test'),str(mode)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
