from pathlib import Path
import subprocess,struct,os
def sdkpath(value):
 if os.name=='nt':
  if value.startswith('/opt/devkitpro'):value='C:/devkitPro'+value[len('/opt/devkitpro'):]
  elif len(value)>3 and value[0]=='/' and value[2]=='/':value=value[1]+':'+value[2:]
 return Path(value)
ROOT=Path(__file__).resolve().parent
B=ROOT/'retail/gen5-transfer/build';B.mkdir(parents=True,exist_ok=True)
T=sdkpath(os.environ.get('LEGACY_DEVKITARM',os.environ.get('DEVKITARM','/opt/devkitpro/devkitARM')))/'bin'
def tool(name):return str(T/('arm-none-eabi-'+name+('.exe' if os.name=='nt' else '')))
subprocess.run([tool('as'),str(ROOT/'retail/gen5-transfer/dlp_arm7_bridge.s'),'-o',str(B/'dlp_arm7_bridge.o')],check=True)
subprocess.run([tool('ld'),'-Ttext=0x0380f620',str(B/'dlp_arm7_bridge.o'),'-o',str(B/'dlp_arm7_bridge.elf')],check=True)
subprocess.run([tool('objcopy'),'-O','binary',str(B/'dlp_arm7_bridge.elf'),str(B/'dlp_arm7_bridge.bin')],check=True)
data=(B/'dlp_arm7_bridge.bin').read_bytes();assert len(data)==68
words=struct.unpack('<17I',data)
header='/* Generated original ARM7 bridge; GPL-3.0-or-later. */\n#ifndef GEN5_DLP_ARM7_PAYLOAD_H\n#define GEN5_DLP_ARM7_PAYLOAD_H\nstatic const uint32_t gen5DlpArm7BridgeCode[]={'+','.join('0x%08xu'%n for n in words)+'};\n#define GEN5_ARM7_RESET_INDEX 16\n#endif\n'
(ROOT/'retail/common/include/gen5_dlp_arm7_payload.h').write_text(header)
print('Generated ARM7 bridge',len(data),'bytes')
