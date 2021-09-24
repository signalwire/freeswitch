#include "config.h"

#ifndef __LITTLE_ENDIAN
#define __LITTLE_ENDIAN 1234
#endif
#ifndef __BIG_ENDIAN
#define __BIG_ENDIAN 4321
#endif
#ifndef __BYTE_ORDER
#ifdef SWITCH_BYTE_ORDER
#define __BYTE_ORDER SWITCH_BYTE_ORDER
#else
#define __BYTE_ORDER __LITTLE_ENDIAN
#endif
#endif

