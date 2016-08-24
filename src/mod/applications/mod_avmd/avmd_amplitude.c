#ifndef __AVMD_AMPLITUDE_H__


#include <math.h>
#include "avmd_amplitude.h"
#include "avmd_psi.h"


double avmd_amplitude(circ_buffer_t *b, size_t i, double f) {
    double result;
    result = sqrt(PSI(b, i) / sin(f * f));
    return result;
}


#endif /* __AVMD_AMPLITUDE_H__ */

