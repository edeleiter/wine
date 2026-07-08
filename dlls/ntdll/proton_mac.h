/*
 * proton-mac: shared constants for the ARM64EC-Wine-on-macOS port.
 *
 * Included by both the PE side (dlls/ntdll/thread.c) and the unix side
 * (dlls/ntdll/unix/virtual.c) so the values that MUST agree across the two
 * compilation worlds are defined exactly once.
 *
 * Copyright 2026 the proton-mac project.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or (at
 * your option) any later version.
 */

#ifndef __WINE_NTDLL_PROTON_MAC_H
#define __WINE_NTDLL_PROTON_MAC_H

/* KUSER_SHARED_DATA virtual address on arm64 macOS.
 *
 * Native arm64 macOS cannot map the low 4 GB (mandatory __PAGEZERO), so the
 * Windows-fixed 0x7ffe0000 KUSER address is unusable. We relocate KUSER to a
 * fixed 16 TB slot that sits ONE 64 KB granule BELOW address_space_start
 * (0x100000010000, see unix/virtual.c), which carves it out of the general
 * allocation range so nothing else can land on it.
 *
 * BOTH definitions of `user_shared_data` (thread.c on the PE side, virtual.c on
 * the unix side) MUST use this value: they map/read the same physical KUSER page
 * at this VA, and a mismatch corrupts every KUSER access. Defining it here makes
 * a divergence impossible. */
#define PROTON_MAC_KUSER_ADDR  ((void *)0x100000000000)

#endif /* __WINE_NTDLL_PROTON_MAC_H */
