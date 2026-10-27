	.cpu cortex-m3
	.arch armv7-m
	.fpu softvfp
	.eabi_attribute 20, 1
	.eabi_attribute 21, 1
	.eabi_attribute 23, 3
	.eabi_attribute 24, 1
	.eabi_attribute 25, 1
	.eabi_attribute 26, 1
	.eabi_attribute 30, 6
	.eabi_attribute 34, 1
	.eabi_attribute 18, 4
	.file	"sizes.c"
	.text
	.global	sizes
	.section	.rodata
	.align	2
	.type	sizes, %object
	.size	sizes, 44
sizes:
	.word	5776
	.word	2684
	.word	2688
	.word	2576
	.word	376
	.word	2640
	.word	2464
	.word	3076
	.word	152
	.word	356
	.word	816
	.ident	"GCC: (Arm GNU Toolchain 14.2.Rel1 (Build arm-14.52)) 14.2.1 20241119"
