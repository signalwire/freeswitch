#ifndef __GOERTZEL_H__
#include <math.h>
#include "goertzel.h"
#include "buffer.h"

/*! \brief Identify frequency components of a signal
 * @author Eric des Courtis
 * @param b A circular buffer
 * @param pos Position in the buffer
 * @param f Frequency to look at
 * @param num Number of samples to look at
 * @return A power estimate for frequency f at position pos in the stream
 */
extern double goertzel(circ_buffer_t *b, size_t pos, double f, size_t num)
{
    double s = 0.0;
    double p = 0.0;
    double p2 = 0.0;
    double coeff;
    size_t i;

    coeff = 2.0 * cos(2.0 * M_PI * f);

    for(i = 0; i < num; i++){
	/* TODO: optimize to avoid GET_SAMPLE when possible */
	s = GET_SAMPLE(b, i + pos) + (coeff * p) - p2;
	p2 = p;
	p = s;
    }

    return (p2 * p2) + (p * p) - (coeff * p2 * p);
}


#endif

