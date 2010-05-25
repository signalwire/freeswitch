#ifndef __AMPLITUDE_H__
#include <math.h>
#include "amplitude.h"
#include "psi.h"

/*! \brief
 * @author Eric des Courtis
 * @param b A circular audio sample buffer
 * @param i Position in the buffer
 * @param f Frequency estimate
 * @return The amplitude at position i 
 */
extern double amplitude(circ_buffer_t *b, size_t i, double f)
{
    double result;

    result = sqrt(PSI(b, i) / sin(f * f));

    return result;
}

#endif

