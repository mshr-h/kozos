	.h8300h
	.section .text
	.global _start
#	.type _start,@function
_start:
	mov.l #_stack, sp
	jsr	@_main

1:
	bra	1b
