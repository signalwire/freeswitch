/*
 * g722_1 - a library for the G.722.1 and Annex C codecs
 *
 * defs.h
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

#define MAX(a,b) (a > b  ?  a  :  b)
#define MIN(a,b) (a < b  ?  a  :  b)

#define FRAME_SIZE                                      (MAX_FRAME_SIZE >> 1)

#define DCT_LENGTH                                      (MAX_DCT_LENGTH >> 1)

#define NUM_CATEGORIES                                  8

#define REGION_POWER_TABLE_SIZE                         64
#define REGION_POWER_TABLE_NUM_NEGATIVES                24

#define NUM_CATEGORIZATION_CONTROL_BITS                 4
#define NUM_CATEGORIZATION_CONTROL_POSSIBILITIES        16

#define MAX_NUM_CATEGORIZATION_CONTROL_BITS             5
#define MAX_NUM_CATEGORIZATION_CONTROL_POSSIBILITIES    32

/* region_size = (BLOCK_SIZE * 0.875)/NUMBER_OF_REGIONS; */
#define REGION_SIZE                                     20

#define NUMBER_OF_REGIONS                               14
#define MAX_NUMBER_OF_REGIONS                           28

/* This value has been changed for fixed point interop */
#define ESF_ADJUSTMENT_TO_RMS_INDEX                     (9-2)

#define MAX_DCT_LENGTH_LOG                              7
#define DCT_LENGTH_LOG                                  6

#define CORE_SIZE                                       10

#if defined(G722_1_USE_FIXED_POINT)

#include "basop32.h"

#define DCT_LENGTH_DIV_2                                160
#define DCT_LENGTH_DIV_4                                80
#define DCT_LENGTH_DIV_8                                40
#define DCT_LENGTH_DIV_16                               20
#define DCT_LENGTH_DIV_32                               10
#define DCT_LENGTH_DIV_64                               5

void adjust_abs_region_power_index(int16_t *absolute_region_power_index, int16_t *mlt_coefs, int16_t number_of_regions);

int16_t samples_to_rmlt_coefs(const int16_t new_samples[],
                              int16_t history[],
                              int16_t coefs[],
                              int dct_length);

void rmlt_coefs_to_samples(int16_t *coefs,
                           int16_t *old_samples,
                           int16_t *out_samples,
                           int dct_length,
                           int16_t mag_shift);

void rmlt_coefs_to_samples(int16_t *coefs,
                           int16_t *old_samples,
                           int16_t *out_samples,
                           int dct_length,
                           int16_t mag_shift);

void categorize(int16_t number_of_available_bits,
                int16_t number_of_regions,
                int16_t num_categorization_control_possibilities,
                int16_t *rms_index,
                int16_t *power_categories,
                int16_t *category_balances);

int16_t calc_offset(int16_t *rms_index, int16_t number_of_regions, int16_t available_bits);

void comp_powercat_and_catbalance(int16_t *power_categories,
                                  int16_t *category_balances,
                                  int16_t *rms_index,
                                  int16_t number_of_available_bits,
                                  int16_t number_of_regions,
                                  int16_t num_categorization_control_possibilities,
                                  int16_t offset);

void dct_type_iv_a(int16_t input[], int16_t output[], int dct_length);

void dct_type_iv_s(int16_t input[], int16_t output[], int dct_length);

#else

#define PI                                              3.141592653589793238462

#define ENCODER_SCALE_FACTOR                            18318.0f

#define REGION_SIZE_INVERSE                             (1.0f/20.0f)

/* The MLT output is incorrectly scaled by the factor
   product of ENCODER_SCALE_FACTOR and sqrt(160.)
   This is now (9/30/96) 1.0/2^(4.5) or 1/22.627.
   In the current implementation this product
   must be an integer power of sqrt(2). The
   integer power is ESF_ADJUSTMENT_TO_RMS_INDEX.
   The -2 is to conform with the range defined in the spec. */

/* Scale factor used to match fixed point model results. */
#define INTEROP_RMLT_SCALE_FACTOR_7                     22.0f
#define INTEROP_RMLT_SCALE_FACTOR_14                    33.0f

void categorize(int number_of_regions,
                int number_of_available_bits,
                int rms_index[MAX_NUMBER_OF_REGIONS],
                int power_categories[MAX_NUMBER_OF_REGIONS],
                int category_balances[MAX_NUM_CATEGORIZATION_CONTROL_POSSIBILITIES - 1]);

void samples_to_rmlt_coefs(const float new_samples[],
                           float old_samples[],
                           float coefs[],
                           int dct_length);

void rmlt_coefs_to_samples(float coefs[],
                           float old_samples[],
                           float out_samples[],
                           int dct_length);

void dct_type_iv(float input[], float output[], int dct_length);

#endif

int16_t get_rand(g722_1_rand_t *randobj);

/*- End of file ------------------------------------------------------------*/
