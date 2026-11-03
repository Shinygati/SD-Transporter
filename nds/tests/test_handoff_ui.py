"""Execute linked ARM screen/network guards; no retail data or physical Wi-Fi."""
from pathlib import Path
import runpy,struct
ns=runpy.run_path(str(Path(__file__).with_name('test_native.py')))
u=ns['u'];s=ns['s'];put=ns['put'];get=ns['get'];call=ns['call'];ret=ns['ret']
from unicorn import UC_HOOK_CODE
a,n,i=ns['ly']['handoffOffsets'];section=ns['ls'][i]
LOCK,COUNT,CURSOR,WIFI,SYNC,LOCKED,HOME,LIST=struct.unpack('<8I',ns['ld'][section[4]+a:section[4]+a+n])
UI=s['ui'][0]
events={'begin':0,'stop':0,'poll':0,'notice':0,'lease':0}
names={'ndsWifiSetupBegin':'begin','syncStop':'stop','syncPoll':'poll','uiNotify':'notice','leaseInspect':'lease'}
def hook(uc,address,size,user):
 for name,key in names.items():
  if address==(s[name][0]&~1):
   events[key]+=1;ret(1 if key=='lease' and locked_on_disk[0] else 0);return
for name in names:u.hook_add(UC_HOOK_CODE,hook,begin=s[name][0]&~1,end=s[name][0]&~1)
locked_on_disk=[False]
put(s['opened'][0],0xffffffff);put(UI+COUNT,0);put(UI+CURSOR,0);put(UI+LOCK,0)
put(s['peer'][0],2);call('beginWifiSetup');assert events['begin']==events['stop']==0 and events['notice']==1
put(s['peer'][0],0xffffffff);put(UI+LOCK,1);call('beginWifiSetup');assert events['begin']==events['stop']==0 and events['notice']==2
put(UI+LOCK,0);put(UI+COUNT,1);locked_on_disk[0]=True;call('beginWifiSetup');assert events['begin']==events['stop']==0 and events['notice']==3
locked_on_disk[0]=False;put(UI+COUNT,0);call('beginWifiSetup');assert events['begin']==1 and events['stop']==1
# A stale listener must not be polled on the Wi-Fi screen, even with an old peer.
for screen,peer,expected in [(WIFI,2,'stop'),(WIFI,0xffffffff,'stop'),(HOME,0xffffffff,'stop'),(LIST,0xffffffff,'stop'),(LOCKED,0xffffffff,'stop'),(SYNC,0xffffffff,'poll'),(HOME,2,'poll')]:
 before=dict(events);put(UI+ns['SCREEN'],screen);put(s['peer'][0],peer);call('pollPhoneSync')
 assert events[expected]==before[expected]+1,(screen,peer,expected,events)
 other='poll' if expected=='stop' else 'stop';assert events[other]==before[other]
print('Actual DS ARM handoff guards: active peer/phone lease block Wi-Fi setup; Wi-Fi scan never accepts handoffs; normal Phone sync and response draining remain enabled.')

# Checkout removes the visible/usable console copy; recovery data is separate.
u.mem_write(s['ee'][0],b'X'*65536)
call('onPhoneEvent',1)
assert get(UI+LOCK)==1
assert bytes(u.mem_read(s['ee'][0],65536))==b'\xff'*65536
# Backup discovery derives the original path but never adds a normal walker.
put(s['recoveryCount'][0],0);normal=get(UI+COUNT)
address=0x02374000
for text in [b'fat:/roms/nds/saves/transfers/backups/Pokemon HeartGold.walker.checkout-123.bak',b'fat:/roms/nds/saves/transfers/backups/Pokemon HeartGold.walker.return-456.bak']:
 u.mem_write(address,text+b'\0');call('rememberRecovery',address)
assert get(s['recoveryCount'][0])==1
assert bytes(u.mem_read(s['recoveryPaths'][0],512)).split(b'\0')[0]==b'fat:/roms/nds/saves/Pokemon HeartGold.walker'
assert get(UI+COUNT)==normal
print('Actual DS ARM checkout clears active bitmap/data; backup discovery stays separate from active walkers.')
