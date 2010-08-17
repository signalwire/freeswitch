/*
 * SpanDSP - a series of DSP components for telephony
 *
 * saturated_tests.c
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2004 Steve Underwood
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

/*! \page saturated_tests_page Saturated arithmetic function tests
\section saturated_tests_page_sec_1 What does it do?
???.

\section saturated_tests_page_sec_2 How does it work?
???.
*/

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define SPANDSP_EXPOSE_INTERNAL_STRUCTURES
#include "spandsp.h"

int main(int argc, char *argv[])
{
    printf("Testing 16 bit saturation\n");
    if (saturate16(10000) != 10000
        ||
        saturate16(-10000) != -10000
        ||
        saturate16(32767) != 32767
        ||
        saturate16(-32768) != -32768
        ||
        saturate16(32768) != 32767
        ||
        saturate16(-32769) != -32768)
    {
        printf("Test failed.\n");
        exit(2);
    }
    printf("Testing 15 bit saturation\n");
    if (saturate15(10000) != 10000
        ||
        saturate15(-10000) != -10000
        ||
        saturate15(16383) != 16383
        ||
        saturate15(-16384) != -16384
        ||
        saturate15(16384) != 16383
        ||
        saturate15(-16385) != -16384)
    {
        printf("Test failed.\n");
        exit(2);
    }
    printf("Testing 16 bit unsigned saturation\n");
    if (saturateu16(10000) != 10000
        ||
        saturateu16(32767) != 32767
        ||
        saturateu16(65535) != 65535
        ||
        saturateu16(65536) != 65535)
    {
        printf("Test failed.\n");
        exit(2);
    }
    printf("Testing 8 bit unsigned saturation\n");
    if (saturateu8(100) != 100
        ||
        saturateu8(127) != 127
        ||
        saturateu8(255) != 255
        ||
        saturateu8(256) != 255)
    {
        printf("Test failed.\n");
        exit(2);
    }
    printf("Testing 16 bit saturation from float\n");
    if (fsaturatef(10000.0f) != 10000
        ||
        fsaturatef(-10000.0f) != -10000
        ||
        fsaturatef(32767.0f) != 32767
        ||
        fsaturatef(-32768.0f) != -32768
        ||
        fsaturatef(32768.0f) != 32767
        ||
        fsaturatef(-32769.0f) != -32768)
    {
        printf("Test failed.\n");
        exit(2);
    }
    printf("Testing 16 bit saturation from double\n");
    if (fsaturate(10000.0) != 10000
        ||
        fsaturate(-10000.0) != -10000
        ||
        fsaturate(32767.0) != 32767
        ||
        fsaturate(-32768.0) != -32768
        ||
        fsaturate(32768.0) != 32767
        ||
        fsaturate(-32769.0) != -32768)
    {
        printf("Test failed.\n");
        exit(2);
    }
    printf("Testing 16 bit fast saturation from float\n");
    if (ffastsaturatef(10000.0f) != 10000
        ||
        ffastsaturatef(-10000.0f) != -10000
        ||
        ffastsaturatef(32767.0f) != 32767
        ||
        ffastsaturatef(-32768.0f) != -32768
        ||
        ffastsaturatef(32768.0f) != 32767
        ||
        ffastsaturatef(-32769.0f) != -32768)
    {
        printf("Test failed.\n");
        exit(2);
    }
    printf("Testing 16 bit fast saturation from double\n");
    if (ffastsaturate(10000.0) != 10000
        ||
        ffastsaturate(-10000.0) != -10000
        ||
        ffastsaturate(32767.0) != 32767
        ||
        ffastsaturate(-32768.0) != -32768
        ||
        ffastsaturate(32768.0) != 32767
        ||
        ffastsaturate(-32769.0) != -32768)
    {
        printf("Test failed.\n");
        exit(2);
    }
    printf("Testing 16 bit float saturation from float\n");
    if (ffsaturatef(10000.0f) != 10000.0f
        ||
        ffsaturatef(-10000.0f) != -10000.0f
        ||
        ffsaturatef(32767.0f) != 32767.0f
        ||
        ffsaturatef(-32768.0f) != -32768.0f
        ||
        ffsaturatef(32768.0f) != 32767.0f
        ||
        ffsaturatef(-32769.0f) != -32768.0f)
    {
        printf("Test failed.\n");
        exit(2);
    }
    printf("Testing 16 bit double saturation from double\n");
    if (ffsaturate(10000.0) != 10000.0
        ||
        ffsaturate(-10000.0) != -10000.0
        ||
        ffsaturate(32767.0) != 32767.0
        ||
        ffsaturate(-32768.0) != -32768.0
        ||
        ffsaturate(32768.0) != 32767.0
        ||
        ffsaturate(-32769.0) != -32768.0)
    {
        printf("Test failed.\n");
        exit(2);
    }
    printf("Testing 16 bit add\n");
    if (saturated_add16(10000, 10000) != 20000
        ||
        saturated_add16(10000, -10000) != 0
        ||
        saturated_add16(-10000, 10000) != 0
        ||
        saturated_add16(-10000, -10000) != -20000
        ||
        saturated_add16(-30000, -30000) != INT16_MIN
        ||
        saturated_add16(30000, 30000) != INT16_MAX)
    {
        printf("Test failed.\n");
        exit(2);
    }
    printf("Testing 32 bit add\n");
    if (saturated_add32(10000, 10000) != 20000
        ||
        saturated_add32(10000, -10000) != 0
        ||
        saturated_add32(-10000, 10000) != 0
        ||
        saturated_add32(-10000, -10000) != -20000
        ||
        saturated_add32(-2000000000, -2000000000) != INT32_MIN
        ||
        saturated_add32(2000000000, 2000000000) != INT32_MAX)
    {
        printf("Test failed.\n");
        exit(2);
    }
    printf("Testing 16 bit subtract\n");
    if (saturated_sub16(10000, 10000) != 0
        ||
        saturated_sub16(10000, -10000) != 20000
        ||
        saturated_sub16(-10000, 10000) != -20000
        ||
        saturated_sub16(-10000, -10000) != 0
        ||
        saturated_sub16(-30000, 30000) != INT16_MIN
        ||
        saturated_sub16(30000, -30000) != INT16_MAX)
    {
        printf("Test failed.\n");
        exit(2);
    }
    printf("Testing 32 bit subtract\n");
    if (saturated_sub32(10000, 10000) != 0
        ||
        saturated_sub32(10000, -10000) != 20000
        ||
        saturated_sub32(-10000, 10000) != -20000
        ||
        saturated_sub32(-10000, -10000) != 0
        ||
        saturated_sub32(-2000000000, 2000000000) != INT32_MIN
        ||
        saturated_sub32(2000000000, -2000000000) != INT32_MAX)
    {
        printf("Test failed.\n");
        exit(2);
    }
    printf("Testing 16 x 16 => 16 bit multiply\n");
    if (saturated_mul16(100, 100) != 0
        ||
        saturated_mul16(255, 255) != 1
        ||
        saturated_mul16(32767, -32768) != -32767
        ||
        saturated_mul16(-32768, 32767) != -32767
        ||
        saturated_mul16(32767, 32767) != 32766
        ||
        saturated_mul16(-32768, -32768) != 32767)
    {
        printf("Test failed.\n");
        exit(2);
    }
    printf("Testing 16 x 16 => 32 bit multiply\n");
    if (saturated_mul16_32(100, 100) != 20000
        ||
        saturated_mul16_32(-100, 100) != -20000
        ||
        saturated_mul16_32(32767, -32768) != -2147418112
        ||
        saturated_mul16_32(-32768, 32767) != -2147418112
        ||
        saturated_mul16_32(32767, 32767) != 2147352578
        ||
        saturated_mul16_32(-32768, -32768) != -2147483648)
    {
        printf("Test failed.\n");
        exit(2);
    }
    printf("Testing 16 bit absolute\n");
    if (saturated_abs16(10000) != 10000
        ||
        saturated_abs16(-10000) != 10000
        ||
        saturated_abs16(32767) != 32767
        ||
        saturated_abs16(-32768) != 32767)
    {
        printf("Test failed.\n");
        exit(2);
    }
    printf("Tests passed.\n");
    return  0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
