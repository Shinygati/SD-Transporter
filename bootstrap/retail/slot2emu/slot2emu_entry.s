@ Virtual GBA Slot-2 blob entry points. GPL-3.0-or-later.
@ Position independent: everything is PC-relative. The control block sits
@ 0x1000 bytes below the blob (see slot2_emu.h).
#include "slot2_emu.h"

	.arm
	.syntax unified
	.section .text.entry, "ax"
	.global _start
_start:
	b	installEntry		@ +0: cardengine trampoline (MPU off, IRQs off)

@ +4: installed as the BIOS debug vector ([0x02FFFD9C] or [0x027FFD9C]).
@ The BIOS calls it in the exception's CPU mode with the MPU disabled.
@ Its save area holds SPSR at +0, the faulting r12 at +4 and LR at +8.
abortEntry:
	adr	r12, saveRegs
	stmia	r12, {r0-r11}
	str	sp, saveSp
	str	lr, saveLr

	mrs	r0, cpsr
	and	r0, r0, #0x1F
	cmp	r0, #0x17		@ only data aborts; undefined instructions etc. pass on
	bne	chain

	adr	r0, _start
	sub	r0, r0, #(SLOT2EMU_BLOB_OFF - SLOT2EMU_CONTROL_OFF)
	ldr	r1, [r0, #SLOT2EMU_CTL_BIOSSTACK]
	ldr	r2, [r1, #4]		@ faulting r12
	str	r2, [r12, #48]
	ldr	r3, [r1, #0]		@ SPSR
	@ Fetch the faulting mode's banked r13/r14 (user mode shares system's).
	and	r4, r3, #0x1F
	cmp	r4, #0x10
	moveq	r4, #0x1F
	mrs	r5, cpsr
	bic	r6, r5, #0x1F
	orr	r6, r6, r4
	msr	cpsr_c, r6
	str	sp, [r12, #52]
	str	lr, [r12, #56]
	msr	cpsr_c, r5
	ldr	r2, [r1, #8]		@ LR_abt
	mov	r1, r12			@ register file r0-r14
	adr	sp, emuStackTop
	bl	slot2EmuAbort
	cmp	r0, #0
	beq	chain

	@ Emulated: resume the game directly after the faulting instruction.
	@ Everything happens with the MPU still off: this blob sits in data-only
	@ memory under the game's MPU setup, so once the MPU is back on it must
	@ not fetch another instruction from here (that was a prefetch abort).
	adr	r12, _start
	sub	r12, r12, #(SLOT2EMU_BLOB_OFF - SLOT2EMU_CONTROL_OFF)
	ldr	r1, [r12, #SLOT2EMU_CTL_BIOSSTACK]
	ldr	r1, [r1]
	msr	spsr_cxsf, r1		@ the game's CPSR, restored by MOVS below
	@ Write back the faulting mode's r13/r14 (an LDM or writeback may change them).
	and	r4, r1, #0x1F
	cmp	r4, #0x10
	moveq	r4, #0x1F
	mrs	r5, cpsr
	bic	r6, r5, #0x1F
	orr	r6, r6, r4
	adr	r2, saveRegs
	msr	cpsr_c, r6
	ldr	sp, [r2, #52]
	ldr	lr, [r2, #56]
	msr	cpsr_c, r5
	@ Belt and braces: let region 3 (narrowed to 0x0C000000-0x0FFFFFFF, where
	@ this blob lives) be executed in privileged modes, so the last fetch
	@ below is legal even if the MPU change takes effect immediately.
	mrc	p15, 0, r2, c5, c0, 3
	bic	r2, r2, #0xF000
	orr	r2, r2, #0x1000
	mcr	p15, 0, r2, c5, c0, 3
	@ CP15 control value with MPU/caches back on, kept in abort-mode sp.
	ldr	r2, [r12, #SLOT2EMU_CTL_CP15CTRL]
	ldr	r3, =0x1005		@ MPU, D-cache and I-cache enables
	and	r2, r2, r3
	mrc	p15, 0, r1, c1, c0, 0
	orr	sp, r1, r2
	mov	lr, r0			@ resume address
	adr	r12, saveRegs
	ldmia	r12, {r0-r12}		@ the game's registers
	mcr	p15, 0, sp, c1, c0, 0	@ MPU back on ...
	movs	pc, lr			@ ... and straight back into the game

chain:
	@ Not ours: hand the exception, untouched, to the previous handler
	@ (the SDK's or the cardengine's), which returns through the BIOS.
	adr	r12, _start
	sub	r12, r12, #(SLOT2EMU_BLOB_OFF - SLOT2EMU_CONTROL_OFF)
	ldr	r12, [r12, #SLOT2EMU_CTL_PREVVECTOR]
	str	r12, chainTarget
	adr	r12, saveRegs
	ldmia	r12, {r0-r11}
	ldr	sp, saveSp
	ldr	lr, saveLr
	ldr	r12, chainTarget
	cmp	r12, #0
	bxne	r12
1:	b	1b			@ no handler at all: stop, as the BIOS would

@ r0 = cardengine valueBits, r1 = CP15 control register value.
installEntry:
	adr	r2, _start
	sub	r2, r2, #(SLOT2EMU_BLOB_OFF - SLOT2EMU_CONTROL_OFF)
	adr	r3, abortEntry
	b	slot2EmuInstall

	.pool
	.balign 4
saveRegs:	.space	15 * 4
saveSp:		.word	0
saveLr:		.word	0
chainTarget:	.word	0
	.balign 8
	.space	0x200
emuStackTop:
