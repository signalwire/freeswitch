/*
 * Contributor(s):
 *
 * Eric des Courtis <eric.des.courtis@benbria.com>
 * Piotr Gregor <piotrgregor@rsyncme.org>
 */


#include <math.h>
#include "avmd_amplitude.h"
#include "avmd_psi.h"


double avmd_amplitude(circ_buffer_t *b, size_t i, double f) {
	double result;
	result = sqrt(PSI(b, i) / sin(f * f));
	return result;
}
