/*
 * SpanDSP - a series of DSP components for telephony
 *
 * t81_t82_arith_coding_tests.c - Tests for the ITU T.81 and T.82 arithmetic
 *                                encoder/decoder, based on the test description
 *                                in T.82
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2009 Steve Underwood
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

/*! \file */

/*! \page t81_t82_arith_coding_tests_page T.81 and T.82 Arithmetic encoder/decoder tests
\section t81_t82_arith_coding_tests_pagesec_1 What does it do
These tests exercise the arithmetic encoder and decoder for T.81 and T.82. As T.85 is based
on T.82, this is also the arithmetic coder for T.85.

These tests are based on T.82 section 7. Nothing beyond the prescibed tests is performed at
the present time.
*/

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#define SPANDSP_EXPOSE_INTERNAL_STRUCTURES

#include "spandsp.h"

#define MSG_SIZE 10000

uint8_t msg[MSG_SIZE];

int32_t msg_len;

static void write_byte(void *user_data, int byte)
{
    if (msg_len < MSG_SIZE)
        msg[msg_len++] = byte;
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    t81_t82_arith_encode_state_t *se;
    t81_t82_arith_decode_state_t *sd;
    int i;
    int j;
    int test_failed;
    int pix;
    const uint8_t *pp;
    /* Test data from T.82 7.1 */
    static const uint16_t pix_7_1[16] =
    {
        0x05E0, 0x0000, 0x8B00, 0x01C4, 0x1700, 0x0034, 0x7FFF, 0x1A3F,
        0x951B, 0x05D8, 0x1D17, 0xE770, 0x0000, 0x0000, 0x0656, 0x0E6A
    };
    /* Test data from T.82 7.1 */
    static const uint16_t cx_7_1[16] =
    {
        0x0FE0, 0x0000, 0x0F00, 0x00F0, 0xFF00, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
    };
    /* Test data from T.82 7.1 - scd with stuffing and SDNORM termination */
    static const uint8_t sde_7_1[32] =
    {
        0x69, 0x89, 0x99, 0x5C, 0x32, 0xEA, 0xFA, 0xA0,
        0xD5, 0xFF, 0x00, 0x52, 0x7F, 0xFF, 0x00, 0xFF,
        0x00, 0xFF, 0x00, 0xC0, 0x00, 0x00, 0x00, 0x3F,
        0xFF, 0x00, 0x2D, 0x20, 0x82, 0x91, 0xFF, 0x02
    };
#define SDE_7_1_LEN         30  /* Don't include the termination SDNORM */
#define SDE_7_1_FULL_LEN    32  /* Include the termination SDNORM */

    printf("T.81/T.82 arithmetic encoder tests, from ITU-T T.82\n\n");

    printf("Arithmetic encoder tests from ITU-T T.82/7.1\n");
    if ((se = t81_t82_arith_encode_init(NULL, write_byte, NULL)) == NULL)
    {
        fprintf(stderr, "Failed to allocate arithmetic encoder!\n");
        exit(2);
    }
    msg_len = 0;
    for (i = 0;  i < 16;  i++)
    {
        for (j = 0;  j < 16;  j++)
        {
            t81_t82_arith_encode(se,
                                 (cx_7_1[i] >> (15 - j)) & 1,
                                 (pix_7_1[i] >> (15 - j)) & 1);
        }
    }
    t81_t82_arith_encode_flush(se);
    if (msg_len != SDE_7_1_LEN  ||  memcmp(msg, sde_7_1, SDE_7_1_LEN))
    {
        printf("Encoded data:  ");
        for (i = 0;  i < msg_len;  i++)
            printf("%02X", msg[i]);
        printf("\n");
        printf("Expected data: ");
        for (i = 0;  i < SDE_7_1_LEN;  i++)
            printf("%02X", sde_7_1[i]);
        printf("\n");
        printf("Test failed\n");
        exit(2);
    }
    printf("Test passed\n");

    printf("Arithmetic decoder tests from ITU-T T.82/7.1\n");
    printf("Decoding byte by byte...\n");
    test_failed = false;
    if ((sd = t81_t82_arith_decode_init(NULL)) == NULL)
    {
        fprintf(stderr, "Failed to allocate arithmetic decoder!\n");
        exit(2);
    }
    pp = sde_7_1;
    sd->pscd_ptr = pp;
    sd->pscd_end = pp + 1;
    for (i = 0;  i < 16;  i++)
    {
        for (j = 0;  j < 16;  j++)
        {
            for (;;)
            {
                pix = t81_t82_arith_decode(sd, (cx_7_1[i] >> (15 - j)) & 1);
                if ((pix >= 0  ||  sd->pscd_end >= sde_7_1 + SDE_7_1_FULL_LEN))
                    break;
                pp++;
                if (sd->pscd_ptr != pp - 1)
                    sd->pscd_ptr = pp;
                sd->pscd_end = pp + 1;
            }
            if (pix < 0)
            {
                printf("Bad pixel %d, byte %" PRIdPTR ".\n\n",
                       i*16 + j + 1,
                       sd->pscd_ptr - sd->pscd_end);
                test_failed = true;
                break;
            }
            if (pix != ((pix_7_1[i] >> (15 - j)) & 1))
            {
                printf("Bad PIX (%d) at pixel %d.\n\n",
                       pix,
                       i*16 + j + 1);
                test_failed = true;
                break;
            }
        }
    }
    if (sd->pscd_ptr != sd->pscd_end - 2)
    {
        printf("%" PRIdPTR " bytes left after decoder finished.\n\n",
               sd->pscd_end - sd->pscd_ptr - 2);
        test_failed = true;
    }
    if (test_failed)
    {
        printf("Test failed\n");
        exit(2);
    }
    printf("Test passed\n");

    printf("Decoding chunk by chunk...\n");
    test_failed = false;
    t81_t82_arith_decode_init(sd);
    sd->pscd_ptr = sde_7_1;
    sd->pscd_end = sde_7_1 + SDE_7_1_FULL_LEN;
    for (i = 0;  i < 16;  i++)
    {
        for (j = 0;  j < 16;  j++)
        {
            pix = t81_t82_arith_decode(sd, (cx_7_1[i] >> (15 - j)) & 1);
            if (pix < 0)
            {
                printf("Bad pixel %d, byte %" PRIdPTR ".\n\n",
                       i*16 + j + 1,
                       sd->pscd_ptr - sd->pscd_end);
                test_failed = true;
                break;
            }
            if (pix != ((pix_7_1[i] >> (15 - j)) & 1))
            {
                printf("Bad PIX (%d) at pixel %d.\n\n",
                       pix,
                       i*16 + j + 1);
                test_failed = true;
                break;
            }
        }
    }
    if (sd->pscd_ptr != sd->pscd_end - 2)
    {
        printf("%" PRIdPTR " bytes left after decoder finished.\n\n",
               sd->pscd_end - sd->pscd_ptr - 2);
        test_failed = true;
    }
    if (test_failed)
    {
        printf("Test failed\n");
        exit(2);
    }
    printf("Test passed\n");

    printf("Tests passed\n");
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
