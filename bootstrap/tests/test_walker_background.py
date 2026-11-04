"""Host regression check for the console's bounded EEPROM writer."""
from pathlib import Path
import os,subprocess,shutil
root=Path(__file__).resolve().parents[1];build=root/'tests/build';build.mkdir(exist_ok=True)
vswhere=Path(os.environ.get('ProgramFiles(x86)','C:/Program Files (x86)'))/'Microsoft Visual Studio/Installer/vswhere.exe'
vs=subprocess.check_output([str(vswhere),'-latest','-requires','Microsoft.VisualStudio.Component.VC.Tools.x86.x64','-property','installationPath'],text=True).strip()
if not vs:raise SystemExit('Install MSVC C++ Build Tools and the Windows SDK for this host check.')
vc=Path(vs)/'VC/Auxiliary/Build/vcvars64.bat'
env=os.environ.copy()
for line in subprocess.check_output('cmd.exe /d /s /c ""'+str(vc)+'" >nul && set"',text=True).splitlines():
 if '=' in line:
  k,v=line.split('=',1);env[k.upper()]=v

def run(args):
 args=[str(x) for x in args];args[0]=shutil.which(args[0],path=env['PATH']) or args[0]
 subprocess.run(args,env=env,check=True)
includes=['/I'+str(root/'tests/host_walker'),'/I'+str(root/'retail/common/include'),'/I'+str(root/'retail/cardenginei/arm7/include')]
for source,name in [(root/'tests/test_walker_background.c','background-test'),(root/'retail/cardenginei/arm7/source/walker_file.c','background-writer')]:
 run(['cl.exe','/nologo','/O2','/MT','/std:c11',*includes,'/c',source,'/Fo'+str(build/(name+'.obj'))])
run(['link.exe','/nologo',build/'background-test.obj',build/'background-writer.obj','kernel32.lib','/OUT:'+str(build/'background-test.exe')])
run([build/'background-test.exe'])
