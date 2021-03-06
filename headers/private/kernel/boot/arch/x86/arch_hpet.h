/*
 * Copyright 2008, Dustin Howett, dustin.howett@gmail.com. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef BOOT_ARCH_HPET_H
#define BOOT_ARCH_HPET_H

#include <SupportDefs.h>
#include <arch/x86/arch_hpet.h>

#ifdef __cplusplus
extern "C" {
#endif

void hpet_init(void);

#ifdef __cplusplus
}
#endif

#endif	/* BOOT_ARCH_HPET_H */
