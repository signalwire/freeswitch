/*
 * @brief   Estimation of amplitude using DESA-2 algorithm.
 * @author Eric des Courtis
 * @par    Modifications: Piotr Gregor < piotrek.gregor gmail.com >
 */


#ifndef __AVMD_AMPLITUDE_H__
#define __AVMD_AMPLITUDE_H__


#include "avmd_buffer.h"


extern double avmd_amplitude(circ_buffer_t *, size_t i, double f);


#endif /* __AVMD_AMPLITUDE_H__ */
