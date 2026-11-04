"""Exercise actual no-print-in-column diagnostic counters."""
from pathlib import Path
import subprocess,tempfile
src=Path(__file__).resolve().parents[1]/'source'
h=r'''
#include <assert.h>
#include "printer_row_audit.h"
static void run(unsigned mask,int drop){
 printer_row_audit_t a={0};uint8_t p[100];
 printer_row_audit_begin(&a);
 for(unsigned x=0;x<1180;x++){
  memset(p,0,100);unsigned active=0;
  const unsigned slot[3]={73,46,15},delay[3]={59,30,0};
  for(unsigned ch=0;ch<3;ch++){
   if(!(mask&(1<<ch)))continue;
   for(unsigned bar=0;bar<3;bar++){
    unsigned first=300+200*bar+delay[ch];
    if(x>=first&&x<first+150 && !(drop&&ch==0&&x==359)){p[slot[ch]]=1<<(ch*2+1);++active;}
   }
  }
  uint8_t before[100];memcpy(before,p,100);
  printer_row_audit_column(&a,p,active,1);
  assert(!memcmp(before,p,100));
 }
 assert(a.columns==1180&&a.fired==1180&&!a.invalid&&!a.other_bits);
 for(unsigned ch=0;ch<3;ch++){
  assert(a.dots[ch]==((mask&(1<<ch))?450U-(drop&&ch==0):0));
  for(unsigned bar=0;bar<3;bar++)assert(a.bars[ch][bar]==((mask&(1<<ch))?150U-(drop&&ch==0&&bar==0):0));
  assert(!a.outside[ch]);
 }
 printer_row_audit_end(&a);assert(!a.active);
 uint8_t zero[100]={0};printer_row_audit_column(&a,zero,0,1);assert(a.columns==1180);
}
int main(void){
 run(3,0);run(7,0);run(3,1);
 printer_row_audit_t a={0};uint8_t p[100]={0};printer_row_audit_begin(&a);
 p[0]=1;printer_row_audit_column(&a,p,1,0);assert(a.invalid==1&&!a.fired&&a.other_bits==1);
 printer_row_audit_begin(&a);assert(a.columns==0&&a.active);
 puts("PASS: per-bar counts, missing-bit detection, frame rejection, no input mutation and reset/end boundaries");
}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp);(p/'test.c').write_text(h)
 subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-I',str(src),str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
