/*
 * Copyright 2019 Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#include <KernelExport.h>
#include <arch/platform.h>
#include <boot/kernel_args.h>
#include <arch_kernel.h>
#include <boot_item.h>

void *gFDT;
phys_addr_t sACPIRootPointer;

status_t
arch_platform_init(struct kernel_args *kernelArgs)
{
	return B_OK;
}


status_t
arch_platform_init_post_vm(struct kernel_args *args)
{
	if (args->arch_args.fdt) {
		gFDT = (void*)(KERNEL_PMAP_BASE + (uint64_t)args->arch_args.fdt.Get());
	}

	if (args->arch_args.acpi_root) {
		sACPIRootPointer = args->arch_args.acpi_root.Get();
		add_boot_item("ACPI_ROOT_POINTER", &sACPIRootPointer, sizeof(sACPIRootPointer));
	}

	return B_OK;
}


status_t
arch_platform_init_post_thread(struct kernel_args *kernelArgs)
{
	return B_OK;
}
