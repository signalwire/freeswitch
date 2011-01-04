/*
 * g722_1 - a library for the G.722.1 and Annex C codecs
 *
 * decoder.c
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
#include <string.h>
#include <math.h>

#include "g722_1/g722_1.h"

#include "defs.h"
#include "huff_tab.h"
#include "tables.h"
#include "bitstream.h"
#include "utilities.h"

#if !defined(G722_1_USE_FIXED_POINT)

static void test_for_frame_errors(g722_1_decode_state_t *s,
                                  int16_t number_of_regions,
                                  int16_t num_categorization_control_possibilities,
                                  int *frame_error_flag,
                                  int16_t categorization_control,
                                  int *absolute_region_power_index);

static void error_handling(int number_of_coefs,
                           int number_of_valid_coefs,
                           int *frame_error_flag,
                           float *decoder_mlt_coefs,
                           float *old_decoder_mlt_coefs);

static void decode_vector_quantized_mlt_indices(g722_1_decode_state_t *s,
                                                int number_of_regions,
                                                float decoder_region_standard_deviation[MAX_NUMBER_OF_REGIONS],
                                                int decoder_power_categories[MAX_NUMBER_OF_REGIONS],
                                                float decoder_mlt_coefs[MAX_DCT_LENGTH],
                                                int rmlt_scale_factor);

static void decode_envelope(g722_1_decode_state_t *s,
                            int number_of_regions,
                            float decoder_region_standard_deviation[MAX_NUMBER_OF_REGIONS],
                            int absolute_region_power_index[MAX_NUMBER_OF_REGIONS]);

static void rate_adjust_categories(int rate_control,
                                   int decoder_power_categories[MAX_NUMBER_OF_REGIONS],
                                   int decoder_category_balances[MAX_NUM_CATEGORIZATION_CONTROL_POSSIBILITIES - 1]);

static int index_to_array(int index,
                          int array[MAX_VECTOR_DIMENSION],
                          int category);

static void decoder(g722_1_decode_state_t *s,
                    float decoder_mlt_coefs[MAX_DCT_LENGTH],
                    float old_decoder_mlt_coefs[MAX_DCT_LENGTH],
                    int frame_error_flag);

/* Decode the bitstream into MLT coefs using G.722.1 Annex C */
static void decoder(g722_1_decode_state_t *s,
                    float decoder_mlt_coefs[MAX_DCT_LENGTH],
                    float old_decoder_mlt_coefs[MAX_DCT_LENGTH],
                    int frame_error_flag)
{
    float decoder_region_standard_deviation[MAX_NUMBER_OF_REGIONS];
    int absolute_region_power_index[MAX_NUMBER_OF_REGIONS];
    int decoder_power_categories[MAX_NUMBER_OF_REGIONS];
    int decoder_category_balances[MAX_NUM_CATEGORIZATION_CONTROL_POSSIBILITIES - 1];
    int num_categorization_control_bits;
    int num_categorization_control_possibilities;
    int number_of_coefs;
    int number_of_valid_coefs;
    int rmlt_scale_factor;
    int rate_control;

    number_of_valid_coefs = s->number_of_regions*REGION_SIZE;

    /* Get some parameters based solely on the bitstream style */
    if (s->number_of_regions == NUMBER_OF_REGIONS)
    {
        number_of_coefs = FRAME_SIZE;
        num_categorization_control_bits = NUM_CATEGORIZATION_CONTROL_BITS;
        num_categorization_control_possibilities = NUM_CATEGORIZATION_CONTROL_POSSIBILITIES;
        rmlt_scale_factor = (int) INTEROP_RMLT_SCALE_FACTOR_7;
    }
    else
    {
        number_of_coefs = MAX_FRAME_SIZE;
        num_categorization_control_bits = MAX_NUM_CATEGORIZATION_CONTROL_BITS;
        num_categorization_control_possibilities = MAX_NUM_CATEGORIZATION_CONTROL_POSSIBILITIES;
        rmlt_scale_factor = (int) INTEROP_RMLT_SCALE_FACTOR_14;
    }

    if (frame_error_flag == 0)
    {
        decode_envelope(s,
                        s->number_of_regions,
                        decoder_region_standard_deviation,
                        absolute_region_power_index);

        rate_control = g722_1_bitstream_get(&s->bitstream, &(s->code_ptr), num_categorization_control_bits);
        s->number_of_bits_left -= num_categorization_control_bits;

        categorize(s->number_of_regions,
                   s->number_of_bits_left,
                   absolute_region_power_index,
                   decoder_power_categories,
                   decoder_category_balances);

        rate_adjust_categories(rate_control,
                               decoder_power_categories,
                               decoder_category_balances);

        decode_vector_quantized_mlt_indices(s,
                                            s->number_of_regions,
                                            decoder_region_standard_deviation,
                                            decoder_power_categories,
                                            decoder_mlt_coefs,
                                            rmlt_scale_factor);

        test_for_frame_errors(s,
                              s->number_of_regions,
                              num_categorization_control_possibilities,
                              &frame_error_flag,
                              rate_control,
                              absolute_region_power_index);
    }
    error_handling(number_of_coefs,
                   number_of_valid_coefs,
                   &frame_error_flag,
                   decoder_mlt_coefs,
                   old_decoder_mlt_coefs);
}
/*- End of function --------------------------------------------------------*/

/* Recover differential_region_power_index from code bits */
static void decode_envelope(g722_1_decode_state_t *s,
                            int number_of_regions,
                            float decoder_region_standard_deviation[MAX_NUMBER_OF_REGIONS],
                            int absolute_region_power_index[MAX_NUMBER_OF_REGIONS])
{
    int region;
    int i;
    int index;
    int differential_region_power_index[MAX_NUMBER_OF_REGIONS];

    /* Recover differential_region_power_index[] from code_bits[]. */
    index = g722_1_bitstream_get(&s->bitstream, &(s->code_ptr), 5);
    s->number_of_bits_left -= 5;
    
    /* ESF_ADJUSTMENT_TO_RMS_INDEX compensates for the current (9/30/96)
       IMLT being scaled too high by the ninth power of sqrt(2). */
    differential_region_power_index[0] = index - ESF_ADJUSTMENT_TO_RMS_INDEX;

    for (region = 1;  region < number_of_regions;  region++)
    {
        index = 0;
        do
        {
            if (g722_1_bitstream_get(&s->bitstream, &(s->code_ptr), 1) == 0)
                index = differential_region_power_decoder_tree[region][index][0];
            else
                index = differential_region_power_decoder_tree[region][index][1];
            s->number_of_bits_left--;
        }
        while (index > 0);
        differential_region_power_index[region] = -index;
    }

    /* Reconstruct absolute_region_power_index[] from differential_region_power_index[]. */
    absolute_region_power_index[0] = differential_region_power_index[0];
  
    for (region = 1;  region < number_of_regions;  region++)
    {
        absolute_region_power_index[region] = absolute_region_power_index[region - 1]
                                            + differential_region_power_index[region]
                                            + DRP_DIFF_MIN;
    }

    /* Reconstruct decoder_region_standard_deviation[] from absolute_region_power_index[]. */
    for (region = 0;  region < number_of_regions;  region++)
    {
        i = absolute_region_power_index[region] + REGION_POWER_TABLE_NUM_NEGATIVES;
        decoder_region_standard_deviation[region] = region_standard_deviation_table[i];
    }
}
/*- End of function --------------------------------------------------------*/

/* Adjust the power categories based on the categorization control */
static void rate_adjust_categories(int rate_control,
                                   int decoder_power_categories[MAX_NUMBER_OF_REGIONS],
                                   int decoder_category_balances[MAX_NUM_CATEGORIZATION_CONTROL_POSSIBILITIES - 1])
{
    int i;
    int region;

    i = 0;
    while (rate_control > 0)
    {
        region = decoder_category_balances[i++];
        decoder_power_categories[region]++;
        rate_control--;
    }
}
/*- End of function --------------------------------------------------------*/

/* Decode MLT coefficients */
static void decode_vector_quantized_mlt_indices(g722_1_decode_state_t *s,
                                                int number_of_regions,
                                                float decoder_region_standard_deviation[MAX_NUMBER_OF_REGIONS],
                                                int decoder_power_categories[MAX_NUMBER_OF_REGIONS],
                                                float decoder_mlt_coefs[MAX_DCT_LENGTH],
                                                int rmlt_scale_factor)
{
    static const float noise_fill_factor_cat5[20] =
    {
        0.70711f, 0.6179f,  0.5005f,  0.3220f,
        0.17678f, 0.17678f, 0.17678f, 0.17678f,
        0.17678f, 0.17678f, 0.17678f, 0.17678f,
        0.17678f, 0.17678f, 0.17678f, 0.17678f,
        0.17678f, 0.17678f, 0.17678f, 0.17678f
    };
    static const float noise_fill_factor_cat6[20] =
    {
        0.70711f, 0.5686f,  0.3563f,  0.25f,
        0.25f,    0.25f,    0.25f,    0.25f,
        0.25f,    0.25f,    0.25f,    0.25f,
        0.25f,    0.25f,    0.25f,    0.25f,
        0.25f,    0.25f,    0.25f,    0.25f
    };

    float standard_deviation;
    float *decoder_mlt_ptr;
    float decoder_mlt_value;
    float temp1;
    float noifillpos;
    float noifillneg;
    int region;
    int category;
    int j;
    int n;
    int k[MAX_VECTOR_DIMENSION];
    int vec_dim;
    int num_vecs;
    int index;
    int signs_index;
    int bit;
    int num_sign_bits;
    int num_bits;
    int ran_out_of_bits_flag;
    int random_word;
    const int16_t *decoder_table_ptr;

    ran_out_of_bits_flag = 0;
    for (region = 0;  region < number_of_regions;  region++)
    {
        category = decoder_power_categories[region];
        decoder_mlt_ptr = &decoder_mlt_coefs[region*REGION_SIZE];
        standard_deviation = decoder_region_standard_deviation[region];
        if (category < NUM_CATEGORIES - 1)
        {
            decoder_table_ptr = table_of_decoder_tables[category];
            vec_dim = vector_dimension[category];
            num_vecs = number_of_vectors[category];

            for (n = 0;  n < num_vecs;  n++)
            {
                num_bits = 0;
                index = 0;
                do
                {
                    if (s->number_of_bits_left <= 0)
                    {
                        ran_out_of_bits_flag = 1;
                        break;
                    }

                    if (g722_1_bitstream_get(&s->bitstream, &(s->code_ptr), 1) == 0)
                        index = *(decoder_table_ptr + 2*index);
                    else
                        index = *(decoder_table_ptr + 2*index + 1);
                    s->number_of_bits_left--;
                }
                while (index > 0);

                if (ran_out_of_bits_flag == 1)
                    break;
                index = -index;
                num_sign_bits = index_to_array(index, k, category);

                if (s->number_of_bits_left >= num_sign_bits)
                {
                    signs_index = 0;
                    bit = 0;
                    if (num_sign_bits != 0)
                    {
                        signs_index = g722_1_bitstream_get(&s->bitstream, &(s->code_ptr), num_sign_bits);
                        s->number_of_bits_left -= num_sign_bits;
                        bit = 1 << (num_sign_bits - 1);
                    }
                    for (j = 0;  j < vec_dim;  j++)
                    {
                        /* This was changed to for fixed point interop
                           A scale factor is used to adjust the decoded mlt value. */
                        decoder_mlt_value = standard_deviation
                                          * mlt_quant_centroid[category][k[j]]
                                          * (float) rmlt_scale_factor;

                        if (decoder_mlt_value != 0)
                        {
                            if ((signs_index & bit) == 0)
                                decoder_mlt_value *= -1;
                            bit >>= 1;
                        }

                        *decoder_mlt_ptr++ = decoder_mlt_value;
                    }
                }
                else
                {
                    ran_out_of_bits_flag = 1;
                    break;
                }
            }

            /* If ran out of bits during decoding do noise fill for remaining regions. */
            if (ran_out_of_bits_flag == 1)
            {
                for (j = region + 1;  j < number_of_regions;  j++)
                    decoder_power_categories[j] = NUM_CATEGORIES - 1;
                category = NUM_CATEGORIES - 1;
                decoder_mlt_ptr = &decoder_mlt_coefs[region*REGION_SIZE];
            }
        }

        if (category == NUM_CATEGORIES - 3)
        {
            decoder_mlt_ptr = &decoder_mlt_coefs[region*REGION_SIZE];
            n = 0;
            for (j = 0;  j < REGION_SIZE;  j++)
            {
                if (*decoder_mlt_ptr != 0)
                {
                    n++;
                    if (fabs(*decoder_mlt_ptr) > (2.0*(float) rmlt_scale_factor*standard_deviation))
                        n += 3;
                }
                decoder_mlt_ptr++;
            }
            if (n > 19)
                n = 19;
            temp1 = noise_fill_factor_cat5[n];

            decoder_mlt_ptr = &decoder_mlt_coefs[region*REGION_SIZE];

            /* noifillpos = standard_deviation * 0.17678; */
            noifillpos = standard_deviation*temp1;
            noifillneg = -noifillpos;

            /* This assumes region_size = 20 */
            random_word = get_rand(&s->randobj);
            for (j = 0;  j < 10;  j++)
            {
                if (*decoder_mlt_ptr == 0)
                {
                    temp1 = noifillpos;
                    if ((random_word & 1) == 0)
                        temp1 = noifillneg;
                    *decoder_mlt_ptr = temp1*(float) rmlt_scale_factor;
                    random_word >>= 1;
                }
                decoder_mlt_ptr++;
            }
            random_word = get_rand(&s->randobj);
            for (j = 0;  j < 10;  j++)
            {
                if (*decoder_mlt_ptr == 0)
                {
                    temp1 = noifillpos;
                    if ((random_word & 1) == 0)
                        temp1 = noifillneg;
                    *decoder_mlt_ptr = temp1*(float) rmlt_scale_factor;
                    random_word >>= 1;
                }
                decoder_mlt_ptr++;
            }
        }

        if (category == NUM_CATEGORIES - 2)
        {
            decoder_mlt_ptr = &decoder_mlt_coefs[region*REGION_SIZE];
            n = 0;
            for (j = 0;  j < REGION_SIZE;  j++)
            {
                if (*decoder_mlt_ptr++ != 0)
                    n++;
            }
            temp1 = noise_fill_factor_cat6[n];

            decoder_mlt_ptr = &decoder_mlt_coefs[region*REGION_SIZE];

            noifillpos = standard_deviation*temp1;
            noifillneg = -noifillpos;

            /* This assumes region_size = 20 */
            random_word = get_rand(&s->randobj);
            for (j = 0;  j < 10;  j++)
            {
                if (*decoder_mlt_ptr == 0)
                {
                    temp1 = noifillpos;
                    if ((random_word & 1) == 0)
                        temp1 = noifillneg;
                    *decoder_mlt_ptr = temp1*(float) rmlt_scale_factor;
                    random_word >>= 1;
                }
                decoder_mlt_ptr++;
            }
            random_word = get_rand(&s->randobj);
            for (j = 0;  j < 10;  j++)
            {
                if (*decoder_mlt_ptr == 0)
                {
                    temp1 = noifillpos;
                    if ((random_word & 1) == 0)
                        temp1 = noifillneg;
                    *decoder_mlt_ptr = temp1*(float) rmlt_scale_factor;
                    random_word >>= 1;
                }
                decoder_mlt_ptr++;
            }
        }

        if (category == NUM_CATEGORIES - 1)
        {
            noifillpos = standard_deviation*0.70711;
            noifillneg = -noifillpos;

            /* This assumes region_size = 20 */
            random_word = get_rand(&s->randobj);
            for (j = 0;  j < 10;  j++)
            {
                temp1 = ((random_word & 1) == 0)  ?  noifillneg  :  noifillpos;
                *decoder_mlt_ptr++ = temp1*(float) rmlt_scale_factor;
                random_word >>= 1;
            }
            random_word = get_rand(&s->randobj);
            for (j = 0;  j < 10;  j++)
            {
                temp1 = ((random_word & 1) == 0)  ?  noifillneg  :  noifillpos;
                decoder_mlt_ptr[j] = temp1*(float) rmlt_scale_factor;
                random_word >>= 1;
            }
        }
    }

    if (ran_out_of_bits_flag)
        s->number_of_bits_left = -1;
}
/*- End of function --------------------------------------------------------*/

/* Compute an array of sign bits with the length of the category vector
   Returns the number of sign bits and the array */
static int index_to_array(int index, int array[MAX_VECTOR_DIMENSION], int category)
{
    int j;
    int q;
    int p;
    int number_of_non_zero;
    int max_bin_plus_one;
    int inverse_of_max_bin_plus_one;

    number_of_non_zero = 0;
    p = index;
    max_bin_plus_one = max_bin[category] + 1;
    inverse_of_max_bin_plus_one = max_bin_plus_one_inverse[category];

    for (j = vector_dimension[category] - 1;  j >= 0;  j--)
    {
        q = (p*inverse_of_max_bin_plus_one) >> 15;
        array[j] = p - q*max_bin_plus_one;
        p = q;
        if (array[j] != 0)
            number_of_non_zero++;
    }
    return number_of_non_zero;
}
/*- End of function --------------------------------------------------------*/

/* Tests for error conditions and sets the frame_error_flag accordingly */
static void test_for_frame_errors(g722_1_decode_state_t *s,
                                  int16_t number_of_regions,
                                  int16_t num_categorization_control_possibilities,
                                  int *frame_error_flag,
                                  int16_t categorization_control,
                                  int *absolute_region_power_index)
{
    int i;

    /* Test for bit stream errors. */
    if (s->number_of_bits_left > 0)
    {
        while (s->number_of_bits_left > 0)
        {
            if (g722_1_bitstream_get(&s->bitstream, &(s->code_ptr), 1) == 0)
                *frame_error_flag = 1;
            s->number_of_bits_left--;
        }
    }
    else
    {
        if (categorization_control < num_categorization_control_possibilities - 1)
        {
            if (s->number_of_bits_left < 0)
                *frame_error_flag |= 2;
        }
    }

    /* Checks to ensure that absolute_region_power_index is within range */
    /* The error flag is set if it is out of range */
    for (i = 0;  i < number_of_regions;  i++)
    {
        if ((absolute_region_power_index[i] + ESF_ADJUSTMENT_TO_RMS_INDEX > 31)
            ||
            (absolute_region_power_index[i] + ESF_ADJUSTMENT_TO_RMS_INDEX < -8))
        {
            *frame_error_flag |= 4;
            break;
        }
    }
}
/*- End of function --------------------------------------------------------*/

static void error_handling(int number_of_coefs,
                           int number_of_valid_coefs,
                           int *frame_error_flag,
                           float *decoder_mlt_coefs,
                           float *old_decoder_mlt_coefs)
{
    /* If both the current and previous frames are errored,
       set the mlt coefficients to 0. If only the current frame
       is errored, repeat the previous frame's MLT coefficients. */
    if (*frame_error_flag)
    {
        vec_copyf(decoder_mlt_coefs, old_decoder_mlt_coefs, number_of_valid_coefs);
        vec_zerof(old_decoder_mlt_coefs, number_of_valid_coefs);
    }
    else
    {
        /* Store in case the next frame has errors. */
        vec_copyf(old_decoder_mlt_coefs, decoder_mlt_coefs, number_of_valid_coefs);
    }
    /* Zero out the upper 1/8 of the spectrum. */
    vec_zerof(&decoder_mlt_coefs[number_of_valid_coefs], number_of_coefs - number_of_valid_coefs);
}
/*- End of function --------------------------------------------------------*/

int g722_1_decode(g722_1_decode_state_t *s, int16_t amp[], const uint8_t g722_1_data[], int len)
{
    float decoder_mlt_coefs[MAX_DCT_LENGTH];
    float famp[MAX_FRAME_SIZE];
    float ftemp;
    int i;
    int j;
    int k;

    for (i = 0, j = 0;  j < len;  i += s->frame_size, j += s->number_of_bits_per_frame/8)
    {
        g722_1_bitstream_init(&s->bitstream);
        s->code_ptr = &g722_1_data[j];
        s->number_of_bits_left = s->number_of_bits_per_frame;
        /* Process the out_words into decoder_mlt_coefs */
        decoder(s,
                decoder_mlt_coefs,
                s->old_decoder_mlt_coefs,
                0);

        /* Convert the decoder_mlt_coefs to samples */
        rmlt_coefs_to_samples(decoder_mlt_coefs, s->old_samples, famp, s->frame_size);
        for (k = 0;  k < s->frame_size;  k++)
        {
            ftemp = famp[k];
            if (ftemp >= 0.0)
                amp[k + i] = (ftemp < 32767.0)  ?  (int) (ftemp + 0.5)  :  32767;
            else 
                amp[k + i] = (ftemp > -32768.0)  ?  (int) (ftemp - 0.5)  :  -32768;
        }
    }
    return i;
}
/*- End of function --------------------------------------------------------*/

int g722_1_fillin(g722_1_decode_state_t *s, int16_t amp[], const uint8_t g722_1_data[], int len)
{
    float decoder_mlt_coefs[MAX_DCT_LENGTH];
    float famp[MAX_FRAME_SIZE];
    float ftemp;
    int i;
    int j;
    int k;
    
    for (i = 0, j = 0;  j < len;  i += s->frame_size, j += s->number_of_bits_per_frame/8)
    {
        g722_1_bitstream_init(&s->bitstream);
        s->code_ptr = &g722_1_data[j];
        s->number_of_bits_left = s->number_of_bits_per_frame;

        /* Process the out_words into decoder MLT_coefs */
        decoder(s,
                decoder_mlt_coefs,
                s->old_decoder_mlt_coefs,
                1);

        /* Convert the decoder MLT coefs to samples */
        rmlt_coefs_to_samples(decoder_mlt_coefs, s->old_samples, famp, s->frame_size);
        for (k = 0;  k < s->frame_size;  k++)
        {
            ftemp = famp[k];
            if (ftemp >= 0.0)
                amp[k + i] = (ftemp < 32767.0)  ?  (int) (ftemp + 0.5)  :  32767;
            else 
                amp[k + i] = (ftemp > -32768.0)  ?  (int) (ftemp - 0.5)  :  -32768;
        }
    }
    return i;
}
/*- End of function --------------------------------------------------------*/
#endif
/*- End of file ------------------------------------------------------------*/
