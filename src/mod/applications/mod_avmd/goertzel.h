#ifndef __GOERTZEL_H__
#define __GOERTZEL_H__

#ifndef _MSC_VER
#include <stdint.h>
#endif
#include "buffer.h"

#if !defined(M_PI)
/* C99 systems may not define M_PI */
#define M_PI 3.14159265358979323846264338327
#endif

extern double goertzel(circ_buffer_t *b, size_t pos, double f, size_t num);

#endif


