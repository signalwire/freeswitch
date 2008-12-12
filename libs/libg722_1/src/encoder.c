/*
 * g722_1 - a library for the G.722.1 and Annex C codecs
 *
 * encoder.c
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
 * $Id: encoder.c,v 1.26 2008/11/21 15:30:22 steveu Exp $
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
#include "huff_tab.h"
#include "tables.h"
#include "bitstream.h"

#if defined(G722_1_USE_FIXED_POINT)

static int16_t compute_region_powers(int16_t *mlt_coefs,
                                     int16_t mag_shift,
                                     int16_t *drp_num_bits,
                                     uint16_t *drp_code_bits,
                                     int16_t *absolute_region_power_index,
                                     int16_t number_of_regions);

static void vector_quantize_mlts(int16_t number_of_available_bits,
                                 int16_t number_of_regions,
                                 int16_t num_categorization_control_possibilities,
                                 int16_t *mlt_coefs,
                                 int16_t *absolute_region_power_index,
                                 int16_t *power_categories,
                                 int16_t *category_balances,
                                 int16_t *p_categorization_control,
                                 int16_t *region_mlt_bit_counts,
                                 uint32_t *region_mlt_bits);

static int16_t vector_huffman(int16_t category,
                              int16_t power_index,
                              int16_t *raw_mlt_ptr,
                              uint32_t *word_ptr);

static void bits_to_words(g722_1_encode_state_t *s,
                          uint32_t *region_mlt_bits,
                          int16_t *region_mlt_bit_counts,
                          int16_t *drp_num_bits,
                          uint16_t *drp_code_bits,
                          uint8_t *out_code,
                          int16_t categorization_control,
                          int16_t number_of_regions,
                          int16_t num_categorization_control_bits,
                          int16_t number_of_bits_per_frame);

static void encoder(g722_1_encode_state_t *s,
                    int16_t number_of_available_bits,
                    int16_t number_of_regions,
                    int16_t *mlt_coefs,
                    int16_t mag_shift,
                    uint8_t *g722_1_data);

/* Stuff the bits into words for output */
static void bits_to_words(g722_1_encode_state_t *s,
                          uint32_t *region_mlt_bits,
                          int16_t *region_mlt_bit_counts,
                          int16_t *drp_num_bits,
                          uint16_t *drp_code_bits,
                          uint8_t *out_code,
                          int16_t categorization_control,
                          int16_t number_of_regions,
                          int16_t num_categorization_control_bits,
                          int16_t number_of_bits_per_frame)
{
    int16_t region;
    int16_t region_bit_count;
    uint32_t *in_word_ptr;
    uint32_t current_code;
    int16_t current_bits;
    int16_t bit_count;

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
                    int16_t number_of_available_bits,
                    int16_t number_of_regions,
                    int16_t *mlt_coefs,
                    int16_t mag_shift,
                    uint8_t *g722_1_data)
{
    int16_t num_categorization_control_bits;
    int16_t num_categorization_control_possibilities;
    int16_t number_of_bits_per_frame;
    int16_t number_of_envelope_bits;
    int16_t categorization_control;
    int16_t region;
    int16_t absolute_region_power_index[MAX_NUMBER_OF_REGIONS];
    int16_t power_categories[MAX_NUMBER_OF_REGIONS];
    int16_t category_balances[MAX_NUM_CATEGORIZATION_CONTROL_POSSIBILITIES - 1];
    int16_t drp_num_bits[MAX_NUMBER_OF_REGIONS + 1];
    uint16_t drp_code_bits[MAX_NUMBER_OF_REGIONS + 1];
    int16_t region_mlt_bit_counts[MAX_NUMBER_OF_REGIONS];
    uint32_t region_mlt_bits[4*MAX_NUMBER_OF_REGIONS];
    int16_t mag_shift_offset;
    int16_t temp;

    /* Initialize variables */
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

    for (region = 0;  region < number_of_regions;  region++)
        region_mlt_bit_counts[region] = 0;

    /* Estimate power envelope. */
    number_of_envelope_bits = compute_region_powers(mlt_coefs,
                                                    mag_shift,
                                                    drp_num_bits,
                                                    drp_code_bits,
                                                    absolute_region_power_index,
                                                    number_of_regions);

    /* Adjust number of available bits based on power envelope estimate */
    temp = sub(number_of_available_bits, number_of_envelope_bits);
    number_of_available_bits = sub(temp, num_categorization_control_bits);

    /* Get categorizations */
    categorize(number_of_available_bits,
               number_of_regions,
               num_categorization_control_possibilities,
               absolute_region_power_index,
               power_categories,
               category_balances);

    /* Adjust absolute_region_category_index[] for mag_shift.
       This assumes that REGION_POWER_STEPSIZE_DB is defined
       to be exactly 3.010299957 or 20.0 times log base 10
       of square root of 2. */
    mag_shift_offset = (mag_shift << 1) + REGION_POWER_TABLE_NUM_NEGATIVES;

    for (region = 0;  region < number_of_regions;  region++)
        absolute_region_power_index[region] += mag_shift_offset;

    /* Adjust the absolute power region index based on the mlt coefs */
    adjust_abs_region_power_index(absolute_region_power_index, mlt_coefs, number_of_regions);

    /* Quantize and code the mlt coefficients based on categorizations */
    vector_quantize_mlts(number_of_available_bits,
                         number_of_regions,
                         num_categorization_control_possibilities,
                         mlt_coefs,
                         absolute_region_power_index,
                         power_categories,
                         category_balances,
                         &categorization_control,
                         region_mlt_bit_counts,
                         region_mlt_bits);

    /* Stuff bits into words */
    bits_to_words(s,
                  region_mlt_bits,
                  region_mlt_bit_counts,
                  drp_num_bits,
                  drp_code_bits,
                  g722_1_data,
                  categorization_control,
                  number_of_regions,
                  num_categorization_control_bits,
                  number_of_bits_per_frame);
}
/*- End of function --------------------------------------------------------*/

/* Adjust the absolute power index */
void adjust_abs_region_power_index(int16_t *absolute_region_power_index,
                                   int16_t *mlt_coefs,
                                   int16_t number_of_regions)
{
    int16_t n;
    int16_t i;
    int16_t region;
    int16_t *raw_mlt_ptr;
    int32_t acca;
    int16_t temp;

    for (region = 0;  region < number_of_regions;  region++)
    {
        n = sub(absolute_region_power_index[region], 39);
        n = shr(n, 1);
        if (n > 0)
        {
            temp = (int16_t) L_mult0(region, REGION_SIZE);

            raw_mlt_ptr = &mlt_coefs[temp];

            for (i = 0;  i < REGION_SIZE;  i++)
            {
                acca = L_shl(*raw_mlt_ptr, 16);
                acca = L_add(acca, 32768L);
                acca = L_shr(acca, n);
                acca = L_shr(acca, 16);
                *raw_mlt_ptr++ = (int16_t) acca;
            }

            temp = sub(absolute_region_power_index[region], shl(n, 1));
            absolute_region_power_index[region] = temp;
        }
    }
}
/*- End of function --------------------------------------------------------*/

/* Compute the power for each of the regions */
static int16_t compute_region_powers(int16_t *mlt_coefs,
                                     int16_t mag_shift,
                                     int16_t *drp_num_bits,
                                     uint16_t *drp_code_bits,
                                     int16_t *absolute_region_power_index,
                                     int16_t number_of_regions)
{
    int16_t *input_ptr;
    int32_t long_accumulator;
    int16_t itemp1;
    int16_t power_shift;
    int16_t region;
    int16_t j;
    int16_t differential_region_power_index[MAX_NUMBER_OF_REGIONS];
    int16_t number_of_bits;
    int32_t acca;
    int16_t temp;
    int16_t temp1;
    int16_t temp2;

    input_ptr = mlt_coefs;
    for (region = 0;  region < number_of_regions;  region++)
    {
        long_accumulator = 0;
        for (j = 0;  j < REGION_SIZE;  j++)
        {
            itemp1 = *input_ptr++;
            long_accumulator = L_mac0(long_accumulator, itemp1, itemp1);
        }

        power_shift = 0;
        acca = long_accumulator & 0x7FFF0000L;
        while (acca > 0)
        {
            long_accumulator = L_shr(long_accumulator, 1);
            acca = long_accumulator & 0x7FFF0000L;
            power_shift = add(power_shift, 1);
        }

        acca = L_sub(long_accumulator, 32767);
        temp = add(power_shift, 15);
        while (acca <= 0  &&  temp >= 0)
        {
            long_accumulator = L_shl(long_accumulator, 1);
            acca = L_sub(long_accumulator, 32767);
            power_shift--;
            temp = add(power_shift, 15);
        }
        long_accumulator = L_shr(long_accumulator, 1);
        /* 28963 corresponds to square root of 2 times REGION_SIZE(20). */
        acca = L_sub(long_accumulator, 28963);

        if (acca >= 0)
            power_shift = add(power_shift, 1);

        acca = mag_shift;
        acca = L_shl(acca, 1);
        acca = L_sub(power_shift, acca);
        acca = L_add(35, acca);
        acca = L_sub(acca, REGION_POWER_TABLE_NUM_NEGATIVES);
        absolute_region_power_index[region] = (int16_t) acca;
    }

    /* Before we differentially encode the quantized region powers, adjust upward the
       valleys to make sure all the peaks can be accurately represented. */
    temp = sub(number_of_regions, 2);

    for (region = temp;  region >= 0;  region--)
    {
        temp1 = sub(absolute_region_power_index[region + 1], DRP_DIFF_MAX);
        temp2 = sub(absolute_region_power_index[region], temp1);
        if (temp2 < 0)
            absolute_region_power_index[region] = temp1;
    }

    /* The MLT is currently scaled too low by the factor
       ENCODER_SCALE_FACTOR(=18318)/32768 * (1./sqrt(160).
       This is the ninth power of 1 over the square root of 2.
       So later we will add ESF_ADJUSTMENT_TO_RMS_INDEX (now 9)
       to drp_code_bits[0]. */

    /* drp_code_bits[0] can range from 1 to 31. 0 is used only as an escape sequence. */
    temp1 = sub(1, ESF_ADJUSTMENT_TO_RMS_INDEX);
    temp2 = sub(absolute_region_power_index[0], temp1);
    if (temp2 < 0)
        absolute_region_power_index[0] = temp1;

    temp1 = sub(31, ESF_ADJUSTMENT_TO_RMS_INDEX);
    temp2 = sub(absolute_region_power_index[0], temp1);
    if (temp2 > 0)
        absolute_region_power_index[0] = temp1;
    differential_region_power_index[0] = absolute_region_power_index[0];

    number_of_bits = 5;
    drp_num_bits[0] = 5;
    drp_code_bits[0] = (uint16_t) add(absolute_region_power_index[0], ESF_ADJUSTMENT_TO_RMS_INDEX);

    /* Lower limit the absolute region power indices to -8 and upper limit them to 31. Such extremes
       may be mathematically impossible anyway. */
    for (region = 1;  region < number_of_regions;  region++)
    {
        temp1 = -8 - ESF_ADJUSTMENT_TO_RMS_INDEX;
        if (absolute_region_power_index[region] < temp1)
            absolute_region_power_index[region] = temp1;

        temp1 = 31 - ESF_ADJUSTMENT_TO_RMS_INDEX;
        if (absolute_region_power_index[region] > temp1)
            absolute_region_power_index[region] = temp1;
    }

    for (region = 1;  region < number_of_regions;  region++)
    {
        j = sub(absolute_region_power_index[region], absolute_region_power_index[region - 1]);
        if (j < DRP_DIFF_MIN)
            j = DRP_DIFF_MIN;
        j -= DRP_DIFF_MIN;
        differential_region_power_index[region] = j;

        temp = absolute_region_power_index[region - 1] + differential_region_power_index[region];
        temp += DRP_DIFF_MIN;
        absolute_region_power_index[region] = temp;

        number_of_bits += differential_region_power_bits[region][j];
        drp_num_bits[region] = differential_region_power_bits[region][j];
        drp_code_bits[region] = differential_region_power_codes[region][j];
    }

    return number_of_bits;
}
/*- End of function --------------------------------------------------------*/

/* Scalar quantized vector Huffman coding (SQVH) */
static void vector_quantize_mlts(int16_t number_of_available_bits,
                                 int16_t number_of_regions,
                                 int16_t num_categorization_control_possibilities,
                                 int16_t *mlt_coefs,
                                 int16_t *absolute_region_power_index,
                                 int16_t *power_categories,
                                 int16_t *category_balances,
                                 int16_t *p_categorization_control,
                                 int16_t *region_mlt_bit_counts,
                                 uint32_t *region_mlt_bits)
{
    int16_t *raw_mlt_ptr;
    int16_t region;
    int16_t category;
    int16_t total_mlt_bits;
    int16_t temp;

    total_mlt_bits = 0;
    /* Start in the middle of the categorization control range. */
    temp = (num_categorization_control_possibilities >> 1) - 1;
    for (*p_categorization_control = 0;  *p_categorization_control < temp;  (*p_categorization_control)++)
    {
        region = category_balances[*p_categorization_control];
        power_categories[region]++;
    }

    for (region = 0;  region < number_of_regions;  region++)
    {
        category = power_categories[region];
        temp = (int16_t) L_mult0(region, REGION_SIZE);
        raw_mlt_ptr = &mlt_coefs[temp];
        if (category < (NUM_CATEGORIES - 1))
        {
            region_mlt_bit_counts[region] = vector_huffman(category,
                                                           absolute_region_power_index[region],
                                                           raw_mlt_ptr,
                                                           &region_mlt_bits[shl(region, 2)]);
        }
        else
        {
            region_mlt_bit_counts[region] = 0;
        }
        total_mlt_bits += region_mlt_bit_counts[region];
    }

    /* If too few bits... */
    while (total_mlt_bits < number_of_available_bits
           &&
           *p_categorization_control > 0)
    {
        (*p_categorization_control)--;
        region = category_balances[*p_categorization_control];
        power_categories[region]--;
        total_mlt_bits -= region_mlt_bit_counts[region];
        category = power_categories[region];
        raw_mlt_ptr = &mlt_coefs[region*REGION_SIZE];

        if (category < (NUM_CATEGORIES - 1))
        {
            region_mlt_bit_counts[region] = vector_huffman(category,
                                                           absolute_region_power_index[region],
                                                           raw_mlt_ptr,
                                                           &region_mlt_bits[shl(region, 2)]);
        }
        else
        {
            region_mlt_bit_counts[region] = 0;
        }
        total_mlt_bits += region_mlt_bit_counts[region];
    }

    /* If too many bits... */
    /* Set up for while loop test */
    while (total_mlt_bits > number_of_available_bits 
           &&
           *p_categorization_control < (num_categorization_control_possibilities - 1))
    {
        region = category_balances[*p_categorization_control];
        power_categories[region]++;
        total_mlt_bits -= region_mlt_bit_counts[region];
        category = power_categories[region];
        temp = (int16_t) L_mult0(region, REGION_SIZE);
        raw_mlt_ptr = &mlt_coefs[temp];
        if (category < (NUM_CATEGORIES - 1))
        {
            region_mlt_bit_counts[region] = vector_huffman(category,
                                                           absolute_region_power_index[region],
                                                           raw_mlt_ptr,
                                                           &region_mlt_bits[shl(region, 2)]);
        }
        else
        {
            region_mlt_bit_counts[region] = 0;
        }
        total_mlt_bits += region_mlt_bit_counts[region];
        (*p_categorization_control)++;
    }
}
/*- End of function --------------------------------------------------------*/

/* Huffman encoding for each region based on category and power_index */
static int16_t vector_huffman(int16_t category,
                              int16_t power_index,
                              int16_t *raw_mlt_ptr,
                              uint32_t *word_ptr)
{
    int16_t inv_of_step_size_times_std_dev;
    int16_t j;
    int16_t n;
    int16_t k;
    int16_t number_of_region_bits;
    int16_t number_of_non_zero;
    int16_t vec_dim;
    int16_t num_vecs;
    int16_t kmax;
    int16_t kmax_plus_one;
    int16_t index,signs_index;
    const int16_t *bitcount_table_ptr;
    const uint16_t *code_table_ptr;
    int32_t code_bits;
    int16_t number_of_code_bits;
    uint32_t current_word;
    int16_t current_word_bits_free;
    int32_t acca;
    int32_t accb;
    int16_t temp;
    int16_t mytemp;
    int16_t myacca;

    /* Initialize variables */
    vec_dim = vector_dimension[category];
    num_vecs = number_of_vectors[category];
    kmax = max_bin[category];
    kmax_plus_one = add(kmax, 1);
    current_word = 0L;
    current_word_bits_free = 32;
    number_of_region_bits = 0;
    /* Set up table pointers */
    bitcount_table_ptr = table_of_bitcount_tables[category];
    code_table_ptr = table_of_code_tables[category];

    /* Compute inverse of step size * standard deviation */
    acca = L_mult(step_size_inverse_table[category], standard_deviation_inverse_table[power_index]);
    acca = L_shr(acca, 1);
    acca = L_add(acca, 4096);
    acca = L_shr(acca, 13);

    mytemp = acca & 0x3;
    acca = L_shr(acca, 2);

    inv_of_step_size_times_std_dev = (int16_t) acca;

    for (n = 0;  n < num_vecs;  n++)
    {
        index = 0;
        signs_index = 0;
        number_of_non_zero = 0;
        for (j = 0;  j < vec_dim;  j++)
        {
            k = abs_s(*raw_mlt_ptr);
            acca = L_mult(k, inv_of_step_size_times_std_dev);
            acca = L_shr(acca, 1);

            myacca = (int16_t) L_mult(k, mytemp);
            myacca = (int16_t) L_shr(myacca, 1);
            myacca = (int16_t) L_add(myacca, int_dead_zone_low_bits[category]);
            myacca = (int16_t) L_shr(myacca, 2);

            acca = L_add(acca, int_dead_zone[category]);
            acca = L_add(acca, myacca);
            acca = L_shr(acca, 13);
            k = (int16_t) acca;
            if (k != 0)
            {
                number_of_non_zero = add(number_of_non_zero, 1);
                signs_index = shl(signs_index, 1);
                if (*raw_mlt_ptr > 0)
                    signs_index = add(signs_index, 1);
                temp = sub(k, kmax);
                if (temp > 0)
                    k = kmax;
            }
            acca = L_shr(L_mult(index, (kmax_plus_one)), 1);
            index = (int16_t) acca;
            index = add(index, k);
            raw_mlt_ptr++;
        }

        code_bits = *(code_table_ptr + index);
        number_of_code_bits = add((*(bitcount_table_ptr + index)), number_of_non_zero);
        number_of_region_bits = add(number_of_region_bits, number_of_code_bits);

        acca = code_bits << number_of_non_zero;
        accb = signs_index;
        acca = L_add(acca, accb);
        code_bits = acca;

        /* MSB of codebits is transmitted first. */
        j = current_word_bits_free - number_of_code_bits;
        if (j >= 0)
        {
            acca = code_bits << j;
            current_word = L_add(current_word, acca);
            current_word_bits_free = j;
        }
        else
        {
            j = negate(j);
            acca = L_shr(code_bits, j);
            current_word = L_add(current_word, acca);

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
    int16_t mlt_coefs[MAX_FRAME_SIZE];
    int16_t mag_shift;
    int i;
    int j;

    for (i = 0, j = 0;  i < len;  i += s->frame_size, j += s->bytes_per_frame)
    {
        mag_shift = samples_to_rmlt_coefs(amp + i, s->history, mlt_coefs, s->frame_size);
        encoder(s,
                s->number_of_bits_per_frame,
                s->number_of_regions,
                mlt_coefs,
                (uint16_t) mag_shift,
                &g722_1_data[j]);
    }
    return j;
}
/*- End of function --------------------------------------------------------*/
#endif

int g722_1_encode_set_rate(g722_1_encode_state_t *s, int bit_rate)
{
    if ((bit_rate < 16000)  ||  (bit_rate > 48000)  ||  ((bit_rate%800) != 0))
        return -1;
    s->bit_rate = bit_rate;
    s->number_of_bits_per_frame = s->bit_rate/50;
    s->bytes_per_frame = s->number_of_bits_per_frame/8;
    return 0;
}
/*- End of function --------------------------------------------------------*/

g722_1_encode_state_t *g722_1_encode_init(g722_1_encode_state_t *s, int bit_rate, int sample_rate)
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
        if ((s = (g722_1_encode_state_t *) malloc(sizeof(*s))) == NULL)
            return NULL;
    }
    memset(s, 0, sizeof(*s));
#if !defined(G722_1_USE_FIXED_POINT)
    for (i = 0;  i < MAX_FRAME_SIZE;  i++)
        s->history[i] = 0.0f;
    /* Scaling factor for fixed point interop */
    if (sample_rate == G722_1_SAMPLE_RATE_16000)
        s->scale_factor = 1.0f/INTEROP_RMLT_SCALE_FACTOR_7;
    else
        s->scale_factor = 1.0f/INTEROP_RMLT_SCALE_FACTOR_14;
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
    s->number_of_bits_per_frame = (int16_t) ((s->bit_rate)/50);
    s->bytes_per_frame = s->number_of_bits_per_frame/8;
    return s;
}
/*- End of function --------------------------------------------------------*/

int g722_1_encode_release(g722_1_encode_state_t *s)
{
    free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
