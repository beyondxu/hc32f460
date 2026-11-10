"""Model S-latch behavior around the actual production column/power functions.
This verifies sequencing, NOT physical nanosecond pulse widths or ink behavior.
"""
from pathlib import Path
import subprocess,tempfile,re,os
src=(Path(__file__).resolve().parents[1]/'source/tph_m0333n.c').read_bytes().decode('latin1')
a=src.index('static __attribute__((always_inline)) inline void TPH_Clear_Latches_CE_Low')
b=src.index('static __attribute__((always_inline)) inline void TPH_Clear_Address',a)
clear=src[a:b]
a=src.index('void TPH_M0333N_Power(');b=src.index('void TPH_FireOneColumn(',a);power=src[a:b]
a=b;b=src.index('/* ========================================================================== */',a);column=src[a:b]
# Model the actual prepared register stores, retaining the real LUT generation.
prepared = 'CM_GPIO->PORRA = w0 >> 16;' in column
lookup = ''
if prepared:
 a=src.index('static const uint8_t s_u8AddrLut[25]');b=src.index('};',a)+2
 lut=src[a:b]
 a=src.index('void TPH_Build_Fast_LUT(void)');b=src.index('static __attribute__((always_inline)) inline void FAST_SET_ADDRESS',a)
 lookup=lut+'\n'+src[a:b].replace('void TPH_Build_Fast_LUT(void)', 'static void TPH_Build_Fast_LUT_actual(void)')
 column,n=re.subn(r'CM_GPIO->PORRA = ([^;]+);',r'host_clear_data(\1);',column);assert n==4
 column,n=re.subn(r'CM_GPIO->POSRA = ([^;]+);',r'host_set_data(\1);',column);assert n==4

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
static uint8_t data,latches[4],low[4],saw_sample[4],observed[100],expected[100];
#define ADDR_PORTB_MASK 0x7407U
#define DATA_PORTA_MASK 0x050FU
static uint16_t s_AddrPortB_Set[25],s_AddrPortB_Clr[25],s_DataPortA_Set[64],s_DataPortA_Clr[64];
static uint32_t physical_pins=0x8000U;
static void check_active(void){
 if(!ce)return;
 assert(irq==1&&address>=0&&address<25);
 for(unsigned sec=0;sec<4;sec++){
  assert(!(latches[sec]&~expected[address*4+sec]));
  observed[address*4+sec]|=latches[sec];
 }
}
static void ce_low(void){ce=0;}
static void ce_high(void){
 assert(!ce&&irq==1);
 for(unsigned sec=0;sec<4;sec++)assert(!latches[sec]);
 ce=1;++highs;check_active();
}
#define CE_LOW() ce_low()
#define CE_HIGH() ce_high()
static void fast_data(uint8_t d){assert(!(d&0xC0));data=d;check_active();}
static void fast_address(unsigned a){assert(!ce&&a<25);for(unsigned i=0;i<4;i++)assert(!latches[i]);address=a;}
#define FAST_SET_DATA(d) fast_data(d)
static void host_apply_data(void){static const unsigned pin[6]={0,2,1,10,8,3};unsigned value=0;for(unsigned i=0;i<6;++i)value|=((physical_pins>>pin[i])&1U)<<i;assert(physical_pins&0x8000U);fast_data(value);}
static void host_clear_data(uint32_t bits){assert(!(bits&~DATA_PORTA_MASK));physical_pins&=~bits;host_apply_data();}
static void host_set_data(uint32_t bits){assert(!(bits&~DATA_PORTA_MASK));physical_pins|=bits;host_apply_data();}
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
LOOKUP
static void TPH_Build_Fast_LUT(void){if(!s_bLutInitialized)++builds;TPH_Build_Fast_LUT_actual();}
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
POWER
COLUMN
static void run(unsigned saved){
 memset(observed,0,100);highs=restores=zero_latches=0;irq=saved;
 memset(latches,0x3F,4); /* emulate retained/undefined previous data */
 uint8_t original[100];memcpy(original,expected,100);
 TPH_FireOneColumn(expected);assert(!memcmp(original,expected,100));
 assert(highs==25&&restores==25&&irq==saved&&!ce&&address==-1);
 assert(!memcmp(observed,expected,100));
 for(unsigned i=0;i<4;i++)assert(!latches[i]&&!low[i]);
 assert(zero_latches>=26*4);
}
int main(void){
 expected[73]=2;expected[46]=8;expected[15]=32;expected[99]=1;
 run(0);run(1);assert(builds==1);
 for(unsigned bit=0;bit<6;++bit){memset(expected,0,100);for(unsigned i=0;i<100;++i)expected[i]=1U<<bit;run(bit&1U);}
 for(unsigned i=0;i<100;++i){expected[i]=(uint8_t)(i%64U);}
 run(0);
 memset(expected,0,100);run(0);
 memset(latches,0x3F,4);TPH_FireOneColumn(NULL);assert(!ce&&address==-1&&irq==0);
 memset(latches,0x3F,4);TPH_M0333N_Power(ENABLE);assert(vl&&vp&&!ce);
 memset(latches,0x3F,4);TPH_M0333N_Power(0);assert(!vl&&!vp&&!ce);
 puts("PASS: no retained bits at CE/address enable, actual 100-byte mapping, zero/NULL cleanup, IRQ mask restoration, power ordering");
}
'''.replace('LOOKUP',lookup).replace('CLEAR',clear).replace('POWER',power).replace('COLUMN',column)
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp);(p/'test.c').write_text(h)
 subprocess.run(['cc','-std=c99','-fsanitize=address,undefined','-fno-omit-frame-pointer','-Wall','-Wextra','-Werror',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
