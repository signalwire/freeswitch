#ifndef __AVMD_PSI_H__
#define __AVMD_PSI_H__


#include "avmd_buffer.h"

#define PSI(b, i) (GET_SAMPLE((b), ((i) + 1))*GET_SAMPLE((b), ((i) + 1))-GET_SAMPLE((b), ((i) + 2))*GET_SAMPLE((b), ((i) + 0)))

#endif /* __AVMD_PSI_H__ */
