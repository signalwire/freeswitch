/*
 * Copyright (c) 1995  Colin Plumb.  All rights reserved.
 * For licensing and other legal details, see the file legal.c.
 */
#ifndef TYPES_H
#define TYPES_H

#include <limits.h>

#if UCHAR_MAX == 0xff
typedef unsigned char word8;
typedef signed char int8;
#endif

#if UINT_MAX == 0xffffu
typedef unsigned word16;
typedef int int16;
#elif USHRT_MAX == 0xffffu
typedef unsigned short word16;
typedef short int16;
#endif

#if UINT_MAX == 0xffffffffu
typedef unsigned word32;
typedef int int32;
#elif ULONG_MAX == 0xffffffffu
typedef unsigned long word32;
typedef long int32;
#endif

#if ULONG_MAX > 0xffffffffu
typedef unsigned long word64;
typedef long int64;
#ifndef HAVE64
#define HAVE64 1
#endif
#elif defined(ULONGLONG_MAX) || defined(ULONG_LONG_MAX) || defined(ULLONG_MAX)
typedef unsigned long long word64;
typedef long long int64;
#ifndef HAVE64
#define HAVE64 1
#endif
#endif

#endif /* !TYPES_H */
