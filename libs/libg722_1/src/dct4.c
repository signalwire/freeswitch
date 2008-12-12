/*
 * g722_1 - a library for the G.722.1 and Annex C codecs
 *
 * dct4.c
 *
 * Adapted by Steve Underwood <steveu@coppice.org> from the reference
 * code supplied with ITU G.722.1, which is:
 *
 *   © 2004 Polycom, Inc.
 *   All rights reserved.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * $Id: dct4.c,v 1.8 2008/09/29 16:09:26 steveu Exp $
 */

#if defined(HAVE_CONFIG_H)
#include <config.h>
#endif

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <memory.h>

#include "g722_1/g722_1.h"

#include "defs.h"

#if !defined(G722_1_USE_FIXED_POINT)

typedef struct
{
    float cosine;
    float minus_sine;
} cos_msin_t;

#include "dct4.h"

static const cos_msin_t *cos_msin_table[] =
{
    cos_msin_5,
    cos_msin_10,
    cos_msin_20,
    cos_msin_40,
    cos_msin_80,
    cos_msin_160,
    cos_msin_320,
    cos_msin_640
};

/*********************************************************************************
 Description: Discrete Cosine Transform, Type IV
*********************************************************************************/
void dct_type_iv(float input[], float output[], int dct_length)
{
    float buffer_a[MAX_DCT_LENGTH];
    float buffer_b[MAX_DCT_LENGTH];
    float buffer_c[MAX_DCT_LENGTH];
    float *in_ptr;
    float *in_ptr_low;
    float *in_ptr_high;
    float *next_in_base;
    float *out_ptr_low;
    float *out_ptr_high;
    float *next_out_base;
    float *out_buffer;
    float *in_buffer;
    float *buffer_swap;
    float *fptr0;
    const float *fptr2;
    const float *core_a;
    float in_val_low;
    float in_val_high;
    float cos_even;
    float cos_odd;
    float msin_even;
    float msin_odd;
    float sum;
    const cos_msin_t **table_ptr_ptr;
    const cos_msin_t *cos_msin_ptr;
    int set_span;
    int set_count;
    int set_count_log;
    int pairs_left;
    int sets_left;
    int i;
    int k;
    int dct_length_log;

    if (dct_length == MAX_DCT_LENGTH)
    {
        core_a = max_dct_core_a;
        dct_length_log = MAX_DCT_LENGTH_LOG;
    }
    else
    {
        core_a = dct_core_a;
        dct_length_log = DCT_LENGTH_LOG;
    }

    /* Do the sum/difference butterflies, the first part of
       converting one N-point transform into N/2 two-point
       transforms, where N = 1 << dct_length_log. */
    in_buffer = input;
    out_buffer = buffer_a;
    for (set_count_log = 0;  set_count_log <= dct_length_log - 2;  set_count_log++)
    {
        /* Initialization for the loop over sets at the current size */
        set_span = dct_length >> set_count_log;

        set_count = 1 << set_count_log;
        in_ptr = in_buffer;
        next_out_base = out_buffer;

        /* Loop over all the sets of this size */
        for (sets_left = set_count;  sets_left > 0;  sets_left--)
        {
            /* Set up output pointers for the current set */
            out_ptr_low = next_out_base;
            next_out_base += set_span;
            out_ptr_high = next_out_base;

            /* Loop over all the butterflies in the current set */
            do
            {
                in_val_low = *in_ptr++;
                in_val_high = *in_ptr++;
                *out_ptr_low++ = in_val_low + in_val_high;
                *--out_ptr_high = in_val_low - in_val_high;
            }
            while (out_ptr_low < out_ptr_high);
        }

        /* Decide which buffers to use as input and output next time.
           Except for the first time (when the input buffer is the
           subroutine input) we just alternate the local buffers. */
        in_buffer = out_buffer;
        if (out_buffer == buffer_a)
            out_buffer = buffer_b;
        else
            out_buffer = buffer_a;
    }

    /* Do dct_size/10 ten-point transforms */
    fptr0 = in_buffer;
    buffer_swap = buffer_c;
    for (pairs_left = 1 << (dct_length_log - 1);  pairs_left > 0;  pairs_left--) 
    {
        fptr2 = core_a;
        for (k = 0;  k < CORE_SIZE;  k++)
        {
            sum = 0;
            for (i = 0;  i < CORE_SIZE;  i++)
                sum += fptr0[i]*fptr2[i];
            fptr2 += CORE_SIZE;
            buffer_swap[k] = sum;
        }
        fptr0 += CORE_SIZE;
        buffer_swap += CORE_SIZE;
    }

    memcpy(in_buffer, buffer_c, dct_length*sizeof(float));

    table_ptr_ptr = cos_msin_table;

    /* Perform rotation butterflies */
    for (set_count_log = dct_length_log - 2;  set_count_log >= 0;  set_count_log--)
    {
        /* Initialization for the loop over sets at the current size */
        set_span = dct_length >> set_count_log;

        set_count = 1 << set_count_log;
        next_in_base = in_buffer;
        if (set_count_log == 0)
            next_out_base = output;
        else
            next_out_base = out_buffer;
        ++table_ptr_ptr;

        /* Loop over all the sets of this size */
        for (sets_left = set_count;  sets_left > 0;  sets_left--)
        {
            /* Set up the pointers for the current set */
            in_ptr_low = next_in_base;
            in_ptr_high = in_ptr_low + (set_span >> 1);
            next_in_base += set_span;
            out_ptr_low = next_out_base;
            next_out_base += set_span;
            out_ptr_high = next_out_base;
            cos_msin_ptr = *table_ptr_ptr;

            /* Loop over all the butterfly pairs in the current set */
            do
            {
                cos_even = (*cos_msin_ptr).cosine;
                msin_even = (*cos_msin_ptr++).minus_sine;
                *out_ptr_low++ = cos_even * *in_ptr_low - msin_even * *in_ptr_high;
                *--out_ptr_high = msin_even * *in_ptr_low++ +  cos_even * *in_ptr_high++;

                cos_odd = (*cos_msin_ptr).cosine;
                msin_odd = (*cos_msin_ptr++).minus_sine;
                *out_ptr_low++ = cos_odd  * *in_ptr_low  + msin_odd  * *in_ptr_high;
                *--out_ptr_high = msin_odd  * *in_ptr_low++  -  cos_odd  * *in_ptr_high++;
            }
            while (out_ptr_low < out_ptr_high);
        }

        /* Swap input and output buffers for next time */
        buffer_swap = in_buffer;
        in_buffer = out_buffer;
        out_buffer = buffer_swap;
    }
}
/*- End of function --------------------------------------------------------*/
#endif
/*- End of file ------------------------------------------------------------*/
