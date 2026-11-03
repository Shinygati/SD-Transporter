"""Host I/O regression tests against production WalkerStore, without Android SDK.
Requires JDK 17+ on PATH, or JAVA_HOME. Android framework classes are test doubles;
permission, sensor, Activity and real AtomicFile behaviour require device tests.
"""
from pathlib import Path
import os,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
java=Path(os.environ['JAVA_HOME'])/'bin' if os.environ.get('JAVA_HOME') else Path('')
suffix='.exe' if os.name=='nt' else ''
def tool(name):return str(java/(name+suffix)) if str(java)!='.' else name+suffix
with tempfile.TemporaryDirectory(prefix='walker-journal-test-') as directory:
    files=list((root/'tests/host').rglob('*.java'))+[root/'app/src/main/java/io/github/sdtransporter/pokewalker'/name for name in ['WalkerStore.java','WalkerSync.java','WalkerProfiles.java']]
    subprocess.run([tool('javac'),'-encoding','UTF-8','-d',directory,*map(str,files)],check=True)
    subprocess.run([tool('java'),'-cp',directory,'io.github.sdtransporter.pokewalker.StoreTest'],check=True)
    subprocess.run([tool('java'),'-cp',directory,'io.github.sdtransporter.pokewalker.SyncAckTest'],check=True)
    subprocess.run([tool('java'),'-cp',directory,'io.github.sdtransporter.pokewalker.ProfileTest'],check=True)
