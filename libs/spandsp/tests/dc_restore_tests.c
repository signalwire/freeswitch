/*
 * SpanDSP - a series of DSP components for telephony
 *
 * dc_restore_tests.c - Tests for the dc_restore functions.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2001 Steve Underwood
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

/*! \page dc_restore_tests_page DC restoration tests
\section dc_restore_tests_page_sec_1 What does it do?
*/

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <time.h>

//#if defined(WITH_SPANDSP_INTERNALS)
#define SPANDSP_EXPOSE_INTERNAL_STRUCTURES
//#endif

#include "spandsp.h"
    
int main (int argc, char *argv[])
{
    awgn_state_t noise_source;
    dc_restore_state_t dc_state;
    int i;
    int idum = 1234567;
    int16_t dirty;
    int estimate;
    int min;
    int max;
    int dc_offset;

    dc_offset = 5000;
    awgn_init_dbm0(&noise_source, idum, -10.0);
    dc_restore_init(&dc_state);
    for (i = 0;  i < 100000;  i++)
    {
        dirty = awgn(&noise_source) + dc_offset;
        dc_restore(&dc_state, dirty);
        if ((i % 1000) == 0)
        {
            printf("Sample %6d: %d (expect %d)\n",
                   i,
                   dc_restore_estimate(&dc_state),
                   dc_offset);
        }
    }
    /* We should have settled by now. Look at the variation we get */
    min = 99999;
    max = -99999;
    for (i = 0;  i < 100000;  i++)
    {
        dirty = awgn(&noise_source) + dc_offset;
        dc_restore(&dc_state, dirty);
        estimate = dc_restore_estimate(&dc_state);
        if (estimate < min)
            min = estimate;
        if (estimate > max)
            max = estimate;
    }
    printf("Spread of DC estimate for an offset of %d was %d to %d\n", dc_offset, min, max);
    if (min < dc_offset - 50  ||  max > dc_offset + 50)
    {
        printf("Test failed.\n");
        exit(2);
    }
    printf("Test passed.\n");
    return  0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
