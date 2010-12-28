/*
 * g722_1 - a library for the G.722.1 and Annex C codecs
 *
 * tables.h
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

#define REGION_POWER_TABLE_SIZE 64
#define NUM_CATEGORIES          8

extern const int16_t vector_dimension[NUM_CATEGORIES];
extern const int16_t number_of_vectors[NUM_CATEGORIES];
/* The last category isn't really coded with scalar quantization. */
extern const int16_t max_bin[NUM_CATEGORIES];
extern const int16_t max_bin_plus_one_inverse[NUM_CATEGORIES];

#if defined(G722_1_USE_FIXED_POINT)
extern const int16_t int_region_standard_deviation_table[REGION_POWER_TABLE_SIZE];
extern const int16_t standard_deviation_inverse_table[REGION_POWER_TABLE_SIZE];
extern const int16_t step_size_inverse_table[NUM_CATEGORIES];
extern const int16_t int_dead_zone[NUM_CATEGORIES];
extern const int16_t int_dead_zone_low_bits[NUM_CATEGORIES];
extern const int16_t samples_to_rmlt_window[DCT_LENGTH];
extern const int16_t max_samples_to_rmlt_window[MAX_DCT_LENGTH];
extern const int16_t rmlt_to_samples_window[DCT_LENGTH];
extern const int16_t max_rmlt_to_samples_window[MAX_DCT_LENGTH];
#else
extern const float region_standard_deviation_table[REGION_POWER_TABLE_SIZE];
extern const float standard_deviation_inverse_table[REGION_POWER_TABLE_SIZE];
extern const float step_size_inverse_table[NUM_CATEGORIES];
extern const float dead_zone[NUM_CATEGORIES];
extern const float samples_to_rmlt_window[DCT_LENGTH];
extern const float max_samples_to_rmlt_window[MAX_DCT_LENGTH];
extern const float rmlt_to_samples_window[DCT_LENGTH];
extern const float max_rmlt_to_samples_window[MAX_DCT_LENGTH];

extern const float step_size[NUM_CATEGORIES];
extern const float region_power_table[REGION_POWER_TABLE_SIZE];
extern const float region_power_table_boundary[REGION_POWER_TABLE_SIZE - 1];
#endif

/*- End of file ------------------------------------------------------------*/
