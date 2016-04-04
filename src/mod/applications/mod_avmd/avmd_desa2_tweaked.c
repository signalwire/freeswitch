#ifndef __AVMD_DESA2_TWEAKED_H__

#include <switch.h>
#include <stdio.h>
#ifdef WIN32
#include <float.h>
#define ISNAN(x) (!!(_isnan(x)))
#else
#define ISNAN(x) (isnan(x))
#endif
#include "avmd_buffer.h"
#include "avmd_desa2_tweaked.h"
#include "avmd_options.h"

#ifdef AVMD_FAST_MATH
#include "avmd_fast_acosf.h"
#endif

double
avmd_desa2_tweaked(circ_buffer_t *b, size_t i)
{
    double d;
    double n;
    double x0;
    double x1;
    double x2;
    double x3;
    double x4;
    double x2sq;
    double result;

    x0 = GET_SAMPLE((b), (i));
    x1 = GET_SAMPLE((b), ((i) + 1));
    x2 = GET_SAMPLE((b), ((i) + 2));
    x3 = GET_SAMPLE((b), ((i) + 3));
    x4 = GET_SAMPLE((b), ((i) + 4));
    x2sq = x2 * x2;
    d = 2.0 * ((x2sq) - (x1 * x3));
    n = ((x2sq) - (x0 * x4)) - ((x1 * x1) 
            - (x0 * x2)) - ((x3 * x3) - (x2 * x4));

/* instead of
#ifdef FASTMATH
    result = 0.5 * (double)fast_acosf((float)n/d);
#else
    result = 0.5 * acos(n/d);
#endif
 we do simplified, modified for speed version : */

    result = n/d;
    if (isinf(result)) {
        if (n < 0.0)
            return -10.0;
        else
            return 10.0;
    }
    return result;
}

#endif  /* __AVMD_DESA2_TWEAKED_H__ */
