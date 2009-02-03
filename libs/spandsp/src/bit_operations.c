/*
 * SpanDSP - a series of DSP components for telephony
 *
 * bit_operations.c - Various bit level operations, such as bit reversal
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2006 Steve Underwood
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
 *
 * $Id: bit_operations.c,v 1.16 2009/02/03 16:28:39 steveu Exp $
 */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <inttypes.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>
#include <memory.h>

#include "spandsp/telephony.h"
#include "spandsp/bit_operations.h"

SPAN_DECLARE(uint16_t) bit_reverse16(uint16_t x)
{
    x = (x >> 8) | (x << 8);
    x = ((x & 0xF0F0) >> 4) | ((x & 0x0F0F) << 4);
    x = ((x & 0xCCCC) >> 2) | ((x & 0x3333) << 2);
    return ((x & 0xAAAA) >> 1) | ((x & 0x5555) << 1);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(uint32_t) bit_reverse32(uint32_t x)
{
    x = (x >> 16) | (x << 16);
    x = ((x & 0xFF00FF00) >> 8) | ((x & 0x00FF00FF) << 8);
    x = ((x & 0xF0F0F0F0) >> 4) | ((x & 0x0F0F0F0F) << 4);
    x = ((x & 0xCCCCCCCC) >> 2) | ((x & 0x33333333) << 2);
    return ((x & 0xAAAAAAAA) >> 1) | ((x & 0x55555555) << 1);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(uint32_t) bit_reverse_4bytes(uint32_t x)
{
    x = ((x & 0xF0F0F0F0) >> 4) | ((x & 0x0F0F0F0F) << 4);
    x = ((x & 0xCCCCCCCC) >> 2) | ((x & 0x33333333) << 2);
    return ((x & 0xAAAAAAAA) >> 1) | ((x & 0x55555555) << 1);
}
/*- End of function --------------------------------------------------------*/

#if defined(__x86_64__)
SPAN_DECLARE(uint64_t) bit_reverse_8bytes(uint64_t x)
{
    x = ((x & 0xF0F0F0F0F0F0F0F0LLU) >> 4) | ((x & 0x0F0F0F0F0F0F0F0FLLU) << 4);
    x = ((x & 0xCCCCCCCCCCCCCCCCLLU) >> 2) | ((x & 0x3333333333333333LLU) << 2);
    return ((x & 0xAAAAAAAAAAAAAAAALLU) >> 1) | ((x & 0x5555555555555555LLU) << 1);
}
/*- End of function --------------------------------------------------------*/
#endif

SPAN_DECLARE(void) bit_reverse(uint8_t to[], const uint8_t from[], int len)
{
#if defined(SPANDSP_MISALIGNED_ACCESS_FAILS)
    int i;
#else
    const uint8_t *y1;
    uint8_t *z1;
    const uint32_t *y4;
    uint32_t *z4;
    uint32_t x4;
#if defined(__x86_64__)
    const uint64_t *y8;
    uint64_t *z8;
    uint64_t x8;
#endif
#endif

#if defined(SPANDSP_MISALIGNED_ACCESS_FAILS)
    /* This code works byte by byte, so it works on machines where misalignment
       is either desperately slow (its a bit slow on practically any machine, but
       some machines make it desparately slow) or fails. */
    for (i = 0;  i < len;  i++)
        to[i] = bit_reverse8(from[i]);
#else
    /* This code is this is based on the woolly assumption that the start of the buffers
       is memory aligned. If it isn't, the routine will be less efficient on some machines,
       but might not work at all on others. */
#if defined(__x86_64__)
    y8 = (const uint64_t *) from;
    z8 = (uint64_t *) to;
    while (len >= sizeof(uint64_t))
    {
        x8 = *y8++;
        x8 = ((x8 & 0xF0F0F0F0F0F0F0F0LLU) >> 4) | ((x8 & 0x0F0F0F0F0F0F0F0FLLU) << 4);
        x8 = ((x8 & 0xCCCCCCCCCCCCCCCCLLU) >> 2) | ((x8 & 0x3333333333333333LLU) << 2);
        *z8++ = ((x8 & 0xAAAAAAAAAAAAAAAALLU) >> 1) | ((x8 & 0x5555555555555555LLU) << 1);
        len -= sizeof(uint64_t);
    }
    y4 = (const uint32_t *) y8;
    z4 = (uint32_t *) z8;
#else
    y4 = (const uint32_t *) from;
    z4 = (uint32_t *) to;
#endif
    while (len >= sizeof(uint32_t))
    {
        x4 = *y4++;
        x4 = ((x4 & 0xF0F0F0F0) >> 4) | ((x4 & 0x0F0F0F0F) << 4);
        x4 = ((x4 & 0xCCCCCCCC) >> 2) | ((x4 & 0x33333333) << 2);
        *z4++ = ((x4 & 0xAAAAAAAA) >> 1) | ((x4 & 0x55555555) << 1);
        len -= sizeof(uint32_t);
    }
    y1 = (const uint8_t *) y4;
    z1 = (uint8_t *) z4;
    while (len-- > 0)
        *z1++ = bit_reverse8(*y1++);
#endif
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) one_bits32(uint32_t x)
{
    x = x - ((x >> 1) & 0x55555555);
    /* We now have 16 2-bit counts */
    x = (x & 0x33333333) + ((x >> 2) & 0x33333333);
    /* We now have 8 4-bit counts */
    x = (x + (x >> 4)) & 0x0F0F0F0F;
    /* We now have 4 8-bit counts */
#if defined(__i386__)  ||  defined(__x86_64__)  ||  defined(__ppc__)  ||  defined(__powerpc__)
    /* If multiply is fast */
    return (x*0x01010101) >> 24;
#else
    /* If multiply is slow */
    x += (x >> 8);
    x += (x >> 16);
    return (x & 0x0000003F);
#endif
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(uint32_t) make_mask32(uint32_t x)
{
    x |= (x >> 1);
    x |= (x >> 2);
    x |= (x >> 4);
    x |= (x >> 8);
    x |= (x >> 16);
    return x;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(uint16_t) make_mask16(uint16_t x)
{
    x |= (x >> 1);
    x |= (x >> 2);
    x |= (x >> 4);
    x |= (x >> 8);
    return x;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
