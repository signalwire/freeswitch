/*
 * g722_1 - a library for the G.722.1 and Annex C codecs
 *
 * decoder.c
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
 * $Id: decoder.c,v 1.21 2008/11/21 15:30:22 steveu Exp $
 */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include <config.h>
#endif

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "g722_1/g722_1.h"

#include "defs.h"
#include "tables.h"
#include "huff_tab.h"
#include "bitstream.h"

int16_t get_rand(g722_1_rand_t *randobj)
{
    int16_t random_word;
    int32_t acca;

    acca = randobj->seed0 + randobj->seed3;
    random_word = (int16_t) acca;

    if ((random_word & 32768L) != 0)
        random_word++;
    randobj->seed3 = randobj->seed2;
    randobj->seed2 = randobj->seed1;
    randobj->seed1 = randobj->seed0;
    randobj->seed0 = random_word;
    return random_word;
}
/*- End of function --------------------------------------------------------*/

#if defined(G722_1_USE_FIXED_POINT)

static void test_for_frame_errors(g722_1_decode_state_t *s,
                                  int16_t number_of_regions,
                                  int16_t num_categorization_control_possibilities,
                                  int *frame_error_flag,
                                  int16_t categorization_control,
                                  int16_t *absolute_region_power_index);

static void error_handling(int16_t number_of_coefs,
                           int16_t number_of_valid_coefs,
                           int *frame_error_flag,
                           int16_t *decoder_mlt_coefs,
                           int16_t *old_decoder_mlt_coefs,
                           int16_t *p_mag_shift,
                           int16_t *p_old_mag_shift);

static void decode_vector_quantized_mlt_indices(g722_1_decode_state_t *s,
                                                int16_t number_of_regions,
                                                int16_t *decoder_region_standard_deviation,
                                                int16_t *dedecoder_power_categories,
                                                int16_t *dedecoder_mlt_coefs);

static void decode_envelope(g722_1_decode_state_t *s,
                            int16_t number_of_regions,
                            int16_t *decoder_region_standard_deviation,
                            int16_t *absolute_region_power_index,
                            int16_t *p_mag_shift);

static void rate_adjust_categories(int16_t categorization_control,
                                   int16_t *decoder_power_categories,
                                   int16_t *decoder_category_balances);

static int16_t index_to_array(int16_t index, int16_t *array, int16_t category);

static void decoder(g722_1_decode_state_t *s,
                    int16_t number_of_regions,
                    int16_t decoder_mlt_coefs[],
                    int16_t *p_mag_shift,
                    int16_t *p_old_mag_shift,
                    int16_t old_decoder_mlt_coefs[],
                    int frame_error_flag);

/***************************************************************************
 Description: Decodes the out_words into mlt coefs using G.722.1 Annex C
***************************************************************************/
void decoder(g722_1_decode_state_t *s,
             int16_t number_of_regions,
             int16_t decoder_mlt_coefs[],
             int16_t *p_mag_shift,
             int16_t *p_old_mag_shift,
             int16_t old_decoder_mlt_coefs[],
             int frame_error_flag)
{
    int16_t decoder_region_standard_deviation[MAX_NUMBER_OF_REGIONS];
    int16_t absolute_region_power_index[MAX_NUMBER_OF_REGIONS];
    int16_t decoder_power_categories[MAX_NUMBER_OF_REGIONS];
    int16_t decoder_category_balances[MAX_NUM_CATEGORIZATION_CONTROL_POSSIBILITIES - 1];
    uint16_t categorization_control;
    int16_t num_categorization_control_bits;
    int16_t num_categorization_control_possibilities;
    int16_t number_of_coefs;
    int16_t number_of_valid_coefs;
 
    number_of_valid_coefs = number_of_regions*REGION_SIZE;

    /* Get some parameters based solely on the bitstream style */
    if (number_of_regions == NUMBER_OF_REGIONS)
    {
        number_of_coefs = DCT_LENGTH;
        num_categorization_control_bits = NUM_CATEGORIZATION_CONTROL_BITS;
        num_categorization_control_possibilities = NUM_CATEGORIZATION_CONTROL_POSSIBILITIES;
    }
    else
    {
        number_of_coefs = MAX_DCT_LENGTH;
        num_categorization_control_bits = MAX_NUM_CATEGORIZATION_CONTROL_BITS;
        num_categorization_control_possibilities = MAX_NUM_CATEGORIZATION_CONTROL_POSSIBILITIES;
    }

    if (frame_error_flag == 0)
    {
        /* Convert the bits to absolute region power index and decoder_region_standard_deviation */
        decode_envelope(s,
                        number_of_regions,
                        decoder_region_standard_deviation,
                        absolute_region_power_index,
                        p_mag_shift);

        /* Fill the categorization_control with NUM_CATEGORIZATION_CONTROL_BITS */
        categorization_control = g722_1_bitstream_get(&s->bitstream, &(s->code_ptr), num_categorization_control_bits);
        s->number_of_bits_left -= num_categorization_control_bits;

        /* Obtain decoder power categories and category balances */
        /* Based on the absolute region power index */
        categorize(s->number_of_bits_left,
                   number_of_regions,
                   num_categorization_control_possibilities,
                   absolute_region_power_index,
                   decoder_power_categories,
                   decoder_category_balances);

        /* Perform adjustmaents to the power categories and category balances based on the cat control */
        rate_adjust_categories(categorization_control,
                               decoder_power_categories,
                               decoder_category_balances);

        /* Decode the quantized bits into mlt coefs */
        decode_vector_quantized_mlt_indices(s,
                                            number_of_regions,
                                            decoder_region_standard_deviation,
                                            decoder_power_categories,
                                            decoder_mlt_coefs);

        test_for_frame_errors(s,
                              number_of_regions,
                              num_categorization_control_possibilities,
                              &frame_error_flag,
                              categorization_control,
                              absolute_region_power_index);
    }

    /* Perform error handling operations */
    error_handling(number_of_coefs,
                   number_of_valid_coefs,
                   &frame_error_flag,
                   decoder_mlt_coefs,
                   old_decoder_mlt_coefs,
                   p_mag_shift,
                   p_old_mag_shift);
}
/*- End of function --------------------------------------------------------*/

/***************************************************************************
 Description: Recover differential_region_power_index from code bits
***************************************************************************/
static void decode_envelope(g722_1_decode_state_t *s,
                            int16_t number_of_regions,
                            int16_t *decoder_region_standard_deviation,
                            int16_t *absolute_region_power_index,
                            int16_t *p_mag_shift)
{
    int16_t region;
    int16_t i;
    int16_t index;
    int16_t differential_region_power_index[MAX_NUMBER_OF_REGIONS];
    int16_t max_index;
    int16_t temp;
    int16_t temp1;
    int16_t temp2;
    int32_t acca;

    /* Get 5 bits from the current code word */
    index = g722_1_bitstream_get(&s->bitstream, &(s->code_ptr), 5);
    s->number_of_bits_left -= 5;

    /* ESF_ADJUSTMENT_TO_RMS_INDEX compensates for the current (9/30/96)
       IMLT being scaled to high by the ninth power of sqrt(2). */
    differential_region_power_index[0] = sub(index, ESF_ADJUSTMENT_TO_RMS_INDEX);

    /* Obtain differential_region_power_index */
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

        differential_region_power_index[region] = negate(index);
    }

    /* Reconstruct absolute_region_power_index[] from differential_region_power_index[]. */
    absolute_region_power_index[0] = differential_region_power_index[0];
    for (region = 1;  region < number_of_regions;  region++)
    {
        acca = L_add(absolute_region_power_index[region - 1], differential_region_power_index[region]);
        acca = L_add(acca, DRP_DIFF_MIN);
        absolute_region_power_index[region] = (int16_t) acca;
    }

    /* Reconstruct decoder_region_standard_deviation[] from absolute_region_power_index[]. */
    /* DEBUG!!!! - This integer method jointly computes the mag_shift
       and the standard deviations already mag_shift compensated. It
       relies on REGION_POWER_STEPSIZE_DB being exactly 3.010299957 db
       or a square root of 2 change in standard deviation. If
       REGION_POWER_STEPSIZE_DB changes, this software must be reworked. */

    temp = 0;
    max_index = 0;
    for (region = 0;  region < number_of_regions;  region++)
    {
        acca = L_add(absolute_region_power_index[region], REGION_POWER_TABLE_NUM_NEGATIVES);
        i = (int16_t) acca;

        temp1 = sub(i, max_index);
        if (temp1 > 0)
            max_index = i;
        temp = add(temp, int_region_standard_deviation_table[i]);
    }
    i = 9;
    temp1 = sub(temp, 8);
    temp2 = sub(max_index, 28);
    while ((i >= 0)  &&  ((temp1 >= 0)  ||  (temp2 > 0)))
    {
        i = sub(i, 1);
        temp = shr(temp, 1);
        max_index = sub(max_index, 2);
        temp1 = sub(temp, 8);
        temp2 = sub(max_index, 28);
    }

    *p_mag_shift = i;

    temp = (int16_t ) (REGION_POWER_TABLE_NUM_NEGATIVES + (*p_mag_shift * 2));
    for (region = 0;  region < number_of_regions;  region++)
    {
        acca = L_add(absolute_region_power_index[region], temp);
        i = (int16_t) acca;
        decoder_region_standard_deviation[region] = int_region_standard_deviation_table[i];
    }

}
/*- End of function --------------------------------------------------------*/

/* Adjust the power categories based on the categorization control */
static void rate_adjust_categories(int16_t categorization_control,
                                   int16_t *decoder_power_categories,
                                   int16_t *decoder_category_balances)
{
    int16_t i;
    int16_t region;

    i = 0;
    while (categorization_control > 0)
    {
        region = decoder_category_balances[i++];
        decoder_power_categories[region]++;
        categorization_control = sub(categorization_control, 1);
    }
}
/*- End of function --------------------------------------------------------*/

/* Decode MLT coefficients */
static void decode_vector_quantized_mlt_indices(g722_1_decode_state_t *s,
                                                int16_t number_of_regions,
                                                int16_t *decoder_region_standard_deviation,
                                                int16_t *decoder_power_categories,
                                                int16_t *decoder_mlt_coefs)
{
    const int16_t noise_fill_factor[3] =
    {
        5793, 8192, 23170
    };
    int j;
    int16_t standard_deviation;
    int16_t *decoder_mlt_ptr;
    int16_t decoder_mlt_value;
    int16_t noifillpos;
    int16_t noifillneg;
    int16_t region;
    int16_t category;
    int16_t n;
    int16_t k[MAX_VECTOR_DIMENSION];
    int16_t vec_dim;
    int16_t num_vecs;
    int16_t index;
    int16_t signs_index;
    int16_t bit;
    int16_t num_sign_bits;
    int16_t ran_out_of_bits_flag;
    int16_t random_word;
    int16_t temp;
    int32_t acca;
    const int16_t *decoder_table_ptr;

    ran_out_of_bits_flag = 0;
    for (region = 0;  region < number_of_regions;  region++)
    {
        category = (int16_t) decoder_power_categories[region];
        acca = L_mult0(region, REGION_SIZE);
        index = (int16_t) acca;
        decoder_mlt_ptr = &decoder_mlt_coefs[index];
        standard_deviation = decoder_region_standard_deviation[region];

        temp = sub(category, 7);
        if (temp < 0)
        {
            /* Get the proper table of decoder tables, vec_dim, and num_vecs for the cat */
            decoder_table_ptr = table_of_decoder_tables[category];
            vec_dim = vector_dimension[category];
            num_vecs = number_of_vectors[category];

            for (n = 0;  n < num_vecs;  n++)
            {
                index = 0;

                /* Get index */
                do
                {
                    if (s->number_of_bits_left <= 0)
                    {
                        ran_out_of_bits_flag = 1;
                        break;
                    }

                    if (g722_1_bitstream_get(&s->bitstream, &(s->code_ptr), 1) == 0)
                    {
                        temp = shl(index, 1);
                        index = (int16_t) *(decoder_table_ptr + temp);
                    }
                    else
                    {
                        temp = shl(index, 1);
                        index = (int16_t) *(decoder_table_ptr + temp + 1);
                    }
                    s->number_of_bits_left--;
                }
                while (index > 0);

                if (ran_out_of_bits_flag != 0)
                    break;

                index = negate(index);

                /* convert index into array used to access the centroid table */
                /* get the number of sign bits in the index */
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
                        acca = L_mult0(standard_deviation, mlt_quant_centroid[category][k[j]]);
                        acca = L_shr(acca, 12);
                        decoder_mlt_value = (int16_t) acca;

                        if (decoder_mlt_value != 0)
                        {
                            if ((signs_index & bit) == 0)
                                decoder_mlt_value = negate(decoder_mlt_value);
                            bit = shr(bit, 1);
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
            /* DEBUG!! - For now also redo all of last region with all noise fill. */
            if (ran_out_of_bits_flag != 0)
            {
                for (j = region + 1;  j < number_of_regions;  j++)
                    decoder_power_categories[j] = 7;
                category = 7;
                decoder_mlt_ptr = &decoder_mlt_coefs[region*REGION_SIZE];
            }
        }

        if (category == (NUM_CATEGORIES - 3)  ||  category == (NUM_CATEGORIES - 2))
        {
            decoder_mlt_ptr = &decoder_mlt_coefs[region*REGION_SIZE];
            noifillpos = mult(standard_deviation, noise_fill_factor[category - 5]);
            noifillneg = negate(noifillpos);
            random_word = get_rand(&s->randobj);

            for (j = 0;  j < 10;  j++)
            {
                if (*decoder_mlt_ptr == 0)
                {
                    *decoder_mlt_ptr = ((random_word & 1) == 0)  ?  noifillneg  :  noifillpos;
                    random_word = shr(random_word, 1);
                }
                /* pointer arithmetic */
                decoder_mlt_ptr++;
            }
            random_word = get_rand(&s->randobj);
            for (j = 0;  j < 10;  j++)
            {
                if (*decoder_mlt_ptr == 0)
                {
                    *decoder_mlt_ptr = ((random_word & 1) == 0)  ?  noifillneg  :  noifillpos;
                    random_word  = shr(random_word,1);
                }
                /* pointer arithmetic */
                decoder_mlt_ptr++;
            }
        }

        if (category == NUM_CATEGORIES - 1)
        {
            index = sub(category, 5);
            noifillpos = mult(standard_deviation, noise_fill_factor[index]);
            noifillneg = negate(noifillpos);
            random_word = get_rand(&s->randobj);
            for (j = 0;  j < 10;  j++)
            {
                *decoder_mlt_ptr++ = ((random_word & 1) == 0)  ?  noifillneg  :  noifillpos;
                random_word >>= 1;
            }
            random_word = get_rand(&s->randobj);
            for (j = 0;  j < 10;  j++)
            {
                decoder_mlt_ptr[j] = ((random_word & 1) == 0)  ?  noifillneg  :  noifillpos;
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
static int16_t index_to_array(int16_t index, int16_t *array, int16_t category)
{
    int16_t j;
    int16_t q;
    int16_t p;
    int16_t number_of_non_zero;
    int16_t max_bin_plus_one;
    int16_t inverse_of_max_bin_plus_one;

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
                                  int16_t *absolute_region_power_index)
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

static void error_handling(int16_t number_of_coefs,
                           int16_t number_of_valid_coefs,
                           int *frame_error_flag,
                           int16_t *decoder_mlt_coefs,
                           int16_t *old_decoder_mlt_coefs,
                           int16_t *p_mag_shift,
                           int16_t *p_old_mag_shift)
{
    int i;

    /* If both the current and previous frames are errored,
       set the mlt coefficients to 0. If only the current frame
       is errored, repeat the previous frame's MLT coefficients. */
    if (*frame_error_flag)
    {
        for (i = 0;  i < number_of_valid_coefs;  i++)
            decoder_mlt_coefs[i] = old_decoder_mlt_coefs[i];
        for (i = 0;  i < number_of_valid_coefs;  i++)
            old_decoder_mlt_coefs[i] = 0;
        *p_mag_shift = *p_old_mag_shift;
        *p_old_mag_shift = 0;
    }
    else
    {
        /* Store in case the next frame has errors. */
        for (i = 0;  i < number_of_valid_coefs;  i++)
            old_decoder_mlt_coefs[i] = decoder_mlt_coefs[i];
        *p_old_mag_shift = *p_mag_shift;
    }

    /* Zero out the upper 1/8 of the spectrum. */
    for (i = number_of_valid_coefs;  i < number_of_coefs;  i++)
        decoder_mlt_coefs[i] = 0;
}
/*- End of function --------------------------------------------------------*/

int g722_1_decode(g722_1_decode_state_t *s, int16_t amp[], const uint8_t g722_1_data[], int len)
{
    int16_t decoder_mlt_coefs[MAX_DCT_LENGTH];
    int16_t mag_shift;
    int i;
    int j;

    for (i = 0, j = 0;  j < len;  i += s->frame_size, j += s->number_of_bits_per_frame/8)
    {
        g722_1_bitstream_init(&s->bitstream);
        s->code_ptr = &g722_1_data[j];
        s->number_of_bits_left = s->number_of_bits_per_frame;

        /* Process the out_words into decoder_mlt_coefs */
        decoder(s,
                s->number_of_regions,
                decoder_mlt_coefs,
                &mag_shift,
                &s->old_mag_shift,
                s->old_decoder_mlt_coefs,
                0);

        /* Convert the decoder_mlt_coefs to samples */
        rmlt_coefs_to_samples(decoder_mlt_coefs, s->old_samples, amp + i, s->frame_size, mag_shift);
    }
    return i;
}
/*- End of function --------------------------------------------------------*/

int g722_1_fillin(g722_1_decode_state_t *s, int16_t amp[], const uint8_t g722_1_data[], int len)
{
    int16_t decoder_mlt_coefs[MAX_DCT_LENGTH];
    int16_t mag_shift;
    int i;
    int j;

    for (i = 0, j = 0;  j < len;  i += s->frame_size, j += s->number_of_bits_per_frame/8)
    {
        g722_1_bitstream_init(&s->bitstream);
        s->code_ptr = &g722_1_data[j];
        s->number_of_bits_left = s->number_of_bits_per_frame;

        /* Process the out_words into decoder MLT coefs */
        decoder(s,
                s->number_of_regions,
                decoder_mlt_coefs,
                &mag_shift,
                &s->old_mag_shift,
                s->old_decoder_mlt_coefs,
                1);

        /* Convert the decoder MLT coefs to samples */
        rmlt_coefs_to_samples(decoder_mlt_coefs, s->old_samples, amp + i, s->frame_size, mag_shift);
        i += s->frame_size;
        break;
    }
    return i;
}
/*- End of function --------------------------------------------------------*/
#endif

int g722_1_decode_set_rate(g722_1_decode_state_t *s, int bit_rate)
{
    if ((bit_rate < 16000)  ||  (bit_rate > 48000)  ||  ((bit_rate%800) != 0))
        return -1;
    s->bit_rate = bit_rate;
    s->number_of_bits_per_frame = (int16_t) ((s->bit_rate)/50);
    s->bytes_per_frame = s->number_of_bits_per_frame/8;
    return 0;
}
/*- End of function --------------------------------------------------------*/

g722_1_decode_state_t *g722_1_decode_init(g722_1_decode_state_t *s, int bit_rate, int sample_rate)
{
#if !defined(G722_1_USE_FIXED_POINT)
    int i;
#endif

    if ((bit_rate < 16000)  ||  (bit_rate > 48000)  ||  ((bit_rate%800) != 0))
        return NULL;
    if (sample_rate != G722_1_SAMPLE_RATE_16000  &&  sample_rate != G722_1_SAMPLE_RATE_32000)
        return NULL;

    if (s == NULL)
    {
        if ((s = (g722_1_decode_state_t *) malloc(sizeof(*s))) == NULL)
            return NULL;
    }
    memset(s, 0, sizeof(*s));
#if !defined(G722_1_USE_FIXED_POINT)
    /* Initialize the coefs history */
    for (i = 0;  i < s->frame_size;  i++)
        s->old_decoder_mlt_coefs[i] = 0.0f;
    for (i = 0;  i < (s->frame_size >> 1);  i++)
        s->old_samples[i] = 0.0f;
#endif
    s->sample_rate = sample_rate;
    if (sample_rate == G722_1_SAMPLE_RATE_16000)
    {
        s->number_of_regions = NUMBER_OF_REGIONS;
        s->frame_size = MAX_FRAME_SIZE >> 1;
    }
    else
    {
        s->number_of_regions = MAX_NUMBER_OF_REGIONS;
        s->frame_size = MAX_FRAME_SIZE;
    }
    s->bit_rate = bit_rate;
    s->number_of_bits_per_frame = s->bit_rate/50;
    s->bytes_per_frame = s->number_of_bits_per_frame/8;

    /* Initialize the random number generator */
    s->randobj.seed0 = 1;
    s->randobj.seed1 = 1;
    s->randobj.seed2 = 1;
    s->randobj.seed3 = 1;

    return s;
}
/*- End of function --------------------------------------------------------*/

int g722_1_decode_release(g722_1_decode_state_t *s)
{
    free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
