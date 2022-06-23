/*
 * Contributor(s):
 *
 * Eric des Courtis <eric.des.courtis@benbria.com>
 * Piotr Gregor <piotrgregor@rsyncme.org>
 */

#include <stdio.h>

#ifdef WIN32
	#include <float.h>
	#define ISNAN(x) (!!(_isnan(x)))
	#define ISINF(x) (isinf(x))
#endif

#include "avmd_buffer.h"
#include "avmd_desa2.h"
#include "avmd_options.h"

#ifdef AVMD_FAST_MATH
	#include "avmd_fast_acosf.h"
#endif


double avmd_desa2(circ_buffer_t *b, size_t i, double *amplitude) {
	double d;
	double n;
	double x0;
	double x1;
	double x2;
	double x3;
	double x4;
	double x2sq;
	double result;
	double PSI_Xn, PSI_Yn, NEEDED;

	x0 = GET_SAMPLE((b), (i));
	x1 = GET_SAMPLE((b), ((i) + 1));
	x2 = GET_SAMPLE((b), ((i) + 2));
	x3 = GET_SAMPLE((b), ((i) + 3));
	x4 = GET_SAMPLE((b), ((i) + 4));

	x2sq = x2 * x2;
	d = 2.0 * ((x2sq) - (x1 * x3));
	if (d == 0.0) {
		*amplitude = 0.0;
		return 0.0;
	}
	PSI_Xn = ((x2sq) - (x0 * x4));
	NEEDED = ((x1 * x1) - (x0 * x2)) + ((x3 * x3) - (x2 * x4));
	n = ((x2sq) - (x0 * x4)) - NEEDED;
	PSI_Yn = NEEDED + PSI_Xn;

#ifdef AVMD_FAST_MATH
	result = 0.5 * (double)fast_acosf((float)n/d);
#else
	result = 0.5 * acos(n/d);
#endif

	if (ISNAN(result)) {
		result = 0.0;
	}
	*amplitude = 2.0 * PSI_Xn / sqrt(PSI_Yn);

	return result;

}
