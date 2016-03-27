/*
 * @brief  DESA-2 algorithm implementation.
 * @author Eric des Courtis
 * @par    Modifications: Piotr Gregor < piotrek.gregor gmail.com >
 */


#ifndef __AVMD_DESA2_H__
#define __AVMD_DESA2_H__


#include <math.h>
#include "avmd_buffer.h"

/* Returns digital frequency estimation. */
extern double avmd_desa2(circ_buffer_t *b, size_t i);


#endif  /* __AVMD_DESA2_H__ */
