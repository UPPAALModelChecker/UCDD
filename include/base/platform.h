/* -*- mode: C++; c-file-style: "stroustrup"; c-basic-offset: 4; indent-tabs-mode: nil; -*- */
/*********************************************************************
 *
 * Filename : platform.h (base)
 * C header.
 *
 * Platform dependant code abstraction layer. This API gives access
 * to miscellanous platform dependent code, such as:
 * - getting time
 * - ... TODO
 *
 * NOTE: C interface and implementation
 *
 * This file is a part of the UPPAAL toolkit.
 * Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
 * All right reserved.
 *
 * v 1.3 reviewed.
 * $Id: platform.h,v 1.4 2005/01/25 22:05:19 behrmann Exp $
 *
 **********************************************************************/

#ifndef INCLUDE_BASE_PLATFORM_H
#define INCLUDE_BASE_PLATFORM_H

#include "base/inttypes.h"
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/** strptime is not available under Windows
 * so we have an implementation of it,
 * otherwise include <time.h> to get the
 * standard (posix) version under Linux.
 */
char* strptime2(const char* buf, const char* fmt, struct tm* tm, time_t* now);

/** Finer grain time measure function.
 * @return number of CPU cycles (TSC asm call).
 * This is officially implemented only starting with Intel Pentium II platforms.
 * Unreliable due to power saving (hibernation, freq scaling), multi-cpu/core.
 */
static inline uint64_t gettsc(void)
{
#if defined(__GNUG__) && defined(i386)
    uint32_t lo, hi;
    asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return (uint64_t)hi << 32 | lo;
#else
    return 0;
#endif
}

/**
 * Returns an OS-specific error description for given error code.
 * (this is a replacement for strerror which is inconsistent
 * with errno.h on MinGW32).
 */
const char* oserror(int error_code);

typedef struct meminfo_t
{
    // host information in kB:
    uint64_t phys_total, phys_avail, phys_cache;  // physical RAM
    uint64_t swap_total, swap_avail;              // physical swap space
    uint64_t virt_total, virt_avail;              // virtual memory
} meminfo_t;

/** Gets the hosting machine memory information. */
void base_getMemInfo(meminfo_t*);

typedef struct procinfo_t
{
    // process information in kB and milliseconds:
    uint64_t mem_virt, mem_work, mem_swap;    // memory configuration
    uint64_t time_user, time_sys, time_real;  // CPU time usage
} procinfo_t;

/** Initializes the process information gathering. */
void base_initProcInfo();
/** Gets the current process memory and time consumption sample. */
void base_getProcInfo(procinfo_t*);
/** Gets the current process memory and time consumption sample and stores the maximum (peak)
 * values. */
void base_getProcInfoMax(procinfo_t*);

/* MAC addresses. */

typedef uint8_t mac_adr_t[8];

typedef struct
{
    size_t size;
    mac_adr_t mac[];
} maclist_t;

/**
 * Get the available MAC addresses. The
 * structure is malloc allocated and should
 * be freed. Returns NULL in case of error.
 */
maclist_t* base_getMAC();

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_BASE_PLATFORM_H */
