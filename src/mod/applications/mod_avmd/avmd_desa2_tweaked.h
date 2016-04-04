/*
 * @brief   Estimator of cosine of digital frequency.
 * @details It is tweaked DESA implementation which
 *          returns partial product of DESA-2 estimation
 *          so that arc cosine transform can be ommited
 *          on all computations, but these values can
 *          be checked for convergence in the same time.
 *          If the partial results converge then frequency
 *          converges too.
 * @author  Piotr Gregor < piotrek.gregor gmail.com >
 * @date    20 Mar 2016
 */


#ifndef __AVMD_DESA2_TWEAKED_H__
#define __AVMD_DESA2_TWEAKED_H__


#include <math.h>
#include "avmd_buffer.h"
#include <switch.h>


/* Instead of returning digital frequency estimation using
 *      result = 0.5 * acos(n/d),
 * which involves expensive computation of arc cosine on
 * each new sample, this function returns only (n/d) factor.
 * The series of these partial DESA-2 results can be still
 * checked for convergence, though measures and thresholds
 * used to assess this will differ from those used for
 * assessment of convergence of instantaneous frequency
 * estimates since transformation of tweaked results
 * to corresponding frequencies is nonlinear.
 * The actual frequency estimation can be retrieved later
 * from this partial result using
 *      0.5 * acos(n/d)
 */
double avmd_desa2_tweaked(circ_buffer_t *b, size_t i);


#endif  /* __AVMD_DESA2_TWEAKED_H__ */
