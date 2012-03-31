/*
 * Copyright (c) 1995  Colin Plumb.  All rights reserved.
 * For licensing and other legal details, see the file legal.c.
 *
 * usuals.h - Typedefs and #defines used widely.
 */
#ifndef USUALS_H
#define USUALS_H

#include <limits.h>

#if UCHAR_MAX == 0xff
typedef unsigned char byte;
typedef signed char int8;
#else
#error This machine has no 8-bit type
#endif

#if UINT_MAX == 0xffffu
typedef unsigned word16;
typedef int int16;
#elif USHRT_MAX == 0xffffu
typedef unsigned short word16;
typedef short int16;
#else
#error This machine has no 16-bit type
#endif

#if UINT_MAX == 0xffffffffu
typedef unsigned int word32;
typedef int int32;
#elif ULONG_MAX == 0xffffffffu
typedef unsigned long word32;
typedef long int32;
#else
#error This machine has no 32-bit type
#endif

#include <string.h>	/* Prototype for memset */
/*
 * Wipe sensitive data.
 * Note that this takes a structure, not a pointer to one!
 */
#define wipe(x) memset(x, 0, sizeof(*(x)))

#endif /* USUALS_H */
