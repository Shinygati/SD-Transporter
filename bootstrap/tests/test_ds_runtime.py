from pathlib import Path
import sys,struct,json,re
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_ARM,UC_HOOK_CODE
from unicorn.arm_const import *
ROOT=Path(__file__).resolve().parents[1]/'retail';OFF=json.loads((Path(__file__).resolve().parent/'build/offsets.json').read_text())
def elf(path):
 data=Path(path).read_bytes();shoff=struct.unpack_from('<I',data,32)[0];count=struct.unpack_from('<H',data,48)[0];step=struct.unpack_from('<H',data,46)[0]
 sections=[struct.unpack_from('<10I',data,shoff+n*step) for n in range(count)];symbols={}
 for sh in sections:
  if sh[1]!=2:continue
  strings=sections[sh[6]];names=data[strings[4]:strings[4]+strings[5]]
  for pos in range(sh[4],sh[4]+sh[5],sh[9]):
   name,value,size,info,other,index=struct.unpack_from('<IIIBBH',data,pos)
   if name:symbols[names[name:names.find(b'\0',name)].decode()]=value
 return data,sections,symbols
loader_data,loader_sections,SYM=elf(ROOT/'bootloader/load0.elf')
MEM=0x02000000;STOP=0x02001000;CODE=0x02004000;HEADER=0x023ff000;MODULE=0x02002000;CE=0x023f9c00;HEAP=CODE+0x400
r0,r1,r2,r3=UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3

def machine():
 u=Uc(UC_ARCH_ARM,UC_MODE_ARM);u.mem_map(MEM,0x400000);u.mem_map(0x06000000,0x40000);u.mem_map(0x04000000,0x10000)
 for sh in loader_sections:
  if sh[2]&2 and sh[5] and sh[1]!=8:u.mem_write(sh[3],loader_data[sh[4]:sh[4]+sh[5]])
 u.mem_write(STOP,b'\x00\x00\xa0\xe1');return u

def put(u,address,value):u.mem_write(address,struct.pack('<I',value))
def get(u,address):return struct.unpack('<I',u.mem_read(address,4))[0]
def regreturn(u,value=0):u.reg_write(r0,value);u.reg_write(UC_ARM_REG_PC,u.reg_read(UC_ARM_REG_LR))
def call(u,addr,args):
 u.reg_write(UC_ARM_REG_CPSR,0x13);u.reg_write(UC_ARM_REG_SP,0x02350000);u.reg_write(UC_ARM_REG_LR,STOP)
 for reg,value in zip([r0,r1,r2,r3],args):u.reg_write(reg,value)
 u.emu_start(addr,STOP,count=3000000)
 assert u.reg_read(UC_ARM_REG_PC)==STOP,'did not return'
 return u.reg_read(r0)

payload_header=(ROOT/'common/include/walker_ds_payload.h').read_text()
def payload_size(name):return int(re.search(r'#define '+name+r' (\d+)u',payload_header).group(1))
PROTOCOL_SIZE=payload_size('WALKER_PROTOCOL_SIZE');STUB_SIZE=payload_size('WALKER_IR_STUB_SIZE')
payload=(ROOT/'nitrofiles/walker_ds.bin').read_bytes();image=bytes((n*17+13)&255 for n in range(65536))

def attach_media(u):
 # Execute the real compiled getFileFromCluster/fileRead code. Mock only
 # physical sectors and FAT chain traversal, using fragmented file chains.
 sectors={};links={};reads=[];target=0x02001400
 for first,next_base,data in [(42,1000,image),(35,2000,bytes(0x2000)+payload)]:
  count=(len(data)+4095)//4096;chain=[first]+list(range(next_base,next_base+count-1))
  for index,cluster in enumerate(chain):
   links[cluster]=chain[index+1] if index+1<len(chain) else 0x0fffffff
   for n in range(8):
    part=data[index*4096+n*512:index*4096+(n+1)*512]
    sectors[100+(cluster-2)*8+n]=part.ljust(512,b'\0')
 for name,value in [('discData',100),('discSecPerClus',8),('discBytePerClus',4096),('prevFirstClust',0xffffffff),('prevClust',0xffffffff),('prevSect',0xffffffff)]:put(u,SYM[name],value)
 u.mem_write(SYM['discBytePerClusShift'],b'\x0c');u.mem_write(SYM['discSecPerClusShift'],b'\x03')
 put(u,SYM['__myio_dldi']+16,target);u.mem_write(target,struct.pack('<I',0xe12fff1e))
 def hook(uc,address,size,user):
  if address==target:
   sector,count,dst=[uc.reg_read(reg) for reg in [r0,r1,r2]]
   reads.append((sector,count))
   uc.mem_write(dst,b''.join(sectors.get(sector+n,bytes(512)) for n in range(count)));regreturn(uc,1)
  elif address==(SYM['FAT_NextCluster']&~1):regreturn(uc,links.get(uc.reg_read(r0),0x0fffffff))
 u.hook_add(UC_HOOK_CODE,hook)
 return reads

def installation(case='valid'):
 u=machine();u.mem_write(HEADER+12,b'IPKE');put(u,HEADER+0x28,CODE);put(u,HEADER+0x2c,0x4000)
 put(u,MODULE+16,0x02200000);put(u,MODULE+24,0x04000000)
 for name,value in [('walkerCluster',42),('ndsBootstrapCluster',35),('walkerPayloadOffset',0x2000),('walkerPayloadSize',len(payload)),('iUncompressedSize',0x4000),('fatTableAddr',0x023ff200),('extendedMemory',0)]:put(u,SYM[name],value)
 put(u,CE+OFF['ce_walkerServices'],0x02003000);u.mem_write(0x02003000,struct.pack('<6I',*(0x02001800+n*0x20 for n in range(6))))
 put(u,CE+OFF['ce_musicBuffer'],0x027f0000)
 # Use the real SDK heap scanner, including the production ordering where
 # patchHiHeapPointer has already changed the literal before walkerBootDs.
 heap_before=0x023dc000 if case=='patched_heap' else 0x023e0000
 put(u,HEAP-4,0xe12fff1e);put(u,HEAP,heap_before)
 if case=='patched_heap':put(u,SYM['patchOffsetCache']+16,HEAP)
 if case=='missing_heap':put(u,HEAP,0)
 if case=='invalid_cached_heap':put(u,SYM['patchOffsetCache']+16,3)
 for addr,values in [(CODE+0x100,[0xe92d47f0]),(CODE+0x180,[0xe7d60005,0xe22000aa,0xe7c60005,0xe2855001]),(CODE+0x200,[0xe92d43f8]),(CODE+0x300,[0xe92d4ff8]),(CODE+0xc0,[0xe92d4008,0xe59f1000,0xe3a00032])]:u.mem_write(addr,struct.pack('<'+'I'*len(values),*values))
 if case=='ambiguous':
  u.mem_write(CODE+0x900,struct.pack('<I',0xe92d47f0));u.mem_write(CODE+0x980,struct.pack('<4I',0xe7d60005,0xe22000aa,0xe7c60005,0xe2855001))
 if case=='no_room':put(u,MODULE+16,0x023d8000)
 if case=='wrong_game':u.mem_write(HEADER+12,b'CPUE')
 initial=bytes(u.mem_read(CODE+0x100,STUB_SIZE));read_calls=[]
 hooks={} if case=='fat_media' else {SYM['getFileFromCluster']&~1:'open',SYM['fileRead']&~1:'read'}
 if case=='fat_media':attach_media(u)
 def hook(uc,address,size,user):
  name=hooks.get(address)
  if name=='open':
   uc.mem_write(uc.reg_read(r0),struct.pack('<6I',uc.reg_read(r1),0,0,0,0,0));regreturn(uc)
  elif name=='read':
   dst,f,off,n=[uc.reg_read(reg) for reg in [r0,r1,r2,r3]];cluster=get(uc,f);read_calls.append((cluster,off,n))
   data=image[off:off+n] if cluster==42 else payload[off-0x2000:off-0x2000+n]
   if case=='short_payload' and cluster==35:data=data[:max(0,len(data)-1)]
   uc.mem_write(dst,data);regreturn(uc,len(data))
 u.hook_add(UC_HOOK_CODE,hook);call(u,SYM['walkerBootDs'],[CE,HEADER,MODULE,0x023fd000])
 c=get(u,CE+OFF['ce_walkerControl'])
 if case not in ('valid','patched_heap','fat_media'):
  assert c==0 and get(u,HEAP)==(0 if case=='missing_heap' else heap_before),(case,'failed installation changed heap')
  assert bytes(u.mem_read(CODE+0x100,STUB_SIZE))==initial,(case,'failed installation changed IR code')
 else:
  base=get(u,HEAP);assert base==min(heap_before,0x023de000)-0x14000 and c==base+0x10000,(case,hex(base),hex(c),read_calls)
  assert bytes(u.mem_read(base,65536))==image
  assert bytes(u.mem_read(base+0x11000,PROTOCOL_SIZE))==payload[:PROTOCOL_SIZE]
  assert get(u,CODE+0x100+0xa8)==base+0x12800
  assert get(u,CODE+0x100+0xa4)==c and get(u,CE+OFF['ce_walkerFlush'])==CODE+0x110
  assert get(u,CODE+0x200)==0xe51ff004 and get(u,CODE+0x300)==0xe3a000aa
  assert get(u,c+0x800)==base+0x11000
 return u,c

for case in ['valid','patched_heap','fat_media','missing_heap','invalid_cached_heap','ambiguous','no_room','wrong_game','short_payload']:installation(case);print('Boot installation:',case,'passed')

# Run the production protocol through the installed DS dispatcher: SELECT
# must start advertising; releasing it keeps the link armed until INIT.
u,c=installation('fat_media');entry=get(u,HEAP)+0x12800;out=CODE+0x2000
u.mem_write(0x04000130,struct.pack('<H',0x03ff))
assert call(u,entry,[c,0,out,0])==0
u.mem_write(0x04000130,struct.pack('<H',0x03fb))
assert call(u,entry,[c,0,out,0])==1
u.mem_write(0x04000130,struct.pack('<H',0x03ff))
assert call(u,entry,[c,0,out,0])==1
assert call(u,entry,[c,2,0,0])==0 and call(u,entry,[c,0,out,0])==0
print('Installed DS protocol: SELECT arms advertising; release preserves it; INIT resets it.')

# Execute the actual relocated ARM9 dispatcher, with a mock protocol entry and DLDI services.
u,c=installation();base=get(u,c+OFF['ctl_eeprom']);ctx=c+0x800;PROTO=0x02001700
put(u,ctx,PROTO);targets=[PROTO]+[0x02001800+n*0x20 for n in range(6)]
for addr in targets:u.mem_write(addr,struct.pack('<I',0xe12fff1e))
media=bytearray(image);state={'terminal':False,'failure':False,'corrupt':False,'writes':0,'reads':0,'resets':0}
def runtime_hook(uc,address,size,user):
 if address==PROTO:
  put(uc,c+OFF['ctl_state'],0 if state['terminal'] else 1)
  if not get(uc,c+OFF['ctl_writeSeq']):
   uc.mem_write(base+123,b'X');put(uc,c+OFF['ctl_writeSeq'],1);uc.mem_write(c+OFF['ctl_dirty'],b'\1')
  put(uc,c+OFF['ctl_respLen'],8);regreturn(uc,12);return
 if address not in targets:return
 idx=targets.index(address)-1
 if idx==0:regreturn(uc)
 elif idx==1:
  dst,_,off,n=[uc.reg_read(reg) for reg in [r0,r1,r2,r3]];data=bytearray(media[off:off+n]);state['reads']+=1
  if state['corrupt']:data[0]^=1
  uc.mem_write(dst,bytes(data));regreturn(uc,n)
 elif idx==2:
  src,_,off,n=[uc.reg_read(reg) for reg in [r0,r1,r2,r3]];state['writes']+=1
  if state['failure']:regreturn(uc,0)
  else:media[off:off+n]=uc.mem_read(src,n);regreturn(uc,n)
 elif idx==3:regreturn(uc)
 elif idx==4:uc.mem_write(0x04000204,struct.pack('<H',0x8888));regreturn(uc)
 elif idx==5:state['resets']+=1;regreturn(uc)
u.hook_add(UC_HOOK_CODE,runtime_hook)
put(u,0x04000208,1);u.mem_write(0x04000204,struct.pack('<H',0x1234))
call(u,base+0x12800,[c,1,0,0]);assert state['writes']==0 and get(u,c+OFF['ctl_flushedSeq'])==0
state['terminal']=True;state['failure']=True
assert call(u,base+0x12800,[c,0,0,0])==0
assert get(u,c+OFF['ctl_respLen'])==0 and get(u,c+OFF['ctl_flushedSeq'])==0 and u.mem_read(c+OFF['ctl_dirty'],1)==b'\1'
state['failure']=False;state['corrupt']=True
assert call(u,base+0x12800,[c,0,0,0])==0 and get(u,c+OFF['ctl_flushedSeq'])==0
state['corrupt']=False
assert call(u,base+0x12800,[c,0,0,0])==12
assert get(u,c+OFF['ctl_flushedSeq'])==1 and get(u,c+OFF['ctl_flushes'])==1 and media[123]==ord('X')
assert u.mem_read(c+OFF['ctl_dirty'],1)==b'\0' and state['resets']==state['reads']
assert get(u,0x04000208)==1 and struct.unpack('<H',u.mem_read(0x04000204,2))[0]==0x1234
old=state['writes'];call(u,base+0x12800,[c,0,0,0]);assert state['writes']==old
print('Relocated DS runtime: coalesced writes, short-write refusal, read-back mismatch, retry, cache invalidation and device-owner restoration passed.')


# The production ARM protocol must not announce final completion while its
# EEPROM is still pending verified write-back.
u,c=installation('valid');protocol=get(u,HEAP)+0x11000;out=CODE+0x2000
put(u,c+OFF['ctl_writeSeq'],9);put(u,c+OFF['ctl_flushedSeq'],8)
put(u,c+OFF['ctl_respLen'],8);u.mem_write(c+OFF['ctl_resp'],bytes([0x68])+bytes(7));u.mem_write(out,bytes([0x55])*8)
assert call(u,protocol,[c,0,out,0])==0
assert get(u,c+OFF['ctl_respLen'])==8 and bytes(u.mem_read(out,8))==bytes([0x55])*8
put(u,c+OFF['ctl_flushedSeq'],9)
assert call(u,protocol,[c,0,out,0])==8 and bytes(u.mem_read(out,1))==bytes([0x68])
put(u,c+OFF['ctl_writeSeq'],10);put(u,c+OFF['ctl_respLen'],8);u.mem_write(c+OFF['ctl_resp'],bytes([4])+bytes(7))
assert call(u,protocol,[c,0,out,0])==8 and bytes(u.mem_read(out,1))==bytes([4])
print('Final completion ACK waits for verified storage; pending reply survives retry; nonfinal replies are unaffected.')
