/*
 * g722_1 - a library for the G.722.1 and Annex C codecs
 *
 * make_tables.c
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
 * $Id: make_tables.c,v 1.5 2008/11/21 15:30:22 steveu Exp $
 */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include <config.h>
#endif

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

#include "g722_1/g722_1.h"

#include "defs.h"
#include "huff_tab.h"

#if defined(PI)
#undef PI
#endif
#define PI                                              3.141592653589793238462
/* These may have been defined in the main header for the codec, so we clear out
   any pre-existing definitions here. */
#if defined(ENCODER_SCALE_FACTOR)
#undef ENCODER_SCALE_FACTOR
#endif
#if defined(DECODER_SCALE_FACTOR)
#undef DECODER_SCALE_FACTOR
#endif
#define ENCODER_SCALE_FACTOR                            18318.0
#define DECODER_SCALE_FACTOR                            18096.0

#define REGION_POWER_TABLE_SIZE                         64
#define NUM_CATEGORIES                                  8
#define MAX_DCT_LENGTH                                  640

#if defined(G722_1_USE_FIXED_POINT)
int16_t int_region_standard_deviation_table[REGION_POWER_TABLE_SIZE];
int16_t standard_deviation_inverse_table[REGION_POWER_TABLE_SIZE];
#else
float region_standard_deviation_table[REGION_POWER_TABLE_SIZE];
float standard_deviation_inverse_table[REGION_POWER_TABLE_SIZE];
#endif

int16_t vector_dimension[NUM_CATEGORIES];

int16_t number_of_vectors[NUM_CATEGORIES];
/* The last category isn't really coded with scalar quantization. */
int16_t max_bin_plus_one_inverse[NUM_CATEGORIES];

const int16_t max_bin[NUM_CATEGORIES] =
{
    13, 9, 6, 4, 3, 2, 1, 1
};

const float step_size[NUM_CATEGORIES] =
{
    0.3536f,
    0.5f,
    0.7071f,
    1.0f,
    1.4142f,
    2.0f,
    2.8284f,
    2.8284f
};

static void generate_sam2coef_tables(void)
{
    int i;
    double angle;

    printf("#if defined(G722_1_USE_FIXED_POINT)\n");

    printf("const int16_t samples_to_rmlt_window[DCT_LENGTH] =\n{\n");
    for (i = 0;  i < DCT_LENGTH;  i++)
    {
        if (i%10 == 0)
            printf("    ");
        angle = (PI/2.0)*((double) i + 0.5)/(double) DCT_LENGTH;
        printf("%5d,", (int) (ENCODER_SCALE_FACTOR*sin(angle)));
        if (i%10 == 9)
            printf("\n");
        else
            printf(" ");
    }
    printf("};\n\n");

    printf("const int16_t max_samples_to_rmlt_window[MAX_DCT_LENGTH] =\n{\n");
    for (i = 0;  i < MAX_DCT_LENGTH;  i++)
    {
        if (i%10 == 0)
            printf("    ");
        angle = (PI/2.0)*((double) i + 0.5)/(double) MAX_DCT_LENGTH;
        printf("%5d,", (int) (ENCODER_SCALE_FACTOR*sin(angle)));
        if (i%10 == 9)
            printf("\n");
        else
            printf(" ");
    }
    printf("};\n\n");

    printf("#else\n");

    printf("const float samples_to_rmlt_window[DCT_LENGTH] =\n{\n");
    for (i = 0;  i < DCT_LENGTH;  i++)
    {
        angle = (PI/2.0)*((double) i + 0.5)/(double) DCT_LENGTH;
        printf("    %.15e,\n", sin(angle));
    }
    printf("};\n\n");

    printf("const float max_samples_to_rmlt_window[MAX_DCT_LENGTH] =\n{\n");
    for (i = 0;  i < MAX_DCT_LENGTH;  i++)
    {
        angle = (PI/2.0)*((double) i + 0.5)/(double) MAX_DCT_LENGTH;
        printf("    %.15le,\n", sin(angle));
    }
    printf("};\n\n");

    printf("#endif\n");
}

static void generate_coef2sam_tables(void)
{
    int i;
    double angle;

    printf("#if defined(G722_1_USE_FIXED_POINT)\n");

    printf("const int16_t rmlt_to_samples_window[DCT_LENGTH] =\n{\n");
    for (i = 0;  i < DCT_LENGTH;  i++)
    {
        if (i%10 == 0)
            printf("    ");
        angle = (PI/2.0)*((double) i + 0.5)/(double) DCT_LENGTH;
        printf("%5d,", (int) (DECODER_SCALE_FACTOR*sin(angle)));
        if (i%10 == 9)
            printf("\n");
        else
            printf(" ");
    }
    printf("};\n\n");

    printf("const int16_t max_rmlt_to_samples_window[MAX_DCT_LENGTH] =\n{\n");
    for (i = 0;  i < MAX_DCT_LENGTH;  i++)
    {
        if (i%10 == 0)
            printf("    ");
        angle = (PI/2.0)*((double) i + 0.5)/(double) MAX_DCT_LENGTH;
        printf("%5d,", (int) (DECODER_SCALE_FACTOR*sin(angle)));
        if (i%10 == 9)
            printf("\n");
        else
            printf(" ");
    }
    printf("};\n\n");

    printf("#else\n");

    printf("const float rmlt_to_samples_window[DCT_LENGTH] =\n{\n");
    for (i = 0;  i < DCT_LENGTH;  i++)
    {
        angle = (PI/2.0)*((double) i + 0.5)/(double) DCT_LENGTH;
        printf("    %.15e,\n", sin(angle));
    }
    printf("};\n\n");

    printf("const float max_rmlt_to_samples_window[MAX_DCT_LENGTH] =\n{\n");
    for (i = 0;  i < MAX_DCT_LENGTH;  i++)
    {
        angle = (PI/2.0)*((double) i + 0.5)/(double) MAX_DCT_LENGTH;
        printf("    %.15e,\n", sin(angle));
    }
    printf("};\n\n");

    printf("#endif\n");
}

int main(int argc, char *argv[])
{
    int i;
    int j;
    int number_of_indices;
    double value;

    if (strcmp(argv[1], "sam2coef") == 0)
    {
        generate_sam2coef_tables();
        return 0;
    }
    
    if (strcmp(argv[1], "coef2sam") == 0)
    {
        generate_coef2sam_tables();
        return 0;
    }
    
    printf("const float region_standard_deviation_table[REGION_POWER_TABLE_SIZE] =\n{\n");
    for (i = 0;  i < REGION_POWER_TABLE_SIZE;  i++)
    {
        value = pow(10.0, 0.10*REGION_POWER_STEPSIZE_DB*(i - REGION_POWER_TABLE_NUM_NEGATIVES));
        printf("    %.15e,\n", sqrt(value));
    }
    printf("};\n\n");

    printf("const float standard_deviation_inverse_table[REGION_POWER_TABLE_SIZE] =\n{\n");
    for (i = 0;  i < REGION_POWER_TABLE_SIZE;  i++)
    {
        value = pow(10.0, 0.10*REGION_POWER_STEPSIZE_DB*(i - REGION_POWER_TABLE_NUM_NEGATIVES));
        printf("    %.15e,\n", 1.0/sqrt(value));
    }
    printf("};\n\n");

    printf("const int16_t max_bin_plus_one_inverse[NUM_CATEGORIES] =\n{\n");
    for (i = 0;  i < NUM_CATEGORIES;  i++)
    {
        printf("    %5d,\n", max_bin[i]);
    }
    printf("};\n\n");

    printf("const int16_t max_bin_plus_one_inverse[NUM_CATEGORIES] =\n{\n");
    for (i = 0;  i < NUM_CATEGORIES;  i++)
    {
        /* Rounding up by 1.0 instead of 0.5 allows us to avoid rounding every time this is used. */
        max_bin_plus_one_inverse[i] = (int) ((32768.0/(max_bin[i] + 1.0)) + 1.0);
        printf("    %5d,\n", max_bin_plus_one_inverse[i]);

        /* Test division for all indices. */
        number_of_indices = 1;
        for (j = 0;  j < vector_dimension[i];  j++)
            number_of_indices *= (max_bin[i] + 1);
        for (j = 0;  j < number_of_indices;  j++)
        {
            if (j/(max_bin[i] + 1) != ((j*max_bin_plus_one_inverse[i]) >> 15))
                printf("max_bin_plus_one_inverse ERROR!! %1d: %5d %3d\n", i, max_bin_plus_one_inverse[i], j);
        }
    }
    printf("};\n\n");

    printf("const float step_size[NUM_CATEGORIES] =\n{\n");
    for (i = 0;  i < NUM_CATEGORIES;  i++)
    {
        printf("    %.15e,\n", step_size[i]);
    }
    printf("};\n\n");

    printf("const float step_size_inverse_table[NUM_CATEGORIES] =\n{\n");
    for (i = 0;  i < NUM_CATEGORIES;  i++)
    {
        printf("    %.15e,\n", 1.0/step_size[i]);
    }
    printf("};\n\n");

    printf("const float region_power_table[REGION_POWER_TABLE_SIZE] =\n{\n");
    /*  region_size = (BLOCK_SIZE * 0.875)/NUMBER_OF_REGIONS; */
    for (i = 0;  i < REGION_POWER_TABLE_SIZE;  i++)
    {
        value = pow(10.0, 0.10*REGION_POWER_STEPSIZE_DB*(i - REGION_POWER_TABLE_NUM_NEGATIVES));
        printf("    %.15e,\n", value);
    }
    printf("};\n\n");

    printf("const float region_power_table_boundary[REGION_POWER_TABLE_SIZE - 1] =\n{\n");
    for (i = 0;  i < REGION_POWER_TABLE_SIZE - 1; i++)
    {
        value = (float) pow(10.0, 0.10*REGION_POWER_STEPSIZE_DB*(0.5 + (i - REGION_POWER_TABLE_NUM_NEGATIVES)));
        printf("    %.15e,\n", value);
    }
    printf("};\n\n");
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
