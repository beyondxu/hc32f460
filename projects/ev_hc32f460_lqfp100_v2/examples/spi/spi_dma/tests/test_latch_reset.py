"""Model S-latch behavior around the actual production column/power functions.
This verifies sequencing, NOT physical nanosecond pulse widths or ink behavior.
"""
from pathlib import Path
import subprocess,tempfile
src=(Path(__file__).resolve().parents[1]/'source/tph_m0333n.c').read_bytes().decode('latin1')
a=src.index('static __attribute__((always_inline)) inline void TPH_Clear_Latches_CE_Low')
b=src.index('static __attribute__((always_inline)) inline void TPH_Clear_Address',a)
clear=src[a:b]
a=src.index('void TPH_M0333N_Power(');b=src.index('void TPH_FireOneColumn(',a);power=src[a:b]
a=b;b=src.index('/* ========================================================================== */',a);column=src[a:b]
a=src.index('static __attribute__((always_inline)) inline void TPH_Pulse_Section')
b=src.index('/* CE gates heating',a)
pulse=src[a:b].replace('DWT->CYCCNT','fake_cycles()')
h=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef int en_functional_state_t;
#define ENABLE 1
#define PIN_VL_EN_PORT 1
#define PIN_VL_EN_PIN 6
#define PIN_TPH_EN_PORT 1
#define PIN_TPH_EN_PIN 5
static bool s_bLutInitialized;
static unsigned ce,irq,highs,restores,builds,zero_latches,vl,vp;
static int address=-1;
static uint32_t ticks, pulse_start, s_u32FireCycles=400;
static bool s_bPulseClockReady=true;
static unsigned pulse_width, pulse_count;
static uint32_t fake_cycles(void){return ticks++;}
static uint8_t data,latches[4],low[4],saw_sample[4],observed[100],expected[100];
static void check_active(void){
 if(!ce)return;
 assert(irq==1&&address>=0&&address<25);
 for(unsigned sec=0;sec<4;sec++){
  assert(!(latches[sec]&~expected[address*4+sec]));
  observed[address*4+sec]|=latches[sec];
 }
}
static void ce_low(void){
 if(ce){unsigned elapsed=ticks-pulse_start;
 if(pulse_count)assert(elapsed==pulse_width);
 pulse_width=elapsed;++pulse_count;assert(elapsed==400);}
 ce=0;
}
static void ce_high(void){
 assert(!ce&&irq==1);
 unsigned active=0;
 for(unsigned sec=0;sec<4;sec++)if(latches[sec])++active;
 assert(active==1);pulse_start=ticks;
 ce=1;++highs;check_active();
}
#define CE_LOW() ce_low()
#define CE_HIGH() ce_high()
static void fast_data(uint8_t d){assert(!(d&0xC0));data=d;check_active();}
static void fast_address(unsigned a){assert(!ce&&a<25);for(unsigned i=0;i<4;i++)assert(!latches[i]);address=a;}
#define FAST_SET_DATA(d) fast_data(d)
#define FAST_SET_ADDRESS(a) fast_address(a)
static void strobe(unsigned sec,unsigned high){
 if(!high){assert(!low[sec]);low[sec]=1;saw_sample[sec]=0;}
 else {assert(low[sec]&&saw_sample[sec]);low[sec]=0;latches[sec]=data;if(!ce&&!data)++zero_latches;check_active();}
}
#define S1_LOW() strobe(0,0)
#define S1_HIGH() strobe(0,1)
#define S2_LOW() strobe(1,0)
#define S2_HIGH() strobe(1,1)
#define S3_LOW() strobe(2,0)
#define S3_HIGH() strobe(2,1)
#define S4_LOW() strobe(3,0)
#define S4_HIGH() strobe(3,1)
static void delay_100ns(void){check_active();}
static void delay_40ns(void){check_active();}
static void delay_ts_400ns(void){for(unsigned i=0;i<4;i++)if(low[i])saw_sample[i]=1;check_active();}
static void delay_600ns(void){assert(!ce);}
static void delay_10us(void){assert(!ce);}
static unsigned __get_PRIMASK(void){return irq;}
static void __disable_irq(void){irq=1;}
static void __DMB(void){}
static void __DSB(void){}
static void __set_PRIMASK(unsigned saved){assert(!ce);irq=saved;++restores;}
static void TPH_Build_Fast_LUT(void){s_bLutInitialized=true;++builds;}
static void TPH_Clear_Address(void){assert(!ce);for(unsigned i=0;i<4;i++)assert(!latches[i]);address=-1;}
static void GPIO_SetPins(unsigned port,unsigned pin){
 assert(port==1&&!ce);
 if(pin==6)vl=1;else{assert(pin==5&&vl);for(unsigned i=0;i<4;i++)assert(!latches[i]);vp=1;}
}
static void GPIO_ResetPins(unsigned port,unsigned pin){
 assert(port==1&&!ce);for(unsigned i=0;i<4;i++)assert(!latches[i]);
 if(pin==5)vp=0;else{assert(pin==6&&!vp);vl=0;}
}
CLEAR
PULSE
POWER
COLUMN
static void run(unsigned saved){
 memset(observed,0,100);highs=restores=zero_latches=pulse_count=0;irq=saved;
 memset(latches,0x3F,4); /* emulate retained/undefined previous data */
 TPH_FireOneColumn(expected);
 unsigned nonzero=0;for(unsigned i=0;i<100;i++)if(expected[i])++nonzero;
 assert(highs==nonzero&&restores==nonzero&&irq==saved&&!ce&&address==-1);
 assert(!memcmp(observed,expected,100));
 for(unsigned i=0;i<4;i++)assert(!latches[i]&&!low[i]);
 assert(zero_latches>=(nonzero+2)*4);
}
int main(void){
 expected[73]=2;expected[46]=8;expected[15]=32;expected[99]=1;
 run(0);ticks=0xFFFFFF00U;run(1);assert(builds==1);
 for(unsigned i=0;i<100;i++)expected[i]=(i%63)+1;
 run(0); /* all addresses/sections and all six domains */
 s_bPulseClockReady=false;memset(observed,0,100);highs=0;
 TPH_FireOneColumn(expected);assert(!highs&&!ce&&address==-1);
 TPH_M0333N_Power(ENABLE);assert(!vl&&!vp&&!ce);
 s_bPulseClockReady=true;
 memset(expected,0,100);run(0);
 memset(latches,0x3F,4);TPH_FireOneColumn(NULL);assert(!ce&&address==-1&&irq==0);
 memset(latches,0x3F,4);TPH_M0333N_Power(ENABLE);assert(vl&&vp&&!ce);
 memset(latches,0x3F,4);TPH_M0333N_Power(0);assert(!vl&&!vp&&!ce);
 puts("PASS: isolated section masks, equal modeled cycle windows, counter wrap, actual 100-byte mapping, empty/NULL/clock failure, IRQ restoration, power ordering");
}
'''.replace('CLEAR',clear).replace('PULSE',pulse).replace('POWER',power).replace('COLUMN',column)
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp);(p/'test.c').write_text(h)
 subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
