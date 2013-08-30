/*
 * SpanDSP - a series of DSP components for telephony
 *
 * alloc.h - memory allocation handling.
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

#if !defined(_SPANDSP_ALLOC_H_)
#define _SPANDSP_ALLOC_H_

/* Notes:
    - Most platforms don't have an aligned realloc function, so we don't try to
      support an aligned realloc on any platform.
    - Some platforms use a special free function for memory which was allocated
      by alligned allocation functions. We use a separate aligned_free function
      on all platforms, for compatibility, even though it may simply reduce to
      free().
 */
  
typedef void *(*span_aligned_alloc_t)(size_t alignment, size_t size);
typedef void (*span_aligned_free_t)(void *ptr);
typedef void *(*span_alloc_t)(size_t size);
typedef void *(*span_realloc_t)(void *ptr, size_t size);
typedef void (*span_free_t)(void *ptr);

#if defined(__cplusplus)
extern "C"
{
#endif

/* Allocate size bytes allocated to ALIGNMENT bytes.  */
SPAN_DECLARE(void *) span_aligned_alloc(size_t alignment, size_t size);

/* Free a block allocated by span_aligned_alloc, or span_aligned_realloc. */
SPAN_DECLARE(void) span_aligned_free(void *ptr);

/* Allocate size bytes of memory. */
SPAN_DECLARE(void *) span_alloc(size_t size);

/* Re-allocate the previously allocated block in ptr, making the new block size bytes long. */
SPAN_DECLARE(void *) span_realloc(void *ptr, size_t size);

/* Free a block allocated by span_alloc or span_realloc. */
SPAN_DECLARE(void) span_free(void *ptr);

SPAN_DECLARE(int) span_mem_allocators(span_alloc_t custom_alloc,
                                      span_realloc_t custom_realloc,
                                      span_free_t custom_free,
                                      span_aligned_alloc_t custom_aligned_alloc,
                                      span_aligned_free_t custom_aligned_free);
                                      
#if defined(__cplusplus)
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/
