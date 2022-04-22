/*
 * Copyright 2019 Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#include <thread.h>
#include <arch_thread.h>

#include <arch_cpu.h>
#include <arch/thread.h>
#include <boot/stage2.h>
#include <kernel.h>
#include <thread.h>
#include <commpage_defs.h>
#include <tls.h>
#include <vm/vm_types.h>
#include <vm/VMAddressSpace.h>
#include <arch_vm.h>
#include <arch/vm_translation_map.h>
#include "VMSAv8TranslationMap.h"

#include <string.h>

//#define TRACE_ARCH_THREAD
#ifdef TRACE_ARCH_THREAD
#	define TRACE(x) dprintf x
#else
#	define TRACE(x) ;
#endif


status_t
arch_thread_init(struct kernel_args *args)
{
	return B_OK;
}


status_t
arch_team_init_team_struct(Team *team, bool kernel)
{
	return B_OK;
}


status_t
arch_thread_init_thread_struct(Thread *thread)
{
	return B_OK;
}


void
arch_thread_init_kthread_stack(Thread* thread, void* _stack, void* _stackTop,
	void (*function)(void*), const void* data)
{
	memset(&thread->arch_info, 0, sizeof(arch_thread));
	thread->arch_info.regs[10] = (uint64_t)data;
	thread->arch_info.regs[11] = (uint64_t)function;
	thread->arch_info.regs[12] = (uint64_t)_stackTop;
}


status_t
arch_thread_init_tls(Thread *thread)
{
	return 0;
}

extern "C" void _arch_context_swap(arch_thread *from, arch_thread *to);

void
arch_thread_context_switch(Thread *from, Thread *to)
{
	VMSAv8TranslationMap* fromMap = (VMSAv8TranslationMap*)from->team->address_space->TranslationMap();
	VMSAv8TranslationMap* toMap = (VMSAv8TranslationMap*)to->team->address_space->TranslationMap();

	if (fromMap != toMap) {
		if (toMap != NULL) {
			WRITE_SPECIALREG(TTBR0_EL1, toMap->_Addr());
		}
		arch_cpu_user_TLB_invalidate();
	}

	dprintf("switch: %lx -> %lx\n", (uint64_t)from, (uint64_t)to);

	_arch_context_swap(&from->arch_info, &to->arch_info);
}

void
arch_thread_dump_info(void *info)
{
}

extern "C" void _eret_with_iframe(iframe *frame);

status_t
arch_thread_enter_userspace(Thread *thread, addr_t entry,
	void *arg1, void *arg2)
{
	panic("userspace entry");

	addr_t threadExitAddr;
	{
		addr_t commpageAdr = (addr_t)thread->team->commpage_address;
		status_t ret = user_memcpy(&threadExitAddr,
			&((addr_t*)commpageAdr)[COMMPAGE_ENTRY_ARM64_THREAD_EXIT],
			sizeof(threadExitAddr));
		ASSERT(ret == B_OK);
		threadExitAddr += commpageAdr;
	}

	iframe frame;
	memset(&frame, 0, sizeof(frame));

	frame.spsr = 0;
	frame.elr = entry;
	frame.x[0] = (uint64_t)arg1;
	frame.x[1] = (uint64_t)arg2;
	frame.lr = threadExitAddr;
	frame.sp = thread->user_stack_base + thread->user_stack_size;

	_eret_with_iframe(&frame);
	return B_ERROR;
}


bool
arch_on_signal_stack(Thread *thread)
{
	return false;
}


status_t
arch_setup_signal_frame(Thread *thread, struct sigaction *sa,
	struct signal_frame_data *signalFrameData)
{
	panic("arch_setup_signal_frame");
	return B_ERROR;
}


int64
arch_restore_signal_frame(struct signal_frame_data* signalFrameData)
{
	return 0;
}


void
arch_check_syscall_restart(Thread *thread)
{
}


void
arch_store_fork_frame(struct arch_fork_arg *arg)
{
	panic("arch_store_fork_frame");
}


void
arch_restore_fork_frame(struct arch_fork_arg *arg)
{
}

extern "C" status_t _arch_cpu_user_memcpy(char *to, const char *from, size_t size, addr_t *faultHandler)
{
	memcpy(to, from, size);
	return B_OK;
}

extern "C" status_t _arch_cpu_user_memset(char *to, char c, size_t count, addr_t *faultHandler)
{
	memset(to, c, count);
	return B_OK;
}

extern "C" ssize_t _arch_cpu_user_strlcpy(char *to, const char *from, size_t size, addr_t *faultHandler)
{
	return strlcpy(to, from, size);
}
