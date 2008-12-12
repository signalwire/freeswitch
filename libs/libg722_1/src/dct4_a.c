/*
 * g722_1 - a library for the G.722.1 and Annex C codecs
 *
 * dct4_a.c
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
 * $Id: dct4_a.c,v 1.8 2008/09/30 14:06:39 steveu Exp $
 */

/*********************************************************************************
* Filename: dct_type_iv_a.c
*
* Purpose:  Discrete Cosine Transform, Type IV used for MLT
*
* The basis functions are
*
*   cos(PI*(t+0.5)*(k+0.5)/block_length)
*
* for time t and basis function number k.  Due to the symmetry of the expression
* in t and k, it is clear that the forward and inverse transforms are the same.
*
*********************************************************************************/

/*! \file */

#if defined(HAVE_CONFIG_H)
#include <config.h>
#endif

#include <inttypes.h>
#include <stdlib.h>

#include "g722_1/g722_1.h"

#include "defs.h"

#if defined(G722_1_USE_FIXED_POINT)

#include "dct4_a.h"

/*********************************************************************************
 Function:    dct_type_iv_a

 Syntax:      void dct_type_iv_a (input, output, dct_length)
                        int16_t   input[], output[], dct_length;

 Description: Discrete Cosine Transform, Type IV used for MLT
*********************************************************************************/

void dct_type_iv_a(int16_t input[], int16_t output[], int dct_length)
{
    int16_t buffer_a[MAX_DCT_LENGTH];
    int16_t buffer_b[MAX_DCT_LENGTH];
    int16_t buffer_c[MAX_DCT_LENGTH];
    int16_t *in_ptr;
    int16_t *in_ptr_low;
    int16_t *in_ptr_high;
    int16_t *next_in_base;
    int16_t *out_ptr_low;
    int16_t *out_ptr_high;
    int16_t *next_out_base;
    int16_t *out_buffer;
    int16_t *in_buffer;
    int16_t *buffer_swap;
    int16_t in_val_low;
    int16_t in_val_high;
    int16_t out_val_low;
    int16_t out_val_high;
    int16_t in_low_even;
    int16_t in_low_odd;
    int16_t in_high_even;
    int16_t in_high_odd;
    int16_t out_low_even;
    int16_t out_low_odd;
    int16_t out_high_even;
    int16_t out_high_odd;
    int16_t *pair_ptr;
    int16_t cos_even;
    int16_t cos_odd;
    int16_t msin_even;
    int16_t msin_odd;
    int16_t neg_cos_odd;
    int16_t neg_msin_even;
    int32_t sum;
    int16_t set_span;
    int16_t set_count;
    int16_t set_count_log;
    int16_t pairs_left;
    int16_t sets_left;
    int16_t i;
    int16_t k;
    int16_t index;
    const cos_msin_t **table_ptr_ptr;
    const cos_msin_t *cos_msin_ptr;
    int16_t temp;
    int32_t acca;
    int16_t dct_length_log;

    /* Do the sum/difference butterflies, the first part of */
    /* converting one N-point transform into N/2 two-point  */
    /* transforms, where N = 1 << DCT_LENGTH_LOG. = 64/128  */
    if (dct_length == DCT_LENGTH)
    {
        dct_length_log = DCT_LENGTH_LOG;

        /* Add bias offsets */
        for (i = 0;  i < dct_length; i++)
            input[i] = add(input[i], anal_bias[i]);
    }
    else
    {
        dct_length_log = MAX_DCT_LENGTH_LOG;
    }
    index = 0L;
    in_buffer = input;
    out_buffer = buffer_a;
    temp = sub(dct_length_log, 2);
    for (set_count_log = 0;  set_count_log <= temp;  set_count_log++)
    {
        /* Initialization for the loop over sets at the current size */
        /* set_span = 1 << (DCT_LENGTH_LOG - set_count_log); */
        set_span = shr(dct_length, set_count_log);

        set_count = shl(1, set_count_log);
        in_ptr = in_buffer;
        next_out_base = out_buffer;

        /* Loop over all the sets of this size */
        for (sets_left = set_count;  sets_left > 0;  sets_left--)
        {
            /* Set up output pointers for the current set */
            out_ptr_low = next_out_base;
            next_out_base = next_out_base + set_span;
            out_ptr_high = next_out_base;

            /* Loop over all the butterflies in the current set */
            do
            {
                in_val_low      = *in_ptr++;
                in_val_high     = *in_ptr++;
                acca            = L_add(in_val_low, in_val_high);
                acca            = L_shr(acca, 1);
                out_val_low     = (int16_t) acca;

                acca            = L_sub(in_val_low, in_val_high);
                acca            = L_shr(acca, 1);
                out_val_high    = (int16_t) acca;

                *out_ptr_low++  = out_val_low;
                *--out_ptr_high = out_val_high;
            }
            while (out_ptr_low < out_ptr_high);
        }

        /* Decide which buffers to use as input and output next time. */
        /* Except for the first time (when the input buffer is the    */
        /* subroutine input) we just alternate the local buffers.     */
        in_buffer = out_buffer;
        if (out_buffer == buffer_a)
            out_buffer = buffer_b;
        else
            out_buffer = buffer_a;
        index = add(index, 1);
    }

    /* Do N/2 two-point transforms,   */
    /* where N =  1 << DCT_LENGTH_LOG */
    pair_ptr = in_buffer;
    buffer_swap = buffer_c;
    temp = sub(dct_length_log, 1);
    temp = shl(1, temp);

    for (pairs_left = temp;  pairs_left > 0;  pairs_left--)
    {
        for (k = 0;  k < CORE_SIZE;  k++)
        {
            sum = 0L;
            for (i = 0;  i < CORE_SIZE;  i++)
                sum = L_mac(sum, pair_ptr[i], dct_core_a[i][k]);
            buffer_swap[k] = xround(sum);
        }
        /* Address arithmetic */
        pair_ptr += CORE_SIZE;
        buffer_swap += CORE_SIZE;
    }

    for (i = 0;  i < dct_length;  i++)
        in_buffer[i] = buffer_c[i];

    table_ptr_ptr = a_cos_msin_table;

    /* Perform rotation butterflies */
    temp = sub(dct_length_log, 2);
    for (set_count_log = temp;  set_count_log >= 0;  set_count_log--)
    {
        /* Initialization for the loop over sets at the current size */
        /* set_span = 1 << (DCT_LENGTH_LOG - set_count_log); */
        set_span = shr(dct_length, set_count_log);
        set_count = shl(1, set_count_log);
        next_in_base = in_buffer;
        next_out_base = (set_count_log == 0)  ?  output  :  out_buffer;

        /* Loop over all the sets of this size */
        for (sets_left = set_count;  sets_left > 0;  sets_left--)
        {
            /* Set up the pointers for the current set */
            in_ptr_low = next_in_base;
            temp = shr(set_span, 1);

            /* Address arithmetic */
            in_ptr_high = in_ptr_low + temp;
            next_in_base += set_span;
            out_ptr_low = next_out_base;
            next_out_base += set_span;
            out_ptr_high = next_out_base;
            cos_msin_ptr = *table_ptr_ptr;

            /* Loop over all the butterfly pairs in the current set */
            do
            {
                /* Address arithmetic */
                in_low_even = *in_ptr_low++;
                in_low_odd = *in_ptr_low++;
                in_high_even = *in_ptr_high++;
                in_high_odd = *in_ptr_high++;
                cos_even = cos_msin_ptr[0].cosine;
                msin_even = cos_msin_ptr[0].minus_sine;
                cos_odd = cos_msin_ptr[1].cosine;
                msin_odd = cos_msin_ptr[1].minus_sine;
                cos_msin_ptr += 2;

                sum = 0L;
                sum = L_mac(sum, cos_even, in_low_even);
                neg_msin_even = negate(msin_even);
                sum = L_mac(sum, neg_msin_even, in_high_even);
                out_low_even = xround(sum);

                sum = 0L;
                sum = L_mac(sum, msin_even,in_low_even);
                sum = L_mac(sum, cos_even, in_high_even);
                out_high_even = xround(sum);

                sum = 0L;
                sum = L_mac(sum, cos_odd, in_low_odd);
                sum = L_mac(sum, msin_odd, in_high_odd);
                out_low_odd = xround(sum);

                sum = 0L;
                sum = L_mac(sum, msin_odd, in_low_odd);
                neg_cos_odd = negate(cos_odd);
                sum = L_mac(sum, neg_cos_odd, in_high_odd);
                out_high_odd = xround(sum);

                *out_ptr_low++ = out_low_even;
                *--out_ptr_high = out_high_even;
                *out_ptr_low++ = out_low_odd;
                *--out_ptr_high = out_high_odd;
            }
            while (out_ptr_low < out_ptr_high);
        }

        /* Swap input and output buffers for next time */
        buffer_swap = in_buffer;
        in_buffer = out_buffer;
        out_buffer = buffer_swap;
        table_ptr_ptr++;
    }
}
/*- End of function --------------------------------------------------------*/
#endif
/*- End of file ------------------------------------------------------------*/
