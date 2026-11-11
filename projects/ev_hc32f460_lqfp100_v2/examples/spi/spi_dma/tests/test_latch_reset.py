"""Run the actual continuous-CE driver against GPIO/clock/IRQ trace models.
Logical protocol checks only; host time is not a prediction of M4 nanoseconds.
"""
from pathlib import Path
import subprocess,tempfile,os,re
P=Path(__file__).resolve().parents[1]; S=P/'source'
src=(S/'tph_m0333n.c').read_bytes().decode('latin1')
a=src.index('static const uint8_t s_u8AddrLut[25]');b=src.index('};',a)+2
lut=src[a:b]
a=src.index('void TPH_Build_Fast_LUT(void)');b=src.index('/* TPH_CONTINUOUS_CE_v1',a)
lookup=src[a:b]
core=(S/'tph_continuous_ce.inc').read_text()
assert core in src
macros='\n'.join(re.findall(r'^#define (?:S[1-4]_(?:LOW|HIGH)|CE_(?:LOW|HIGH))\(\).*$',src,re.M))
h=r'''
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
#include <array>
using en_functional_state_t=int;
#define ENABLE 1
#define PIN_VL_EN_PORT 1
#define PIN_VL_EN_PIN 6
#define PIN_TPH_EN_PORT 1
#define PIN_TPH_EN_PIN 5
#define ADDR_PORTB_MASK 0x7407U
#define DATA_PORTA_MASK 0x050FU
static bool s_bLutInitialized;
static uint16_t s_AddrPortB_Set[25],s_AddrPortB_Clr[25],s_DataPortA_Set[64],s_DataPortA_Clr[64];
static uint32_t SystemCoreClock=200000000;
static uint32_t tick,irq;
static bool clock_frozen,vl,vp,ce;
static uint16_t pa=0x8000,pb,pc,ph;
static unsigned ce_edges,restores;
static bool sample_low[4];
static unsigned sample_a[4],sample_d[4];
static uint32_t sample_t[4];
struct Sample {unsigned a,d,s;uint32_t time;};
static std::vector<Sample> trace;
static std::vector<unsigned> power_events;
static unsigned address(){
 static const unsigned pin[7]={2,10,1,12,14,13,0};unsigned a=0;
 for(unsigned i=0;i<7;++i)a|=((pb>>pin[i])&1U)<<i;return a;
}
static unsigned data(){
 static const unsigned pin[6]={0,2,1,10,8,3};unsigned d=0;
 for(unsigned i=0;i<6;++i)d|=((pa>>pin[i])&1U)<<i;return d;
}
static unsigned s_state(unsigned s){return s==0?(pc>>15)&1U:s==1?(ph>>1)&1U:s==2?ph&1U:(pc>>14)&1U;}
static void check_stable(){if(!ce)return;for(unsigned s=0;s<4;++s)if(sample_low[s])assert(sample_a[s]==address()&&sample_d[s]==data());}
static void store(unsigned port,bool set,uint32_t value){
 check_stable();unsigned old_s[4];for(unsigned s=0;s<4;++s)old_s[s]=s_state(s);
 bool old_ce=ce;
 uint16_t *p=port==0?&pa:port==1?&pb:port==2?&pc:&ph;
 if(set)*p|=value;else *p&=~value;
 tick+=2;ce=(pb>>15)&1U;
 assert(pa&0x8000); // unrelated PA15 must survive every data update
 if(ce!=old_ce){++ce_edges;power_events.push_back(ce?3:4);if(ce){assert(vl&&vp);for(unsigned s=0;s<4;++s)assert(s_state(s));}else for(unsigned s=0;s<4;++s)sample_low[s]=false;}
 for(unsigned s=0;s<4;++s){
  if(!ce)continue;
  unsigned now=s_state(s);
  if(old_s[s]&&!now){assert(!sample_low[s]);sample_low[s]=true;sample_a[s]=address();sample_d[s]=data();sample_t[s]=tick;if(address())assert(irq==1);}
  if(!old_s[s]&&now&&sample_low[s]){assert(tick-sample_t[s]>=80);assert(address()==sample_a[s]&&data()==sample_d[s]);sample_low[s]=false;trace.push_back({address(),data(),s,sample_t[s]});}
 }
 check_stable();
}
struct Register {unsigned port;bool set;void operator=(uint32_t value){store(port,set,value);}};
struct Gpio {Register PORRA{0,false},POSRA{0,true},PORRB{1,false},POSRB{1,true},PORRC{2,false},POSRC{2,true},PORRH{3,false},POSRH{3,true};};
static Gpio gpio;
#define CM_GPIO (&gpio)
struct Counter {operator uint32_t() const {if(!clock_frozen)++tick;return tick;}};
struct Dwt {uint32_t CTRL=0;Counter CYCCNT;};static Dwt dwt;
#define DWT (&dwt)
struct Debug {uint32_t DEMCR=0;};static Debug debug;
#define CoreDebug (&debug)
#define CoreDebug_DEMCR_TRCENA_Msk 1U
#define DWT_CTRL_CYCCNTENA_Msk 1U
static void delay_ts_400ns(){tick+=80;check_stable();}
static void delay_10us(){tick+=2000;}
static uint32_t __get_PRIMASK(){return irq;}
static void __disable_irq(){irq=1;}
static void __set_PRIMASK(uint32_t saved){assert(!address()&&!data());for(unsigned s=0;s<4;++s)assert(!sample_low[s]&&s_state(s));irq=saved;++restores;}
static void __DMB(){}
static void __DSB(){}
static void __ISB(){}
static void GPIO_SetPins(unsigned port,unsigned pin){assert(port==1&&!ce);if(pin==6){vl=true;power_events.push_back(1);}else{assert(pin==5&&vl);vp=true;power_events.push_back(2);}}
static void GPIO_ResetPins(unsigned port,unsigned pin){assert(port==1&&!ce);if(pin==5){if(vp)power_events.push_back(5);vp=false;}else{assert(pin==6&&!vp);if(vl)power_events.push_back(6);vl=false;}}
LUT
LOOKUP
MACROS
CORE
static void run(const std::array<uint8_t,100>& column,unsigned saved){
 std::array<uint8_t,100> original=column;
 unsigned edges=ce_edges,old_restores=restores;trace.clear();irq=saved;
 TPH_FireOneColumn(column.data());
 assert(original==column&&ce&&vl&&vp&&!address()&&!data()&&irq==saved);
 assert(ce_edges==edges&&restores==old_restores+1);
 std::vector<Sample> live;
 for(auto sample:trace)if(sample.a)live.push_back(sample);
 assert(live.size()==26*4);
 for(unsigned add=0;add<26;++add)for(unsigned sec=0;sec<4;++sec){
  auto sample=live[add*4+sec];assert(sample.a==s_u8AddrLut[add<25?add:24]&&sample.s==sec);
  assert(sample.d==(add<25?(column[add*4+sec]&63U):0));
 }
 // Stop group is part of the same loop; each matching-section period follows
 // the same instruction trace even at Add25 -> stop (same physical address).
 for(unsigned sec=0;sec<4;++sec){auto period=live[4+sec].time-live[sec].time;for(unsigned add=1;add<25;++add)assert(live[(add+1)*4+sec].time-live[add*4+sec].time==period);}
 for(unsigned s=0;s<4;++s)assert(!sample_low[s]&&s_state(s));
 assert(trace.size()==26*4+4); // final Ax=0 reset, CE still HIGH
}
int main(){
 std::array<uint8_t,100> c{};
 TPH_FireOneColumn(c.data());assert(trace.empty()&&!ce);
 TPH_M0333N_Power(ENABLE);assert(ce&&vl&&vp);
 assert((power_events==std::vector<unsigned>{1,2,3}));
 unsigned e=ce_edges;TPH_M0333N_Power(ENABLE);assert(ce_edges==e);
 for(unsigned bit=0;bit<6;++bit){c.fill(1U<<bit);run(c,bit&1U);}
 c.fill(0);run(c,0);c[99]=63;run(c,1);
 for(unsigned i=0;i<100;++i)c[i]=(i*37U+11U)&63U;run(c,0);
 uint32_t last=tick;run(c,0);assert(tick-last>=12000U); // conservative idle gap
 auto n=trace.size();e=ce_edges;TPH_FireOneColumn(nullptr);assert(trace.size()==n&&ce_edges==e&&ce);
 // unsigned counter wrap is valid for the short idle gap
 tick=0xfffffff0U;s_u32StopCycle=tick;run(c,1);
 TPH_M0333N_Power(0);assert(!ce&&!vp&&!vl&&irq==1);
 assert((power_events==std::vector<unsigned>{1,2,3,4,5,6}));
 for(unsigned s=0;s<4;++s)assert(!s_state(s));
 n=trace.size();TPH_FireOneColumn(c.data());assert(trace.size()==n);
 clock_frozen=true;TPH_M0333N_Power(ENABLE);assert(!s_bHeadPowered&&!ce&&!vl&&!vp);
 puts("PASS: actual LUT/GPIO mapping, 25+1 cadence, continuous CE, zero stop, idle reset, power order, IRQ restore, idle gap/wrap, NULL/OFF and clock failure");
}
'''
h=h.replace('LUT\n',lut+'\n').replace('LOOKUP\n',lookup+'\n').replace('MACROS\n',macros+'\n').replace('CORE\n',core+'\n')
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp);(p/'test.cpp').write_text(h)
 subprocess.run(['c++','-std=c++17','-O1','-fsanitize=address,undefined','-fno-omit-frame-pointer','-Wall','-Wextra','-Wno-misleading-indentation','-Werror',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
