"""Continuous-CE delivery/source agreement and real Cortex-M4 object checks.
Checks structure and compiler output, not hardware edge timing or ink.
"""
from pathlib import Path
import re,subprocess,tempfile
P=Path(__file__).resolve().parents[1];S=P/'source';src=(S/'tph_m0333n.c').read_bytes().decode('latin1')
core=(S/'tph_continuous_ce.inc').read_text();assert core in src
column=core[core.index('void TPH_FireOneColumn('):]
body=column[column.index('do {'):column.index('} while (group != end);')]
assert 'groups[26]' in column and 'groups + 26U' in column
assert 'add < 25U ? add : 24U' in column
assert 'pColumn100B[add * 4U + sec] & 0x3FU' in column
assert body.count('delay_ts_400ns();')==4
for forbidden in ['CE_HIGH();','CE_LOW();','pColumn100B','s_DataPortA','s_AddrPortB','delay_600ns','delay_100ns','delay_40ns','printf','DWT->','__set_PRIMASK']:
 assert forbidden not in body,forbidden
assert 'CE_HIGH();' not in column and 'CE_LOW();' not in column
assert column.index('TPH_Reset_Idle_CE_High();')<column.index('__set_PRIMASK')
assert core.count('CE_HIGH();')==1 # normal ON path only
root=P.parents[4]
arm=Path('/workspace/.setup/tools/usr/bin/arm-none-eabi-gcc');objdump=arm.with_name('arm-none-eabi-objdump')
if arm.exists():
 incs=[S,root/'drivers/hc32_ll_driver/inc',root/'drivers/cmsis/Include',root/'drivers/cmsis/Device/HDSC/hc32f4xx/Include']
 sample=src[src.index('inline void delay_ts_400ns'):src.index('inline void delay_600ns')]
 nop_count=len(re.findall(r'\bnop\b',sample))
 with tempfile.TemporaryDirectory() as tmp:
  t=Path(tmp)
  for opt in ['-O2','-Os']:
   o=t/(opt+'.o')
   r=subprocess.run([str(arm),'-mcpu=cortex-m4','-mthumb','-mfpu=fpv4-sp-d16','-mfloat-abi=hard','-DHC32F460','-DUSE_DDL_DRIVER',opt,'-Wall','-Wextra','-Werror','-fstack-usage',*['-I'+str(i) for i in incs],'-c',str(S/'tph_m0333n.c'),'-o',str(o)],capture_output=True)
   assert r.returncode==0,r.stderr.decode('utf-8',errors='replace')
   asm=subprocess.check_output([str(objdump),'-d',str(o)],text=True)
   chunks=re.split(r'\n[0-9a-f]+ <([^>]+)>:\n',asm)
   windows=[]
   for i in range(1,len(chunks),2):
    name,section=chunks[i:i+2]
    if not name.startswith('TPH_FireOneColumn'):continue
    if 'dmb\tsy' not in section:continue
    first=section.index('dmb\tsy');last=section.index('dsb\tsy',first);live=section[first:last]
    assert not re.search(r'\t(?:bl|blx)(?:\.|\s)',live),live
    assert len(re.findall(r'\bnop\b',live))==8*nop_count # four in repeated group + four idle reset
    windows.append(name)
   assert windows,(opt,'missing actual live window')
   stack=(t/(opt+'.su')).read_text()
   sizes=[int(line.split('\t')[1]) for line in stack.splitlines() if 'TPH_FireOneColumn' in line]
   print(opt,'live=',','.join(windows),'stack_bytes=',sizes)
 print('PASS: real M4 -O2/-Os objects, no warnings or calls in live train, preserved sample NOP blocks')
print('PASS: complete delivery matches production; 25+zero stop share live path, no CE switching/source LUTs/timers/added delays in train')
