/*
 * g722_1 - a library for the G.722.1 and Annex C codecs
 *
 * commonf.c
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
 * $Id: commonf.c,v 1.11 2008/09/30 14:06:39 steveu Exp $
 */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include <config.h>
#endif

#include <inttypes.h>
#include <stdlib.h>
#include <math.h>

#include "g722_1/g722_1.h"

#include "defs.h"
#include "tables.h"
#include "huff_tab.h"

#if !defined(G722_1_USE_FIXED_POINT)

/****************************************************************************************
 Description: Computes a series of categorizations    
****************************************************************************************/
void categorize(int number_of_regions,
                int number_of_available_bits,
                int rms_index[MAX_NUMBER_OF_REGIONS],
                int power_categories[MAX_NUMBER_OF_REGIONS],
                int category_balances[MAX_NUM_CATEGORIZATION_CONTROL_POSSIBILITIES - 1])
{
    int region;
    int i;
    int expected_number_of_code_bits;
    int delta;
    int offset;
    int test_offset;
    int num_categorization_control_possibilities;
    int max_rate_categories[MAX_NUMBER_OF_REGIONS];
    int min_rate_categories[MAX_NUMBER_OF_REGIONS];
    int temp_category_balances[2*MAX_NUM_CATEGORIZATION_CONTROL_POSSIBILITIES];
    int raw_max;
    int raw_min;
    int raw_max_index;
    int raw_min_index;
    int max_rate_pointer;
    int min_rate_pointer;
    int max;
    int min;
    int itemp0;

    if (number_of_regions == NUMBER_OF_REGIONS)
        num_categorization_control_possibilities = NUM_CATEGORIZATION_CONTROL_POSSIBILITIES;
    else
        num_categorization_control_possibilities = MAX_NUM_CATEGORIZATION_CONTROL_POSSIBILITIES;

    /* At higher bit rates, there is an increase for most categories in average bit
       consumption per region. We compensate for this by pretending we have fewer
       available bits. 
    */
    if (number_of_regions == NUMBER_OF_REGIONS) 
    {
        if (number_of_available_bits > FRAME_SIZE)
            number_of_available_bits = FRAME_SIZE + (((number_of_available_bits - FRAME_SIZE)*5) >> 3);
    }
    else if (number_of_regions == MAX_NUMBER_OF_REGIONS)
    {
        if (number_of_available_bits > MAX_FRAME_SIZE)
            number_of_available_bits = MAX_FRAME_SIZE + (((number_of_available_bits - MAX_FRAME_SIZE)*5) >> 3);
    }

    offset = -32;
    delta = 32;
    do
    {
        test_offset = offset + delta;
        for (region = 0;  region < number_of_regions;  region++)
        {
            i = (test_offset - rms_index[region]) >> 1;
            if (i < 0)
                i = 0;
            else if (i > NUM_CATEGORIES - 1)
                i = NUM_CATEGORIES - 1;
            power_categories[region] = i;
        }
        expected_number_of_code_bits = 0;
        for (region = 0;  region < number_of_regions;  region++)
             expected_number_of_code_bits += expected_bits_table[power_categories[region]];

        if (expected_number_of_code_bits >= number_of_available_bits - 32)
            offset = test_offset;

        delta >>= 1;
    }
    while (delta > 0);

    for (region = 0;  region < number_of_regions;  region++)
    {
        i = (offset - rms_index[region]) >> 1;
        if (i < 0)
            i = 0;
        else if (i > NUM_CATEGORIES - 1)
            i = NUM_CATEGORIES - 1;
        power_categories[region] = i;
    }
    expected_number_of_code_bits = 0;
    for (region = 0;  region < number_of_regions;  region++)
         expected_number_of_code_bits += expected_bits_table[power_categories[region]];

    for (region = 0;  region < number_of_regions;  region++)
    {
        max_rate_categories[region] = power_categories[region];
        min_rate_categories[region] = power_categories[region];
    }

    max = expected_number_of_code_bits;
    min = expected_number_of_code_bits;
    max_rate_pointer = num_categorization_control_possibilities;
    min_rate_pointer = num_categorization_control_possibilities;

    raw_min_index = 0;
    raw_max_index = 0;
    for (i = 0;  i < num_categorization_control_possibilities - 1;  i++)
    {
        if (max + min <= 2*number_of_available_bits)
        {
            raw_min = 99;

            /* Search from lowest freq regions to highest for best region to reassign to
               a higher bit rate category. */
            for (region = 0;  region < number_of_regions;  region++)
            {
                if (max_rate_categories[region] > 0)
                {
                    itemp0 = offset - rms_index[region] - 2*max_rate_categories[region];
                    if (itemp0 < raw_min)
                    {
                        raw_min = itemp0;
                        raw_min_index = region;
                    }
                }
            }
            max_rate_pointer--;
            temp_category_balances[max_rate_pointer] = raw_min_index;

            max -= expected_bits_table[max_rate_categories[raw_min_index]];
            max_rate_categories[raw_min_index] -= 1;
            max += expected_bits_table[max_rate_categories[raw_min_index]];
        }
        else
        {
            raw_max = -99;

            /* Search from highest freq regions to lowest for best region to reassign to
               a lower bit rate category. */
            for (region = number_of_regions - 1;  region >= 0;  region--)
            {
                if (min_rate_categories[region] < NUM_CATEGORIES - 1)
                {
                    itemp0 = offset - rms_index[region] - 2*min_rate_categories[region];
                    if (itemp0 > raw_max)
                    {
                        raw_max = itemp0;
                        raw_max_index = region;
                    }
                }
            }
            temp_category_balances[min_rate_pointer] = raw_max_index;
            min_rate_pointer++;

            min -= expected_bits_table[min_rate_categories[raw_max_index]];
            min_rate_categories[raw_max_index]++;
            min += expected_bits_table[min_rate_categories[raw_max_index]];
        }
    }

    for (i = 0;  i < number_of_regions;  i++)
        power_categories[i] = max_rate_categories[i];

    for (i = 0;  i < num_categorization_control_possibilities - 1;  i++)
        category_balances[i] = temp_category_balances[max_rate_pointer++];
}
/*- End of function --------------------------------------------------------*/
#endif
/*- End of file ------------------------------------------------------------*/
