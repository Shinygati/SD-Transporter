"""Verify the installed SDK's zero-cache path performs direct media I/O."""
from pathlib import Path
import runpy
ns=runpy.run_path(str(Path(__file__).with_name('test_native.py')))
inner=0x02378000
assert ns['call']('dvmDiscCacheCreate',inner,0,8)==inner
print('Installed DS filesystem: zero cache pages returns the underlying disc directly; read-back checks cannot be satisfied by the libdvm write-back cache.')
