@ Virtual Pokéwalker: replacement for the game's IR routines. GPL-3.0-or-later.
@ Copied over CARD_SpiWaitReadRange (ARM, in the game's main binary);
@ CARD_SpiWaitWriteRange is patched to jump to walkerStubWrite and
@ CARD_SpiWaitInit (start of every IR session) to walkerStubInit. Position
@ independent; the bootloader fills in `control` and `entry`.
@
@   readEntry:  r0 = buffer             -> r0 = bytes received
@   writeEntry: r0 = buffer, r1 = length -> r0 = length
@
@ The walker blob lives in memory the game's MPU setup doesn't let it run
@ from, so: IRQs off, write back and drop the whole D-cache (the game's
@ buffer and stack are then current in RAM), MPU off, call, MPU back on.
@ Nothing is kept on the stack across the MPU switch (a cached stack line
@ would not be seen with the MPU off).
	.arm
	.syntax unified
	.section .text
	.global walkerStubStart, walkerStubWrite, walkerStubInit, walkerStubControl, walkerStubEntry, walkerStubEnd
walkerStubStart:
	mov	r2, #0
	mov	r1, r0
	mov	r0, #0			@ WALKER_OP_READ
	b	common
walkerStubInit:
	mov	r2, #0
	mov	r1, #0
	mov	r0, #2			@ WALKER_OP_INIT
	b	common
walkerStubWrite:
	mov	r2, r1
	mov	r1, r0
	mov	r0, #1			@ WALKER_OP_WRITE
common:
	push	{r4-r8, lr}
	mov	r5, r0
	mov	r6, r1
	mov	r7, r2
	mrs	r4, cpsr
	orr	r3, r4, #0xC0
	msr	cpsr_c, r3		@ IRQ/FIQ off
	mov	r1, #0			@ clean+invalidate the 4-way D-cache by set/way
1:	mov	r0, #0
2:	orr	r2, r1, r0
	mcr	p15, 0, r2, c7, c14, 2
	add	r0, r0, #32
	cmp	r0, #0x400
	bne	2b
	adds	r1, r1, #0x40000000
	bne	1b
	mcr	p15, 0, r1, c7, c10, 4	@ drain write buffer (r1 = 0)
	mrc	p15, 0, r8, c1, c0, 0
	bic	r0, r8, #1
	mcr	p15, 0, r0, c1, c0, 0	@ MPU (and caches) off
	ldr	r0, walkerStubControl
	mov	r1, r5
	mov	r2, r6
	mov	r3, r7
	ldr	r12, walkerStubEntry
	blx	r12
	mcr	p15, 0, r8, c1, c0, 0	@ MPU back on
	msr	cpsr_c, r4
	pop	{r4-r8, lr}
	bx	lr
walkerStubControl:
	.word	0
walkerStubEntry:
	.word	0
walkerStubEnd:
