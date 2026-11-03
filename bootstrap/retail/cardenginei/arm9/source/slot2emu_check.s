@ Virtual GBA Slot-2: keep the blob's abort handler installed. GPL-3.0-or-later.
@ Called from cardRead() with r0 = blob entry, r1 = ce9 valueBits.
@ Fast path: debug vector already ours, MPU region 3 narrowed off the slot and the SDK
@ cartridge record says "AGB cartridge present". Otherwise clean the D-cache,
@ switch the MPU off and let the blob install itself (see slot2emu.c).
#ifndef TWLSDK
	.arm
	.syntax unified
	.text
	.global slot2EmuCheck
	.type slot2EmuCheck, %function
slot2EmuCheck:
	tst	r1, #0x800		@ dsiBios: BIOS debug vector address
	ldrne	r2, =0x02FFFD9C
	ldreq	r2, =0x027FFD9C
	ldr	r2, [r2]
	add	r3, r0, #4
	cmp	r2, r3
	bne	1f
	mrc	p15, 0, r2, c6, c3, 0	@ region 3: narrowed to the extra RAM, or off
	ldr	r3, =0x0C000033
	cmp	r2, r3
	tstne	r2, #1
	bne	1f
	ldr	r2, =0x027FFC35
	ldrb	r2, [r2]
	and	r2, r2, #3
	cmp	r2, #1
	bxeq	lr
1:	push	{r4-r6, lr}
	mov	r4, r0
	mov	r5, r1
	mrs	r6, cpsr
	orr	r2, r6, #0xC0
	msr	cpsr_c, r2		@ IRQ/FIQ off
	mov	r1, #0			@ clean+invalidate the 4 KiB, 4-way D-cache
2:	mov	r0, #0
3:	orr	r2, r1, r0
	mcr	p15, 0, r2, c7, c14, 2
	add	r0, r0, #32
	cmp	r0, #0x400
	bne	3b
	adds	r1, r1, #0x40000000
	bne	2b
	mcr	p15, 0, r1, c7, c10, 4	@ drain write buffer (r1 = 0)
	mrc	p15, 0, r1, c1, c0, 0
	bic	r2, r1, #1
	mcr	p15, 0, r2, c1, c0, 0	@ MPU (and so both caches) off
	push	{r1}
	mov	r0, r5			@ blob: r0 valueBits, r1 CP15 control
	blx	r4
	pop	{r1}
	mcr	p15, 0, r1, c1, c0, 0	@ MPU back on
	msr	cpsr_c, r6
	pop	{r4-r6, pc}
	.pool
#endif
