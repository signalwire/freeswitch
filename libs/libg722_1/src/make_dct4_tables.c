/*
 * g722_1 - a library for the G.722.1 and Annex C codecs
 *
 * make_dct4_tables.c
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
#include <math.h>
#include <stdio.h>

#include "g722_1/g722_1.h"

#if defined(PI)
#undef PI
#endif
#define PI                                              3.141592653589793238462

#include "defs.h"

static void set_up_one_table(int length)
{
    int index;
    double angle;
    double scale;

    scale = PI/(double) (4*length);
    printf("static const cos_msin_t cos_msin_%d[%d] =\n", length, length);
    printf("{\n");
    for (index = 0;  index < length - 1;  index++)
    {
        angle = scale*((double) index + 0.5);
        printf("    {%.15ef, %.15ef},\n", cos(angle), -sin(angle));
    }
    angle = scale*((double) index + 0.5);
    printf("    {%.15ef, %.15ef}\n", cos(angle), -sin(angle));
    printf("};\n\n");
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    int length_log;
    int i;
    int k;
    int dct_size;
    double scale;

    dct_size = MAX_DCT_LENGTH;

    length_log = 0;
    while ((dct_size & 1) == 0)
    {
        length_log++;
        dct_size >>= 1;
    }

    scale = sqrt(2.0/MAX_DCT_LENGTH);
    printf("static const float max_dct_core_a[] =\n");
    printf("{\n");
    for (k = 0;  k < 10;  ++k)
    {
        for (i = 0;  i < 10;  ++i)
        {
            printf("    %22.15ef%s\n",
                   cos(PI*(k + 0.5) * (i + 0.5)/10.0)*scale,
                   (k == 9  &&  i == 9)  ?  ""  :  ",");
        }
    }
    printf("};\n\n");

    scale = sqrt(2.0/DCT_LENGTH);
    printf("static const float dct_core_a[] =\n");
    printf("{\n");
    for (k = 0;  k < 10;  ++k)
    {
        for (i = 0;  i < 10;  ++i)
        {
            printf("    %22.15ef%s\n",
                   cos(PI*(k + 0.5) * (i + 0.5)/10.0)*scale,
                   (k == 9  &&  i == 9)  ?  ""  :  ",");
        }
    }
    printf("};\n\n");

    for (i = 0;  i <= length_log;  i++)
        set_up_one_table(dct_size << i);
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
