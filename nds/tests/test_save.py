"""Execute the linked DS save/load code with deterministic card I/O faults."""
from pathlib import Path
import runpy,struct,zlib
ns=runpy.run_path(str(Path(__file__).with_name('test_native.py')))
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_ARM,UC_HOOK_CODE
from unicorn.arm_const import *
u=Uc(UC_ARCH_ARM,UC_MODE_ARM);u.mem_map(0x02000000,0x400000);u.mem_map(0x02ff0000,0x10000)
s=ns['s'];d=ns['d']
for sh in ns['sections']:
 if sh[2]&2 and sh[5] and sh[1]!=8 and 0x02000000<=sh[3]<0x02400000:u.mem_write(sh[3],d[sh[4]:sh[4]+sh[5]])
STOP=0x023fff00;regs=[UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3]
def put(a,v):u.mem_write(a,struct.pack('<I',v))
def get(a):return struct.unpack('<I',u.mem_read(a,4))[0]
def text(a):return bytes(u.mem_read(a,600)).split(b'\0')[0].decode()
def ret(v=0):u.reg_write(regs[0],v&0xffffffff);u.reg_write(UC_ARM_REG_PC,u.reg_read(UC_ARM_REG_LR))
def call(name,*args):
 u.reg_write(UC_ARM_REG_CPSR,0x13);u.reg_write(UC_ARM_REG_SP,0x023df000);u.reg_write(UC_ARM_REG_LR,STOP)
 for r,a in zip(regs,args):u.reg_write(r,a)
 u.emu_start(s[name][0],STOP,count=30000000);assert u.reg_read(UC_ARM_REG_PC)==STOP,(name,hex(u.reg_read(UC_ARM_REG_PC)),sorted([(abs(v[0]-u.reg_read(UC_ARM_REG_PC)),k) for k,v in s.items()])[:3])
 return u.reg_read(regs[0])
base=bytearray(b'\xff'*65536);base[:8]=b'nintendo';base[0xed:0xed+104]=bytes(104)
base[0xed+0x5b]=3;base[0xed+12:0xed+14]=struct.pack('<H',12345);base[0xed+0x48:0xed+0x4c]=b'\x35\x01\xff\xff'
base[0xed+104]=(1+sum(base[0xed:0xed+104]))&255;base[0x1ed:0x1ed+105]=base[0xed:0xed+105]
base[0x156:0x156+24]=bytes(24);base[0x156+24]=1;base[0x256:0x256+25]=base[0x156:0x156+25]
base[0x8f00:0x8f02]=struct.pack('<H',74);base[0x8f0c]=15;base[0x8f52:0x8f54]=struct.pack('<H',21)
files={};fault={'drop':False};path='fat:/roms/test.walker'
hooks={s[n][0]&~1:n for n in ['readFile','writeFile','sdRenameReplace','leaseInspect','__syscall_getreent','fopen','mkdir','now']}
def hook(uc,addr,size,user):
 name=hooks.get(addr)
 if not name:return
 a,b,c,e=[uc.reg_read(r) for r in regs]
 if name=='readFile':
  data=files.get(text(a));ok=data is not None and len(data)==65536
  if ok:uc.mem_write(b,data)
  ret(int(ok))
 elif name=='writeFile':files[text(a)]=bytes(uc.mem_read(b,65536));ret(1)
 elif name=='sdRenameReplace':
  src,dst=text(a),text(b)
  if not(fault['drop'] and dst==path):files[dst]=files.pop(src)
  ret(0)
 elif name=='__syscall_getreent':ret(0x02360000)
 elif name=='now':ret(1000000)
 else:ret(0)
u.hook_add(UC_HOOK_CODE,hook)
a,n,i=ns['ly']['handoffOffsets'];section=ns['ls'][i]
LOCK,COUNT,*_=struct.unpack('<8I',ns['ld'][section[4]+a:section[4]+a+n]);UI=s['ui'][0]
put(UI+COUNT,1);put(UI+LOCK,0);put(s['opened'][0],0);u.mem_write(s['paths'][0],path.encode()+b'\0')
for dropped in [True,False]:
 files.clear();files[path]=bytes(base);u.mem_write(s['ee'][0],bytes(base));put(s['diskCrc'][0],zlib.crc32(base));put(s['dirty'][0],1)
 assert call('walkerStorePokemon',s['ee'][0],0,0xffffffff,1000000)==1
 fault['drop']=dropped;assert call('save')==(0 if dropped else 1)
 if dropped:
  assert get(s['dirty'][0])==1 and files[path]==bytes(base)
 else:
  assert get(s['dirty'][0])==0
  u.mem_write(s['ee'][0],bytes(65536));assert call('load',0)==1
  assert call('walkerPokemonId',s['ee'][0],0)==21
print('Actual DS save/load: silently dropped final replacement refuses success and retains dirty data; verified caught Pokemon survives clearing RAM and reloading the card file.')
