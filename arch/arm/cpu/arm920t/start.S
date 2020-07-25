/* SPDX-License-Identifier: GPL-2.0+ */
/*
 *  armboot - Startup Code for ARM920 CPU-core
 *
 *  Copyright (c) 2001	Marius Gröger <mag@sysgo.de>
 *  Copyright (c) 2002	Alex Züpke <azu@sysgo.de>
 *  Copyright (c) 2002	Gary Jennejohn <garyj@denx.de>
 */

#include <asm-offsets.h>
#include <asm/assembler.h>
#include <common.h>
#include <elf.h>
#include <config.h>
#include <linux/linkage.h>
/*
 *************************************************************************
 *
 * Startup Code (called from the ARM reset exception vector)
 *
 * do important init only if we don't start from memory!
 * relocate armboot to ram
 * setup stack
 * jump to second stage
 *
 *************************************************************************
 */
.globl _code_start
_code_start:
	.word _start

	.globl	reset

reset:
	/*
	 * set the cpu to SVC32 mode
	 */
	mrs	r0, cpsr
	bic	r0, r0, #0x1f
	orr	r0, r0, #0xd3
	msr	cpsr, r0

#if	defined(CONFIG_AT91RM9200DK) || defined(CONFIG_AT91RM9200EK)
	/*
	 * relocate exception table
	 */
	ldr	r0, =_start
	ldr	r1, =0x0
	mov	r2, #16
copyex:
	subs	r2, r2, #1
	ldr	r3, [r0], #4
	str	r3, [r1], #4
	bne	copyex
#endif


#define WTCON 0x53000000
#define INTMOD 0x4A000004
#define INTMSK 0x4A000008
#define INTSUBMSK 0x4A00001C
#define CLKDIVN 0x4C000014
#define MPLLCON	0x4C000004
//#define S3C2440_MPLL_400MHZ ((0x5c<<12) | (0x01<<4) | (0x01<<0))
#define S3C2440_MPLL_400MHZ ((0x7f<<12) | (0x02<<4) | (0x01<<0))
	
	ldr	r0, =WTCON
	mov	r1, #0x0
	str	r1, [r0]

	/*
	 * mask all IRQs by setting all bits in the INTMR - default
	*/
	mov	r1, #0xffffffff
	//mvn 	r1, #0
	ldr	r0, =INTMSK
	str	r1, [r0]
	//ldr	r1, =0x3ff
	ldr	r1, =0x7fff
	ldr	r0, =INTSUBMSK
	str	r1, [r0]

	/* Original code has noted this part, for later will call clock_init */
	/* FCLK:HCLK:PCLK = 1:2:4 */
	/* default FCLK is 120 MHz ! */
	ldr	r0, =CLKDIVN
	mov	r1, #5
	str	r1, [r0]

//	mrc p15, 0, r1, c1, c0, 0
//	orr r1, r1, #0xc0000000
//	mcr p15, 0, r1, c1, c0, 0
//
//	ldr r0, =MPLLCON
//	ldr r1, =S3C2440_MPLL_400MHZ
//	str r1, [r0]

	/*
	 * we do sys-critical inits only at reboot,
	 * not when booting from ram!
	 */
#ifndef CONFIG_SKIP_LOWLEVEL_INIT
#if 0
	adr	r0, _start
	ldr	r1, =CONFIG_SYS_TEXT_BASE
	cmp	r0, r1
	blne	cpu_init_crit
#endif
	bl	cpu_init_crit

#endif


relocate:
	//mov	r0, #0
	adr	r0, _code_start
	ldr	r1, =CONFIG_SYS_TEXT_BASE
	cmp	r0, r1
	bhi	stack_setup
	/* light first Led */
        //ldr r4, =0x56000010
        //mov r5, #(0x00015400)
        //str r5, [r4]
        //ldr r4, =0x56000014
        //mov r5, #(0x000001C0)
        //str r5, [r4]

	b	CopyPrepare

stack_setup:
	
	/* light all Led test */
        ldr r4, =0x56000010
        mov r5, #(0x00015400)
        str r5, [r4]
        ldr r4, =0x56000014
        mov r5, #(0x00000000)
        str r5, [r4]

	ldr	r0, =CONFIG_SYS_TEXT_BASE
	sub	r0, r0, #0x4000 /* MALLOC LEN */
	sub	r0, r0, #CFG_GBL_DATA_SIZE

#ifdef CONFIG_USE_IRQ
	/*temporary add, this should not be executed*/
	sub	r0, r0, #(CONFIG_STACKSIZE_IRQ+CONFIG_STACKSIZE_FIQ)
#endif
	sub	sp, r0, #200

#ifndef CONFIG_SKIP_LOWLEVEL_INIT
	bl	clock_init
#endif

//clear_bss:
//	ldr	r0, =__bss_start
//	ldr	r1, =__bss_end
//	mov	r2, #0x00000000
//
//clbss_l:
//	str	r2, [r0]
//	add	r0, r0, #4
//	cmp	r0, r1
//	ble	clbss_l
//	b	_main
	ldr	pc,=_main

JUMP1:
	/* jump to _main in SDRAM */
	ldr	r2, =_start
	ldr	r3, =_main
	sub	r2, r3, r2
	ldr	r3, =CONFIG_SYS_TEXT_BASE
	add	r2, r2, r3
/*------------------------------------------------------------------------------*/

	.globl	c_runtime_cpu_setup
c_runtime_cpu_setup:

	mov	pc, lr

/*
 *************************************************************************
 *
 * CPU_init_critical registers
 *
 * setup important registers
 * setup memory timing
 *
 *************************************************************************
 */
#ifndef CONFIG_SKIP_LOWLEVEL_INIT
cpu_init_crit:
	/*
	 * flush v4 I/D caches
	 */
	mov	r0, #0
	mcr	p15, 0, r0, c7, c7, 0	/* flush v3/v4 cache */
	mcr	p15, 0, r0, c8, c7, 0	/* flush v4 TLB */

	/*
	 * disable MMU stuff and caches
	 */
	mrc	p15, 0, r0, c1, c0, 0
	bic	r0, r0, #0x00002300	@ clear bits 13, 9:8 (--V- --RS)
	bic	r0, r0, #0x00000087	@ clear bits 7, 2:0 (B--- -CAM)
	orr	r0, r0, #0x00000002	@ set bit 1 (A) Align
	orr	r0, r0, #0x00001000	@ set bit 12 (I) I-Cache
	mcr	p15, 0, r0, c1, c0, 0

#if 1
#ifndef CONFIG_SKIP_LOWLEVEL_INIT_ONLY
	/*
	 * before relocating, we have to setup RAM timing
	 * because memory timing is board-dependend, you will
	 * find a lowlevel_init.S in your board directory.
	 */
	mov	ip, lr

	bl	lowlevel_init
	mov	lr, ip
#endif
#endif
	mov	pc, lr
#endif /* CONFIG_SKIP_LOWLEVEL_INIT */

CopyPrepare:
#define LENGTH_UBOOT 0x10000
#define NAND_CTL_BASE 0x4E000000
/* Offset */
#define oNFCONF 0x00
#define oNFCONT 0x04
#define oNFCMD 0x08
#define oNFSTAT 0x20
#define DW_STACK_START 0x33F0FFFC
 	@reset NAND
	mov r1, #NAND_CTL_BASE
	ldr r2, =( (7<<12)|(7<<8)|(7<<4)|(0<<0) )
	str r2, [r1, #oNFCONF]
	ldr r2, [r1, #oNFCONF]
	ldr r2, =( (1<<4)|(0<<1)|(1<<0) ) @ Active low CE Control
	str r2, [r1, #oNFCONT]
	ldr r2, [r1, #oNFCONT]
	ldr r2, =(0x6) @ RnB Clear
	str r2, [r1, #oNFSTAT]
	ldr r2, [r1, #oNFSTAT]
	mov r2, #0xff @ RESET command
	strb r2, [r1, #oNFCMD]
	mov r3, #0 @ wait
nand1:
	add r3, r3, #0x1
	cmp r3, #0xa
	blt nand1
nand2:
	ldr r2, [r1, #oNFSTAT] @ wait ready
	tst r2, #0x4
	beq nand2

	ldr r2, [r1, #oNFCONT]
	orr r2, r2, #0x2 @ Flash Memory Chip Disable
	str r2, [r1, #oNFCONT]
	@ get read to call C functions (for nand_read())
	ldr sp, =DW_STACK_START @ setup stack pointer
	mov fp, #0 @ no previous frame, so fp=0
	@ copy U-Boot to RAM
	ldr r0, =CONFIG_SYS_TEXT_BASE
	mov r1, #0x0
	mov r2, #LENGTH_UBOOT
	bl nand_read_ll
	tst r0, #0x0
	beq ok_nand_read
bad_nand_read:
loop2:
 	b loop2 @ infinite loop
ok_nand_read:
	@ verify
	mov r0, #0
	ldr r1, =CONFIG_SYS_TEXT_BASE
	mov r2, #0x400 @ 4 bytes * 1024 = 4K-bytes
go_next:
	ldr r3, [r0], #4
	ldr r4, [r1], #4
	teq r3, r4
	bne notmatch
	subs r2, r2, #4
	beq stack_setup
	bne go_next
notmatch:
loop3:
	b loop3 @ infinite loop
