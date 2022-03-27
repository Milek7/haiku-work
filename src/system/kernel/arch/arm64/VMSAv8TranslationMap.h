#pragma once

#include <vm/VMTranslationMap.h>
#include <arch_cpu_defs.h>

struct VMSAv8TranslationMap : public VMTranslationMap {
public:
    VMSAv8TranslationMap(bool kernel, phys_addr_t pageTable, int pageBits, int vaBits, int minBlockLevel);
    ~VMSAv8TranslationMap();

	virtual	bool				Lock();
	virtual	void				Unlock();

	virtual	addr_t				MappedSize() const;
	virtual	size_t				MaxPagesNeededToMap(addr_t start,
									addr_t end) const;

	virtual	status_t			Map(addr_t virtualAddress,
									phys_addr_t physicalAddress,
									uint32 attributes, uint32 memoryType,
									vm_page_reservation* reservation);
	virtual	status_t			Unmap(addr_t start, addr_t end);

	virtual	status_t			UnmapPage(VMArea* area, addr_t address,
									bool updatePageQueue);
/*
	virtual	void				UnmapPages(VMArea* area, addr_t base,
									size_t size, bool updatePageQueue);
	virtual	void				UnmapArea(VMArea* area,
									bool deletingAddressSpace,
									bool ignoreTopCachePageFlags);
*/

	virtual	status_t			Query(addr_t virtualAddress,
									phys_addr_t* _physicalAddress,
									uint32* _flags);
	virtual	status_t			QueryInterrupt(addr_t virtualAddress,
									phys_addr_t* _physicalAddress,
									uint32* _flags);

	virtual	status_t			Protect(addr_t base, addr_t top,
									uint32 attributes, uint32 memoryType);

	virtual	status_t			ClearFlags(addr_t virtualAddress,
									uint32 flags);

	virtual	bool				ClearAccessedAndModified(
									VMArea* area, addr_t address,
									bool unmapIfUnaccessed,
									bool& _modified);

	virtual	void				Flush();

	enum HWFeature {
		HW_ACCESS = 0x1,
		HW_DIRTY = 0x2
	};

	static uint32_t fHwFeature;
	static uint64_t fMair;

	static uint64_t GetMemoryAttr(uint32 attributes, uint32 memoryType, bool isKernel);
	static int CalcStartLevel(int vaBits, int pageBits);

private:
    bool fIsKernel;
    phys_addr_t fPageTable;
	int fPageBits;
	int fVaBits;
	int fMinBlockLevel;

	int fInitialLevel;

	enum class VMAction {
		MAP,
		SET_ATTR,
		CLEAR_FLAGS,
		UNMAP
	};

	uint64_t tmp_pte; // todo: remove kludge

private:
	static uint8_t MairIndex(uint8_t type);
	uint64_t ClearAttrFlags(uint64_t attr, uint32 flags);
	uint64_t MoveAttrFlags(uint64_t newAttr, uint64_t oldAttr);
	bool ValidateVa(addr_t va);
	uint64_t* TableFromPa(phys_addr_t pa);
	uint64_t MakeBlock(phys_addr_t pa, int level, uint64_t attr);
	void FreeTable(phys_addr_t ptPa, int level);
	phys_addr_t MakeTable(phys_addr_t ptPa, int level, int index, vm_page_reservation* reservation);
	void MapRange(phys_addr_t ptPa, int level, addr_t va, phys_addr_t pa, size_t size, VMAction action, uint64_t attr, vm_page_reservation* reservation);
	bool WalkTable(phys_addr_t ptPa, int level, addr_t va, phys_addr_t *pa, uint64_t *attr);
};
