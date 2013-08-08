/*
 * SpanDSP - a series of DSP components for telephony
 *
 * alloc.c - memory allocation handling.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2013 Steve Underwood
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 2.1,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <limits.h>
#include <stdio.h>
#include <stdarg.h>
#include <fcntl.h>
#define __USE_ISOC11
#include <stdlib.h>
#if defined(HAVE_MALLOC_H)
#include <malloc.h>
#endif
#include <inttypes.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/time.h>
#include <time.h>

#include "spandsp/telephony.h"
#include "spandsp/alloc.h"

static void *fake_aligned_alloc(size_t alignment, size_t size);

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4232)	/* address of dllimport is not static, identity not guaranteed */
#endif

span_alloc_t __span_alloc = malloc;
#if defined(HAVE_ALIGNED_ALLOC)
span_aligned_alloc_t __span_aligned_alloc = aligned_alloc;
#elif defined(HAVE_MEMALIGN)
span_aligned_alloc_t __span_aligned_alloc = memalign;
#elif defined(HAVE_POSIX_MEMALIGN)
static void *fake_posix_memalign(size_t alignment, size_t size);
span_aligned_alloc_t __span_aligned_alloc = fake_posix_memalign;
#else
span_aligned_alloc_t __span_aligned_alloc = fake_aligned_alloc;
#endif
span_realloc_t __span_realloc = realloc;
span_free_t __span_free = free;

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#if defined(HAVE_ALIGNED_ALLOC)
#elif defined(HAVE_MEMALIGN)
#elif defined(HAVE_POSIX_MEMALIGN)
static void *fake_posix_memalign(size_t alignment, size_t size)
{
    void *ptr;

    /* Make posix_memalign look like the more modern aligned_alloc */
    posix_memalign(&ptr, alignment, size);
    return ptr;
}
/*- End of function --------------------------------------------------------*/
#endif

static void *fake_aligned_alloc(size_t alignment, size_t size)
{
    return NULL;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void *) span_alloc(size_t size)
{
    return __span_alloc(size);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void *) span_aligned_alloc(size_t alignment, size_t size)
{
    return __span_aligned_alloc(alignment, size);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void *) span_realloc(void *ptr, size_t size)
{
    return __span_realloc(ptr, size);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) span_free(void *ptr)
{
    __span_free(ptr);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) span_mem_allocators(span_alloc_t custom_alloc,
                                      span_aligned_alloc_t custom_aligned_alloc,
                                      span_realloc_t custom_realloc,
                                      span_free_t custom_free)
{
    if (custom_alloc == NULL  ||  custom_realloc == NULL  ||  custom_free == NULL)
        return -1;
    __span_alloc = custom_alloc;
    if (custom_aligned_alloc)
        __span_aligned_alloc = custom_aligned_alloc;
    else
        __span_aligned_alloc = fake_aligned_alloc;
    __span_realloc = custom_realloc;
    __span_free = custom_free;
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
