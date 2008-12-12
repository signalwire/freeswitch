/*
 * g722_1 - a library for the G.722.1 and Annex C codecs
 *
 * sam2coef.c
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
 * $Id: sam2coef.c,v 1.12 2008/10/02 11:43:54 steveu Exp $
 */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include <config.h>
#endif

#include <inttypes.h>
#include <stdlib.h>

#include "g722_1/g722_1.h"

#include "defs.h"
#include "sam2coef.h"

/************************************************************************************
  Purpose:  Convert Samples to Reversed MLT (Modulated Lapped Transform) Coefficients
 
  The "Reversed MLT" is an overlapped block transform which uses even symmetry
  on the left, odd symmetry on the right and a Type IV DCT as the block transform.
  It is thus similar to a MLT which uses odd symmetry on the left, even symmetry
  on the right and a Type IV DST as the block transform.  In fact, it is equivalent
  to reversing the order of the samples, performing an MLT and then negating all
  the even-numbered coefficients.
***************************************************************************/

#if defined(G722_1_USE_FIXED_POINT)
int16_t samples_to_rmlt_coefs(const int16_t new_samples[],
                              int16_t old_samples[],
                              int16_t coefs[],
                              int dct_length)
{
    int i;
    int half_dct_length;
    int last;
    int16_t mag_shift;
    int16_t n;
    int16_t windowed_data[MAX_DCT_LENGTH];
    const int16_t *win;
    int32_t acca;
    int32_t accb;
    int16_t temp;
    int16_t temp1;
    int16_t temp2;

    half_dct_length = dct_length >> 1;

    if (dct_length == DCT_LENGTH)
        win = samples_to_rmlt_window;
    else
        win = max_samples_to_rmlt_window;
    /* Get the first half of the windowed samples */
    last = half_dct_length - 1;
    for (i = 0;  i < half_dct_length;  i++)
    {
        acca = 0L;
        acca = L_mac(acca, win[last - i], old_samples[last - i]);
        acca = L_mac(acca, win[half_dct_length + i], old_samples[half_dct_length + i]);
        temp = xround(acca);
        windowed_data[i] = temp;
    }
    /* Get the second half of the windowed samples */
    last = dct_length - 1;
    for (i = 0;  i < half_dct_length;  i++)
    {
        acca = 0L;
        acca = L_mac(acca, win[last - i], new_samples[i]);
        acca = L_mac(acca, negate(win[i]), new_samples[last - i]);
        temp = xround(acca);
        windowed_data[half_dct_length + i] = temp;
    }

    /* Save the new samples for next time, when they will be the old samples. */
    for (i = 0;  i < dct_length;  i++)
        old_samples[i] = new_samples[i];

    /* Calculate how many bits to shift up the input to the DCT. */
    temp1 = 0;
    for (i = 0;  i < dct_length;  i++)
    {
        temp2 = abs_s(windowed_data[i]);
        temp = sub(temp2, temp1);
        if (temp > 0)
            temp1 = temp2;
    }

    mag_shift = 0;
    temp = sub(temp1, 14000);
    if (temp < 0)
    {
        temp = sub(temp1, 438);
        temp = (temp < 0)  ?  add(temp1, 1)  :  temp1;
        accb = L_mult(temp, 9587);
        acca = L_shr(accb, 20);
        temp = norm_s((int16_t) acca);
        mag_shift = (temp == 0)  ?  9  :  sub(temp, 6);
    }

    acca = 0;
    for (i = 0;  i < dct_length;  i++)
    {
        temp = abs_s(windowed_data[i]);
        acca = L_add(acca, temp);
    }

    acca = L_shr(acca, 7);
    if (temp1 < acca)
        mag_shift = sub(mag_shift, 1);
    if (mag_shift > 0)
    {
        for (i = 0;  i < dct_length;  i++)
            windowed_data[i] = shl(windowed_data[i], mag_shift);
    }
    else if (mag_shift < 0)
    {
        n = negate(mag_shift);
        for (i = 0;  i < dct_length;  i++)
            windowed_data[i] = shr(windowed_data[i], n);
    }

    /* Perform a Type IV DCT on the windowed data to get the coefficients */
    dct_type_iv_a(windowed_data, coefs, dct_length);

    return mag_shift;
}
/*- End of function --------------------------------------------------------*/
#else
void samples_to_rmlt_coefs(const float new_samples[],
                           float old_samples[],
                           float coefs[],
                           int dct_length)
{
    int i;
    int half_dct_length;
    int last;
    float sum;
    float windowed_data[MAX_DCT_LENGTH];
    const float *win;

    half_dct_length = dct_length >> 1;
   
    if (dct_length == DCT_LENGTH)
        win = samples_to_rmlt_window;
    else
        win = max_samples_to_rmlt_window;
    /* Get the first half of the windowed samples. */
    last = half_dct_length - 1;
    for (i = 0;  i < half_dct_length;  i++)
    {
        sum = win[last - i]*old_samples[last - i];
        sum += win[half_dct_length + i]*old_samples[half_dct_length + i];
        windowed_data[i] = sum;
    }
    /* Get the second half of the windowed samples. */
    last = dct_length - 1;
    for (i = 0;  i < half_dct_length;  i++)
    {
        sum = win[last - i]*new_samples[i];
        sum -= win[i]*new_samples[last - i];
        windowed_data[half_dct_length + i] = sum;
    }
    /* Save the new samples for next time, when they will be the old samples. */
    for (i = 0;  i < dct_length;  i++)
        old_samples[i] = new_samples[i];

    /* Perform a Type IV DCT on the windowed data to get the coefficients. */
    dct_type_iv(windowed_data, coefs, dct_length);
}
/*- End of function --------------------------------------------------------*/
#endif
/*- End of file ------------------------------------------------------------*/
