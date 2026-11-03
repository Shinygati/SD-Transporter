	.section .text.start, "ax"
	.arm
	.global _start
_start:
	ldr	sp, =0x02300000
	ldr	r0, =gameMain
	mov	lr, pc
	bx	r0
done:	b	done
	.pool
