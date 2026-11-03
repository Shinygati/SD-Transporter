"""Execute the generated Gen V source policy without retail ROMs or saves.

Run build_gen5_sd_adapter.py first. Needs Unicorn; this validates our adapter
policy, not translated clients or native Japanese/Korean save compatibility.
"""
from pathlib import Path
import json, struct
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM, UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_LR, UC_ARM_REG_SP, UC_ARM_REG_PC

root = Path(__file__).resolve().parents[1]
build = root / 'retail/gen5-transfer/build'
meta = json.loads((build / 'symbols.json').read_text())
symbols = meta['symbols']
cpu = Uc(UC_ARCH_ARM, UC_MODE_ARM)
cpu.mem_map(0x02000000, 0x01000000)
cpu.mem_write(meta['payload_base'], (build / 'adapter.bin').read_bytes())
header, stop = 0x02ed0000, 0x02ee0000
rejected = [False]

def reject(uc, address, size, user):
    rejected[0] = True
    uc.emu_stop()

cpu.hook_add(UC_HOOK_CODE, reject, begin=symbols['failed'], end=symbols['failed'])

def call(name, argument):
    rejected[0] = False
    cpu.reg_write(UC_ARM_REG_SP, 0x02eff000)
    cpu.reg_write(UC_ARM_REG_LR, stop)
    cpu.reg_write(UC_ARM_REG_R0, argument)
    cpu.emu_start(symbols[name], stop, count=10000)
    assert rejected[0] or cpu.reg_read(UC_ARM_REG_PC) == stop
    return cpu.reg_read(UC_ARM_REG_R0)

for prefix, kind in [('ADA', 0), ('APA', 0), ('CPU', 1), ('IPK', 2), ('IPG', 2)]:
    for region in 'EPFDISJK':
        data = bytes(12) + (prefix + region).encode() + bytes(496)
        cpu.mem_write(header, data)
        assert call('gen5_source_classify', header) == kind
        assert not rejected[0]
        assert cpu.mem_read(header, 512) == data

for code in ('CPUZ', 'CPU\x00', 'ZZZE', 'IRBE'):
    cpu.mem_write(header + 12, code.encode())
    call('gen5_source_classify', header)
    assert rejected[0], code

for language in range(11):
    expected = 0 if language in (1, 2, 3, 4, 5, 7, 8) else 3
    assert call('gen5_language', language) == expected
    assert not rejected[0]
assert call('gen5_language', 0xffffffff) == 3
print('Gen IV source families/regions, unchanged headers and language policy passed')
