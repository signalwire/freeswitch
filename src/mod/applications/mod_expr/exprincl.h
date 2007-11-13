/*
    File: exprincl.h
    Auth: Brian Allen Vanderburg II
    Date: Thursday, April 24, 2003
    Desc: Includes, macros, etc needed by this library

    This file is part of ExprEval.
*/

#ifndef __BAVII_EXPRINCL_H
#define __BAVII_EXPRINCL_H


/* Includes and macros and whatnot for building the library */

#ifdef _MSC_VER
#if (_MSC_VER >= 1400)			// VC8+
#ifndef _CRT_SECURE_NO_DEPRECATE
#define _CRT_SECURE_NO_DEPRECATE
#endif
#ifndef _CRT_NONSTDC_NO_DEPRECATE
#define _CRT_NONSTDC_NO_DEPRECATE
#endif
#endif // VC8+
#endif

/* Memory routines.  memory.h for VC++, mem.h for BC++ */
#ifdef __TURBOC__
#include <mem.h>
#else
#include <memory.h>
#endif

/* Memory allocation */
#include <stdlib.h>

/* String routines */
#include <string.h>

/* Character manipulation routines */
#include <ctype.h>

/* Standard routines */
#include <stdlib.h>

/* Math routines */
#include <math.h>

/* Time */
#include <time.h>


/* Math constants.  VC++ does not seem to have these */
#ifndef M_E
#define M_E 2.7182818284590452354
#endif

#ifndef M_LOG2E
#define M_LOG2E 1.4426950408889634074
#endif

#ifndef M_LOG10E
#define M_LOG10E 0.43429448190325182765
#endif

#ifndef M_LN2
#define M_LN2 0.69314718055994530942
#endif

#ifndef M_LN10
#define M_LN10 2.30258509299404568402
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef M_PI_2
#define M_PI_2 1.57079632679489661923
#endif

#ifndef M_PI_4
#define M_PI_4 0.78539816339744830962
#endif

#ifndef M_1_PI
#define M_1_PI 0.31830988618379067154
#endif

#ifndef M_2_PI
#define M_2_PI 0.63661977236758134308
#endif

#ifndef M_1_SQRTPI
#define M_1_SQRTPI 0.56418958354776
#endif

#ifndef M_2_SQRTPI
#define M_2_SQRTPI 1.12837916709551257390
#endif

#ifndef M_SQRT2
#define M_SQRT2 1.41421356237309504880
#endif

#ifndef M_1_SQRT2
#define M_1_SQRT2 0.70710678118654752440
#endif



#endif /* __BAVII_EXPRINCL_H */
