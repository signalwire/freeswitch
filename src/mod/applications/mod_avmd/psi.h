#ifndef __PSI_H__
#define __PSI_H__
#include "buffer.h"

#define PSI(b, i) (GET_SAMPLE((b), ((i) + 1))*GET_SAMPLE((b), ((i) + 1))-GET_SAMPLE((b), ((i) + 2))*GET_SAMPLE((b), ((i) + 0)))

#endif


