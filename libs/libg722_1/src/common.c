/*
 * g722_1 - a library for the G.722.1 and Annex C codecs
 *
 * common.c
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
 * $Id: common.c,v 1.6 2008/09/30 14:06:39 steveu Exp $
 */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include <config.h>
#endif

#include <inttypes.h>
#include <stdlib.h>

#include "g722_1/g722_1.h"

#include "defs.h"
#include "huff_tab.h"
#include "tables.h"

#if defined(G722_1_USE_FIXED_POINT)

static void compute_raw_pow_categories(int16_t *power_categories,
                                       int16_t *rms_index,
                                       int16_t number_of_regions,
                                       int16_t offset);

/****************************************************************************************
 Function:    categorize

 Syntax:      void categorize(int16_t number_of_available_bits,
                              int16_t number_of_regions,
                              int16_t num_categorization_control_possibilities,
                              int16_t rms_index,
                              int16_t power_categories,
                              int16_t category_balances)

                  inputs:   number_of_regions
                            num_categorization_control_possibilities
                            number_of_available_bits
                            rms_index[MAX_NUMBER_OF_REGIONS]

                  outputs:  power_categories[MAX_NUMBER_OF_REGIONS]
                            category_balances[MAX_NUM_CATEGORIZATION_CONTROL_POSSIBILITIES-1]

 Description: Computes a series of categorizations

 WMOPS:     7kHz |    24kbit    |     32kbit
          -------|--------------|----------------
            AVG  |    0.14      |     0.14
          -------|--------------|----------------
            MAX  |    0.15      |     0.15
          -------|--------------|----------------

           14kHz |    24kbit    |     32kbit     |     48kbit
          -------|--------------|----------------|----------------
            AVG  |    0.42      |     0.45       |     0.48
          -------|--------------|----------------|----------------
            MAX  |    0.47      |     0.52       |     0.52
          -------|--------------|----------------|----------------

****************************************************************************************/
void categorize(int16_t number_of_available_bits,
                int16_t number_of_regions,
                int16_t num_categorization_control_possibilities,
                int16_t *rms_index,
                int16_t *power_categories,
                int16_t *category_balances)
{

    int16_t offset;
    int16_t temp;
    int16_t frame_size;

    /* At higher bit rates, there is an increase for most categories in average bit
       consumption per region. We compensate for this by pretending we have fewer
       available bits. */
    if (number_of_regions == NUMBER_OF_REGIONS)
        frame_size = DCT_LENGTH;
    else
        frame_size = MAX_DCT_LENGTH;

    temp = sub(number_of_available_bits, frame_size);
    if (temp > 0)
    {
        number_of_available_bits = sub(number_of_available_bits, frame_size);
        number_of_available_bits = (int16_t) L_mult0(number_of_available_bits, 5);
        number_of_available_bits = shr(number_of_available_bits, 3);
        number_of_available_bits = add(number_of_available_bits, frame_size);
    }

    /* calculate the offset using the original category assignments */
    offset = calc_offset(rms_index, number_of_regions, number_of_available_bits);

    /* compute the power categories based on the uniform offset */
    compute_raw_pow_categories(power_categories, rms_index, number_of_regions,offset);

    /* adjust the category assignments */
    /* compute the new power categories and category balances */
    comp_powercat_and_catbalance(power_categories ,category_balances, rms_index, number_of_available_bits, number_of_regions, num_categorization_control_possibilities, offset);
}
/*- End of function --------------------------------------------------------*/

/***************************************************************************
 Function:    comp_powercat_and_catbalance

 Syntax:      void comp_powercat_and_catbalance(int16_t *power_categories,
                                                int16_t *category_balances,
                                                int16_t *rms_index,
                                                int16_t number_of_available_bits,
                                                int16_t number_of_regions,
                                                int16_t num_categorization_control_possibilities,
                                                int16_t offset)


                inputs:   *rms_index
                          number_of_available_bits
                          number_of_regions
                          num_categorization_control_possibilities
                          offset

                outputs:  *power_categories
                          *category_balances


 Description: Computes the power_categories and the category balances

 WMOPS:     7kHz |    24kbit    |     32kbit
          -------|--------------|----------------
            AVG  |    0.10      |     0.10
          -------|--------------|----------------
            MAX  |    0.11      |     0.11
          -------|--------------|----------------

           14kHz |    24kbit    |     32kbit     |     48kbit
          -------|--------------|----------------|----------------
            AVG  |    0.32      |     0.35       |     0.38
          -------|--------------|----------------|----------------
            MAX  |    0.38      |     0.42       |     0.43
          -------|--------------|----------------|----------------

***************************************************************************/
void comp_powercat_and_catbalance(int16_t *power_categories,
                                  int16_t *category_balances,
                                  int16_t *rms_index,
                                  int16_t number_of_available_bits,
                                  int16_t number_of_regions,
                                  int16_t num_categorization_control_possibilities,
                                  int16_t offset)
{

    int16_t expected_number_of_code_bits;
    int16_t region;
    int16_t max_region;
    int16_t j;
    int16_t max_rate_categories[MAX_NUMBER_OF_REGIONS];
    int16_t min_rate_categories[MAX_NUMBER_OF_REGIONS];
    int16_t temp_category_balances[2*MAX_NUM_CATEGORIZATION_CONTROL_POSSIBILITIES];
    int16_t raw_max;
    int16_t raw_min;
    int16_t raw_max_index;
    int16_t raw_min_index;
    int16_t max_rate_pointer;
    int16_t min_rate_pointer;
    int16_t max;
    int16_t min;
    int16_t itemp0;
    int16_t itemp1;
    int16_t min_plus_max;
    int16_t two_x_number_of_available_bits;
    int16_t temp;

    expected_number_of_code_bits = 0;
    raw_max_index = 0;
    raw_min_index = 0;

    for (region = 0;  region < number_of_regions;  region++)
        expected_number_of_code_bits = add(expected_number_of_code_bits, expected_bits_table[power_categories[region]]);


    for (region = 0;  region < number_of_regions;  region++)
    {
        max_rate_categories[region] = power_categories[region];
        min_rate_categories[region] = power_categories[region];
    }

    max = expected_number_of_code_bits;
    min = expected_number_of_code_bits;
    max_rate_pointer = num_categorization_control_possibilities;
    min_rate_pointer = num_categorization_control_possibilities;

    for (j = 0;  j < num_categorization_control_possibilities - 1;  j++)
    {
        min_plus_max = add(max, min);
        two_x_number_of_available_bits = shl(number_of_available_bits, 1);

        temp = sub(min_plus_max, two_x_number_of_available_bits);
        if (temp <= 0)
        {
            raw_min = 99;
            /* Search from lowest freq regions to highest for best */
            /* region to reassign to a higher bit rate category.   */
            for (region = 0;  region < number_of_regions;  region++)
            {
                if (max_rate_categories[region] > 0)
                {
                    itemp0 = shl(max_rate_categories[region], 1);
                    itemp1 = sub(offset, rms_index[region]);
                    itemp0 = sub(itemp1, itemp0);

                    temp = sub(itemp0, raw_min);
                    if (temp < 0)
                    {
                        raw_min = itemp0;
                        raw_min_index = region;
                    }
                }
            }
            max_rate_pointer = sub(max_rate_pointer, 1);
            temp_category_balances[max_rate_pointer] = raw_min_index;

            max = sub(max,expected_bits_table[max_rate_categories[raw_min_index]]);
            max_rate_categories[raw_min_index] = sub(max_rate_categories[raw_min_index], 1);

            max = add(max,expected_bits_table[max_rate_categories[raw_min_index]]);
        }
        else
        {
            raw_max = -99;
            /* Search from highest freq regions to lowest for best region to reassign to
            a lower bit rate category. */
            max_region = sub(number_of_regions, 1);
            for (region = max_region;  region >= 0;  region--)
            {
                temp = sub(min_rate_categories[region], (NUM_CATEGORIES - 1));
                if (temp < 0)
                {
                    itemp0 = shl(min_rate_categories[region], 1);
                    itemp1 = sub(offset, rms_index[region]);
                    itemp0 = sub(itemp1, itemp0);

                    temp = sub(itemp0, raw_max);
                    if (temp > 0)
                    {
                        raw_max = itemp0;
                        raw_max_index = region;
                    }
                }
            }
            temp_category_balances[min_rate_pointer] = raw_max_index;
            min_rate_pointer = add(min_rate_pointer, 1);
            min = sub(min, expected_bits_table[min_rate_categories[raw_max_index]]);

            min_rate_categories[raw_max_index] = add(min_rate_categories[raw_max_index], 1);
            min = add(min, expected_bits_table[min_rate_categories[raw_max_index]]);
        }
    }

    for (region = 0;  region < number_of_regions;  region++)
        power_categories[region] = max_rate_categories[region];

    for (j = 0;  j < num_categorization_control_possibilities - 1;  j++)
        category_balances[j] = temp_category_balances[max_rate_pointer++];
}
/*- End of function --------------------------------------------------------*/

/***************************************************************************
 Function:    calc_offset

 Syntax:      offset=calc_offset(int16_t *rms_index,int16_t number_of_regions,int16_t available_bits)

                input:  int16_t *rms_index
                        int16_t number_of_regions
                        int16_t available_bits

                output: int16_t offset

 Description: Calculates the the category offset.  This is the shift required
              To get the most out of the number of available bits.  A binary
              type search is used to find the offset.

 WMOPS:     7kHz |    24kbit    |     32kbit
          -------|--------------|----------------
            AVG  |    0.04      |     0.04
          -------|--------------|----------------
            MAX  |    0.04      |     0.04
          -------|--------------|----------------

           14kHz |    24kbit    |     32kbit     |     48kbit
          -------|--------------|----------------|----------------
            AVG  |    0.08      |     0.08       |     0.08
          -------|--------------|----------------|----------------
            MAX  |    0.09      |     0.09       |     0.09
          -------|--------------|----------------|----------------

***************************************************************************/
int16_t calc_offset(int16_t *rms_index,int16_t number_of_regions,int16_t available_bits)
{
    int16_t answer;
    int16_t delta;
    int16_t test_offset;
    int16_t region,j;
    int16_t power_cats[MAX_NUMBER_OF_REGIONS];
    int16_t bits;
    int16_t offset;
    int16_t temp;

    /* initialize vars */
    answer = -32;
    delta = 32;

    do
    {
        test_offset = add(answer, delta);

        /* obtain a category for each region */
        /* using the test offset             */
        for (region = 0;  region < number_of_regions;  region++)
        {
            j = sub(test_offset, rms_index[region]);
            j = shr(j, 1);

            /* Ensure j is between 0 and NUM_CAT-1 */
            if (j < 0)
                j = 0;
            temp = sub(j, NUM_CATEGORIES - 1);
            if (temp > 0)
                j = sub(NUM_CATEGORIES, 1);
            power_cats[region] = j;
        }
        bits = 0;

        /* compute the number of bits that will be used given the cat assignments */
        for (region = 0;  region < number_of_regions;  region++)
            bits = add(bits, expected_bits_table[power_cats[region]]);

        /* If (bits > available_bits - 32) then divide the offset region for the bin search */
        offset = sub(available_bits, 32);
        temp = sub(bits, offset);
        if (temp >= 0)
            answer = test_offset;
        delta = shr(delta, 1);
    }
    while (delta > 0);

    return answer;
}
/*- End of function --------------------------------------------------------*/

/***************************************************************************
 Function:    compute_raw_pow_categories

 Syntax:      void compute_raw_pow_categories(int16_t *power_categories,
                                              int16_t *rms_index,
                                              int16_t number_of_regions,
                                              int16_t offset)
              inputs:  *rms_index
                       number_of_regions
                       offset

              outputs: *power_categories



 Description: This function computes the power categories given the offset
              This is kind of redundant since they were already computed
              in calc_offset to determine the offset.

 WMOPS:          |    24kbit    |     32kbit
          -------|--------------|----------------
            AVG  |    0.01      |     0.01
          -------|--------------|----------------
            MAX  |    0.01      |     0.01
          -------|--------------|----------------

           14kHz |    24kbit    |     32kbit     |     48kbit
          -------|--------------|----------------|----------------
            AVG  |    0.01      |     0.01       |     0.01
          -------|--------------|----------------|----------------
            MAX  |    0.01      |     0.01       |     0.01
          -------|--------------|----------------|----------------

***************************************************************************/
static void compute_raw_pow_categories(int16_t *power_categories, int16_t *rms_index, int16_t number_of_regions, int16_t offset)
{
    int16_t region;
    int16_t j;
    int16_t temp;

    for (region = 0;  region < number_of_regions;  region++)
    {
        j = sub(offset, rms_index[region]);
        j = shr(j, 1);

        /* make sure j is between 0 and NUM_CAT-1 */
        if (j < 0)
            j = 0;
        temp = sub(j, (NUM_CATEGORIES - 1));
        if (temp > 0)
            j = sub(NUM_CATEGORIES, 1);

        power_categories[region] = j;
    }
}
/*- End of function --------------------------------------------------------*/
#endif
/*- End of file ------------------------------------------------------------*/
