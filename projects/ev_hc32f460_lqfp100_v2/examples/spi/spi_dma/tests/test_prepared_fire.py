"""Continuous-CE delivery/source agreement and real Cortex-M4 object checks.
Checks structure and compiler output, not hardware edge timing or ink.
"""
from pathlib import Path
import re,subprocess,tempfile
P=Path(__file__).resolve().parents[1];S=P/'source';src=(S/'tph_m0333n.c').read_bytes().decode('latin1')
core=(S/'tph_continuous_ce.inc').read_text();assert core in src
direct=(S/'tph_direct_port_fire.inc').read_text();assert direct in core
assert src.index('__disable_irq();',src.index('void TPH_FireOneColumn('))<src.index('const uint32_t keep_a',src.index('void TPH_FireOneColumn('))
column=core[core.index('void TPH_FireOneColumn('):]
body=column[column.index('do {'):column.index('} while (group != end);')]
assert 'groups[26]' in column and 'groups + 26U' in column
assert 'uint16_t address;' in column and 'uint16_t data[4];' in column
assert '"+r"' not in body and 'This is a compiler barrier' in body
assert 'add < 25U ? add : 24U' in column
assert 'pColumn100B[add * 4U + sec] & 0x3FU' in column
assert body.count('delay_ts_400ns();')==4
assert body.count('CM_GPIO->PODRA =')==4 and body.count('CM_GPIO->PODRB =')==1
assert all(x not in body for x in ['CM_GPIO->PORRA','CM_GPIO->POSRA','CM_GPIO->PORRB','CM_GPIO->POSRB'])
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
  for opt,pad,cooldown in [(o,p,c) for o in ['-O2','-Os'] for p in [0,4,8,12,16] for c in [0,1000]]:
   o=t/(opt+str(pad)+'-'+str(cooldown)+'.o')
   r=subprocess.run([str(arm),'-mcpu=cortex-m4','-mthumb','-mfpu=fpv4-sp-d16','-mfloat-abi=hard','-DHC32F460','-DUSE_DDL_DRIVER',opt,'-Wall','-Wextra','-Werror','-fstack-usage','-DTPH_DIRECT_PORT_PAD_NOPS='+str(pad),'-DTPH_COLUMN_COOLDOWN_US='+str(cooldown),*['-I'+str(i) for i in incs],'-c',str(S/'tph_m0333n.c'),'-o',str(o)],capture_output=True)
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
    group_asm=re.split(r'\tbne(?:\.[a-z]+)?\s',live,maxsplit=1)[0]
    assert len(re.findall(r'\tstrh(?:\.[a-z]+)?\s',group_asm))==13,(opt,pad,group_asm)
    # Inspect the actual back-edge target, excluding one-time setup.
    branch=re.search(r'\tbne(?:\.[a-z]+)?\s+([0-9a-f]+)\s',live)
    assert branch,(opt,pad,live)
    target=int(branch.group(1),16)
    loop_start=re.search(r'(?m)^\s*'+format(target,'x')+r':',live)
    assert loop_start,(opt,pad,live)
    repeat=live[loop_start.start():branch.end()]
    assert len(re.findall(r'\tldrh(?:\.[a-z]+)?\s',repeat))==5,(opt,pad,repeat)
    assert not re.search(r'\t(?:uxth|mov|movs|ldr|ldrd)(?:\.[a-z]+)?\s',repeat),(opt,pad,repeat)
    actual_nops=len(re.findall(r'\bnop\b',live))
    runs=[];run=0
    for line in live.splitlines():
     if re.search(r'\tnop(?:\s|$)',line):run+=1
     elif run:runs.append(run);run=0
    if run:runs.append(run)
    assert runs.count(nop_count)==8,(opt,pad,runs)
    if pad:assert pad in runs,(opt,pad,runs)
    # A compiler-generated, skipped literal-pool alignment NOP may also occur.
    assert 0<=actual_nops-(8*nop_count+pad)<=3,(opt,pad,actual_nops,runs)
    windows.append(name)
   assert windows,(opt,'missing actual live window')
   stack=(t/(opt+str(pad)+'-'+str(cooldown)+'.su')).read_text()
   sizes=[int(line.split('\t')[1]) for line in stack.splitlines() if 'TPH_FireOneColumn' in line]
   print(opt,'cooldown_us=',cooldown,'pad=',pad,'live=',','.join(windows),'stack_bytes=',sizes,'repeat_halfword_loads=5 redundant_moves_truncations=0')
 print('PASS: M4 -O2/-Os at pads 0/4/8/12/16 and cooldown 0/1000us; no warnings or calls in live train, sample NOP blocks unchanged; five halfword preloads, no repeated moves/truncations')
print('PASS: complete delivery matches production; 25+zero stop share live path, five A/D output writes per group, no CE switching/source LUTs/timers; only optional calibrated group padding')
