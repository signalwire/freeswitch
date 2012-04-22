/*
 * g722_1 - a library for the G.722.1 and Annex C codecs
 *
 * coef2sam.c
 *
 * Adapted by Steve Underwood <steveu@coppice.org> from the reference
 * code supplied with ITU G.722.1, which is:
 *
 *   (C) 2004 Polycom, Inc.
 *   All rights reserved.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include <config.h>
#endif

#include <inttypes.h>
#include <stdlib.h>

#include "g722_1/g722_1.h"

#include "defs.h"
#include "coef2sam.h"
#include "utilities.h"

/* Convert Reversed MLT (Modulated Lapped Transform) Coefficients to Samples
 
   The "Reversed MLT" is an overlapped block transform which uses even symmetry
   on the left, odd symmetry on the right and a Type IV DCT as the block transform.
   It is thus similar to a MLT which uses odd symmetry on the left, even symmetry
   on the right and a Type IV DST as the block transform.  In fact, it is equivalent
   to reversing the order of the samples, performing an MLT and then negating all
   the even-numbered coefficients. */

#if defined(G722_1_USE_FIXED_POINT)
void rmlt_coefs_to_samples(int16_t coefs[],
                           int16_t old_samples[],
                           int16_t out_samples[],
                           int dct_length,
                           int16_t mag_shift)
{
    int i;
    int half_dct_length;
    int last;
    int16_t new_samples[MAX_DCT_LENGTH];
    const int16_t *win;
    int32_t sum;

    half_dct_length = dct_length >> 1;

    /* Perform a Type IV (inverse) DCT on the coefficients */
    dct_type_iv_s(coefs, new_samples, dct_length);

    if (mag_shift > 0)
    {
        for (i = 0;  i < dct_length;  i++)
            new_samples[i] = shr(new_samples[i], mag_shift);
    }
    else if (mag_shift < 0)
    {
        mag_shift = negate(mag_shift);
        for (i = 0;  i < dct_length;  i++)
            new_samples[i] = shl(new_samples[i], mag_shift);
    }

    win = (dct_length == DCT_LENGTH)  ?  rmlt_to_samples_window  :  max_rmlt_to_samples_window;
    last = half_dct_length - 1;
    for (i = 0;  i < half_dct_length;  i++)
    {
        /* Get the first half of the windowed samples */
        sum = L_mult(win[i], new_samples[last - i]);
        sum = L_mac(sum, win[dct_length - i - 1], old_samples[i]);
        out_samples[i] = xround(L_shl(sum, 2));
        /* Get the second half of the windowed samples */
        sum = L_mult(win[half_dct_length + i], new_samples[i]);
        sum = L_mac(sum, negate(win[last - i]), old_samples[last - i]);
        out_samples[half_dct_length + i] = xround(L_shl(sum, 2));
    }

    /* Save the second half of the new samples for
       next time, when they will be the old samples. */
    vec_copyi16(old_samples, &new_samples[half_dct_length], half_dct_length);
}
/*- End of function --------------------------------------------------------*/
#else
void rmlt_coefs_to_samples(float coefs[],
                           float old_samples[],
                           float out_samples[],
                           int dct_length)
{
    int i;
    int half_dct_length;
    int last;
    float new_samples[MAX_DCT_LENGTH];
    const float *win;
    float sum;

    half_dct_length = dct_length >> 1;

    /* Perform a Type IV (inverse) DCT on the coefficients */
    dct_type_iv(coefs, new_samples, dct_length);

    win = (dct_length == DCT_LENGTH)  ?  rmlt_to_samples_window  :  max_rmlt_to_samples_window;
    last = half_dct_length - 1;
    for (i = 0;  i < half_dct_length;  i++)
    {
        /* Get the first half of the windowed samples */
        sum = win[i]*new_samples[last - i];
        sum += win[dct_length - i - 1]*old_samples[i];
        out_samples[i] = sum;
        /* Get the second half of the windowed samples */
        sum = win[half_dct_length + i]*new_samples[i];
        sum -= win[last - i]*old_samples[last - i];
        out_samples[half_dct_length + i] = sum;
    }

    /* Save the second half of the new samples for next time, when they will
       be the old samples. */
    vec_copyf(old_samples, &new_samples[half_dct_length], half_dct_length);
}
/*- End of function --------------------------------------------------------*/
#endif
/*- End of file ------------------------------------------------------------*/
