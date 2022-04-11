/*
 * Copyright 2019 Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#include <boot/stage2.h>
#include <kernel.h>
#include <debug.h>
#include <int.h>

#include <timer.h>
#include <arch/timer.h>
#include <arch/cpu.h>

static uint64 sTimerTicksUS;
static bigtime_t sTimerMaxInterval;

#define TIMER_ARMED (1)
#define TIMER_MASKED (2 | 1)
#define TIMER_IRQ 30

void
arch_timer_set_hardware_timer(bigtime_t timeout)
{
	if (timeout > sTimerMaxInterval)
		timeout = sTimerMaxInterval;

	WRITE_SPECIALREG(CNTP_TVAL_EL0, timeout * sTimerTicksUS);
	WRITE_SPECIALREG(CNTP_CTL_EL0, TIMER_ARMED);
}


void
arch_timer_clear_hardware_timer()
{
	WRITE_SPECIALREG(CNTP_CTL_EL0, TIMER_MASKED);
}

int32
arch_timer_interrupt(void *data)
{
	WRITE_SPECIALREG(CNTP_CTL_EL0, TIMER_MASKED);
	return timer_interrupt();
}

int
arch_init_timer(kernel_args *args)
{
	sTimerTicksUS = READ_SPECIALREG(CNTFRQ_EL0) / 1000000;
	sTimerMaxInterval = INT32_MAX / sTimerTicksUS;

	WRITE_SPECIALREG(CNTP_CTL_EL0, TIMER_MASKED);
	reserve_io_interrupt_vectors(1, TIMER_IRQ, INTERRUPT_TYPE_IRQ);
	install_io_interrupt_handler(TIMER_IRQ, &arch_timer_interrupt, NULL, 0);

	return B_OK;
}


bigtime_t
system_time(void)
{
	return READ_SPECIALREG(CNTPCT_EL0) / sTimerTicksUS;
}
