from pathlib import Path
import subprocess,struct,json
import argparse,os,shutil
parser=argparse.ArgumentParser();parser.add_argument('--devkitarm',default=os.environ.get('LEGACY_DEVKITARM',os.environ.get('DEVKITARM','')));parser.add_argument('--sdk',default=os.environ.get('LEGACY_LIBNDS',os.environ.get('LIBNDS','')));args=parser.parse_args()
r=Path(__file__).resolve().parents[1]/'retail';tool=Path(args.devkitarm)/'bin';sdk=Path(args.sdk);b=Path(__file__).resolve().parent/'build';b.mkdir(exist_ok=True)
exe='.exe' if os.name=='nt' else ''
loader=['walkerCluster','slot2RomCluster','gen5RomCluster','gen5SaveCluster','gen5RomSize','walkerPayloadOffset','walkerPayloadSize']
ce=['walkerControl','walkerServices','walkerFlush','musicBuffer']
ctl=['state','respLen','armed','writeSeq','flushedSeq','flushes','dirty','eeprom']
keys=['loader_'+x for x in loader]+['loader_size']+['ce_'+x for x in ce]+['ctl_'+x for x in ctl]
s='#include <stddef.h>\n#include "load_crt0.h"\n#include "walker_emu.h"\nconst unsigned offsets[] = {'+','.join(['offsetof(loadCrt0,'+x+')' for x in loader]+['sizeof(loadCrt0)']+['offsetof(cardengineArm9,'+x+')' for x in ce]+['offsetof(WalkerControl,'+x+')' for x in ctl])+'};\n'
(b/'offsets.c').write_text(s)
subprocess.run([str(tool/('arm-none-eabi-gcc'+exe)),'-DB4DS','-I'+str(r/'common/include'),'-I'+str(sdk/'include'),'-c',str(b/'offsets.c'),'-o',str(b/'offsets.o')],check=True)
subprocess.run([str(tool/('arm-none-eabi-objcopy'+exe)),'-j','.rodata','-O','binary',str(b/'offsets.o'),str(b/'offsets.bin')],check=True)
values=struct.unpack('<'+'I'*len(keys),(b/'offsets.bin').read_bytes());result=dict(zip(keys,values))
for folder,file in [('bootloader','load0'),('bootloader2','load2'),('bootloaderi','loadi0'),('bootloaderi2','loadi2')]:
 output=subprocess.check_output([str(tool/('arm-none-eabi-nm'+exe)),'-n',str(r/f'{folder}/{file}.elf')],text=True)
 symbols={p[2]:int(p[0],16) for line in output.splitlines() if len(p:=line.split())==3}
 base=symbols['_start']
 for key in loader:assert symbols[key]-base==result['loader_'+key],(folder,key,hex(symbols[key]-base),result['loader_'+key])
 assert symbols['startUp']>=base+result['loader_size']
 headroom=symbols['__sp_usr']-symbols['__bss_end']
 minimum=4096 if folder.startswith('bootloaderi') else 6144
 assert headroom>=minimum,(folder,'insufficient loader stack headroom',headroom)
 print(folder,'launch header matches C layout; stack headroom',headroom)
# B4DS cardengine C fields must agree with the packed assembly header too.
checked=0
for image in sorted((r/'cardengine').glob('arm9*/build/*.elf')):
 output=subprocess.check_output([str(tool/('arm-none-eabi-nm'+exe)),'-n',str(image)],text=True)
 symbols={parts[2]:int(parts[0],16) for line in output.splitlines() if len(parts:=line.split())==3}
 if 'walkerServices' not in symbols:continue
 for field in ce:
  assert symbols[field]-symbols['ce9']==result['ce_'+field],(str(image),field)
 checked+=1
assert checked>=5,checked
print('B4DS cardengine headers match C layout:',checked)
(b/'offsets.json').write_text(json.dumps(result,indent=2))
print('All four launch headers are protected from parameter/startup overlap.')
