/* SPDX-License-Identifier: GPL-2.0-only
 * Galaxy A8 2018 (SM-A530F, jackpotlte) XNU loader entry.
 *
 * arm64 Linux Image header so Samsung S-Boot treats us like a kernel. Same
 * contract as shim/dreamlte/start.S: S-Boot jumps here with the MMU off and
 * x0 = DTB. Position independent (linked at 0, run wherever S-Boot placed us).
 *
 * One difference from dreamlte is handled here: S-Boot may enter at EL2
 * instead of EL1. At EL2 every EL1 system-register access below traps, and
 * with VBAR_EL2 unset the SoC resets before the loader can write a single
 * byte - a silent boot loop that looks like a rejected image. The Linux
 * kernel shipped with this ROM handles exactly that case in el2_setup()
 * (Eureka 4.4 arch/arm64/kernel/head.S): set HCR_EL2.RW, hand the EL1 timers
 * to EL1, install EL2 vectors, SCTLR_EL1 = 0x30d0, then eret to EL1h. We do
 * the same and record the entry EL for the banner/pstore.
 */
	.section .text.head, "ax"
	.globl _head
_head:
	b	_start			/* code0 */
	.long	0			/* code1 */
	.quad	0x80000			/* text_offset */
	.quad	_end			/* image_size (linked at 0) */
	.quad	0xa			/* flags: LE, 4K pages, place anywhere */
	.quad	0
	.quad	0
	.quad	0
	.ascii	"ARM\x64"		/* magic */
	.long	0

	.text
	.globl _start
_start:
	msr	daifset, #0xf
	mov	x19, x0			/* DTB */
	mrs	x20, CurrentEL
	lsr	x20, x20, #2		/* 1 = EL1, 2 = EL2, 3 = EL3 */
	mrs	x21, sctlr_el1		/* exactly as S-Boot left it (logged) */

	cmp	x20, #2
	b.ne	el1_entry

	/* ---- EL2 handoff: drop to EL1 (el2_setup equivalent) ---- */
	msr	spsel, #1
	mrs	x0, cnthctl_el2
	orr	x0, x0, #3		/* EL1PCTEN|EL1PCEN: EL1 timer access */
	msr	cnthctl_el2, x0
	msr	cntvoff_el2, xzr	/* physical and virtual counters agree */
	mov	x0, #(1 << 31)		/* HCR_EL2.RW: EL1 is AArch64, no stage 2 */
	msr	hcr_el2, x0
	msr	vttbr_el2, xzr
	mov	x0, #0x30d0		/* SCTLR_EL1: MMU/caches off, little endian */
	msr	sctlr_el1, x0
	adr	x0, vectors_el2
	msr	vbar_el2, x0		/* an EL2 fault hangs instead of resetting */
	mov	x0, #0x3c5		/* DAIF masked, EL1h */
	msr	spsr_el2, x0
	adr	x0, el1_entry
	msr	elr_el2, x0
	isb
	eret

el1_entry:
	msr	spsel, #1
	adr	x1, _stack_top
	mov	sp, x1
	adr	x1, vectors
	msr	vbar_el1, x1

	/* Run with the MMU off whatever S-Boot left. x21 (the value S-Boot
	 * wrote) is logged; on the EL2 path SCTLR_EL1 is already 0x30d0. */
	cmp	x20, #2
	b.eq	1f
	mrs	x2, sctlr_el1
	bic	x2, x2, #1		/* M */
	bic	x2, x2, #4		/* C */
	bic	x2, x2, #0x1000		/* I */
	msr	sctlr_el1, x2
	tlbi	vmalle1
	ic	iallu
	dsb	sy
	isb
1:
	adr	x1, _bss_start
	adr	x2, _bss_end
2:	cmp	x1, x2
	b.hs	3f
	str	xzr, [x1], #8
	b	2b
3:	mov	x0, x19			/* DTB */
	mov	x1, x20			/* entry EL */
	mov	x2, x21			/* S-Boot's SCTLR_EL1 */
	bl	jt_main			/* returns XNU entry PA, or 0 */
	cbz	x0, 4f
	mov	x16, x0
	adr	x1, jt_boot_args
	ldr	x0, [x1]		/* boot_args PA for start_first_cpu */
	/* XNU's entry expects the MMU and caches off. Data was written with the
	 * MMU off (Device memory) and the copies were cleaned to PoC anyway. */
	mrs	x14, sctlr_el1
	bic	x14, x14, #1		/* M */
	bic	x14, x14, #4		/* C */
	bic	x14, x14, #0x1000	/* I */
	msr	sctlr_el1, x14
	ic	iallu
	dsb	sy
	isb
	br	x16
4:	wfe
	b	4b

/* EL1 exception vectors (installed above). */
.macro vent slot
	.balign	0x80
	mov	x0, #\slot
	mov	x1, sp
	b	exc_common
.endm

	.balign	0x800
vectors:
	vent 0
	vent 1
	vent 2
	vent 3
	vent 4
	vent 5
	vent 6
	vent 7
	vent 8
	vent 9
	vent 10
	vent 11
	vent 12
	vent 13
	vent 14
	vent 15

exc_common:
	bl	jt_exception		/* never returns */
5:	wfe
	b	5b

/* EL2 exception vectors: only used between the EL2 handoff and the eret to
 * EL1. A fault there has no usable stack or console, so hang (visible as a
 * phone that neither boots nor resets) rather than reset silently. */
	.balign	0x800
vectors_el2:
	.rept	16
	b	el2_hang
	.balign	0x80
	.endr
el2_hang:
	wfe
	b	el2_hang
