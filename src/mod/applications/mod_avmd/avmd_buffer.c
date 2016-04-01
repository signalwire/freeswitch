#ifndef __BUFFER_H__
#include "avmd_buffer.h"
#endif

extern size_t next_power_of_2(size_t v)
{
    size_t prev;
    size_t tmp = 1;

    v++;

    do {
        prev = v;
        v &= ~tmp;
        tmp <<= 1;
    } while (v != 0);

    prev <<= 1;

    return prev;
}


