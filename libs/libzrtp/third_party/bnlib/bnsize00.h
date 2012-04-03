/*
 * Copyright (c) 1995  Colin Plumb.  All rights reserved.
 * For licensing and other legal details, see the file legal.c.
 *
 * bnsize00.h - pick the correct machine word size to use.
 */
#include "lbn.h"	/* Get basic information */

#if !BNSIZE64 && !BNSIZE32 && !BNSIZE16 && defined(BNWORD64)
# if defined(BNWORD128) || (defined(lbnMulAdd1_64) && defined(lbnMulSub1_64))
#  define BNSIZE64 1
# elif defined(mul64_ppmm) || defined(mul64_ppmma) || defined(mul64_ppmmaa)
#  define BNSIZE64 1
# endif
#endif

#if !BNSIZE64 && !BNSIZE32 && !BNSIZE16 && defined(BNWORD32)
# if defined(BNWORD64) || (defined(lbnMulAdd1_32) && defined(lbnMulSub1_32))
#  define BNSIZE32 1
# elif defined(mul32_ppmm) || defined(mul32_ppmma) || defined(mul32_ppmmaa)
#  define BNSIZE32 1
# endif
#endif

#if !BNSIZE64 && !BNSIZE32 && !BNSIZE16 && defined(BNWORD16)
# if defined(BNWORD32) || (defined(lbnMulAdd1_16) && defined(lbnMulSub1_16))
#  define BNSIZE16 1
# elif defined(mul16_ppmm) || defined(mul16_ppmma) || defined(mul16_ppmmaa)
#  define BNSIZE16 1
# endif
#endif

#if !BNSIZE64 && !BNSIZE32 && !BNSIZE16
#error Unable to find a viable word size to compile bignum library.
#endif
