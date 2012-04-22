/*
 * g722_1 - a library for the G.722.1 and Annex C codecs
 *
 * encoder.c
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

static int compute_region_powers(int number_of_regions,
                                 float mlt_coefs[MAX_DCT_LENGTH],
                                 int drp_num_bits[MAX_NUMBER_OF_REGIONS],
                                 int drp_code_bits[MAX_NUMBER_OF_REGIONS],
                                 int absolute_region_power_index[MAX_NUMBER_OF_REGIONS]);

static int vector_huffman(int category,
                          int power_index,
                          float *raw_mlt_ptr,
                          int32_t *word_ptr);

static void vector_quantize_mlts(int number_of_regions,
                                 int num_categorization_control_possibilities,
                                 int number_of_available_bits,
                                 float mlt_coefs[MAX_DCT_LENGTH],
                                 int absolute_region_power_index[MAX_NUMBER_OF_REGIONS],
                                 int power_categories[MAX_NUMBER_OF_REGIONS],
                                 int category_balances[MAX_NUM_CATEGORIZATION_CONTROL_POSSIBILITIES - 1],
                                 int *p_rate_control,
                                 int region_mlt_bit_counts[MAX_NUMBER_OF_REGIONS],
                                 int32_t region_mlt_bits[4*MAX_NUMBER_OF_REGIONS]);

static void encoder(g722_1_encode_state_t *s,
                    int number_of_available_bits,
                    int number_of_regions,
                    float mlt_coefs[MAX_DCT_LENGTH],
                    uint8_t g722_1_data[MAX_BITS_PER_FRAME/8]);

/* Stuff the bits into words for output */
static void bits_to_words(g722_1_encode_state_t *s,
                          int32_t *region_mlt_bits,
                          int *region_mlt_bit_counts,
                          int *drp_num_bits,
                          int *drp_code_bits,
                          uint8_t *out_code,
                          int categorization_control,
                          int number_of_regions,
                          int num_categorization_control_bits,
                          int number_of_bits_per_frame)
{
    int region;
    int region_bit_count;
    int32_t *in_word_ptr;
    uint32_t current_code;
    int current_bits;
    int bit_count;

    /* First set up the categorization control bits to look like one more set of region power bits. */
    drp_num_bits[number_of_regions] = num_categorization_control_bits;
    drp_code_bits[number_of_regions] = categorization_control;

    bit_count = 0;
    /* These code bits are right justified. */
    for (region = 0;  region <= number_of_regions;  region++)
    {
        g722_1_bitstream_put(&s->bitstream, &out_code, drp_code_bits[region], drp_num_bits[region]);
        bit_count += drp_num_bits[region];
    }

    /* These code bits are left justified. */
    for (region = 0;  (region < number_of_regions)  &&  (bit_count < number_of_bits_per_frame);  region++)
    {
        in_word_ptr = &region_mlt_bits[4*region];
        region_bit_count = region_mlt_bit_counts[region];
        while ((region_bit_count > 0)  &&  (bit_count < number_of_bits_per_frame))
        {
            current_bits = MIN(32, region_bit_count);
            current_code = *in_word_ptr++;
            current_code >>= (32 - current_bits);
            g722_1_bitstream_put(&s->bitstream, &out_code, current_code, current_bits);
            bit_count += current_bits;
            region_bit_count -= current_bits;
        }
    }

    /* Fill out with 1's. */
    while (bit_count < number_of_bits_per_frame)
    {
        current_bits = MIN(32, number_of_bits_per_frame - bit_count);
        g722_1_bitstream_put(&s->bitstream, &out_code, 0xFFFFFFFF, current_bits);
        bit_count += current_bits;
    }
    g722_1_bitstream_flush(&s->bitstream, &out_code);
}
/*- End of function --------------------------------------------------------*/

/* Encode the MLT coefs into out_words using G.722.1 Annex C */
static void encoder(g722_1_encode_state_t *s,
                    int number_of_available_bits,
                    int number_of_regions,
                    float mlt_coefs[MAX_DCT_LENGTH],
                    uint8_t g722_1_data[MAX_BITS_PER_FRAME/8])
{
    int num_categorization_control_bits;
    int num_categorization_control_possibilities;
    int number_of_bits_per_frame;
    int number_of_envelope_bits;
    int rate_control;
    int region;
    int absolute_region_power_index[MAX_NUMBER_OF_REGIONS];
    int power_categories[MAX_NUMBER_OF_REGIONS];
    int category_balances[MAX_NUM_CATEGORIZATION_CONTROL_POSSIBILITIES - 1];
    int drp_num_bits[MAX_NUMBER_OF_REGIONS + 1];
    int drp_code_bits[MAX_NUMBER_OF_REGIONS + 1];
    int region_mlt_bit_counts[MAX_NUMBER_OF_REGIONS];
    int32_t region_mlt_bits[4*MAX_NUMBER_OF_REGIONS];

    /* Initialize variables. */
    if (number_of_regions == NUMBER_OF_REGIONS)
    {
        num_categorization_control_bits = NUM_CATEGORIZATION_CONTROL_BITS;
        num_categorization_control_possibilities = NUM_CATEGORIZATION_CONTROL_POSSIBILITIES;
    } 
    else
    {
        num_categorization_control_bits = MAX_NUM_CATEGORIZATION_CONTROL_BITS;
        num_categorization_control_possibilities = MAX_NUM_CATEGORIZATION_CONTROL_POSSIBILITIES;
    } 

    number_of_bits_per_frame = number_of_available_bits;

    /* Estimate power envelope. */
    number_of_envelope_bits = compute_region_powers(number_of_regions,
                                                    mlt_coefs,
                                                    drp_num_bits,
                                                    drp_code_bits,
                                                    absolute_region_power_index);

    number_of_available_bits -= number_of_envelope_bits;
    number_of_available_bits -= num_categorization_control_bits;

    categorize(number_of_regions,
               number_of_available_bits,
               absolute_region_power_index,
               power_categories,
               category_balances);

    /* Adjust absolute_region_category_index[] for mag_shift.
       This assumes that REGION_POWER_STEPSIZE_DB is defined
       to be exactly 3.010299957 or 20.0 times log base 10
       of square root of 2. */
    for (region = 0;  region < number_of_regions;  region++)
    {
        absolute_region_power_index[region] += REGION_POWER_TABLE_NUM_NEGATIVES;
        region_mlt_bit_counts[region] = 0;
    }

    vector_quantize_mlts(number_of_regions,
                         num_categorization_control_possibilities,
                         number_of_available_bits,
                         mlt_coefs,
                         absolute_region_power_index,
                         power_categories,
                         category_balances,
                         &rate_control,
                         region_mlt_bit_counts,
                         region_mlt_bits);

    /* Stuff bits into words */
    bits_to_words(s,
                  region_mlt_bits,
                  region_mlt_bit_counts,
                  drp_num_bits,
                  drp_code_bits,
                  g722_1_data,
                  rate_control,
                  number_of_regions,
                  num_categorization_control_bits,
                  number_of_bits_per_frame);
}
/*- End of function --------------------------------------------------------*/

/* Compute the power for each of the regions */
static int compute_region_powers(int number_of_regions,
                                 float mlt_coefs[MAX_DCT_LENGTH],
                                 int drp_num_bits[MAX_NUMBER_OF_REGIONS],
                                 int drp_code_bits[MAX_NUMBER_OF_REGIONS],
                                 int absolute_region_power_index[MAX_NUMBER_OF_REGIONS])
{
    float *input_ptr;
    int iterations;
    float ftemp0;
    int index;
    int index_min;
    int index_max;
    int region;
    int j;
    int differential_region_power_index[MAX_NUMBER_OF_REGIONS];
    int number_of_bits;

    input_ptr = mlt_coefs;
    for (region = 0;  region < number_of_regions;  region++)
    {
        ftemp0 = vec_dot_prodf(input_ptr, input_ptr, REGION_SIZE);
        ftemp0 *= REGION_SIZE_INVERSE;
        input_ptr += REGION_SIZE;

        index_min = 0;
        index_max = REGION_POWER_TABLE_SIZE;
        for (iterations = 0;  iterations < 6;  iterations++)
        {
            index = (index_min + index_max) >> 1;
            if (ftemp0 < region_power_table_boundary[index - 1])
                index_max = index;
            else
                index_min = index;
        }
        absolute_region_power_index[region] = index_min - REGION_POWER_TABLE_NUM_NEGATIVES;
    }

    /* Before we differentially encode the quantized region powers, adjust upward the
       valleys to make sure all the peaks can be accurately represented. */
    for (region = number_of_regions - 2;  region >= 0;  region--)
    {
        if (absolute_region_power_index[region] < absolute_region_power_index[region+1] - DRP_DIFF_MAX)
            absolute_region_power_index[region] = absolute_region_power_index[region+1] - DRP_DIFF_MAX;
    }

    /* The MLT is currently scaled too low by the factor
       ENCODER_SCALE_FACTOR(=18318)/32768 * (1.0/sqrt(160).
       This is the ninth power of 1 over the square root of 2.
       So later we will add ESF_ADJUSTMENT_TO_RMS_INDEX (now 9)
       to drp_code_bits[0]. */

    /* drp_code_bits[0] can range from 1 to 31. 0 will be used only as an escape sequence. */
    if (absolute_region_power_index[0] < 1 - ESF_ADJUSTMENT_TO_RMS_INDEX)
        absolute_region_power_index[0] = 1 - ESF_ADJUSTMENT_TO_RMS_INDEX;
    if (absolute_region_power_index[0] > 31 - ESF_ADJUSTMENT_TO_RMS_INDEX)
        absolute_region_power_index[0] = 31 - ESF_ADJUSTMENT_TO_RMS_INDEX;

    differential_region_power_index[0] = absolute_region_power_index[0];
    number_of_bits = 5;
    drp_num_bits[0] = 5;
    drp_code_bits[0] = absolute_region_power_index[0] + ESF_ADJUSTMENT_TO_RMS_INDEX;

    /* Lower limit the absolute region power indices to -8 and upper limit them to 31. Such extremes
       may be mathematically impossible anyway.*/
    for (region = 1;  region < number_of_regions;  region++)
    {
        if (absolute_region_power_index[region] < -8 - ESF_ADJUSTMENT_TO_RMS_INDEX)
            absolute_region_power_index[region] = -8 - ESF_ADJUSTMENT_TO_RMS_INDEX;
        if (absolute_region_power_index[region] > 31 - ESF_ADJUSTMENT_TO_RMS_INDEX)
            absolute_region_power_index[region] = 31 - ESF_ADJUSTMENT_TO_RMS_INDEX;
    }

    for (region = 1;  region < number_of_regions;  region++)
    {
        j = absolute_region_power_index[region] - absolute_region_power_index[region - 1];
        if (j < DRP_DIFF_MIN)
            j = DRP_DIFF_MIN;
        j -= DRP_DIFF_MIN;
        differential_region_power_index[region] = j;
        absolute_region_power_index[region] = absolute_region_power_index[region - 1]
                                            + differential_region_power_index[region]
                                            + DRP_DIFF_MIN;

        number_of_bits += differential_region_power_bits[region][j];
        drp_num_bits[region] = differential_region_power_bits[region][j];
        drp_code_bits[region] = differential_region_power_codes[region][j];
    }

    return number_of_bits;
}
/*- End of function --------------------------------------------------------*/

/* Scalar quantized vector Huffman coding (SQVH) */
static void vector_quantize_mlts(int number_of_regions,
                                 int num_categorization_control_possibilities,
                                 int number_of_available_bits,
                                 float mlt_coefs[MAX_DCT_LENGTH],
                                 int absolute_region_power_index[MAX_NUMBER_OF_REGIONS],
                                 int power_categories[MAX_NUMBER_OF_REGIONS],
                                 int category_balances[MAX_NUM_CATEGORIZATION_CONTROL_POSSIBILITIES - 1],
                                 int *p_rate_control,
                                 int region_mlt_bit_counts[MAX_NUMBER_OF_REGIONS],
                                 int32_t region_mlt_bits[4*MAX_NUMBER_OF_REGIONS])
{
    float *raw_mlt_ptr;
    int region;
    int category;
    int total_mlt_bits;

    total_mlt_bits = 0;

    /* Start in the middle of the rate control range. */
    for (*p_rate_control = 0;  *p_rate_control < ((num_categorization_control_possibilities >> 1) - 1);  (*p_rate_control)++)
    {
        region = category_balances[*p_rate_control];
        power_categories[region]++;
    }

    for (region = 0;  region < number_of_regions;  region++)
    {
        category = power_categories[region];
        raw_mlt_ptr = &mlt_coefs[region*REGION_SIZE];
        if (category < NUM_CATEGORIES - 1)
        {
            region_mlt_bit_counts[region] = vector_huffman(category,
                                                           absolute_region_power_index[region],
                                                           raw_mlt_ptr,
                                                           &region_mlt_bits[4*region]);
        }
        else
        {
            region_mlt_bit_counts[region] = 0;
        }
        total_mlt_bits += region_mlt_bit_counts[region];
    }

    /* If too few bits... */
    while ((total_mlt_bits < number_of_available_bits)  &&  (*p_rate_control > 0))
    {
        (*p_rate_control)--;
        region = category_balances[*p_rate_control];
        power_categories[region]--;
        total_mlt_bits -= region_mlt_bit_counts[region];

        category = power_categories[region];
        raw_mlt_ptr = &mlt_coefs[region*REGION_SIZE];
        if (category < NUM_CATEGORIES - 1)
        {
            region_mlt_bit_counts[region] = vector_huffman(category,
                                                           absolute_region_power_index[region],
                                                           raw_mlt_ptr,
                                                           &region_mlt_bits[4*region]);
        }
        else
        {
            region_mlt_bit_counts[region] = 0;
        }
        total_mlt_bits += region_mlt_bit_counts[region];
    }

    /* If too many bits... */
    while ((total_mlt_bits > number_of_available_bits)  &&  (*p_rate_control < num_categorization_control_possibilities - 1))
    {
        region = category_balances[*p_rate_control];
        power_categories[region]++;
        total_mlt_bits -= region_mlt_bit_counts[region];

        category = power_categories[region];
        raw_mlt_ptr = &mlt_coefs[region*REGION_SIZE];
        if (category < NUM_CATEGORIES - 1)
        {
            region_mlt_bit_counts[region] = vector_huffman(category,
                                                           absolute_region_power_index[region],
                                                           raw_mlt_ptr,
                                                           &region_mlt_bits[4*region]);
        }
        else
        {
            region_mlt_bit_counts[region] = 0;
        }
        total_mlt_bits += region_mlt_bit_counts[region];
        (*p_rate_control)++;
    }
}
/*- End of function --------------------------------------------------------*/

/* Huffman encoding for each region based on category and power_index  */
static int vector_huffman(int category,
                          int power_index,
                          float *raw_mlt_ptr,
                          int32_t *word_ptr)
{
    float inv_of_step_size_times_std_dev;
    int j;
    int n;
    int k;
    int number_of_region_bits;
    int number_of_non_zero;
    int vec_dim;
    int num_vecs;
    int kmax;
    int kmax_plus_one;
    int index;
    int signs_index;
    const int16_t *bitcount_table_ptr;
    const uint16_t *code_table_ptr;
    int code_bits;
    int number_of_code_bits;
    int current_word;
    int current_word_bits_free;

    vec_dim = vector_dimension[category];
    num_vecs = number_of_vectors[category];
    kmax = max_bin[category];
    kmax_plus_one = kmax + 1;

    current_word = 0;
    current_word_bits_free = 32;

    number_of_region_bits = 0;

    bitcount_table_ptr = table_of_bitcount_tables[category];
    code_table_ptr = table_of_code_tables[category];

    inv_of_step_size_times_std_dev = step_size_inverse_table[category]
                                   * standard_deviation_inverse_table[power_index];

    for (n = 0;  n < num_vecs;  n++)
    {
        index = 0;
        signs_index = 0;
        number_of_non_zero = 0;
        for (j = 0;  j < vec_dim;  j++)
        {
            k = (int) (fabs(*raw_mlt_ptr)*inv_of_step_size_times_std_dev + dead_zone[category]);
            if (k != 0)
            {
                number_of_non_zero++;
                signs_index <<= 1;
                if (*raw_mlt_ptr > 0)
                    signs_index++;
                if (k > kmax)
                    k = kmax;
            }
            index = index*kmax_plus_one + k;
            raw_mlt_ptr++;
        }

        code_bits = code_table_ptr[index];
        number_of_code_bits = bitcount_table_ptr[index] + number_of_non_zero;
        number_of_region_bits += number_of_code_bits;

        code_bits = (code_bits << number_of_non_zero) + signs_index;

        /* MSB of codebits is transmitted first. */
        j = current_word_bits_free - number_of_code_bits;
        if (j >= 0)
        {
            current_word += code_bits << j;
            current_word_bits_free = j;
        }
        else
        {
            j = -j;
            current_word += code_bits >> j;
            *word_ptr++ = current_word;
            current_word_bits_free = 32 - j;
            current_word = code_bits << current_word_bits_free;
        }
    }

    *word_ptr++ = current_word;

    return number_of_region_bits;
}
/*- End of function --------------------------------------------------------*/

int g722_1_encode(g722_1_encode_state_t *s, uint8_t g722_1_data[], const int16_t amp[], int len)
{
    float mlt_coefs[MAX_FRAME_SIZE];
    float famp[MAX_FRAME_SIZE];
    int i;
    int j;
    int k;

    for (i = 0, j = 0;  i < len;  i += s->frame_size, j += s->bytes_per_frame)
    {
        for (k = 0;  k < s->frame_size;  k++)
            famp[k] = amp[k + i];
        samples_to_rmlt_coefs(famp, s->history, mlt_coefs, s->frame_size);
        /* This is for fixed point interop */
        for (k = 0;  k < s->frame_size;  k++)
            mlt_coefs[k] *= s->scale_factor;
        encoder(s,
                s->number_of_bits_per_frame,
                s->number_of_regions,
                mlt_coefs,
                &g722_1_data[j]);
    }
    return j;
}
/*- End of function --------------------------------------------------------*/
#endif
/*- End of file ------------------------------------------------------------*/
