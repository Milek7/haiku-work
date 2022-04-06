/*
 * Copyright 2009, Haiku Inc.
 * Distributed under the terms of the MIT License.
 */


#include "pci_controller.h"

#include <kernel/debug.h>
#include <util/Vector.h>

#include "pci_private.h"

#include <ACPI.h> // module
#include "acpi.h" // acpica

addr_t gPCIeBase;
uint8 gStartBusNumber;
uint8 gEndBusNumber;
addr_t gPCIioBase;

extern pci_controller pci_controller_ecam;

enum RangeType
{
	RANGE_IO,
	RANGE_MEM
};

struct PciRange
{
	RangeType type;
	phys_addr_t hostAddr;
	phys_addr_t pciAddr;
	size_t length;
};

static Vector<PciRange> *sRanges;

template<typename T>
ACPI_ADDRESS64_ATTRIBUTE AcpiCopyAddressAttr(const T &src)
{
	ACPI_ADDRESS64_ATTRIBUTE dst;

	dst.Granularity = src.Granularity;
	dst.Minimum = src.Minimum;
	dst.Maximum = src.Maximum;
	dst.TranslationOffset = src.TranslationOffset;
	dst.AddressLength = src.AddressLength;

	return dst;
}

static acpi_status
AcpiCrsScanCallback(ACPI_RESOURCE *res, void *context)
{
	Vector<PciRange> &ranges = *(Vector<PciRange>*)context;

	ACPI_ADDRESS64_ATTRIBUTE address;
	if (res->Type == ACPI_RESOURCE_TYPE_ADDRESS16)
		address = AcpiCopyAddressAttr(((ACPI_RESOURCE_ADDRESS16*)&res->Data)->Address);
	else if (res->Type == ACPI_RESOURCE_TYPE_ADDRESS32)
		address = AcpiCopyAddressAttr(((ACPI_RESOURCE_ADDRESS32*)&res->Data)->Address);
	else if (res->Type == ACPI_RESOURCE_TYPE_ADDRESS64)
		address = AcpiCopyAddressAttr(((ACPI_RESOURCE_ADDRESS64*)&res->Data)->Address);
	else
		return AE_OK;

	ACPI_RESOURCE_ADDRESS &common = *((ACPI_RESOURCE_ADDRESS*)&res->Data);

	if (common.ResourceType != 0 && common.ResourceType != 1)
		return AE_OK;

	ASSERT(address.Minimum + address.AddressLength - 1 == address.Maximum);

	PciRange range;
	range.type = (common.ResourceType == 0 ? RANGE_MEM : RANGE_IO);
	range.hostAddr = address.Minimum + address.TranslationOffset;
	range.pciAddr = address.Minimum;
	range.length = address.AddressLength;
	ranges.PushBack(range);

	return AE_OK;
}

status_t
pci_controller_init(void)
{
	if (gPCIRootNode == NULL)
		return B_NO_INIT;

	dprintf("PCI: pci_controller_init\n");

	status_t res;
	acpi_module_info *acpiModule;
	acpi_device_module_info *acpiDeviceModule;
	acpi_device acpiDevice;

	res = get_module(B_ACPI_MODULE_NAME, (module_info**)&acpiModule);
	if (res != B_OK)
		return B_ERROR;

	ACPI_TABLE_MCFG *mcfg;
	res = acpiModule->get_table(ACPI_SIG_MCFG, 0, (void**)&mcfg);
	if (res != B_OK)
		return B_ERROR;

	{
		device_node* acpiParent = gDeviceManager->get_parent_node(gPCIRootNode);
		if (acpiParent == NULL)
			return B_ERROR;

		res = gDeviceManager->get_driver(acpiParent,
			(driver_module_info**)&acpiDeviceModule, (void**)&acpiDevice);
		if (res != B_OK)
			return B_ERROR;

		gDeviceManager->put_node(acpiParent);
	}

	sRanges = new Vector<PciRange>();

	acpi_status acpi_res = acpiDeviceModule->walk_resources(acpiDevice,
		(ACPI_STRING)METHOD_NAME__CRS, AcpiCrsScanCallback, sRanges);

	if (ACPI_FAILURE(acpi_res))
		return B_ERROR;

	for (Vector<PciRange>::Iterator it = sRanges->Begin(); it != sRanges->End(); it++) {
		if (it->type != RANGE_IO)
			continue;

		if (gPCIioBase != 0) {
			dprintf("PCI: multiple io ranges not supported!");
			continue;
		}

		area_id area = map_physical_memory("pci io",
			it->hostAddr, it->length, B_ANY_KERNEL_ADDRESS,
			B_KERNEL_READ_AREA | B_KERNEL_WRITE_AREA, (void **)&gPCIioBase);

		if (area < 0)
			return B_ERROR;
	}

	ACPI_MCFG_ALLOCATION* end = (ACPI_MCFG_ALLOCATION*) ((char*)mcfg + mcfg->Header.Length);
	ACPI_MCFG_ALLOCATION* alloc = (ACPI_MCFG_ALLOCATION*) (mcfg + 1);

	if (alloc + 1 != end)
		dprintf("PCI: multiple host bridges not supported!");

	for (; alloc < end; alloc++) {
		dprintf("PCI: mechanism addr: %" B_PRIx64 ", seg: %x, start: %x, end: %x\n",
			alloc->Address, alloc->PciSegment, alloc->StartBusNumber, alloc->EndBusNumber);

		if (alloc->PciSegment != 0) {
			dprintf("PCI: multiple segments not supported!");
			continue;
		}

		gStartBusNumber = alloc->StartBusNumber;
		gEndBusNumber = alloc->EndBusNumber;

		area_id area = map_physical_memory("pci config",
			alloc->Address, (gEndBusNumber - gStartBusNumber + 1) << 20, B_ANY_KERNEL_ADDRESS,
			B_KERNEL_READ_AREA | B_KERNEL_WRITE_AREA, (void **)&gPCIeBase);

		if (area < 0)
			break;

		dprintf("PCI: ecam controller found\n");
		return pci_controller_add(&pci_controller_ecam, NULL);
	}

	return B_ERROR;
}


phys_addr_t
pci_ram_address(phys_addr_t addr)
{
	for (Vector<PciRange>::Iterator it = sRanges->Begin(); it != sRanges->End(); it++) {
		if (addr >= it->pciAddr && addr < it->pciAddr + it->length) {
			phys_addr_t result = addr - it->pciAddr;
			if (it->type != RANGE_IO)
				result += it->hostAddr;
			return result;
		}
	}

	dprintf("PCI: requested translation of invalid address %" B_PRIxPHYSADDR "\n", addr);
	return 0;
}
