/*
 * Copyright 2007, François Revol, revol@free.fr.
 * Distributed under the terms of the MIT License.
 *
 * Copyright 2003-2006, Axel Dörfler, axeld@pinc-software.de.
 * Distributed under the terms of the MIT License.
 *
 * Copyright 2001, Travis Geiselbrecht. All rights reserved.
 * Distributed under the terms of the NewOS License.
 */


#include <arch/debug_console.h>
#include <arch/generic/debug_uart.h>
#include <arch/arm/arch_uart_pl011.h>
#include <boot/kernel_args.h>
#include <kernel.h>
#include <vm/vm.h>
#include <string.h>


static DebugUART *sArchDebugUART = NULL;


void
arch_debug_remove_interrupt_handler(uint32 line)
{
}


void
arch_debug_install_interrupt_handlers(void)
{
}


int
arch_debug_blue_screen_try_getchar(void)
{
	// TODO: Implement correctly!
	return arch_debug_blue_screen_getchar();
}


char
arch_debug_blue_screen_getchar(void)
{
	return arch_debug_serial_getchar();
}


int
arch_debug_serial_try_getchar(void)
{
	if (sArchDebugUART == NULL)
		return -1;

	return sArchDebugUART->GetChar(false);
}


char
arch_debug_serial_getchar(void)
{
	if (sArchDebugUART == NULL)
		return '\0';

	return sArchDebugUART->GetChar(true);
}


void
arch_debug_serial_putchar(const char c)
{
	if (sArchDebugUART == NULL)
		return;

	sArchDebugUART->PutChar(c);
}


void
arch_debug_serial_puts(const char *s)
{
	while (*s != '\0') {
		char ch = *s;
		if (ch == '\n') {
			arch_debug_serial_putchar('\r');
			arch_debug_serial_putchar('\n');
		} else if (ch != '\r')
			arch_debug_serial_putchar(ch);
		s++;
	}
}


void
arch_debug_serial_early_boot_message(const char *string)
{
	arch_debug_serial_puts(string);
}


status_t
arch_debug_console_init(kernel_args *args)
{
	if (args->arch_args.uart.kind[0] != 0) {
		// we expect that debug uart is already mapped into kernel virtual address space by the efi loader
		addr_t base = args->arch_args.uart.regs.start;
		int64 clock = args->arch_args.uart.clock;

		if (clock == 0)
			clock = 0x16e3600;
		sArchDebugUART = arch_get_uart_pl011(base, clock);
	}

	if (sArchDebugUART == NULL)
		return B_ERROR;

	sArchDebugUART->InitEarly();

	return B_OK;
}


status_t
arch_debug_console_init_settings(kernel_args *args)
{
	return B_OK;
}
