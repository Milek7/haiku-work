#pragma once

#include <vm/VMTranslationMap.h>
#include <arch_cpu_defs.h>

struct PMAPPhysicalPageMapper : public VMPhysicalPageMapper
{
	virtual	status_t			GetPage(phys_addr_t physicalAddress,
									addr_t* _virtualAddress,
									void** _handle);
	virtual	status_t			PutPage(addr_t virtualAddress,
									void* handle);

	virtual	status_t			GetPageCurrentCPU(
									phys_addr_t physicalAddress,
									addr_t* _virtualAddress,
									void** _handle) { return GetPage(physicalAddress, _virtualAddress, _handle); }
	virtual	status_t			PutPageCurrentCPU(addr_t virtualAddress,
									void* _handle) { return PutPage(virtualAddress, _handle); }

	virtual	status_t			GetPageDebug(phys_addr_t physicalAddress,
									addr_t* _virtualAddress,
									void** _handle) { return GetPage(physicalAddress, _virtualAddress, _handle); }
	virtual	status_t			PutPageDebug(addr_t virtualAddress,
									void* _handle) { return PutPage(virtualAddress, _handle); }

	virtual	status_t			MemsetPhysical(phys_addr_t address, int value,
									phys_size_t length);
	virtual	status_t			MemcpyFromPhysical(void* to, phys_addr_t from,
									size_t length, bool user);
	virtual	status_t			MemcpyToPhysical(phys_addr_t to,
									const void* from, size_t length,
									bool user);
	virtual	void				MemcpyPhysicalPage(phys_addr_t to,
									phys_addr_t from);
};