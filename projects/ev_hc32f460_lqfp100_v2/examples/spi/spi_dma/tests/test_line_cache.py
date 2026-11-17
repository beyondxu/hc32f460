"""Real cache state machine: exact row, CRC, replay/arm bounds, no premature readiness."""
from pathlib import Path
import subprocess,tempfile,os
S=Path(__file__).resolve().parents[1]/'source'
h=r'''
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include "printer_line_cache.h"
int main(void){
 printer_line_cache_t*c=calloc(1,sizeof(*c));uint8_t*row=malloc(PL_ROW_BYTES),p[100],status[100];
 assert(sizeof(*c)+16384U<0x2EFF8U);
 for(unsigned i=0;i<PL_ROW_BYTES;i++)row[i]=(i*13U+i/100U)&63U;
 assert(pl_crc((const uint8_t*)"123456789",9)==0xCBF43926U);
 uint32_t crc=pl_crc(row,PL_ROW_BYTES);
 for(unsigned mode=0;mode<5;mode++){
  pl_command(p,PL_BEGIN,7,720,crc^(mode==1?1U:0U));assert(pl_receive(c,p)&&c->state==PL_LOADING);
  unsigned count=mode==2?1179U:1180U;
  for(unsigned i=0;i<count;i++){assert(pl_receive(c,row+i*100));if(i<count-1)assert(c->state==PL_LOADING);}
  assert(!c->fired);
  if(mode==1)assert(c->state==PL_ERROR&&c->error==PL_BAD_CRC);
  else if(mode!=2)assert(c->state==PL_READY&&!memcmp(c->data,row,PL_ROW_BYTES));
  if(c->state==PL_READY){memset(p,0,100);assert(pl_receive(c,p)&&c->state==PL_READY&&c->received==PL_COLUMNS&&!memcmp(c->data,row,PL_ROW_BYTES));}
  pl_status(c,status);assert(pl_status_valid(status));status[80]^=1;assert(!pl_status_valid(status));
  pl_command(p,PL_ARM,mode==3?8U:7U,720,crc);pl_receive(c,p);
  if(mode==1||mode==2||mode==3)assert(c->state==PL_ERROR&&!c->fired);
  else {assert(c->state==PL_ARMED);c->state=PL_RUNNING;c->fired=1180;c->state=PL_DONE;pl_status(c,status);assert(pl_get16(status+14)==1180);}
 }
 pl_command(p,PL_BEGIN,9,599,crc);pl_receive(c,p);assert(c->state==PL_ERROR);
 pl_command(p,PL_BEGIN,9,720,crc);pl_receive(c,p);memset(p,0,100);p[30]=0x40;pl_receive(c,p);assert(c->state==PL_ERROR&&c->error==PL_BAD_DATA&&!c->received);
 pl_command(p,PL_BEGIN,10,720,crc);p[90]=1;pl_receive(c,p);assert(c->state==PL_ERROR);
 pl_command(p,PL_CANCEL,10,720,crc);pl_receive(c,p);assert(c->state==PL_ERROR&&c->error==PL_CANCELED);
 free(c);free(row);puts("PASS exact118000-byte cache, capacity, CRC, incomplete/wrong-token arm, period/data/reserved bounds, cancellation");
}
'''
with tempfile.TemporaryDirectory() as t:
 p=Path(t);(p/'test.c').write_text(h)
 subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(S),str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
