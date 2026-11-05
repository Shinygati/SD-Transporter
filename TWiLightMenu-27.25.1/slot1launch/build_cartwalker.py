"""Build the separate DSi/3DS cartridge launcher with the legacy libnds SDK.

Set DEVKITARM to the compiler and LIBNDS to libnds 1.8.3 + libfat 1.1.5.
Windows additionally needs native ndstool; no MSYS shell is used.
"""
from pathlib import Path
import os,subprocess,re,sys

root=Path(__file__).resolve().parent
repo=root.parents[1]
universal=root.parent/'universal'
arm=Path(os.environ['DEVKITARM'])
sdk=Path(os.environ['LIBNDS'])
dkp=Path(os.environ.get('DEVKITPRO','C:/devkitPro' if os.name=='nt' else '/opt/devkitpro'))
suffix='.exe' if os.name=='nt' else ''
cc=arm/'bin'/('arm-none-eabi-gcc'+suffix)
cxx=arm/'bin'/('arm-none-eabi-g++'+suffix)
objcopy=arm/'bin'/('arm-none-eabi-objcopy'+suffix)
build=root/'build-cartwalker';build.mkdir(exist_ok=True)
cart=root/'cartwalker'
logs=[]
def run(cmd,cwd=build):
 cmd=[str(x) for x in cmd]
 r=subprocess.run(cmd,cwd=cwd,capture_output=True,text=True)
 logs.append(' '.join(cmd)+'\n'+r.stdout+r.stderr)
 (build/'build.log').write_text('\n'.join(logs),encoding='utf-8')
 if r.returncode:print(logs[-1]);raise SystemExit(r.returncode)

def compile_sources(name,sources,cpu,includes=(),defines=()):
 dest=build/name;dest.mkdir(exist_ok=True);objects=[]
 for i,source in enumerate(sources):
  source=Path(source);obj=dest/(str(i)+'-'+source.name+'.o');objects.append(obj)
  flags=['-O2','-g','-Wall','-ffunction-sections','-fdata-sections','-fno-strict-aliasing',
         '-fstack-usage','-march='+('armv4t' if cpu==7 else 'armv5te'),'-mthumb-interwork','-DCARTWALKER',
         '-DARM'+str(cpu),'-I'+str(sdk/'include'),'-I'+str(cart),'-I'+str(build)]
  flags+=['-I'+str(p) for p in includes]
  flags+=['-I'+str(repo/'bootstrap/retail/common/include')]
  flags+=['-D'+d for d in defines]
  if source.suffix=='.s':flags+=['-x','assembler-with-cpp']
  elif source.suffix=='.cpp':flags+=['-std=gnu++17','-fno-exceptions','-fno-rtti','-mthumb']
  else:flags+=['-std=gnu11','-mthumb']
  run([cxx if source.suffix=='.cpp' else cc,*flags,'-c',source,'-o',obj])
 return objects

def files(path):return sorted(p for p in Path(path).iterdir() if p.suffix in ['.c','.cpp','.s'])
def embed(name,data):
 (build/(name+'.h')).write_text('#pragma once\nextern const unsigned char '+name+'[];\nextern const unsigned '+name+'_size;\n')
 source=build/(name+'.c')
 source.write_text('const unsigned char '+name+'[] __attribute__((aligned(4)))={'+','.join(map(str,data))+'};\nconst unsigned '+name+'_size='+str(len(data))+';\n')
 return source

# Dedicated generated driver: retain upstream licence, bound its polling loops.
driver=(repo/'bootstrap/retail/common/source/my_sdmmc.c').read_text()
needle='while((sdmmc_read16(REG_SDSTATUS1) & TMIO_STAT1_CMD_BUSY));'
assert needle in driver
driver=driver.replace(needle,'unsigned cartTimeout=2000000; while(sdmmc_read16(REG_SDSTATUS1)&TMIO_STAT1_CMD_BUSY) { if(!--cartTimeout){ctx->error=4;return;} }')
needle='u16 status0 = 0;\n\twhile(1)\n\t{'
assert needle in driver
driver=driver.replace(needle,'u16 status0 = 0;\n unsigned cartPoll=4000000;\n\twhile(1)\n\t{\n if(!--cartPoll){ctx->error=4;return;}')
needle='u32 temp2 = 0;\n    do {'
assert needle in driver
driver=driver.replace(needle,'u32 temp2 = 0;\n unsigned cartInit=1024;\n    do {')
needle='// CMD55\n            sdmmc_send_command'
assert needle in driver
driver=driver.replace(needle,'// CMD55\n            if(!--cartInit)return -1;\n            sdmmc_send_command')
bounded=build/'cart_sdmmc.c';bounded.write_text(driver)
inc=[repo/'bootstrap/retail/common/include',repo/'bootstrap/retail/common/include_fat',universal/'include',root/'cardengine_arm7/source',root/'cardengine_arm7/include']
objects=compile_sources('engine',files(root/'cardengine_arm7/source')+[cart/'runtime.c',bounded],7,inc)
engine=build/'cart_engine.elf'
run([cc,'-nostartfiles','-march=armv4t','-T',root/'cardengine_arm7/cardengine.ld',
     '-Wl,--gc-sections','-Wl,-Map='+str(build/'cart_engine.map'),*objects,'-L'+str(sdk/'lib'),'-lnds7','-lc','-lgcc','-o',engine])
enginebin=build/'cart_engine.bin';run([objcopy,'-O','binary',engine,enginebin])
nm=subprocess.check_output([str(arm/'bin'/('arm-none-eabi-nm'+suffix)),str(engine)],text=True)
end=int(next(line.split()[0] for line in nm.splitlines() if line.split()[-1:] == ['__bss_end']),16)
size=(end-0x037c0000+3)&~3
assert enginebin.stat().st_size<=size<=0x10000
enginebin.write_bytes(enginebin.read_bytes().ljust(size,b'\0'))
assert enginebin.stat().st_size<=0x10000

extra=[root/'bootloader/source'/p for p in ['twltool','polarssl','gm9i']]
sources=files(root/'bootloader/source')+files(universal/'source/tonccpy')+[cart/'install.c']
for path in extra:sources+=files(path)
objects=compile_sources('loader',sources,7,[root/'bootloader/source',universal/'include',*extra])
loader=build/'cart_loader.elf'
run([cc,'-nostartfiles','-march=armv4t','-T',root/'bootloader/load.ld','-Wl,--nmagic',
     '-Wl,--gc-sections','-Wl,-Map='+str(build/'cart_loader.map'),*objects,'-L'+str(sdk/'lib'),'-lnds7','-lc','-lgcc','-o',loader])
loaderbin=build/'cart_loader.bin';run([objcopy,'-O','binary',loader,loaderbin])
assert loaderbin.stat().st_size<128*1024

objects=compile_sources('arm7',files(root/'arm7/source'),7,[root/'arm7/include'])
arm7elf=build/'cart_arm7.elf'
run([cc,'-specs=ds_arm7.specs','-march=armv4t',*objects,'-L'+str(sdk/'lib'),'-lnds7','-o',arm7elf])

embedded=[embed('load_bin',loaderbin.read_bytes()),embed('loadAlt_bin',loaderbin.read_bytes()),
          embed('cardengine_arm7_bin',enginebin.read_bytes())]
sources=files(root/'arm9/source')+files(universal/'source/tonccpy')+[
 universal/'source/common/inifile.cpp',universal/'source/common/stringtool.cpp',
 cart/'prepare.cpp',cart/'map.c',repo/'shared/walkerfile.c',repo/'shared/species.c',*embedded]
objects=compile_sources('arm9',sources,9,[root/'arm9/include',universal/'include',
 root/'arm9/source',repo/'shared',repo/'bootstrap/retail/arm9/include'],['STANDALONE','_NO_MAIN_ALL'])
arm9elf=build/'cart_arm9.elf'
run([cxx,'-specs=ds_arm9.specs','-march=armv5te',*objects,'-L'+str(sdk/'lib'),'-lfat','-lnds9',
     '-Wl,--gc-sections','-Wl,-Map='+str(build/'cart_arm9.map'),'-o',arm9elf])
output=root/'Virtual-Pokewalker-Cartridge.nds'
run([dkp/'tools/bin'/('ndstool'+suffix),'-c',output,'-9',arm9elf,'-7',arm7elf,
     '-g','####','00','CARTWALKER','-b',root/'cartwalker-icon.bmp','Virtual Pokewalker;Real HGSS cartridge;SD Transporter'])
assert output.read_bytes()[0x12]==2
compile_sources('layout',[cart/'layout.c'],7)
print(output)
print('Build complete. Physical DSi/3DS cartridge validation is still required.')
