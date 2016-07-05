/*
 * @brief  DESA-2 algorithm implementation.
 *
 * Contributor(s):
 *
 * Eric des Courtis <eric.des.courtis@benbria.com>
 * Piotr Gregor <piotrek.gregor gmail.com>:
 */


#ifndef __AVMD_DESA2_H__
#define __AVMD_DESA2_H__


#include <math.h>
#include "avmd_buffer.h"

/* Returns digital frequency estimation and amplitude estimation. */
extern double avmd_desa2(circ_buffer_t *b, size_t i, double *amplitude) __attribute__ ((nonnull(1,3)));


#endif  /* __AVMD_DESA2_H__ */
