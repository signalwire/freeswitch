#ifndef __GOERTZEL_H__
#define __GOERTZEL_H__

#include <stdint.h>
#include "buffer.h"

extern double goertzel(circ_buffer_t *b, size_t pos, double f, size_t num);

#endif


