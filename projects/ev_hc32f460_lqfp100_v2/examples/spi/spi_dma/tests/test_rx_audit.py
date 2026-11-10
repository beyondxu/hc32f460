"""Actual receive audit vs independent CRC32 and per-domain/address/section totals."""
from pathlib import Path
import tempfile,subprocess,zlib,random
D=Path(__file__).resolve().parents[1]/'source'
h=r'''
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include "printer_row_audit.h"
int main(int argc,char**argv){assert(argc==12);FILE*f=fopen(argv[1],"rb");assert(f);uint8_t p[100],before[100];printer_row_audit_t a={0};printer_row_audit_begin(&a);unsigned columns=0;
 while(fread(p,1,100,f)==100){unsigned active=0;for(unsigned i=0;i<100;++i)for(unsigned bit=0;bit<6;++bit)active+=(p[i]>>bit)&1U;memcpy(before,p,100);printer_row_audit_column(&a,p,active,1);assert(!memcmp(p,before,100));++columns;}
 fclose(f);assert(columns==1180&&a.columns==1180&&a.fired==1180&&!a.invalid&&!a.canceled);
 assert((a.rx_crc^0xFFFFFFFFU)==strtoul(argv[2],NULL,16));assert(a.rx_domains==strtoul(argv[3],NULL,16));assert(a.rx_addresses==strtoul(argv[4],NULL,16));assert(a.rx_sections==strtoul(argv[5],NULL,16));
 for(unsigned bit=0;bit<6;++bit)assert(a.domain_dots[bit]==strtoul(argv[bit+6],NULL,10));
 printer_row_audit_end(&a);uint32_t crc=a.rx_crc;memset(p,0,100);printer_row_audit_column(&a,p,0,1);assert(a.columns==1180&&a.rx_crc==crc);
 printer_row_audit_begin(&a);assert(a.columns==0&&a.rx_crc==0xFFFFFFFFU&&!a.rx_domains&&!a.rx_addresses&&!a.rx_sections);for(unsigned bit=0;bit<6;++bit)assert(!a.domain_dots[bit]);
 p[0]=0x80;printer_row_audit_column(&a,p,0,0);assert(a.columns==1&&a.invalid==1&&!a.fired&&!a.rx_domains);
}
'''
def expected(data):
 counts=[0]*6;domains=addresses=sections=0
 for i,b in enumerate(data):
  if b&63:domains|=b&63;addresses|=1<<((i%100)//4);sections|=1<<(i%4)
  for bit in range(6):counts[bit]+=(b>>bit)&1
 return [f'{zlib.crc32(data):08X}',f'{domains:X}',f'{addresses:X}',f'{sections:X}',*map(str,counts)]
with tempfile.TemporaryDirectory() as tmp:
 t=Path(tmp);(t/'test.c').write_text(h)
 subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-I'+str(D),str(t/'test.c'),'-o',str(t/'test')],check=True)
 cases=[bytes(118000)]
 for addr in [2,5]:
  for mask in [1,2,3]:
   data=bytearray(118000)
   for x in range(170,1065):
    if x%97:data[x*100+(addr-1)*4+1]=mask
   cases.append(bytes(data))
 rng=random.Random(781);cases.append(bytes(rng.randrange(64) for _ in range(118000)))
 for data in cases:
  (t/'input.bin').write_bytes(data)
  r=subprocess.run([str(t/'test'),str(t/'input.bin'),*expected(data)],capture_output=True,text=True,check=True)
  rx=[line for line in r.stdout.splitlines() if 'HC32 RX_AUDIT' in line];assert len(rx)==1 and len(rx[0])<180
 # Object-check the real header routines on the target CPU. This is not a full Keil link.
 arm=Path('/workspace/.setup/tools/usr/bin/arm-none-eabi-gcc')
 if arm.exists():subprocess.run([str(arm),'-std=gnu99','-mcpu=cortex-m4','-mthumb','-mfpu=fpv4-sp-d16','-mfloat-abi=hard','-O2','-Wall','-Wextra','-Werror','-I'+str(D),'-c',str(t/'test.c'),'-o',str(t/'test.o')],check=True)
print('PASS: all118000 received bytes including zero columns, independent CRC32, exact D1-D6/address/section counts, no data mutation, reset/end/invalid-frame behavior, compact end-only log, M4 object check')
