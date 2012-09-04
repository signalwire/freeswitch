/*
 * SpanDSP - a series of DSP components for telephony
 *
 * t35_tests.c - Tests for T.35.
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

/*! \file */

/*! \page t35_tests_page T.35 tests
\section t35_tests_page_sec_1 What does it do?
*/

/* Enable the following definition to enable direct probing into the structures */
//#define WITH_SPANDSP_INTERNALS

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <sndfile.h>

//#if defined(WITH_SPANDSP_INTERNALS)
#define SPANDSP_EXPOSE_INTERNAL_STRUCTURES
//#endif

#include "spandsp.h"

int main(int argc, char *argv[])
{
    int i;
    int j;
    uint8_t msg[50];
    const char *vendor;
    const char *country;
    const char *model;
    const char *real_country;
    int first_hit;

    printf("Sweep through all the possible countries\n");
    for (i = 0;  i < 256;  i++)
    {
        country = t35_country_code_to_str(i, 0);
        real_country = t35_real_country_code_to_str(i, 0);
        if (country  ||  real_country)
        {
            printf("%3d '%s' %d '%s'\n",
                   i,
                   (country)  ?  country  :  "???",
                   t35_real_country_code(i, 0),
                   (real_country)  ?  real_country  :  "???");
        }
    }

    printf("\nSweep through all the possible vendors within each country\n");
    for (i = 0;  i < 256;  i++)
    {
        msg[0] = i;
        msg[1] = '\x00';
        msg[2] = '\x00';
        first_hit = TRUE;
        for (j = 0;  j < 65536;  j++)
        {
            msg[1] = (j >> 8) & 0xFF;
            msg[2] = j & 0xFF;
            if ((vendor = t35_vendor_to_str(msg, 3)))
            {
                if (first_hit)
                {
                    if ((real_country = t35_real_country_code_to_str(i, 0)))
                        printf("%s\n", real_country);
                    else
                        printf("???\n");
                    first_hit = FALSE;
                }
                printf("    0x%02x 0x%02x 0x%02x '%s'\n", msg[0], msg[1], msg[2], vendor);
            }
        }
    }

    printf("\nTry a decode of a full NSF string\n");
    t35_decode((uint8_t *) "\x00\x00\x0E\x00\x00\x00\x96\x0F\x01\x02\x00\x10\x05\x02\x95\xC8\x08\x01\x49\x02\x41\x53\x54\x47",
               13,
               &country,
               &vendor,
               &model);
    printf("Decoded as %s %s %s\n", (country)  ?  country  :  "???", (vendor)  ?  vendor  :  "???", (model)  ?  model  :  "???");

    printf("Tests passed\n");
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
