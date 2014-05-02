/*
 * SpanDSP - a series of DSP components for telephony
 *
 * make_t43_gray_code_tables.c - Generate the Gray code tables for T.43 image
 *                               compression.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2012 Steve Underwood
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

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <memory.h>
#include <math.h>

int main(int argc, char *argv[])
{
    int i;
    int j;
    int gray;
    int new_gray;
    int restore;

    printf("/* THIS FILE WAS AUTOMATICALLY GENERATED - ANY MODIFICATIONS MADE TO THIS");
    printf("   FILE MAY BE OVERWRITTEN DURING FUTURE BUILDS OF THE SOFTWARE */\n");
    printf("\n");

    printf("static const int16_t gray_code[4096] =\n{\n");
    for (i = 0;  i < 4096;  i++)
    {
        gray = i & 0x800;
        restore = i;
        for (j = 10;  j >= 0;  j--)
        {
            if (((i >> (j + 1)) & 1) ^ ((i >> j) & 1))
                gray |= (1 << j);
        }
        printf("    0x%04x, /* 0x%04x */\n", gray, restore);

        /* Now reverse the process and check we get back where we start */
        restore = gray & 0x800;
        for (j = 10;  j >= 0;  j--)
        {
            if (((restore >> (j + 1)) & 1) ^ ((gray >> j) & 1))
                restore |= (1 << j);
        }

        if (i != restore)
        {
            printf("Ah\n");
            exit(2);
        }
    }
    printf("};\n\n");

    printf("static const int16_t anti_gray_code[4096] =\n{\n");
    for (i = 0;  i < 4096;  i++)
    {
        gray = i;
        restore = gray & 0x800;
        for (j = 10;  j >= 0;  j--)
        {
            if (((restore >> (j + 1)) & 1) ^ ((gray >> j) & 1))
                restore |= (1 << j);
        }
        printf("    0x%04x, /* 0x%04x */\n", restore, gray);

        /* Now reverse the process and check we get back where we start */
        new_gray = restore & 0x800;
        for (j = 10;  j >= 0;  j--)
        {
            if (((restore >> (j + 1)) & 1) ^ ((restore >> j) & 1))
                new_gray |= (1 << j);
        }

        if (gray != new_gray)
        {
            printf("Ah\n");
            exit(2);
        }
    }
    printf("};\n");

    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
