/*
 * Copyright 2007-2011, Ingo Weinhold, ingo_weinhold@gmx.de.
 * Distributed under the terms of the MIT License.
 */

// This file is used to get C structure offsets into assembly code.
// The build system assembles the file and processes the output to create
// a header file with macro definitions, that can be included from assembly
// code.


#include <computed_asm_macros.h>

#include <arch_cpu.h>
#include <cpu.h>
#include <ksignal.h>
#include <ksyscalls.h>
#include <thread_types.h>


#define DEFINE_MACRO(macro, value) DEFINE_COMPUTED_ASM_MACRO(macro, value)

#define DEFINE_OFFSET_MACRO(prefix, structure, member) \
	DEFINE_MACRO(prefix##_##member, offsetof(struct structure, member));

#define DEFINE_SIZEOF_MACRO(prefix, structure) \
	DEFINE_MACRO(prefix##_sizeof, sizeof(struct structure));


void
dummy()
{
	DEFINE_SIZEOF_MACRO(IFRAME, iframe);
	DEFINE_OFFSET_MACRO(IFRAME, iframe, elr);
	DEFINE_OFFSET_MACRO(IFRAME, iframe, spsr);
	DEFINE_OFFSET_MACRO(IFRAME, iframe, x);
	DEFINE_OFFSET_MACRO(IFRAME, iframe, sp);
	DEFINE_OFFSET_MACRO(IFRAME, iframe, esr);
	DEFINE_OFFSET_MACRO(IFRAME, iframe, far);
}