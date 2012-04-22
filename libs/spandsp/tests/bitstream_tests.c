/*
 * SpanDSP - a series of DSP components for telephony
 *
 * bitstream_tests.c
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2007 Steve Underwood
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
 */

/*! \page bitstream_tests_page Bitstream tests
\section bitstream_tests_page_sec_1 What does it do?

\section bitstream_tests_page_sec_2 How is it used?
*/

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>

//#if defined(WITH_SPANDSP_INTERNALS)
#define SPANDSP_EXPOSE_INTERNAL_STRUCTURES
//#endif

#include "spandsp.h"

uint8_t buffer[256];

#define PATTERN             0x11111111
#define SEQUENCE_LENGTH     17

uint8_t left[] =
{
    0x28,       /* 2 of 4, 3, 2, 1 */
    0xC8,       /* 1 of 6, 5, 2 of 4 */
    0xAE,       /* 3 of 7, 5 of 6 */
    0x67,       /* 4 of 8, 4 of 7 */
    0x74,       /* 4 of 9, 4 of 8 */
    0x43,       /* 3 of 10, 5 of 9 */
    0x32,       /* 1 of 11, 7 of 10 */
    0xAA,       /* 8 of 11 */
    0xAE,       /* 6 of 12, 2 of 11 */
    0xED,       /* 2 of 13, 6 of 12 */
    0x99,       /* 8 of 13 */
    0x8E,       /* 5 of 14, 3 of 13 */
    0xEE,       /* 8 of 14 */
    0xEE,       /* 7 of 15, 1 of 14 */
    0xEE,       /* 8 of 15 */
    0xFF,       /* 8 of 16 */
    0xFF,       /* 8 of 16 */
    0x88,       /* 8 of 17 */
    0x88,       /* 8 of 17 */
    0x00        /* 1 of 17 */
};
uint8_t right[] =
{
    0xD2,       /* 1, 2, 3, 2 of 4 */
    0x90,       /* 2 of 4, 5, 1 of 6 */
    0xCA,       /* 5 of 6, 3 of 7 */
    0x7C,       /* 4 of 7, 4 of 8 */
    0x87,       /* 4 of 8, 4 of 9 */
    0x28,       /* 5 of 9, 3 of 10 */
    0x33,       /* 7 of 10, 1 of 11 */
    0x55,       /* 8 of 11 */
    0xED,       /* 2 of 11, 6 of 12 */
    0x2E,       /* 6 of 12, 2 of 13 */
    0x33,       /* 8 of 13 */
    0xEB,       /* 3 of 13, 5 of 14 */
    0xEE,       /* 8 of 14 */
    0xDC,       /* 1 of 14, 7 of 15 */
    0xDD,       /* 8 of 15 */
    0xFF,       /* 8 of 16 */
    0xFF,       /* 8 of 16 */
    0x10,       /* 8 of 17 */
    0x11,       /* 8 of 17 */
    0x01        /* 1 of 17 */
};

int main(int argc, char *argv[])
{
    int i;
    bitstream_state_t state;
    bitstream_state_t *s;
    const uint8_t *r;
    uint8_t *w;
    uint8_t *cc;
    unsigned int x;
    int total_bits;

    s = bitstream_init(&state, TRUE);
    w = buffer;
    total_bits = 0;
    for (i = 0;  i < SEQUENCE_LENGTH;  i++)
    {
        bitstream_put(s, &w, PATTERN*i, i + 1);
        total_bits += (i + 1);
    }
    bitstream_flush(s, &w);
    printf("%d bits written\n", total_bits);

    for (cc = buffer;  cc < w;  cc++)
        printf("%02X ", *cc);
    printf("\n");
    for (cc = right;  cc < right + sizeof(right);  cc++)
        printf("%02X ", *cc);
    printf("\n");

    if ((w - buffer) != sizeof(right)  ||  memcmp(buffer, right, sizeof(right)))
    {
        printf("Test failed\n");
        exit(2);
    }

    s = bitstream_init(&state, TRUE);
    r = buffer;
    for (i = 0;  i < SEQUENCE_LENGTH;  i++)
    {
        x = bitstream_get(s, &r, i + 1);
        if (x != ((PATTERN*i) & ((1 << (i + 1)) - 1)))
        {
            printf("Error 0x%X 0x%X\n", x, ((PATTERN*i) & ((1 << (i + 1)) - 1)));
            printf("Test failed\n");
            exit(2);
        }
    }

    s = bitstream_init(&state, FALSE);
    w = buffer;
    total_bits = 0;
    for (i = 0;  i < SEQUENCE_LENGTH;  i++)
    {
        bitstream_put(s, &w, PATTERN*i, i + 1);
        total_bits += (i + 1);
    }
    bitstream_flush(s, &w);
    printf("%d bits written\n", total_bits);
    
    for (cc = buffer;  cc < w;  cc++)
        printf("%02X ", *cc);
    printf("\n");
    for (cc = left;  cc < left + sizeof(left);  cc++)
        printf("%02X ", *cc);
    printf("\n");

    if ((w - buffer) != sizeof(left)  ||  memcmp(buffer, left, sizeof(left)))
    {
        printf("Test failed\n");
        exit(2);
    }

    s = bitstream_init(&state, FALSE);
    r = buffer;
    for (i = 0;  i < SEQUENCE_LENGTH;  i++)
    {
        x = bitstream_get(s, &r, i + 1);
        if (x != ((PATTERN*i) & ((1 << (i + 1)) - 1)))
        {
            printf("Error 0x%X 0x%X\n", x, ((PATTERN*i) & ((1 << (i + 1)) - 1)));
            printf("Test failed\n");
            exit(2);
        }
    }

    printf("Tests passed.\n");
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
