/*
 * SpanDSP - a series of DSP components for telephony
 *
 * bit_operations_tests.c
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2006 Steve Underwood
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Id: bit_operations_tests.c,v 1.14 2008/05/13 13:17:25 steveu Exp $
 */

/*! \page bit_operations_tests_page Bit operations tests
\section bit_operations_tests_page_sec_1 What does it do?
These tests check the operation of efficient bit manipulation routines, by comparing
their operation with very dumb brute force versions of the same functionality.

\section bit_operations_tests_page_sec_2 How is it used?
*/

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>

#include "spandsp.h"

uint8_t from[1000000];
uint8_t to[1000000];

static __inline__ int top_bit_dumb(unsigned int data)
{
    int i;
    
    if (data == 0)
        return -1;
    for (i = 31;  i >= 0;  i--)
    {
        if ((data & (1 << i)))
            return i;
    }
    return -1;
}
/*- End of function --------------------------------------------------------*/

static __inline__ int bottom_bit_dumb(unsigned int data)
{
    int i;
    
    if (data == 0)
        return -1;
    for (i = 0;  i < 32;  i++)
    {
        if ((data & (1 << i)))
            return i;
    }
    return -1;
}
/*- End of function --------------------------------------------------------*/

static __inline__ uint8_t bit_reverse8_dumb(uint8_t data)
{
    int i;
    int result;
    
    result = 0;
    for (i = 0;  i < 8;  i++)
    {
        result = (result << 1) | (data & 1); 
        data >>= 1;
    }
    return result;
}
/*- End of function --------------------------------------------------------*/

static __inline__ uint32_t bit_reverse_4bytes_dumb(uint32_t data)
{
    int i;
    uint32_t result;
    
    result = 0;
    for (i = 0;  i < 8;  i++)
    {
        result = (result << 1) | (data & 0x01010101); 
        data >>= 1;
    }
    return result;
}
/*- End of function --------------------------------------------------------*/

static __inline__ uint16_t bit_reverse16_dumb(uint16_t data)
{
    int i;
    uint16_t result;
    
    result = 0;
    for (i = 0;  i < 16;  i++)
    {
        result = (result << 1) | (data & 1); 
        data >>= 1;
    }
    return result;
}
/*- End of function --------------------------------------------------------*/

static __inline__ uint32_t bit_reverse32_dumb(uint32_t data)
{
    int i;
    uint32_t result;
    
    result = 0;
    for (i = 0;  i < 32;  i++)
    {
        result = (result << 1) | (data & 1); 
        data >>= 1;
    }
    return result;
}
/*- End of function --------------------------------------------------------*/

static __inline__ int parity8_dumb(uint8_t x)
{
    uint8_t y;
    int i;

    for (y = 0, i = 0;  i < 8;  i++)
    {
        y ^= (x & 1);
        x >>= 1;
    }
    return y;
}
/*- End of function --------------------------------------------------------*/

static __inline__ int one_bits32_dumb(uint32_t x)
{
    int i;
    int bits;
    
    bits = 0;
    for (i = 0;  i < 32;  i++)
    {
        if (x & 1)
            bits++;
        x >>= 1;
    }
    return bits;
}
/*- End of function --------------------------------------------------------*/
    
int main(int argc, char *argv[])
{
    int i;
    uint32_t x;
    uint8_t ax;
    uint8_t bx;
    uint16_t ax16;
    uint16_t bx16;
    uint32_t ax32;
    uint32_t bx32;

    for (i = 0, x = 0;  i < 100000;  i++)
    {
        ax = top_bit_dumb(x);
        bx = top_bit(x);
        if (ax != bx)
        {
            printf("Test failed: top bit mismatch 0x%" PRIx32 " -> %u %u\n", x, ax, bx);
            exit(2);
        }
        ax = bottom_bit_dumb(x);
        bx = bottom_bit(x);
        if (ax != bx)
        {
            printf("Test failed: bottom bit mismatch 0x%" PRIx32 " -> %u %u\n", x, ax, bx);
            exit(2);
        }
        x = rand();
    }
    for (i = 0;  i < 256;  i++)
    {
        ax = bit_reverse8_dumb(i);
        bx = bit_reverse8(i);
        if (ax != bx)
        {
            printf("Test failed: bit reverse 8 - %02x %02x %02x\n", i, ax, bx);
            exit(2);
        }
    }
    for (i = 0;  i < 1000000;  i++)
        from[i] = rand();
    bit_reverse(to, from, 1000000);
    for (i = 0;  i < 1000000;  i++)
    {
        if (bit_reverse8_dumb(from[i]) != to[i])
        {
            printf("Test failed: bit reverse - at %d, %02x %02x %02x\n", i, from[i], bit_reverse8(from[i]), to[i]);
            exit(2);
        }
    }
    for (i = 0;  i < 256;  i++)
    {
        x = i | (((i + 1) & 0xFF) << 8) | (((i + 2) & 0xFF) << 16) | (((i + 3) & 0xFF) << 24);
        ax32 = bit_reverse_4bytes_dumb(x);
        bx32 = bit_reverse_4bytes(x);
        if (ax32 != bx32)
        {
            printf("Test failed: bit reverse 4 bytes - %" PRIx32 " %" PRIx32 " %" PRIx32 "\n", x, ax32, bx32);
            exit(2);
        }
    }
    for (i = 0;  i < 65536;  i++)
    {
        ax16 = bit_reverse16_dumb(i);
        bx16 = bit_reverse16(i);
        if (ax16 != bx16)
        {
            printf("Test failed: bit reverse 16 - %x %x %x\n", i, ax16, bx16);
            exit(2);
        }
    }
    for (i = 0;  i < 0x7FFFFF00;  i += 127)
    {
        ax32 = bit_reverse32_dumb(i);
        bx32 = bit_reverse32(i);
        if (ax32 != bx32)
        {
            printf("Test failed: bit reverse 32 - %d %" PRIx32 " %" PRIx32 "\n", i, ax32, bx32);
            exit(2);
        }
    }

    for (i = 0;  i < 256;  i++)
    {
        ax = parity8(i);
        bx = parity8_dumb(i);
        if (ax != bx)
        {
            printf("Test failed: parity 8 - %x %x %x\n", i, ax, bx);
            exit(2);
        }
    }

    for (i = -1;  i < 32;  i++)
    {
        ax32 = most_significant_one32(1 << i);
        if (ax32 != (1 << i))
        {
            printf("Test failed: most significant one 32 - %x %" PRIx32 " %x\n", i, ax32, (1 << i));
            exit(2);
        }
        ax32 = least_significant_one32(1 << i);
        if (ax32 != (1 << i))
        {
            printf("Test failed: least significant one 32 - %x %" PRIx32 " %x\n", i, ax32, (1 << i));
            exit(2);
        }
    }

    for (i = 0x80000000;  i < 0x800FFFFF;  i++)
    {
        ax = one_bits32_dumb(i);
        bx = one_bits32(i);
        if (ax != bx)
        {
            printf("Test failed: one bits - %d, %x %x\n", i, ax, bx);
            exit(2);
        }
    }

    printf("Tests passed.\n");
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
