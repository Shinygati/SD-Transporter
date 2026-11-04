"""Run the linked ARM storage adapter against alignment-strict fake media.
No real card, firmware, walker data or physical hardware is used.
"""
from pathlib import Path
import runpy,struct
from unicorn import UC_HOOK_CODE
ns=runpy.run_path(str(Path(__file__).with_name('test_native.py')))
u,s,call,put,get,ret=[ns[x] for x in ['u','s','call','put','get','ret']]
from unicorn.arm_const import UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3
regs=[UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3]
INNER=0x02378000;VT=0x02378100;DISC=0x02380000;BOUNCE=0x02390000
READ=0x023a0000;WRITE=READ+4;FLUSH=READ+8;DESTROY=READ+12
u.mem_write(INNER,struct.pack('<I I H B B I',VT,1,3,0,9,200))
u.mem_write(VT,struct.pack('<4I',DESTROY|1,READ|1,WRITE|1,FLUSH|1))
media=bytearray((i*13+7)&255 for i in range(200*512));events={'read':0,'write':0,'fail':False,'locks':0}
mock={s[n][0]&~1:n for n in ['calloc','memalign','free','mutexLock','mutexUnlock']}
def hook(uc,addr,size,user):
 a,b,c,d=[uc.reg_read(x) for x in regs]
 if addr in mock:
  n=mock[addr]
  if n=='calloc':u.mem_write(DISC,b'\0'*128);ret(DISC)
  elif n=='memalign':assert a==32 and b==4096;ret(BOUNCE)
  elif n=='mutexLock':events['locks']+=1;ret()
  elif n=='mutexUnlock':events['locks']-=1;ret()
  else:ret()
 elif addr in [READ,WRITE]:
  assert a==INNER and 0x02000000<=b<0x02ff0000
  assert b%(32 if addr==READ else 4)==0,(hex(b),addr)
  assert events['locks']==1
  key='read' if addr==READ else 'write';events[key]+=1
  if events['fail']:ret(0);return
  if addr==READ:u.mem_write(b,bytes(media[c*512:(c+d)*512]))
  else:media[c*512:(c+d)*512]=bytes(u.mem_read(b,d*512))
  ret(1)
 elif addr in [FLUSH,DESTROY]:ret()
u.hook_add(UC_HOOK_CODE,hook)
wrapped=call('__wrap_dvmDiscCacheCreate',INNER,0,8);assert wrapped==DISC
vtable=get(DISC)
for name,offset in [('adapterRead',4),('adapterWrite',8)]:s[name]=(get(vtable+offset),0,0)
for ptr in [0x023b0000,0x023b0001,0x02ff0001]:
 assert call('adapterRead',DISC,ptr,3,19)==1
 assert bytes(u.mem_read(ptr,19*512))==bytes(media[3*512:22*512])
 block=bytes((i*7+23)&255 for i in range(19*512));u.mem_write(ptr,block)
 assert call('adapterWrite',DISC,ptr,30,19)==1
 assert bytes(media[30*512:49*512])==block
events['fail']=True
assert call('adapterRead',DISC,0x023b0001,3,1)==0
assert call('adapterWrite',DISC,0x023b0001,3,1)==0
assert events['locks']==0
print('Linked ARM direct storage: aligned, unaligned and DTCM buffers read/write correctly; multi-chunk transfers, strict media alignment and immediate read/write failures verified. No write-back data is retained.')
