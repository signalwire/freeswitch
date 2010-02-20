#include "bnsize00.h"

#if BNSIZE16
#error Using 16-bit math library
#elif BNSIZE32
#error Using 32-bit math library
#elif BNSIZE64
#error Using 64-bit math library
#else
#error No math library size defined
#endif
