"""Prepared CE source/delivery agreement and target object checks.
Checks instruction structure, not physical nanoseconds or actual ink.
"""
from pathlib import Path
import re,subprocess,tempfile
P=Path(__file__).resolve().parents[1];S=P/'source';src=(S/'tph_m0333n.c').read_bytes().decode('latin1');replacement=(S/'tph_fire_prepared.inc').read_text()
a=src.index('void TPH_FireOneColumn(');b=src.index('/* ========================================================================== */',a);column=src[a:b].strip()
assert column==replacement[replacement.index('void TPH_FireOneColumn('):].strip()
w=column[column.index('CE_HIGH();'):column.index('CE_LOW();')]
assert w.count('delay_ts_400ns();')==4
for forbidden in ['delay_100ns','delay_40ns','s_DataPortA','pColumn100B','FAST_SET_ADDRESS','__set_PRIMASK']:
 assert forbidden not in w,forbidden
assert column.index('CM_GPIO->POSRA = (uint16_t)w0;')<column.index('CE_HIGH();')<column.index('S1_LOW();')
assert column.index('S4_HIGH();')<column.index('CE_LOW();')<column.index('__set_PRIMASK')
assert column.count('data_idx++')==4 and 'add < 25' in column
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
   r=subprocess.run([str(arm),'-mcpu=cortex-m4','-mthumb','-mfpu=fpv4-sp-d16','-mfloat-abi=hard','-DHC32F460','-DUSE_DDL_DRIVER',opt,*['-I'+str(i) for i in incs],'-c',str(S/'tph_m0333n.c'),'-o',str(o)],capture_output=True)
   assert r.returncode==0,r.stderr.decode('utf-8',errors='replace')
   asm=subprocess.check_output([str(objdump),'-d','--disassemble=TPH_FireOneColumn',str(o)],text=True)
   first=asm.index('dmb\tsy');end=asm.index('dsb\tsy',first);body=asm[first:end]
   assert len(re.findall(r'\bnop\b',body))==4*nop_count
   assert not re.search(r'\t(?:bl|blx|bx)(?:\.|\s)',body)
 print('PASS: real M4 -O2/-Os objects; four full sample NOP blocks and no function calls in CE region')
print('PASS: function-only delivery equals tested production function; source/LUT reads and first D setup precede CE, no added A/D/S waits, full S4 before CE low/IRQ restore')
