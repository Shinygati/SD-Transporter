from pathlib import Path
import struct,sys,os,subprocess
from unicorn import *
from unicorn.arm_const import *
r=Path(__file__).resolve().parents[1]
def sdkpath(value):
 if os.name=='nt':
  if value.startswith('/opt/devkitpro'):value='C:/devkitPro'+value[len('/opt/devkitpro'):]
  elif len(value)>3 and value[0]=='/' and value[2]=='/':value=value[1]+':'+value[2:]
 return Path(value)
dkp=sdkpath(os.environ.get('DEVKITPRO','C:/devkitPro' if os.name=='nt' else '/opt/devkitpro'))
arm=sdkpath(os.environ.get('DEVKITARM',str(dkp/'devkitARM')))
compiler=arm/'bin'/('arm-none-eabi-gcc.exe' if os.name=='nt' else 'arm-none-eabi-gcc')
subprocess.run([str(compiler),'-DARM9','-D__NDS__','-I'+str(r/'source'),'-I'+str(dkp/'calico/include'),'-I'+str(dkp/'libnds/include'),'-c',str(r/'tests/layout.c'),'-o',str(r/'tests/layout.o')],check=True)
def elf(path):
 d=Path(path).read_bytes();off=struct.unpack_from('<I',d,32)[0];step,count=struct.unpack_from('<HH',d,46)
 sections=[struct.unpack_from('<10I',d,off+i*step) for i in range(count)];symbols={}
 for sh in sections:
  if sh[1]!=2:continue
  st=sections[sh[6]];names=d[st[4]:st[4]+st[5]]
  for i in range(sh[4],sh[4]+sh[5],sh[9]):
   n,a,size,info,other,index=struct.unpack_from('<IIIBBH',d,i)
   if n:symbols[names[n:names.find(b'\0',n)].decode()]=(a,size,index)
 return d,sections,symbols
d,sections,s=elf(r/'Pokewalker-NDS.elf');ld,ls,ly=elf(r/'tests/layout.o');a,n,i=ly['layout'];sh=ls[i];o=struct.unpack('<19I',ld[sh[4]+a:sh[4]+a+n]);N,SCREEN,PHASE,COUNT,CURSOR,NAMES,SECURITY,KEY,KC,BS,SL,SSID,AUTH,FRAME,SELECTION,SS,TP,FL,FS=o
u=Uc(UC_ARCH_ARM,UC_MODE_ARM);u.mem_map(0x02000000,0x400000);u.mem_map(0x02ff0000,0x10000)
for sh in sections:
 if sh[2]&2 and sh[5] and sh[1]!=8 and 0x02000000<=sh[3]<0x02400000:u.mem_write(sh[3],d[sh[4]:sh[4]+sh[5]])
STOP=0x023fff00;STATE=0x02310000;APS=0x02320000;IP=0x02325000
regs=[UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3]
def put(a,v):u.mem_write(a,struct.pack('<I',v))
def get(a):return struct.unpack('<I',u.mem_read(a,4))[0]
def cstring(a):return bytes(u.mem_read(a,512)).split(b'\0')[0]
def ret(v=0):u.reg_write(regs[0],v);u.reg_write(UC_ARM_REG_PC,u.reg_read(UC_ARM_REG_LR))
def call(name,*args):
 u.reg_write(UC_ARM_REG_CPSR,0x13);u.reg_write(UC_ARM_REG_SP,0x023df000);u.reg_write(UC_ARM_REG_LR,STOP)
 for reg,v in zip(regs,args):u.reg_write(reg,v)
 u.emu_start(s[name][0],STOP,count=3000000)
 assert u.reg_read(UC_ARM_REG_PC)==STOP,name
 return u.reg_read(regs[0])
state={'wl':0,'assoc':0,'time':1000,'complete':False,'connect':0,'render':0,'saved':None,'temp':None,'pos':0,'fw':0}
u.mem_write(0x02fffda2,b'\x20');u.mem_write(IP,b'192.168.1.24\0')
for name,mask,index in [('Home WEP',2,0),('Open guest',1,1),('Modern WPA',128,2),('',2,3)]:
 u.mem_write(APS+index*BS+SL,struct.pack('<H',len(name)));u.mem_write(APS+index*BS+SSID,name.encode());u.mem_write(APS+index*BS+AUTH,bytes([mask]))
mocknames=['__syscall_getreent','Wifi_InitDefault','Wifi_AssocStatus','wlmgrGetState','wlmgrStop','wfcBeginScan','wfcGetScanBssList','wfcBeginConnect','wfcClearConnSlots','wfcLoadSlot','wfcLoadFromNvram','wfcBeginAutoConnect','Wifi_GetIPInfo','inet_ntoa','time','mkdir','fopen','fread','fgetc','fwrite','fflush','fclose','sdRenameReplace']
hooks={s[name][0]&~1:name for name in mocknames}
def hook(uc,addr,size,user):
 if addr==(s['ndsUiBegin'][0]&~1):state['render']+=1
 name=hooks.get(addr)
 if not name:return
 a,b,c,e=[uc.reg_read(x) for x in regs]
 if name=='__syscall_getreent':ret(0x02360000)
 elif name=='Wifi_InitDefault':ret(1)
 elif name=='Wifi_AssocStatus':ret(state['assoc'])
 elif name=='wlmgrGetState':ret(state['wl'])
 elif name=='wlmgrStop':state['wl']=0;state['assoc']=0;ret()
 elif name=='wfcBeginScan':state['wl']=3;state['filter_ssid']=bytes(u.mem_read(a+FS,struct.unpack('<H',u.mem_read(a+FL,2))[0]));ret(1)
 elif name=='wfcGetScanBssList':
  put(a,4 if state['complete'] else 0);ret(APS if state['complete'] else 0)
 elif name=='wfcBeginConnect':
  state['connect']+=1;state['chosen']=bytes(u.mem_read(a,BS));state['auth']=bytes(u.mem_read(b,32));state['assoc']=2;ret(1)
 elif name=='wfcLoadFromNvram':state['fw']+=1;ret()
 elif name=='wfcLoadSlot':state['slot']=bytes(u.mem_read(a,256));ret(1)
 elif name=='wfcBeginAutoConnect':ret(1)
 elif name=='Wifi_GetIPInfo':ret(0x1801a8c0)
 elif name=='inet_ntoa':ret(IP)
 elif name=='time':u.reg_write(regs[1],0);ret(state['time'])
 elif name=='fopen':
  mode=cstring(b);state['pos']=0
  ret(0x02340000 if mode==b'wb' or state['saved'] is not None else 0)
 elif name=='fread':
  chunk=state['saved'][:b*c];u.mem_write(a,chunk);state['pos']+=len(chunk);ret(len(chunk)//b)
 elif name=='fgetc':ret(0xffffffff)
 elif name=='fwrite':state['temp']=bytes(u.mem_read(a,b*c));ret(c)
 elif name=='sdRenameReplace':state['saved']=state['temp'];ret()
 else:ret()
u.hook_add(UC_HOOK_CODE,hook)
# Hardware gate: only DS Lite/iQue Lite, excluding original DS and DSi.
for model,expected in [(0x20,1),(0x63,1),(0,0),(0x57,0)]:
 u.mem_write(0x02fffda2,bytes([model]));assert call('ndsWifiAvailable')==expected
u.mem_write(0x02fffda2,b'\x20')
call('ndsWifiSetupBegin',STATE);assert get(STATE+PHASE)==0
call('ndsWifiSetupPoll',STATE);assert get(STATE+PHASE)==0
state['complete']=True;state['wl']=2;call('ndsWifiSetupPoll',STATE)
assert get(STATE+COUNT)==4 and get(STATE+PHASE)==1
assert bytes(u.mem_read(STATE+SECURITY,4))==b'\x01\x00\x02\x01'
put(STATE+CURSOR,2);call('ndsWifiSetupInput',STATE,1);assert state['connect']==0 and get(STATE+PHASE)==1
# WEP key editor and exact validation; START cannot connect with a short key.
put(STATE+CURSOR,0);call('ndsWifiSetupInput',STATE,1);assert get(STATE+PHASE)==2
call('ndsWifiSetupInput',STATE,1);assert cstring(STATE+KEY)==b'0'
call('ndsWifiSetupInput',STATE,1<<10);assert cstring(STATE+KEY)==b''
call('ndsWifiSetupInput',STATE,1<<3);assert state['connect']==0
for key,mode in [('0011223344',1),('00112233445566778899AABBCC',2),('00112233445566778899AABBCCDDEEFF',3)]:
 put(STATE+PHASE,2);state['wl']=2;u.mem_write(STATE+KEY,key.encode()+b'\0')
 call('ndsWifiSetupInput',STATE,1<<3)
 assert get(STATE+PHASE)==3 and state['auth'][:len(key)//2]==bytes.fromhex(key)
 assert state['chosen'][AUTH]==mode
# A completed connection saves a CRC-protected profile and erases the entry buffer.
state['assoc']=4;call('ndsWifiSetupPoll',STATE)
assert get(STATE+PHASE)==4 and state['saved'][:8]==b'PWIFI01\0' and cstring(STATE+KEY)==b''
state['assoc']=0;state['wl']=2;assert call('ndsWifiConnectSaved')==1 and state.get('slot')
fw=state['fw'];state['saved']=b'corrupt'.ljust(len(state['saved']),b'\0');call('ndsWifiConnectSaved');assert state['fw']==fw+1
# Open networks skip the key editor.
state['wl']=2;state['assoc']=0;call('ndsWifiSetupBegin',STATE);call('ndsWifiSetupPoll',STATE)
put(STATE+CURSOR,1);call('ndsWifiSetupInput',STATE,1);assert get(STATE+PHASE)==3 and state['chosen'][AUTH]==0
state['time']+=41;call('ndsWifiSetupPoll',STATE);assert get(STATE+PHASE)==5
assert call('ndsWifiSetupInput',STATE,2)==1
print('Actual ARM Wi-Fi setup: hardware gating, async scan, WPA refusal, WEP 40/104/128 keys, open AP, profile save/reload/corruption fallback, timeout and cancellation passed.')
# Selecting a blank SSID enters the editor before asking for a WEP key.
state['wl']=2;state['assoc']=0;state['complete']=True
call('ndsWifiSetupBegin',STATE);call('ndsWifiSetupPoll',STATE)
put(STATE+CURSOR,3);call('ndsWifiSetupInput',STATE,1);assert get(STATE+PHASE)==6
before=state['connect'];call('ndsWifiSetupInput',STATE,1<<3)
assert get(STATE+PHASE)==6 and state['connect']==before
# Keyboard: literal space, uppercase A, lowercase a, punctuation, deletion.
call('ndsWifiSetupInput',STATE,1);assert cstring(STATE+SS)==b' '
put(STATE+KC,33);call('ndsWifiSetupInput',STATE,1)
call('ndsWifiSetupInput',STATE,1<<8);assert get(STATE+TP)==1
put(STATE+KC,17);call('ndsWifiSetupInput',STATE,1)
put(STATE+KC,46);call('ndsWifiSetupInput',STATE,1)
assert cstring(STATE+SS)==b' Aa~'
call('ndsWifiSetupInput',STATE,1<<10);assert cstring(STATE+SS)==b' Aa'
# A 32-byte name keeps case/spaces/symbols and refuses a 33rd byte.
name=b'Hidden Home-Aa_0123456789-XYZ!abc?Q'[:32]
assert len(name)==32
u.mem_write(STATE+SS,name+b'\0');call('ndsWifiSetupInput',STATE,1)
assert cstring(STATE+SS)==name
call('ndsWifiSetupInput',STATE,1<<3);assert get(STATE+PHASE)==2
u.mem_write(STATE+KEY,b'0011223344\0');call('ndsWifiSetupInput',STATE,1<<3)
assert state['chosen'][SL:SL+2]==struct.pack('<H',32) and state['chosen'][SSID:SSID+32]==name
state['assoc']=4;call('ndsWifiSetupPoll',STATE);assert get(STATE+PHASE)==4
state['assoc']=0;state['wl']=2;call('ndsWifiConnectSaved');assert name in state['slot']
# Y works even before a scan has returned any results, then sends a directed scan.
state['complete']=False;state['wl']=2;call('ndsWifiSetupBegin',STATE)
call('ndsWifiSetupInput',STATE,1<<11);assert get(STATE+PHASE)==6
u.mem_write(STATE+SS,b'Invisible Guest\0');call('ndsWifiSetupInput',STATE,1<<3)
assert get(STATE+PHASE)==0
call('ndsWifiSetupPoll',STATE);assert state['filter_ssid']==b'Invisible Guest'
state['complete']=True;state['wl']=2;call('ndsWifiSetupPoll',STATE)
# If the directed scan reports an empty name, keep the name the user entered.
assert cstring(STATE+NAMES+3*33)==b'Invisible Guest'
put(STATE+CURSOR,3);call('ndsWifiSetupInput',STATE,1);assert get(STATE+PHASE)==2
# Cancelling name entry returns to network selection without connecting.
call('ndsWifiSetupInput',STATE,2);call('ndsWifiSetupInput',STATE,1<<11)
before=state['connect'];assert get(STATE+PHASE)==6
call('ndsWifiSetupInput',STATE,2);assert get(STATE+PHASE)==1 and state['connect']==before
print('Hidden SSID: blank-name selection, empty-name refusal, full keyboard, 32-byte limit, exact-name association/profile reuse, directed scan and cancel passed.')
# Native idle UI must not redraw unchanged frames; changing the selection must.
u.mem_write(STATE,bytes(N));put(STATE+SCREEN,2)
put(s['fontPixels'][0],s['default_font_bin'][0]);put(s['fontCount'][0],256)
call('uiTop',STATE);call('uiBottom',STATE);before=state['render']
for frame in range(1,10):put(STATE+FRAME,frame);call('uiTop',STATE);call('uiBottom',STATE)
assert state['render']==before
put(STATE+SELECTION,1);call('uiBottom',STATE);assert state['render']==before+1
print('Actual ARM native renderer: idle screens reused; menu selection redraws immediately.')
