/*
 * g722_1 - a library for the G.722.1 and Annex C codecs
 *
 * huff_tab.h
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
 * $Id: huff_tab.h,v 1.4 2008/09/30 14:06:40 steveu Exp $
 */

#define REGION_POWER_STEPSIZE_DB    3.010299957
#define ABS_REGION_POWER_LEVELS     32
#define DIFF_REGION_POWER_LEVELS    24

#define DRP_DIFF_MIN                -12
#define DRP_DIFF_MAX                11

#define MAX_NUM_BINS                16
#define MAX_VECTOR_INDICES          625
#define MAX_VECTOR_DIMENSION        5

extern const int16_t differential_region_power_bits[MAX_NUMBER_OF_REGIONS][DIFF_REGION_POWER_LEVELS];
extern const uint16_t differential_region_power_codes[MAX_NUMBER_OF_REGIONS][DIFF_REGION_POWER_LEVELS];
extern const int16_t differential_region_power_decoder_tree[MAX_NUMBER_OF_REGIONS][DIFF_REGION_POWER_LEVELS - 1][2];
#if defined(G722_1_USE_FIXED_POINT)
extern const int16_t mlt_quant_centroid[NUM_CATEGORIES][MAX_NUM_BINS];
#else
extern const float mlt_quant_centroid[NUM_CATEGORIES - 1][MAX_NUM_BINS];
#endif
extern const int16_t expected_bits_table[NUM_CATEGORIES];

extern const int16_t *table_of_bitcount_tables[NUM_CATEGORIES - 1];
extern const uint16_t *table_of_code_tables[NUM_CATEGORIES - 1];

extern const int16_t *table_of_decoder_tables[NUM_CATEGORIES - 1];

/*- End of file ------------------------------------------------------------*/
