"""CPU tests for the virtual GBA Slot-2 blob (retail/slot2emu).

Runs the real compiled blob on an emulated ARM946E-S (Unicorn). The GBA slot
is left unmapped; every access faults, and the harness performs what the DSi
ARM9 BIOS does on a data abort (save SPSR / r12 / LR at 0x02FFFD90, enter
abort mode, call [0x02FFFD9C]). The blob must emulate the access and resume
the game itself.

Covers: header reads of every width, unaligned word rotation, LDM, open bus,
module-ID image, Macronix flash ID / bank switch / sector erase / byte program
/ readback, wide backup reads, broken command sequences, dirty-sector and
write-sequence tracking, the install routine (vector chaining, MPU region
disable, SDK cartridge record) and chaining of unrelated faults.
"""
import struct, sys, os, subprocess, tempfile
from pathlib import Path
from unicorn import Uc, UcError, UC_ARCH_ARM, UC_MODE_ARM, UC_HOOK_CODE
from unicorn.arm_const import *

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
INC = ROOT / 'retail/common/include/slot2_emu_payload.h'
TEMP = tempfile.TemporaryDirectory(prefix='slot2-tests-')
BUILD = Path(TEMP.name)
TOOLBIN = Path(os.environ.get('LEGACY_DEVKITARM', os.environ.get('DEVKITARM', '/opt/devkitpro/devkitARM'))) / 'bin'
def tool(name):
    path = TOOLBIN / ('arm-none-eabi-' + name + ('.exe' if os.name == 'nt' else ''))
    return str(path) if path.exists() else 'arm-none-eabi-' + name

def load_blob():
    text = INC.read_text()
    body = text[text.index('{') + 1:text.rindex('}')]
    return bytes(int(x, 16) for x in body.replace('\n', ',').split(',') if x.strip())

BLOB = load_blob()
BASE = 0x02F00000                 # reserved area (mirror at +0)
CTL = BASE + 0x20000
ENTRY = BASE + 0x21000
BIOS_SAVE = 0x02FFFD90
VECTOR = 0x02FFFD9C
BIOS_RET = 0x01FF0000             # where the handler would return to the BIOS
PREV = 0x01FF0100                 # previous (SDK) handler
MODULE_INFO = 0x027FFC30
GAMECODE = struct.unpack('<I', b'BPEE')[0]

failures = []
def check(name, got, want):
    if got != want:
        failures.append('%s: got %#x want %#x' % (name, got, want))

def make_cpu():
    uc = Uc(UC_ARCH_ARM, UC_MODE_ARM)
    try:
        uc.ctl_set_cpu_model(UC_CPU_ARM_946)
    except Exception:
        pass
    uc.mem_map(0x02000000, 0x01000000)   # main RAM (incl. reserve, BIOS area)
    uc.mem_map(0x01FF0000, 0x10000)      # stand-ins for BIOS return / old handler
    uc.mem_write(BIOS_RET, struct.pack('<I', 0xEAFFFFFE))  # b .
    uc.mem_write(PREV, struct.pack('<I', 0xEAFFFFFE))
    uc.mem_write(ENTRY, BLOB)
    return uc

def make_control(uc, flash_type=3, size=0x20000):
    header = bytearray(0x200)
    header[0xAC:0xB0] = b'BPEE'
    header[0xB0:0xB2] = b'01'
    header[0xB2] = 0x96
    header[0xB5:0xB8] = b'\x00\x00\x00'
    header[0xBE:0xC0] = struct.pack('<H', 0x1234)
    ctl = bytearray(0x1000)
    struct.pack_into('<7I', ctl, 0, 0x4D453253, 1, flash_type, size, size, BASE, GAMECODE)
    struct.pack_into('<HH', ctl, 0x1C, 0x3130, 0x1234)
    ctl[0x23] = 0xC2 if flash_type == 3 else 0x32
    ctl[0x24] = 0x09 if flash_type == 3 else 0x1B
    struct.pack_into('<H', ctl, 0x28, 0xBEEF)
    struct.pack_into('<I', ctl, 0x3C, PREV)        # prevVector
    struct.pack_into('<I', ctl, 0x40, BIOS_SAVE)   # biosStack
    struct.pack_into('<I', ctl, 0x44, 0)           # cp15ctrl: keep the MPU off here
    ctl[0x100:0x300] = header
    uc.mem_write(CTL, bytes(ctl))
    mirror = bytes(((i & 0xFF) ^ 0x5A) for i in range(size))
    uc.mem_write(BASE, mirror)
    uc.mem_write(VECTOR, struct.pack('<I', ENTRY + 4))
    return mirror

def u32(uc, a):
    return struct.unpack('<I', uc.mem_read(a, 4))[0]

GAME_REGS = [UC_ARM_REG_R0 + i for i in range(13)]

MOVS_PC_LR = struct.pack('<I', 0xE1B0F00E)
assert BLOB.count(MOVS_PC_LR) == 1
RESUME_INSN = ENTRY + BLOB.index(MOVS_PC_LR)   # the blob's return to the game

def bios_abort(uc):
    """Emulate the BIOS data-abort path. Returns (pc after the handler, faulting
    pc, thumb). The handler resumes after the instruction, or at it again when
    it ran a whole byte loop and leaves the last pass to the game."""
    pc = uc.reg_read(UC_ARM_REG_PC)
    cpsr = uc.reg_read(UC_ARM_REG_CPSR)
    thumb = cpsr & 0x20
    uc.mem_write(BIOS_SAVE, struct.pack('<III', cpsr, uc.reg_read(UC_ARM_REG_R12), pc + 8))
    uc.reg_write(UC_ARM_REG_CPSR, 0xD7)          # abort mode, ARM, IRQ/FIQ off
    uc.reg_write(UC_ARM_REG_SP, BIOS_SAVE)
    uc.reg_write(UC_ARM_REG_LR, BIOS_RET)
    uc.reg_write(UC_ARM_REG_R12, u32(uc, VECTOR))
    # 'until' is checked before every instruction: stop on the blob's MOVS,
    # then execute exactly that one instruction. A handler that chains
    # instead ends spinning at PREV ('b .').
    uc.ctl_flush_tb()   # translated blocks carry the old stop address
    uc.emu_start(u32(uc, VECTOR), RESUME_INSN, count=2000000)
    if uc.reg_read(UC_ARM_REG_PC) == RESUME_INSN:
        uc.ctl_flush_tb()
        uc.emu_start(RESUME_INSN, 0, count=1)
    uc.ctl_flush_tb()
    return uc.reg_read(UC_ARM_REG_PC), pc, thumb

def resumed_ok(at, pc, thumb):
    return at in (pc, pc + (2 if thumb else 4))

def run_game(uc, binary):
    uc.mem_write(0x02000000, binary)
    uc.reg_write(UC_ARM_REG_CPSR, 0xDF)          # system mode, like a NitroSDK game
    pc, end, faults = 0x02000000, 0x02000010, 0
    while True:
        try:
            uc.emu_start(pc, end, count=5000000)
            return faults
        except UcError as e:
            if 'UNMAPPED' not in str(e).upper() and 'MEM' not in str(e).upper():
                raise
            faults += 1
            at, fault, thumb = bios_abort(uc)
            if not resumed_ok(at, fault, thumb):
                raise RuntimeError('blob did not resume (stopped at %#x, fault at %#x)' % (at, fault))
            if uc.reg_read(UC_ARM_REG_CPSR) & 0x1F != 0x1F:
                raise RuntimeError('blob returned in the wrong CPU mode')
            pc = at | (1 if thumb else 0)

def test_game(mode):
    elf_syms = {}
    import subprocess
    for line in subprocess.check_output([tool('nm'), str(BUILD / ('game_%s.elf' % mode))], text=True).splitlines():
        p = line.split()
        if len(p) == 3: elf_syms[p[2]] = int(p[0], 16)
    uc = make_cpu()
    mirror = make_control(uc)
    faults = run_game(uc, (BUILD / ('game_%s.bin' % mode)).read_bytes())
    r = struct.unpack('<64I', uc.mem_read(elf_syms['results'], 256))
    tag = '[%s] ' % mode
    check(tag + 'fixed byte', r[0], 0x96)
    check(tag + 'game code', r[1], GAMECODE)
    check(tag + 'maker', r[2], 0x3130)
    check(tag + 'module id', r[3], 0x1234)
    check(tag + 'module image', r[4], 0xBEEF)
    check(tag + 'open bus', r[5], 0x1A2B)
    check(tag + 'unaligned rotate', r[6], ((GAMECODE >> 8) | (GAMECODE << 24)) & 0xFFFFFFFF)
    check(tag + 'ldmia header', r[7], GAMECODE)
    check(tag + 'flash id', r[8], 0x09C2)
    check(tag + 'erase', r[9], 1)
    check(tag + 'program', r[10], 1)
    want = 0
    for i in range(0x1000):
        want = (want * 31 + ((i * 7 + 3) & 0xFF)) & 0xFFFFFFFF
    check(tag + 'readback sum', r[11], want)
    check(tag + 'bank 0 untouched', r[12], mirror[0x3000])
    check(tag + 'wide read repeats byte', r[13], mirror[0x10] * 0x0101)
    check(tag + 'broken command ignored', r[14], mirror[4])
    check(tag + 'finished', r[15], 0xC0FFEE)
    check(tag + 'unused slot space reads open bus', r[16], 0xFF)
    m = bytes(uc.mem_read(BASE, 0x20000))
    check(tag + 'mirror sector written', int(m[0x13000:0x14000] == bytes(((i * 7 + 3) & 0xFF) for i in range(0x1000))), 1)
    check(tag + 'rest of mirror unchanged', int(m[:0x13000] == mirror[:0x13000] and m[0x14000:] == mirror[0x14000:]), 1)
    dirty = bytes(uc.mem_read(CTL + 0x7C, 32))
    check(tag + 'only sector 0x13 dirty', int(dirty == bytes(1 if i == 0x13 else 0 for i in range(32))), 1)
    check(tag + 'write sequence', u32(uc, CTL + 0x58), 1 + 0x1000)
    check(tag + 'no unhandled faults', u32(uc, CTL + 0x60), 0)
    check(tag + 'fault count', u32(uc, CTL + 0x5C) + u32(uc, CTL + 0x64), faults)
    print('%s: %d emulated slot accesses' % (mode, faults))

def assemble(source, address):
    import subprocess, tempfile, os
    with tempfile.TemporaryDirectory() as d:
        s = os.path.join(d, 't.s'); o = os.path.join(d, 't.o'); b = os.path.join(d, 't.bin')
        Path(s).write_text('.syntax unified\n.arm\n' + source + '\n')
        subprocess.run([tool('as'), '-mcpu=arm946e-s', s, '-o', o], check=True)
        subprocess.run([tool('ld'), '-Ttext=0x%x' % address, o, '-o', o + '.elf'], check=True)
        subprocess.run([tool('objcopy'), '-O', 'binary', o + '.elf', b], check=True)
        return Path(b).read_bytes()

def test_install():
    uc = make_cpu()
    make_control(uc)
    uc.mem_write(VECTOR, struct.pack('<I', 0x0200BEEF))       # the SDK's handler
    uc.mem_write(MODULE_INFO, b'\xff' * 12)
    code = assemble('''
        ldr sp, =0x02300000
        ldr r0, =0x04000033      @ region 0: I/O 64 MiB
        mcr p15, 0, r0, c6, c0, 0
        ldr r0, =0x0200002B      @ region 1: main RAM 4 MiB
        mcr p15, 0, r0, c6, c1, 0
        ldr r0, =0x08000035      @ region 3: GBA slot + DSi extra RAM, 128 MiB
        mcr p15, 0, r0, c6, c3, 0
        ldr r0, =0x08000031      @ region 4: a 32 MiB slot-only region
        mcr p15, 0, r0, c6, c4, 0
        ldr r0, =0x00000035      @ region 5: 128 MiB background at 0 (no overlap)
        mcr p15, 0, r0, c6, c5, 0
        mov r0, #0x800           @ valueBits: dsiBios
        ldr r1, =0x00012078      @ CP15 control value
        ldr r2, =0x%x
        mov lr, pc
        bx r2
        mrc p15, 0, r0, c6, c0, 0
        mrc p15, 0, r1, c6, c1, 0
        mrc p15, 0, r3, c6, c3, 0
        mrc p15, 0, r5, c6, c4, 0
        mrc p15, 0, r6, c6, c5, 0
        ldr r4, =0x02100000
        stmia r4, {r0, r1, r3, r5, r6}
    done: b done
        .pool
    ''' % ENTRY, 0x02000000)
    uc.mem_write(0x02000000, code)
    uc.reg_write(UC_ARM_REG_CPSR, 0xDF)
    done = 0x02000000 + code.index(struct.pack('<I', 0xEAFFFFFE))
    uc.emu_start(0x02000000, done, count=100000)
    regions = struct.unpack('<5I', uc.mem_read(0x02100000, 20))
    check('install: vector', u32(uc, VECTOR), ENTRY + 4)
    check('install: previous handler kept', u32(uc, CTL + 0x3C), 0x0200BEEF)
    check('install: bios save area', u32(uc, CTL + 0x40), BIOS_SAVE)
    check('install: cp15 control', u32(uc, CTL + 0x44), 0x12078)
    check('install: region 0 kept', regions[0], 0x04000033)
    check('install: region 1 kept', regions[1], 0x0200002B)
    check('install: region 3 narrowed to extra RAM', regions[2], 0x0C000033)
    check('install: slot-only region disabled', regions[3], 0x08000030)
    check('install: unrelated region kept', regions[4], 0x00000035)
    info = bytes(uc.mem_read(MODULE_INFO, 12))
    check('install: module id', struct.unpack('<H', info[0:2])[0], 0x1234)
    check('install: agb flag set, pull-out clear', info[5] & 3, 1)
    check('install: maker', struct.unpack('<H', info[6:8])[0], 0x3130)
    check('install: game code', struct.unpack('<I', info[8:12])[0], GAMECODE)
    # A second install must not chain to itself.
    uc.emu_start(0x02000000, done, count=100000)
    check('install: idempotent', u32(uc, CTL + 0x3C), 0x0200BEEF)

def test_chain():
    """A fault outside the slot goes to the previous handler with state intact."""
    uc = make_cpu()
    make_control(uc)
    code = assemble('''
        ldr sp, =0x02300000
        ldr r1, =0x07000000
        mov r0, #11
        mov r2, #22
        ldr r3, [r1]
    done: b done
        .pool
    ''', 0x02000000)
    uc.mem_write(0x02000000, code)
    uc.reg_write(UC_ARM_REG_CPSR, 0xDF)
    try:
        uc.emu_start(0x02000000, 0x02000000 + 20, count=1000)
        failures.append('chain: expected a fault')
        return
    except UcError:
        pass
    at, _, _ = bios_abort(uc)
    check('chain: reached previous handler', at, PREV)
    check('chain: r0 kept', uc.reg_read(UC_ARM_REG_R0), 11)
    check('chain: r2 kept', uc.reg_read(UC_ARM_REG_R2), 22)
    check('chain: sp kept', uc.reg_read(UC_ARM_REG_SP), BIOS_SAVE)
    check('chain: lr kept', uc.reg_read(UC_ARM_REG_LR), BIOS_RET)
    check('chain: counted', u32(uc, CTL + 0x60), 1)

def test_sram_and_flash64():
    for flash_type, size, ident in ((1, 0x8000, None), (2, 0x10000, 0x1B32)):
        uc = make_cpu()
        mirror = make_control(uc, flash_type, size)
        code = assemble('''
            ldr sp, =0x02300000
            ldr r1, =0x0A000000
            mov r0, #0xAA
            ldr r2, =0x5555
            strb r0, [r1, r2]
            mov r0, #0x55
            ldr r2, =0x2AAA
            strb r0, [r1, r2]
            mov r0, #0x90
            ldr r2, =0x5555
            strb r0, [r1, r2]
            ldrb r4, [r1]
            ldrb r5, [r1, #1]
            mov r0, #0xF0
            strb r0, [r1]
            mov r0, #0x77
            strb r0, [r1, #0x40]
            ldrb r6, [r1, #0x40]
            ldr r7, =0x02100000
            stmia r7, {r4, r5, r6}
        done: b done
            .pool
        ''', 0x02000000)
        done = 0x02000000 + code.index(struct.pack('<I', 0xEAFFFFFE))
        uc.mem_write(0x02000000, code)
        uc.reg_write(UC_ARM_REG_CPSR, 0xDF)
        pc = 0x02000000
        while True:
            try:
                uc.emu_start(pc, done, count=10000); break
            except UcError:
                at, fault, thumb = bios_abort(uc)
                if not resumed_ok(at, fault, thumb): raise RuntimeError('no resume')
                pc = at
        a, b, c = struct.unpack('<3I', uc.mem_read(0x02100000, 12))
        name = 'sram' if flash_type == 1 else 'flash64'
        if ident is None:
            # SRAM has no command set: every store lands in memory.
            check(name + ': store/load', c, 0x77)
            check(name + ': read before store', a, mirror[0])
            check(name + ': store reached mirror', uc.mem_read(BASE, 1)[0], 0xF0)
            check(name + ': command bytes are plain data', uc.mem_read(BASE + 0x5555, 1)[0], 0x90)
        else:
            check(name + ': id', a | (b << 8), ident)
            check(name + ': plain store ignored', c, mirror[0x40])

def run_asm(uc, code, done, anywhere=False):
    uc.mem_write(0x02000000, code)
    uc.reg_write(UC_ARM_REG_CPSR, 0xDF)
    pc, faults = 0x02000000, 0
    while True:
        try:
            uc.emu_start(pc, done, count=2000000)
            return faults
        except UcError:
            faults += 1
            at, fault, thumb = bios_abort(uc)
            if at == PREV or not (anywhere or resumed_ok(at, fault, thumb)):
                raise RuntimeError('no resume (at %#x, fault %#x)' % (at, fault))
            pc = at

# The two byte loops from NitroSDK's CTRDG flash library, instruction for
# instruction (CTRDGi_ReadAgbFlashCore / CTRDGi_VerifyFlashSectorCore).
SDK_LOOPS = '''
    b start
read:                       @ r1 flash, r5 dst, r4 count
    cmp r4, #0x0
    sub r4, r4, #0x1
    beq 2f
1:  ldrb r0, [r1], #0x1
    cmp r4, #0x0
    sub r4, r4, #0x1
    strb r0, [r5], #0x1
    bne 1b
2:  bx lr
verify:                     @ r0 ram, r1 flash, r2 count
    cmp r2, #0x0
    sub r2, r2, #0x1
    beq 4f
3:  ldrb r12, [r1], #0x1
    ldrb r3, [r0], #0x1
    cmp r12, r3
    subne r0, r1, #0x1
    bxne lr
    cmp r2, #0x0
    sub r2, r2, #0x1
    bne 3b
4:  mov r0, #0x0
    bx lr
start:
    ldr sp, =0x02300000
'''

def test_bulk():
    """The SDK's byte loops run in a handful of aborts with the same result."""
    for length in (0x1000, 0x2345, 1, 2):
        uc = make_cpu()
        mirror = make_control(uc)
        code = assemble(SDK_LOOPS + '''
            ldr r1, =0x0A000123
            ldr r5, =0x02100000
            ldr r4, =%d
            bl read
            ldr r7, =0x02200000
            stmia r7, {r0, r1, r4, r5}
            ldr r0, =0x02100000
            ldr r1, =0x0A000123
            ldr r2, =%d
            bl verify
            str r0, [r7, #16]
        done: b done
            .pool
        ''' % (length, length), 0x02000000)
        done = 0x02000000 + code.index(struct.pack('<I', 0xEAFFFFFE))
        faults = run_asm(uc, code, done)
        tag = 'bulk %#x: ' % length
        got = bytes(uc.mem_read(0x02100000, length))
        check(tag + 'copied', int(got == mirror[0x123:0x123 + length]), 1)
        r0, r1, r4, r5, ver = struct.unpack('<5I', uc.mem_read(0x02200000, 20))
        check(tag + 'last byte', r0, mirror[0x123 + length - 1])
        check(tag + 'src end', r1, 0x0A000123 + length)
        check(tag + 'count end', r4, 0xFFFFFFFF)
        check(tag + 'dst end', r5, 0x02100000 + length)
        check(tag + 'verify ok', ver, 0)
        check(tag + 'no unhandled', u32(uc, CTL + 0x60), 0)
        bound = 2 * (2 + length // 0x1000)
        if faults > bound:
            failures.append(tag + '%d faults, expected at most %d' % (faults, bound))
        print('bulk %#x bytes: %d aborts for copy + verify' % (length, faults))
    # A mismatch is reported at the right address.
    uc = make_cpu()
    mirror = make_control(uc)
    uc.mem_write(0x02100000, mirror[0x200:0x1200])
    uc.mem_write(0x02100000 + 0x777, bytes([mirror[0x200 + 0x777] ^ 1]))
    code = assemble(SDK_LOOPS + '''
        ldr r0, =0x02100000
        ldr r1, =0x0A000200
        ldr r2, =0x1000
        bl verify
        ldr r7, =0x02200000
        str r0, [r7]
    done: b done
        .pool
    ''', 0x02000000)
    done = 0x02000000 + code.index(struct.pack('<I', 0xEAFFFFFE))
    run_asm(uc, code, done)
    check('bulk verify: mismatch address', u32(uc, 0x02200000), 0x0A000200 + 0x777)

# CTRDGi_WriteFlashSectorCoreMX's byte loop and CTRDGi_ProgramFlashByteMX,
# instruction for instruction around the parts the blob matches.
SDK_PROGRAM = '''
    b start
program:                    @ r0 src, r1 flash address
    stmdb sp!, {lr}
    sub sp, sp, #0x4
    ldr ip, =0x0A005555
    mov r3, #0xaa
    ldr r2, =0x0A002AAA
    strb r3, [r12, #0x0]
    mov r3, #0x55
    mov lr, r0
    strb r3, [r2, #0x0]
    mov r0, #0xa0
    strb r0, [r12, #0x0]
    ldrb r2, [lr, #0x0]
    ldr r3, =polling
    mov r0, #0x1
    strb r2, [r1, #0x0]
    ldrb r2, [lr, #0x0]
    ldr r3, [r3, #0x0]
    blx r3
    add sp, sp, #0x4
    ldmia sp!, {lr}
    bx lr
    .pool
polling: .word poll
poll:                       @ r1 address, r2 expected
    ldrb r0, [r1]
    cmp r0, r2
    bne poll
    mov r0, #0
    bx lr
sector:                     @ r4 src, r5 flash address, r8 -> remaining
    stmdb sp!, {r4-r8,lr}
    ldrh r1, [r8, #0x0]
    cmp r1, #0x0
    beq 6f
5:  mov r0, r4
    mov r1, r5
    bl program
    movs r6, r0
    bne 6f
    ldrh r0, [r8, #0x0]
    add r4, r4, #0x1
    add r5, r5, #0x1
    sub r0, r0, #0x1
    strh r0, [r8, #0x0]
    ldrh r0, [r8, #0x0]
    cmp r0, #0x0
    bne 5b
6:  ldr r7, =0x02200000
    str r4, [r7]
    str r5, [r7, #4]
    str r6, [r7, #8]
    ldmia sp!, {r4-r8,lr}
    bx lr
    .pool
start:
    ldr sp, =0x02300000
'''

def test_program(modern=False, registers=None, length=0x1000, bank=0, fallback=None):
    """Sector programming runs in one abort; a lone byte in one abort."""
    uc = make_cpu()
    mirror = make_control(uc)
    uc.mem_write(CTL + 0x54, struct.pack('<I', bank))
    data = bytes(((i * 13 + 5) & 0xFF) for i in range(length))
    uc.mem_write(0x02100000, data)
    source = SDK_PROGRAM + '''
        ldr r8, =0x02280000
        ldr r0, =%d
        strh r0, [r8]
        ldr r4, =0x02100000
        ldr r5, =0x0A00A000
        bl sector
        mrs r0, cpsr
        ldr r7, =0x02200000
        str r0, [r7, #20]
        ldr r0, =0x02100010
        ldr r1, =0x0A000040
        bl program
        ldr r7, =0x02200000
        str r0, [r7, #12]
        str sp, [r7, #16]
    done: b done
        .pool
    ''' % length
    if modern:
        source = source.replace('stmdb sp!, {lr}\n    sub sp, sp, #0x4', 'stmdb sp!, {r3,lr}')
        source = source.replace('add sp, sp, #0x4\n    ldmia sp!, {lr}\n    bx lr', 'ldmia sp!, {r3,pc}')
    if registers:
        import re
        source = source.replace('r4-r8', 'r4,r5,r6,r7,r8')
        source = re.sub(r'\br[4-8]\b', lambda m: registers.get(m[0], m[0]), source)
        # PUSH/POP lists are sets; keep them sorted after the permutation.
        source = re.sub(r'\{r[4-8](?:,r[4-8])+,lr\}', '{r4,r5,r6,r7,r8,lr}', source)
    if fallback == 'sector':
        source = source.replace('movs r6, r0', 'adds r6, r0, #0')
    elif fallback == 'byte':
        source = source.replace('mov r0, #0x1', 'mov r0, #0x2')
    code = assemble(source, 0x02000000)
    done = 0x02000000 + code.index(struct.pack('<I', 0xEAFFFFFE))
    faults = run_asm(uc, code, done, anywhere=True)
    r4, r5, r6, single, sp = struct.unpack('<5I', uc.mem_read(0x02200000, 20))
    m = bytes(uc.mem_read(BASE, 0x20000))
    offset = bank * 0x10000
    check('program: sector in mirror', int(m[offset+0xA000:offset+0xA000+length] == data), 1)
    check('program: single byte', m[offset+0x40], data[0x10])
    expected = bytearray(mirror)
    expected[offset+0xA000:offset+0xA000+length] = data
    expected[offset+0x40] = data[0x10]
    check('program: nothing else changed', int(m == expected), 1)
    check('program: src end', r4, 0x02100000 + length)
    check('program: dst end', r5, 0x0A00A000 + length)
    check('program: result', r6, 0)
    check('program: single result', single, 0)
    check('program: stack balanced', sp, 0x02300000)
    check('program: remaining cleared', struct.unpack('<H', uc.mem_read(0x02280000, 2))[0], 0)
    check('program: final caller flags', u32(uc, 0x02200000 + 20) & 0xE0000000, 0x60000000)
    dirty = bytes(uc.mem_read(CTL + 0x7C, 32))
    check('program: dirty sectors', int(dirty == bytes(1 if i in (bank*16, bank*16+0xA) else 0 for i in range(32))), 1)
    expected_faults = 5 * (length + 1) if fallback == 'byte' else length + 1 if fallback == 'sector' else 2
    check('program: bounded aborts', faults, expected_faults)
    check('program: no unhandled', u32(uc, CTL + 0x60), 0)
    print('program %#x + 1 bytes (modern=%s, registers=%s, bank=%d): %d aborts' % (length, modern, registers, bank, faults))


def test_read_edges():
    """Array fast copies preserve byte-bus boundaries, ID mode and alignment."""
    cases = [(3, 0x20000, 0, 0, 0, 4096, align) for align in range(4)]
    cases += [(3,0x20000,1,0,0x123,4097,3), (3,0x20000,0,0,0xfff0,16,0),
              (2,0x10000,0,0,0x100,64,0), (2,0x10000,1,0,0x100,64,0),
              (1,0x8000,0,0,0x7ff0,64,0), (1,0x8000,0,0,0xffe0,32,0),
              (3,0x20000,0,1,0,64,0), (2,0x10000,0,1,0,64,0)]
    cases += [(3,0x20000,0,0,offset,length,offset&3)
              for length in (1,2,3,4,31,32,33,4095,4096,8193)
              for offset in (0,1,2,3)]
    for chip,size,bank,mode,off,length,alignment in cases:
        uc=make_cpu(); mirror=make_control(uc,chip,size)
        uc.mem_write(CTL+0x50,struct.pack('<II',mode,bank))
        dst=0x02100000+alignment
        uc.mem_write(dst-8,b'\xa5'*(length+16))
        code=assemble(SDK_LOOPS+'''
            ldr r1, =%d
            ldr r5, =%d
            ldr r4, =%d
            bl read
        done: b done
            .pool
        '''%(0x0a000000+off,dst,length),0x02000000)
        done=0x02000000+code.index(struct.pack('<I',0xEAFFFFFE))
        faults=run_asm(uc,code,done)
        expected=bytearray()
        for k in range(length):
            a=off+k
            if chip==1: expected.append(mirror[a&(size-1)])
            elif mode and a<2:expected.append((0xc2 if chip==3 else 0x32) if a==0 else (9 if chip==3 else 0x1b))
            elif bank*0x10000+a>=size:expected.append(255)
            else:expected.append(mirror[bank*0x10000+a])
        assert uc.mem_read(dst,length)==expected,(chip,bank,mode,off,length,alignment)
        assert uc.mem_read(dst-8,8)==b'\xa5'*8 and uc.mem_read(dst+length,8)==b'\xa5'*8
        assert bytes(uc.mem_read(BASE,size))==mirror
        assert u32(uc,CTL+0x58)==0 and uc.mem_read(CTL+0x7c,32)==bytes(32)
        assert uc.reg_read(UC_ARM_REG_R1)==0x0a000000+off+length
        assert uc.reg_read(UC_ARM_REG_R5)==dst+length
        assert uc.reg_read(UC_ARM_REG_R4)==0xffffffff
        assert uc.reg_read(UC_ARM_REG_R0)==expected[-1]
        assert u32(uc,CTL+0x60)==0
    print('array read alignment/boundary/ID-mode checks:',len(cases),'passed')

if __name__ == '__main__':
    for mode in ('arm', 'thumb'):
        elf = BUILD / ('game_%s.elf' % mode)
        subprocess.run([tool('gcc'), '-mcpu=arm946e-s', '-m'+mode, '-mthumb-interwork', '-O2', '-ffreestanding', '-nostdlib', '-nostartfiles', '-T', str(HERE/'slot2_test_game.ld'), str(HERE/'slot2_test_start.s'), str(HERE/'slot2_test_game.c'), '-o', str(elf)], check=True)
        subprocess.run([tool('objcopy'), '-O', 'binary', str(elf), str(elf.with_suffix('.bin'))], check=True)
    test_read_edges()

    test_game('arm')
    test_game('thumb')
    test_install()
    test_chain()
    test_sram_and_flash64()
    test_bulk()
    test_program()
    test_program(modern=True, registers={'r4': 'r7', 'r5': 'r4', 'r7': 'r5'}, bank=1)
    test_program(registers={'r4': 'r6', 'r5': 'r4', 'r6': 'r8', 'r8': 'r7', 'r7': 'r5'})
    test_program(modern=True, length=32, fallback='sector')
    test_program(modern=True, length=32, fallback='byte')
    if failures:
        print('\n'.join(failures))
        sys.exit('%d failure(s)' % len(failures))
    print('all slot-2 blob checks passed')
