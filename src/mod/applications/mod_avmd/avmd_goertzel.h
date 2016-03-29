/*
 * @brief   Goertzel algorithm.
 * @author  Eric des Courtis
 */


#ifndef __AVMD_GOERTZEL_H__
#define __AVMD_GOERTZEL_H__


#ifndef _MSC_VER
#include <stdint.h>
#endif
#include "avmd_buffer.h"

#if !defined(M_PI)
/* C99 systems may not define M_PI */
#define M_PI 3.14159265358979323846264338327
#endif


/*! \brief Identify frequency components of a signal
 * @author Eric des Courtis
 * @param b A circular buffer
 * @param pos Position in the buffer
 * @param f Frequency to look at
 * @param num Number of samples to look at
 * @return A power estimate for frequency f at position pos in the stream
 */
extern double avmd_goertzel(circ_buffer_t *b, size_t pos, double f, size_t num);


#endif /* __AVMD_GOERTZEL_H__ */
