/*
 * Contributor(s):
 *
 * Eric des Courtis <eric.des.courtis@benbria.com>
 */


#include <math.h>

#ifndef __AVMD_GOERTZEL_H__
	#include "avmd_goertzel.h"
#endif

#ifndef __AVMD_BUFFER_H__
	#include "avmd_buffer.h"
#endif


extern double avmd_goertzel(circ_buffer_t *b, size_t pos, double f, size_t num)
{
	double s = 0.0;
	double p = 0.0;
	double p2 = 0.0;
	double coeff;
	size_t i;

	coeff = 2.0 * cos(2.0 * M_PI * f);

	for (i = 0; i < num; i++) {
		/* TODO: optimize to avoid GET_SAMPLE when possible */
		s = GET_SAMPLE(b, i + pos) + (coeff * p) - p2;
		p2 = p;
		p = s;
	}

	return (p2 * p2) + (p * p) - (coeff * p2 * p);
}
