# Execute the real installed DS accept() API, not a POSIX/Winsock substitute.
from pathlib import Path
import runpy,struct
ns=runpy.run_path(str(Path(__file__).with_name('test_native.py')))
u=ns['u'];s=ns['s'];call=ns['call'];put=ns['put'];get=ns['get'];ret=ns['ret'];regs=ns['regs'];state=ns['state']
from unicorn import UC_HOOK_CODE
ERR=0x02372000;RECORD=0x02371000;ADDRESS=0x02373000;LENGTH=ADDRESS+32
put(s['socketlist'][0],0xe001);put(s['socketlist'][0]+4,0x02370000)
u.mem_write(RECORD+0x32,struct.pack('>H',12345));put(RECORD+0x2c,0x04030201)
events={'libc_close':0,'socket_close':0,'accepted':0}
def socket_hook(uc,address,size,user):
 if address==(s['__errno'][0]&~1):ret(ERR)
 elif address==(s['mutexUnlock'][0]&~1):ret()
 elif address==(s['sgIP_TCP_Accept'][0]&~1):
  events['accepted']+=1;ret(RECORD)
 elif address==(s['recv'][0]&~1):put(ERR,11);ret(0xffffffff)
 elif address==(s['closesocket'][0]&~1):events['socket_close']+=1;ret()
 elif 'close' in s and address==(s['close'][0]&~1):events['libc_close']+=1;ret()
u.hook_add(UC_HOOK_CODE,socket_hook)
assert call('accept',1,0,0)==0xffffffff and get(ERR)==22
print('Installed DS accept rejects NULL address arguments with EINVAL.')
# Real accept + real ioctl must accept a queued Android connection through syncPoll.
put(s['networkState'][0],2);put(s['listener'][0],1);put(s['peer'][0],0xffffffff)
call('syncPoll')
assert get(s['peer'][0])==2, ('server did not accept Android',hex(get(s['peer'][0])),events,get(ERR),hex(get(s['socketlist'][0])),get(s['networkState'][0]))
assert events['accepted']==1
call('syncStop')
assert events['socket_close']==2 and events['libc_close']==0,'NDS sockets must use closesocket, not file close'
print('Actual DS transport: Android connection accepted; nonblocking poll works; listener/peer closed with closesocket.')
