.syntax unified
.arch armv4t
.arm
.global gen5DlpArm7Bridge
.global gen5DlpArm7ResetTarget
gen5DlpArm7Bridge:
    ldr r4, shared
    ldr r5, ready
wait_ready:
    ldr r0, [r4, #12]
    cmp r0, r5
    bne wait_ready
    mov r0, #1
    str r0, [r4, #16]
    @ Keep the native MB callback that relocates received ARM9/ARM7 segments.
    ldr r3, native_copy
    mov lr, pc
    bx r3
    mov r0, #1
    ldr r3, gen5DlpArm7ResetTarget
    bx r3
shared: .word 0x02fffa0c
ready: .word 0x44523547
native_copy: .word 0x0380f47d
gen5DlpArm7ResetTarget: .word 0
