/*
 * Copyright 2019-2022 Haiku, Inc. All rights reserved.
 * Released under the terms of the MIT License.
 */

#include <boot/platform.h>
#include <boot/stage2.h>

#include "mmu.h"
#include "efi_platform.h"

#include "aarch64.h"
#include "arch_mmu.h"

// #define TRACE_MMU
#ifdef TRACE_MMU
#	define TRACE(x) dprintf x
#else
#	define TRACE(x) ;
#endif


ARMv8TranslationRegime::TranslationDescriptor translation4Kb48bits = {
	{L0_SHIFT, L0_ADDR_MASK, false, true, false },
	{L1_SHIFT, Ln_ADDR_MASK, true, true,  false },
	{L2_SHIFT, Ln_ADDR_MASK, true, true,  false },
	{L3_SHIFT, Ln_ADDR_MASK, false, false, true }
};


ARMv8TranslationRegime CurrentRegime(translation4Kb48bits);
/* ARM port */
static uint64_t* sPageDirectory = NULL;
// static uint64_t* sFirstPageTable = NULL;
static uint64_t* sNextPageTable = NULL;
// static uint64_t* sLastPageTable = NULL;


const char*
granule_type_str(int tg)
{
	switch (tg) {
		case TG_4KB:
			return "4KB";
		case TG_16KB:
			return "16KB";
		case TG_64KB:
			return "64KB";
		default:
			return "Invalid Granule";
	}
}


void
arch_mmu_dump_table(uint64* table, uint8 currentLevel)
{
	ARMv8TranslationTableDescriptor ttd(table);

	if (currentLevel >= CurrentRegime.MaxLevels()) {
		// This should not happen
		panic("Too many levels ...");
		return;
	}

	uint64 EntriesPerLevel = arch_mmu_entries_per_granularity(CurrentRegime.Granularity());
	for (uint i = 0 ; i < EntriesPerLevel; i++) {
		if (!ttd.IsInvalid()) {
			TRACE(("Level %d, @%0lx: TTD %016lx\t", currentLevel, ttd.Location(), ttd.Value()));
			if (ttd.IsTable() && currentLevel < 3) {
				TRACE(("Table! Next Level:\n"));
				arch_mmu_dump_table(ttd.Dereference(), currentLevel + 1);
			}
			if (ttd.IsBlock() || (ttd.IsPage() && currentLevel == 3)) {
				TRACE(("Block/Page"));

				if (i & 1) { // 2 entries per row
					TRACE(("\n"));
				} else {
					TRACE(("\t"));
				}
			}
		}
		ttd.Next();
	}
}


void
arch_mmu_dump_present_tables()
{
#ifdef TRACE_MMU
	if (arch_mmu_enabled()) {
		uint64 address = arch_mmu_base_register();
		TRACE(("Under TTBR0: %lx\n", address));

		arch_mmu_dump_table(reinterpret_cast<uint64*>(address), 0);

		/* We are willing to transition, but still in EL2, present MMU configuration
		 * for user is present in EL2 by TTBR0_EL2. Kernel side is not active, but
		 * allocated under sPageDirectory, defined under TTBR1_EL1.
		 */
		if (address != 0ul) {
			TRACE(("Under allocated TTBR1_EL1:\n"));
			arch_mmu_dump_table(sPageDirectory, 0);
		}
	}
#endif
}


void arch_mmu_setup_EL1() {

	// Possibly inherit TCR from EL2
	uint64 tcr;
	if (arch_exception_level() == 2)
		tcr = READ_SPECIALREG(TCR_EL2);
	else
		tcr = READ_SPECIALREG(TCR_EL1);

	// Enable TTBR1
	tcr &= ~TCR_EPD1_DISABLE;

	// Set space for kernel space
	tcr &= ~T1SZ_MASK; // Clear
	// TODO: Compiler dependency?
	tcr |= TCR_T1SZ(__builtin_popcountl(KERNEL_BASE));

	dprintf("Configuring TCR_EL1: %8lx\n", tcr);

	WRITE_SPECIALREG(TCR_EL1, tcr);
}


uint64
map_region(addr_t virt_addr, addr_t  phys_addr, size_t size,
	uint32_t level, uint64_t flags, uint64* descriptor)
{
	ARMv8TranslationTableDescriptor ttd(descriptor);

	if (level >= CurrentRegime.MaxLevels()) {
		panic("Too many levels at mapping\n");
	}

	uint64 currentLevelSize = CurrentRegime.EntrySize(level);

	ttd.JumpTo(CurrentRegime.DescriptorIndex(virt_addr, level));

	uint64 remainingSizeInTable = CurrentRegime.TableSize(level)
		- currentLevelSize * CurrentRegime.DescriptorIndex(virt_addr, level);

	TRACE(("Level %x, Processing desc %lx indexing %lx\n",
		level, reinterpret_cast<uint64>(descriptor), ttd.Location()));

	if (ttd.IsInvalid()) {
		// If the physical has the same alignment we could make a block here
		// instead of using a complete next level table
		if (size >= currentLevelSize && CurrentRegime.Aligned(phys_addr, level)) {
			// Set it as block or page
			if (CurrentRegime.BlocksAllowed(level)) {
				ttd.SetAsBlock(reinterpret_cast<uint64*>(phys_addr), flags);
			} else {
				// Most likely in Level 3...
				ttd.SetAsPage(reinterpret_cast<uint64*>(phys_addr), flags);
			}

			// Expand!
			int64 expandedSize = (size > remainingSizeInTable)?remainingSizeInTable:size;

			do {
				phys_addr += currentLevelSize;
				expandedSize -= currentLevelSize;
				if (expandedSize > 0) {
					ttd.Next();
					if (CurrentRegime.BlocksAllowed(level)) {
						ttd.SetAsBlock(reinterpret_cast<uint64*>(phys_addr), flags);
					} else {
						// Most likely in Level 3...
						ttd.SetAsPage(reinterpret_cast<uint64*>(phys_addr), flags);
					}
				}
			} while (expandedSize > 0);

			return (size > remainingSizeInTable)?(size - remainingSizeInTable):0;

		} else {
			// Set it to next level
			uint64 offset = 0;
			uint64 remainingSize = size;
			do {
				uint64* page = NULL;
				if (ttd.IsInvalid()) {
					// our region is too small would need to create a level below
					page = CurrentRegime.AllocatePage();
					ttd.SetToTable(page, flags);
				} else if (ttd.IsTable()) {
					// Next table is allocated, follow it
					page = ttd.Dereference();
				} else {
					panic("Required contiguous descriptor in use by Block/Page for %lx\n", ttd.Location());
				}

				uint64 unprocessedSize = map_region(virt_addr + offset,
					phys_addr + offset, remainingSize, level + 1, flags, page);

				offset = remainingSize - unprocessedSize;

				remainingSize = unprocessedSize;

				ttd.Next();

			} while (remainingSize > 0);

			return 0;
		}

	} else {

		if ((ttd.IsBlock() && CurrentRegime.BlocksAllowed(level))
			|| (ttd.IsPage() && CurrentRegime.PagesAllowed(level))
		) {
			// TODO: Review, overlap? expand?
			panic("Re-setting a Block/Page descriptor for %lx\n", ttd.Location());
			return 0;
		} else if (ttd.IsTable() && CurrentRegime.TablesAllowed(level)) {
			// Next Level
			map_region(virt_addr, phys_addr, size, level + 1, flags, ttd.Dereference());
			return 0;
		} else {
			panic("All descriptor types processed for %lx\n", ttd.Location());
			return 0;
		}
	}
}


static void
map_range(addr_t virt_addr, phys_addr_t phys_addr, size_t size, uint64_t flags)
{
	TRACE(("map 0x%0lx --> 0x%0lx, len=0x%0lx, flags=0x%0lx\n",
		(uint64_t)virt_addr, (uint64_t)phys_addr, (uint64_t)size, flags));

	// TODO: Review why we get ranges with 0 size ...
	if (size == 0) {
		TRACE(("Requesing 0 size map\n"));
		return;
	}

	// TODO: Review this case
	if (phys_addr == READ_SPECIALREG(TTBR1_EL1)) {
		TRACE(("Trying to map the TTBR itself?!\n"));
		return;
	}

	if (arch_mmu_read_access(virt_addr) && arch_mmu_read_access(virt_addr + size)) {
		TRACE(("Range already covered in current MMU\n"));
		return;
	}

	uint64 address;

	if (arch_mmu_is_kernel_address(virt_addr)) {
		// Use TTBR1
		address = READ_SPECIALREG(TTBR1_EL1);
	} else {
		// ok, but USE instead TTBR0
		address = READ_SPECIALREG(TTBR0_EL1);
	}

	map_region(virt_addr, phys_addr, size, 0, flags, reinterpret_cast<uint64*>(address));

// 	for (addr_t offset = 0; offset < size; offset += B_PAGE_SIZE) {
// 		map_page(virt_addr + offset, phys_addr + offset, flags);
// 	}

	ASSERT_ALWAYS(insert_virtual_allocated_range(virt_addr, size) >= B_OK);
}


static void
build_physical_memory_list(size_t memory_map_size,
	efi_memory_descriptor* memory_map, size_t descriptor_size,
	uint32_t descriptor_version)
{
	addr_t addr = (addr_t)memory_map;

	gKernelArgs.num_physical_memory_ranges = 0;

	// First scan: Add all usable ranges
	for (size_t i = 0; i < memory_map_size / descriptor_size; ++i) {
		efi_memory_descriptor* entry = (efi_memory_descriptor*)(addr + i * descriptor_size);
		switch (entry->Type) {
		case EfiLoaderCode:
		case EfiLoaderData:
			entry->VirtualStart = entry->PhysicalStart;
			break;
		case EfiBootServicesCode:
		case EfiBootServicesData:
		case EfiConventionalMemory: {
			// Usable memory.
			uint64_t base = entry->PhysicalStart;
			uint64_t size = entry->NumberOfPages * B_PAGE_SIZE;
			insert_physical_memory_range(base, size);
			break;
		}
		case EfiACPIReclaimMemory:
			// ACPI reclaim -- physical memory we could actually use later
			break;
		case EfiRuntimeServicesCode:
		case EfiRuntimeServicesData:
			entry->VirtualStart = entry->PhysicalStart;
			break;
		case EfiMemoryMappedIO:
			entry->VirtualStart = entry->PhysicalStart;
			break;
		}
	}

	uint64_t initialPhysicalMemory = total_physical_memory();

	// Second scan: Remove everything reserved that may overlap
	for (size_t i = 0; i < memory_map_size / descriptor_size; ++i) {
		efi_memory_descriptor* entry = (efi_memory_descriptor*)(addr + i * descriptor_size);
		switch (entry->Type) {
		case EfiLoaderCode:
		case EfiLoaderData:
		case EfiBootServicesCode:
		case EfiBootServicesData:
		case EfiConventionalMemory:
			break;
		default:
			uint64_t base = entry->PhysicalStart;
			uint64_t size = entry->NumberOfPages * B_PAGE_SIZE;
			remove_physical_memory_range(base, size);
		}
	}

	gKernelArgs.ignored_physical_memory
		+= initialPhysicalMemory - total_physical_memory();

	sort_address_ranges(gKernelArgs.physical_memory_range,
		gKernelArgs.num_physical_memory_ranges);
}


static void
build_physical_allocated_list(size_t memory_map_size,
	efi_memory_descriptor* memory_map, size_t descriptor_size,
	uint32_t descriptor_version)
{
	addr_t addr = (addr_t)memory_map;
	for (size_t i = 0; i < memory_map_size / descriptor_size; ++i) {
		efi_memory_descriptor* entry = (efi_memory_descriptor*)(addr + i * descriptor_size);
		switch (entry->Type) {
		case EfiLoaderData: {
			uint64_t base = entry->PhysicalStart;
			uint64_t size = entry->NumberOfPages * B_PAGE_SIZE;
			insert_physical_allocated_range(base, size);
			break;
		}
		default:
			;
		}
	}

	sort_address_ranges(gKernelArgs.physical_allocated_range,
		gKernelArgs.num_physical_allocated_ranges);
}


void
arch_mmu_init()
{
	// Stub
}


void
arch_mmu_post_efi_setup(size_t memory_map_size,
	efi_memory_descriptor* memory_map, size_t descriptor_size,
	uint32_t descriptor_version)
{
	build_physical_allocated_list(memory_map_size, memory_map,
		descriptor_size, descriptor_version);

	// Switch EFI to virtual mode, using the kernel pmap.
	kRuntimeServices->SetVirtualAddressMap(memory_map_size, descriptor_size,
		descriptor_version, memory_map);
#ifdef DUMP_RANGES_AFTER_EXIT_SERIVCES
	TRACE(("phys memory ranges:\n"));
	for (uint32_t i = 0; i < gKernelArgs.num_physical_memory_ranges; i++) {
		uint32_t start = (uint32_t)gKernelArgs.physical_memory_range[i].start;
		uint32_t size = (uint32_t)gKernelArgs.physical_memory_range[i].size;
		TRACE(("    0x%08x-0x%08x, length 0x%08x\n",
			start, start + size, size));
	}

	TRACE(("allocated phys memory ranges:\n"));
	for (uint32_t i = 0; i < gKernelArgs.num_physical_allocated_ranges; i++) {
		uint32_t start = (uint32_t)gKernelArgs.physical_allocated_range[i].start;
		uint32_t size = (uint32_t)gKernelArgs.physical_allocated_range[i].size;
		TRACE(("    0x%08x-0x%08x, length 0x%08x\n",
			start, start + size, size));
	}

	TRACE(("allocated virt memory ranges:\n"));
	for (uint32_t i = 0; i < gKernelArgs.num_virtual_allocated_ranges; i++) {
		uint32_t start = (uint32_t)gKernelArgs.virtual_allocated_range[i].start;
		uint32_t size = (uint32_t)gKernelArgs.virtual_allocated_range[i].size;
		TRACE(("    0x%08x-0x%08x, length 0x%08x\n",
			start, start + size, size));
	}
#endif
}


void
arch_mmu_allocate_kernel_page_tables(void)
{
	uint64* page = CurrentRegime.AllocatePage();

	if (page != NULL) {
		WRITE_SPECIALREG(TTBR1_EL1, page);
		sPageDirectory = page;
	} else {
		panic("Not enough memory for kernel initial page\n");
	}
}


uint32_t
arch_mmu_generate_post_efi_page_tables(size_t memory_map_size,
	efi_memory_descriptor* memory_map, size_t descriptor_size,
	uint32_t descriptor_version)
{
	addr_t memory_map_addr = (addr_t)memory_map;

	MemoryAttributeIndirection currentMair;

// 	arch_mmu_allocate_page_tables();
	arch_mmu_allocate_kernel_page_tables();

	build_physical_memory_list(memory_map_size, memory_map,
		descriptor_size, descriptor_version);

	TRACE(("Mapping Code & Data\n"));

	for (size_t i = 0; i < memory_map_size / descriptor_size; ++i) {
		efi_memory_descriptor* entry = (efi_memory_descriptor*)(memory_map_addr + i * descriptor_size);
		switch (entry->Type) {
		case EfiLoaderCode:
		case EfiLoaderData:
			map_range(entry->VirtualStart, entry->PhysicalStart,
				entry->NumberOfPages * B_PAGE_SIZE,
				ARMv8TranslationTableDescriptor::DefaultCodeAttribute
				| currentMair.MaskOf(MAIR_NORMAL_WB));
			break;
		default:
			;
		}
	}

	TRACE(("Mapping EFI_MEMORY_RUNTIME\n"));
	for (size_t i = 0; i < memory_map_size / descriptor_size; ++i) {
		efi_memory_descriptor* entry = (efi_memory_descriptor*)(memory_map_addr + i * descriptor_size);
		if ((entry->Attribute & EFI_MEMORY_RUNTIME) != 0)
			map_range(entry->VirtualStart, entry->PhysicalStart,
				entry->NumberOfPages * B_PAGE_SIZE,
				ARMv8TranslationTableDescriptor::DefaultCodeAttribute | currentMair.MaskOf(MAIR_NORMAL_WB));
	}

	TRACE(("Mapping \"next\" regions\n"));
	void* cookie = NULL;
	addr_t vaddr;
	phys_addr_t paddr;
	size_t size;
	while (mmu_next_region(&cookie, &vaddr, &paddr, &size)) {
		map_range(vaddr, paddr, size,
			ARMv8TranslationTableDescriptor::DefaultCodeAttribute
			| currentMair.MaskOf(MAIR_NORMAL_WB));
	}

/*  TODO: Not an UART here... inspect dtb?
	// identity mapping for the debug uart
	map_range(0x09000000, 0x09000000, B_PAGE_SIZE,
		ARMv8TranslationTableDescriptor::DefaultPeripheralAttribute
		| currentMair.MaskOf(MAIR_DEVICE_nGnRnE));
*/

/*  TODO: Whole physical map already covered ...
	// identity mapping for page table area
	uint64_t page_table_area = (uint64_t)sFirstPageTable;
	map_range(page_table_area, page_table_area, PAGE_TABLE_AREA_SIZE,
		ARMv8TranslationTableDescriptor::DefaultCodeAttribute
		| currentMair.MaskOf(MAIR_NORMAL_WB));
*/

	map_range(KERNEL_PMAP_BASE, 0, KERNEL_PMAP_SIZE - 1, ARMv8TranslationTableDescriptor::DefaultCodeAttribute | currentMair.MaskOf(MAIR_NORMAL_WB));

	sort_address_ranges(gKernelArgs.virtual_allocated_range,
		gKernelArgs.num_virtual_allocated_ranges);

	addr_t vir_pgdir;
	platform_bootloader_address_to_kernel_address((void*)sPageDirectory, &vir_pgdir);

	gKernelArgs.arch_args.phys_pgdir = (uint64)sPageDirectory;
	gKernelArgs.arch_args.vir_pgdir = (uint32)vir_pgdir;
	gKernelArgs.arch_args.next_pagetable = (uint64)(sNextPageTable) - (uint64)sPageDirectory;

	TRACE(("gKernelArgs.arch_args.phys_pgdir     = 0x%08x\n",
		(uint32_t)gKernelArgs.arch_args.phys_pgdir));
	TRACE(("gKernelArgs.arch_args.vir_pgdir      = 0x%08x\n",
		(uint32_t)gKernelArgs.arch_args.vir_pgdir));
	TRACE(("gKernelArgs.arch_args.next_pagetable = 0x%08x\n",
		(uint32_t)gKernelArgs.arch_args.next_pagetable));

	return (uint64_t)sPageDirectory;
}
