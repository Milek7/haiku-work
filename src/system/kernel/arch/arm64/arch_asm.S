/*
 * Copyright 2019 Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#include <arch/arm/arch_cpu.h>
#include <asm_defs.h>
#include "asm_offsets.h"

.text

.macro xchg_sp xt
add	sp, sp, \xt
sub	\xt, sp, \xt
sub	sp, sp, \xt
.endm

.macro EXCEPTION_HANDLER el name func
	FUNCTION(handle_\name):
		// avoid using sp in case it is misaligned
		// swap sp with x28 and use it instead
		xchg_sp x28

		// x28 is now the stack top, make room for IFRAME
		sub x28, x28, #(IFRAME_sizeof)

		stp	    x0,  x1, [x28, #(IFRAME_x + 0 * 8)]
		stp	    x2,  x3, [x28, #(IFRAME_x + 2 * 8)]
		stp	    x4,  x5, [x28, #(IFRAME_x + 4 * 8)]
		stp	    x6,  x7, [x28, #(IFRAME_x + 6 * 8)]
		stp	    x8,  x9, [x28, #(IFRAME_x + 8 * 8)]
		stp	   x10, x11, [x28, #(IFRAME_x + 10 * 8)]
		stp	   x12, x13, [x28, #(IFRAME_x + 12 * 8)]
		stp	   x14, x15, [x28, #(IFRAME_x + 14 * 8)]
		stp	   x16, x17, [x28, #(IFRAME_x + 16 * 8)]
		stp	   x18, x19, [x28, #(IFRAME_x + 18 * 8)]
		stp	   x20, x21, [x28, #(IFRAME_x + 20 * 8)]
		stp	   x22, x23, [x28, #(IFRAME_x + 22 * 8)]
		stp	   x24, x25, [x28, #(IFRAME_x + 24 * 8)]
		stp	   x26, x27, [x28, #(IFRAME_x + 26 * 8)]
		mov    x0,  sp // original x28 that we swapped with sp
		stp	   x0,  x29, [x28, #(IFRAME_x + 28 * 8)]
		str	   x30,      [x28, #(IFRAME_x + 30 * 8)]

	.if \el == 0
		mrs x0, SP_EL0
	.else
		// add sizeof back here to store original sp
		add x0, x28, #(IFRAME_sizeof)
	.endif

		mrs x1, ELR_EL1
		mrs x2, SPSR_EL1
		mrs x3, ESR_EL1
		mrs x4, FAR_EL1

		str x0, [x28, #(IFRAME_sp)]
		str x1, [x28, #(IFRAME_elr)]
		str x2, [x28, #(IFRAME_spsr)]
		str x3, [x28, #(IFRAME_esr)]
		str x4, [x28, #(IFRAME_far)]

		// prepare aligned sp for C function
		and sp, x28, #0xfffffffffffffff0

		// call C handler, passing IFRAME in x0
		mov x0, x28
		bl \func

		// x28 is callee-saved so it still points to IFRAME
		// use x29 there because we want to use it at the very end
		ldr x29, [x28, #(IFRAME_sp)]

		ldr x1,  [x28, #(IFRAME_elr)]
		ldr x2,  [x28, #(IFRAME_spsr)]
		msr ELR_EL1, x1
		msr SPSR_EL1, x2

		ldp	    x0,  x1, [x28, #(IFRAME_x + 0 * 8)]
		ldp	    x2,  x3, [x28, #(IFRAME_x + 2 * 8)]
		ldp	    x4,  x5, [x28, #(IFRAME_x + 4 * 8)]
		ldp	    x6,  x7, [x28, #(IFRAME_x + 6 * 8)]
		ldp	    x8,  x9, [x28, #(IFRAME_x + 8 * 8)]
		ldp	   x10, x11, [x28, #(IFRAME_x + 10 * 8)]
		ldp	   x12, x13, [x28, #(IFRAME_x + 12 * 8)]
		ldp	   x14, x15, [x28, #(IFRAME_x + 14 * 8)]
		ldp	   x16, x17, [x28, #(IFRAME_x + 16 * 8)]
		ldp	   x18, x19, [x28, #(IFRAME_x + 18 * 8)]
		ldp	   x20, x21, [x28, #(IFRAME_x + 20 * 8)]
		ldp	   x22, x23, [x28, #(IFRAME_x + 22 * 8)]
		ldp	   x24, x25, [x28, #(IFRAME_x + 24 * 8)]
		ldp	   x26, x27, [x28, #(IFRAME_x + 26 * 8)]
		// x28 and x29 will be restored later
		ldr	   x30, [x28, #(IFRAME_x + 30 * 8)]

	.if \el == 0
		// load stack pointer for EL0 from IFRAME
		msr SP_EL0, x29

		// unwind our own stack pointer
		add sp, x28, #(IFRAME_sizeof)
	.else
		// we stored original pointer to IFRAME, no need to unwind again there
		mov sp, x29
	.endif

		// finally restore x28 and x29
		ldp x28, x29, [x28, #(IFRAME_x + 28 * 8)]

		eret
	FUNCTION_END(handle_\name)
.endm

.macro	vector	name
	.align 7
	b	handle_\name
.endm

.macro	vempty
	.align 7
	brk	0xfff
	1: b	1b
.endm

.align 11
.globl _exception_vectors
_exception_vectors:
	vempty             /* Synchronous EL1t */
	vempty             /* IRQ EL1t */
	vempty             /* FIQ EL1t */
	vempty             /* Error EL1t */

	vector el1h_sync   /* Synchronous EL1h */
	vector el1h_irq    /* IRQ EL1h */
	vector el1h_fiq    /* FIQ EL1h */
	vector el1h_error  /* Error EL1h */

	vector el0_sync    /* Synchronous 64-bit EL0 */
	vector el0_irq     /* IRQ 64-bit EL0 */
	vector el0_fiq     /* FIQ 64-bit EL0 */
	vector el0_error   /* Error 64-bit EL0 */

	vempty             /* Synchronous 32-bit EL0 */
	vempty             /* IRQ 32-bit EL0 */
	vempty             /* FIQ 32-bit EL0 */
	vempty             /* Error 32-bit EL0 */

EXCEPTION_HANDLER 1 el1h_sync do_el1h_sync
EXCEPTION_HANDLER 1 el1h_irq intr_irq_handler_el1
EXCEPTION_HANDLER 1 el1h_fiq intr_fiq_handler_el1
EXCEPTION_HANDLER 1 el1h_error do_el1h_error

EXCEPTION_HANDLER 0 el0_sync do_el0_sync
EXCEPTION_HANDLER 0 el0_irq intr_irq_handler_el0
EXCEPTION_HANDLER 0 el0_fiq intr_fiq_handler_el0
EXCEPTION_HANDLER 0 el0_error do_el0_error

FUNCTION(_arch_context_swap):
	// save
	stp x19, x20, [x0], #16
	stp x21, x22, [x0], #16
	stp x23, x24, [x0], #16
	stp x25, x26, [x0], #16
	stp x27, x28, [x0], #16
	stp x29, x30, [x0], #16

	mov x2, sp
	str x2, [x0], #8

	stp  d8,  d9, [x0], #16
	stp d10, d11, [x0], #16
	stp d12, d13, [x0], #16
	stp d14, d15, [x0], #16

	// restore
	ldp x19, x20, [x1], #16
	ldp x21, x22, [x1], #16
	ldp x23, x24, [x1], #16
	ldp x25, x26, [x1], #16
	ldp x27, x28, [x1], #16
	ldp x29, x30, [x1], #16

	ldr x2, [x1], #8
	mov sp, x2

	ldp  d8,  d9, [x1], #16
	ldp d10, d11, [x1], #16
	ldp d12, d13, [x1], #16
	ldp d14, d15, [x1], #16

	// pass x29 as argument to thread entry function
	mov x0, x29
	ret
FUNCTION_END(_arch_context_swap)

/* status_t arch_cpu_user_memcpy(void *to, const void *from, size_t size, addr_t *faultHandler) */
FUNCTION(_arch_cpu_user_memcpy):
	mov		x0, xzr
	ret
FUNCTION_END(_arch_cpu_user_memcpy)

/* status_t arch_cpu_user_memset(void *to, char c, size_t count, addr_t *faultHandler) */
FUNCTION(_arch_cpu_user_memset):
	mov		x0, xzr
	ret
FUNCTION_END(_arch_cpu_user_memset)

/* ssize_t arch_cpu_user_strlcpy(void *to, const void *from, size_t size, addr_t *faultHandler) */
FUNCTION(_arch_cpu_user_strlcpy):
	mov		x0, xzr
	ret
FUNCTION_END(_arch_cpu_user_strlcpy)

/*!	\fn void arch_debug_call_with_fault_handler(cpu_ent* cpu,
		jmp_buf jumpBuffer, void (*function)(void*), void* parameter)

	Called by debug_call_with_fault_handler() to do the dirty work of setting
	the fault handler and calling the function. If the function causes a page
	fault, the arch_debug_call_with_fault_handler() calls longjmp() with the
	given \a jumpBuffer. Otherwise it returns normally.

	debug_call_with_fault_handler() has already saved the CPU's fault_handler
	and fault_handler_stack_pointer and will reset them later, so
	arch_debug_call_with_fault_handler() doesn't need to care about it.

	\param cpu The \c cpu_ent for the current CPU.
	\param jumpBuffer Buffer to be used for longjmp().
	\param function The function to be called.
	\param parameter The parameter to be passed to the function to be called.
*/
FUNCTION(arch_debug_call_with_fault_handler):
	ldr x4, =fault
	str x4, [x0, #CPU_ENT_fault_handler]
	str x1, [x0, #CPU_ENT_fault_handler_stack_pointer]

	mov x0, x3
	br x2

fault:
	mov x0, sp
	mov x1, #1
	b longjmp
FUNCTION_END(arch_debug_call_with_fault_handler)
