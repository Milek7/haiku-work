/*
 * Copyright 2019-2022 Haiku, Inc. All rights reserved.
 * Released under the terms of the MIT License.
 */

#include "string.h"

#include <boot/platform.h>
#include <boot/stage2.h>
#include <arch_acpi.h>

#include "serial.h"
#include "acpi.h"

#include <arch/arm/arch_uart_pl011.h>

static char sUART[sizeof(ArchUARTPL011)];

void
arch_handle_acpi()
{
	acpi_spcr *spcr = (acpi_spcr*)acpi_find_table(ACPI_SPCR_SIGNATURE);
	if (spcr != NULL) {
		uart_info &uart = gKernelArgs.arch_args.uart;
		strcpy(uart.kind, UART_KIND_PL011);
		uart.regs.start = spcr->base_address.address;
		uart.regs.size = B_PAGE_SIZE;
		uart.irq = spcr->gisv;
		uart.clock = spcr->clock;

		gUART = new(sUART) ArchUARTPL011(uart.regs.start, uart.clock != 0 ? uart.clock : 0x16e3600);

		dprintf("discovered uart from acpi: base=%lx, irq=%u, clock=%lu\n", uart.regs.start, uart.irq, uart.clock);
	}
}