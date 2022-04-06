/*
 * Copyright 2009, Haiku Inc.
 * Distributed under the terms of the MIT License.
 */


#include "pci_io.h"
#include "pci_private.h"

extern addr_t gPCIioBase;

status_t
pci_io_init()
{
	return B_OK;
}


uint8
pci_read_io_8(int mapped_io_addr)
{
	volatile uint8* ptr = (uint8*)(gPCIioBase + mapped_io_addr);
	return *ptr;
}


void
pci_write_io_8(int mapped_io_addr, uint8 value)
{
	volatile uint8* ptr = (uint8*)(gPCIioBase + mapped_io_addr);
	*ptr = value;
}


uint16
pci_read_io_16(int mapped_io_addr)
{
	volatile uint16* ptr = (uint16*)(gPCIioBase + mapped_io_addr);
	return *ptr;
}


void
pci_write_io_16(int mapped_io_addr, uint16 value)
{
	volatile uint16* ptr = (uint16*)(gPCIioBase + mapped_io_addr);
	*ptr = value;
}


uint32
pci_read_io_32(int mapped_io_addr)
{
	volatile uint32* ptr = (uint32*)(gPCIioBase + mapped_io_addr);
	return *ptr;
}


void
pci_write_io_32(int mapped_io_addr, uint32 value)
{
	volatile uint32* ptr = (uint32*)(gPCIioBase + mapped_io_addr);
	*ptr = value;
}
