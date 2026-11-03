"""Build our relocatable SD adapter and verified received-client layout table.
Set DEVKITARM to a devkitARM installation; no retail executable is an input.
"""
from pathlib import Path
import struct,subprocess,json,os
ROOT=Path(__file__).resolve().parent
B=ROOT/'retail/gen5-transfer/build';B.mkdir(parents=True,exist_ok=True)
C=ROOT/'retail/common/include'
def sdkpath(value):
 if os.name=='nt':
  if value.startswith('/opt/devkitpro'):value='C:/devkitPro'+value[len('/opt/devkitpro'):]
  elif len(value)>3 and value[0]=='/' and value[2]=='/':value=value[1]+':'+value[2:]
 return Path(value)
T=sdkpath(os.environ.get('LEGACY_DEVKITARM',os.environ.get('DEVKITARM','/opt/devkitpro/devkitARM')))/'bin'
def tool(name):
 p=T/('arm-none-eabi-'+name)
 return str(p.with_suffix('.exe') if os.name=='nt' else p)
BASE=0x0212c5a0
def build(base,suffix):
 ld=B/('adapter'+suffix+'.ld');elf=B/('adapter'+suffix+'.elf');raw=B/('adapter'+suffix+'.bin')
 ld.write_text('SECTIONS { . = 0x%x; .text : { *(.text*) *(.rodata*) *(.data*) } .bss : { *(.bss*) *(COMMON) } adapter_end = .; /DISCARD/ : { *(.comment*) *(.ARM.attributes*) } }\n'%base)
 subprocess.run([tool('gcc'),'-mcpu=arm946e-s','-marm','-Os','-ffreestanding','-fno-builtin','-fno-unwind-tables','-fno-asynchronous-unwind-tables','-nostdlib','-nostartfiles','-Wall','-Wextra','-Wl,-T,'+str(ld),str(ROOT/'retail/gen5-transfer/sd_adapter.c'),'-o',str(elf)],check=True)
 subprocess.run([tool('objcopy'),'-O','binary',str(elf),str(raw)],check=True)
 nm=subprocess.check_output([tool('nm'),'-an',str(elf)],text=True)
 syms={v[2]:int(v[0],16) for l in nm.splitlines() if len(v:=l.split())==3}
 arena=(syms['adapter_end']+31)&~31
 data=raw.read_bytes().ljust(arena-base,b'\0');raw.write_bytes(data)
 return data,syms,arena
data,syms,arena=build(BASE,'')
other,_,_=build(BASE+0x10000,'-relocation-check')
assert len(data)==len(other) and len(data)%4==0
relocations=[]
for off in range(0,len(data),4):
 a,b=struct.unpack_from('<I',data,off)[0],struct.unpack_from('<I',other,off)[0]
 if a!=b:
  assert BASE<=a<arena and b-a==0x10000,(hex(off),hex(a),hex(b))
  relocations.append(off)
profiles=json.loads((ROOT/'retail/gen5-transfer/client_profiles.json').read_text())['profiles']
hooks={'header':'gen5_header','header_alt':'gen5_header','rom_raw':'gen5_rom_raw','rom_read':'gen5_rom_read','backup_worker':'gen5_backup_worker','ack':'gen5_ack','id':'gen5_id','id_alt':'gen5_id','present':'gen5_present','noop':'gen5_noop','init':'gen5_noop','terminate':'gen5_noop','enabled':'gen5_enabled','card_init':'gen5_noop','pullout':'gen5_noop'}
h='/* Generated original SD adapter; no retail executable bytes. */\n#ifndef GEN5_ADAPTER_PAYLOAD_H\n#define GEN5_ADAPTER_PAYLOAD_H\n'
h+=f'#define GEN5_PAYLOAD_BASE 0x{BASE:08x}u\n#define GEN5_ARENA_LO 0x{arena:08x}u\n#define GEN5_LANGUAGE_OFFSET 0x{syms["gen5_language"]-BASE:x}u\n#define GEN5_CLASSIFY_OFFSET 0x{syms["gen5_source_classify"]-BASE:x}u\n'
h+='static const unsigned char gen5Payload[]={'+','.join(str(x) for x in data)+'};\n'
h+='static const unsigned short gen5Relocations[]={'+','.join(hex(x) for x in relocations)+'};\n'
legacy={profiles[0]['addresses'][name]:fn for name,fn in hooks.items()}
h+='static const unsigned int gen5Hooks[][2]={'+','.join('{0x%xu,0x%xu}'%(a,syms[s]) for a,s in legacy.items())+'};\n'
h+='#define GEN5_LANGUAGE_BL 0x%08xu\n#endif\n'%(0xeb000000|(((syms['gen5_language']-profiles[0]['addresses']['language']-8)//4)&0xffffff))
(C/'gen5_adapter_payload.h').write_text(h)
from gen5_profile_codec import emit
(C/'gen5_client_profiles.h').write_text(emit(profiles,hooks,syms,BASE))

# Bootloaders only need compact identities; keep installer hooks out of their
# limited VRAM/stack area as the regional profile count grows.
identity='/* Generated verified regional client identities. */\n#ifndef GEN5_CLIENT_IDENTITIES_H\n#define GEN5_CLIENT_IDENTITIES_H\n#include <stdint.h>\n'
identity+='typedef struct { uint32_t base,entry,length,module,bssStart,originalArena,sdk,codeCRC; } Gen5ClientIdentity;\nstatic const Gen5ClientIdentity gen5ClientIdentities[]={\n'
for p in profiles:
 vals=[p[x] for x in ('base','entry','length','module','bss_start','original_arena','sdk','code_crc')]
 identity+=' /* '+p['key']+' */ {'+','.join('0x%xu'%v for v in vals)+'},\n'
identity+='};\n#define GEN5_CLIENT_IDENTITY_COUNT (sizeof(gen5ClientIdentities)/sizeof(gen5ClientIdentities[0]))\n#endif\n'
(C/'gen5_client_identities.h').write_text(identity)

(B/'symbols.json').write_text(json.dumps({'symbols':syms,'arena_lo':arena,'payload_base':BASE,'relocations':relocations,'hooks':{hex(a):s for a,s in legacy.items()},'hook_names':hooks,'payload_bytes':len(data),'profiles':profiles},indent=2)+'\n')
print('SD adapter',len(data),'bytes;',len(relocations),'relocations;',len(profiles),'verified client layouts')
