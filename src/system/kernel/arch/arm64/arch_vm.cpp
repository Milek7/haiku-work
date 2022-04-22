/*
 * Copyright 2019 Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#include <KernelExport.h>

#include <kernel.h>
#include <boot/kernel_args.h>

#include <vm/vm.h>
#include <vm/vm_types.h>
#include <arch/vm.h>


status_t
arch_vm_init(kernel_args *args)
{
	dprintf("arch_vm_init\n");
	return B_OK;
}


status_t
arch_vm_init2(kernel_args *args)
{
	dprintf("arch_vm_init2\n");
	return B_OK;
}


status_t
arch_vm_init_post_area(kernel_args *args)
{
	dprintf("arch_vm_init_post_area\n");
	return B_OK;
}


status_t
arch_vm_init_end(kernel_args *args)
{
	dprintf("arch_vm_init_end\n");
	return B_OK;
}


status_t
arch_vm_init_post_modules(kernel_args *args)
{
	dprintf("arch_vm_init_post_modules\n");
	return B_OK;
}


void
arch_vm_aspace_swap(struct VMAddressSpace *from, struct VMAddressSpace *to)
{
	dprintf("arch_vm_aspace_swap\n");
}


bool
arch_vm_supports_protection(uint32 protection)
{
	// User-RO/Kernel-RW is not possible
	if ((protection & B_READ_AREA) != 0 &&
		(protection & B_WRITE_AREA) == 0 &&
		(protection & B_KERNEL_WRITE_AREA) != 0) {
		return false;
	}

	return true;
}


void
arch_vm_unset_memory_type(VMArea *area)
{
	//dprintf("arch_vm_unset_memory_type\n");
}


status_t
arch_vm_set_memory_type(VMArea *area, phys_addr_t physicalBase, uint32 type)
{
	//dprintf("arch_vm_set_memory_type %lx %x\n", physicalBase, type);
	return B_OK;
}
