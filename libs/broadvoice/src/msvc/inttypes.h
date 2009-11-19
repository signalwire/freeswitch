/*
 * SpanDSP - a series of DSP components for telephony
 *
 * inttypes.h - a fudge for MSVC, which lacks this header
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2006 Michael Jerris
 *
 *
 * This file is released in the public domain.
 *
 */

#if !defined(_INTTYPES_H_)
#define _INTTYPES_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _MSC_VER
#if (_MSC_VER >= 1400) // VC8+
#ifndef _CRT_SECURE_NO_DEPRECATE
#define _CRT_SECURE_NO_DEPRECATE
#endif
#ifndef _CRT_NONSTDC_NO_DEPRECATE
#define _CRT_NONSTDC_NO_DEPRECATE
#endif
#endif // VC8+
#include <windows.h>
typedef unsigned __int8		uint8_t;
typedef unsigned __int16	uint16_t;
typedef unsigned __int32	uint32_t;
typedef unsigned __int64    uint64_t;
typedef __int8		int8_t;
typedef __int16		int16_t;
typedef __int32		int32_t;
typedef __int64		int64_t;
#define inline __inline
#define __inline__ __inline
#define  INT16_MAX   0x7fff 
#define  INT16_MIN   (-INT16_MAX - 1) 
#define _MMX_H_

/* disable the following warnings 
 * C4100: The formal parameter is not referenced in the body of the function. The unreferenced parameter is ignored. 
 * C4200: Non standard extension C zero sized array
 * C4706: assignment within conditional expression
 * C4244: conversion from 'type1' to 'type2', possible loss of data
 * C4295: array is too small to include a terminating null character
 * C4125: decimal digit terminates octal escape sequence
 */
#pragma warning(disable:4100 4200 4706 4295 4125)

#pragma comment(lib, "ws2_32.lib")

#define strncasecmp _strnicmp
#define strcasecmp _stricmp
#define snprintf _snprintf

#if !defined(INFINITY)
#define INFINITY 0x7fffffff
#endif
#endif

#define PACKAGE "ilbc"
#define VERSION "0.0.1andabit"

#define INT32_MAX	(2147483647)
#define INT32_MIN	(-2147483647 - 1)

#define PRId8 "d"
#define PRId16 "d"
#define PRId32 "ld"
#define PRId64 "lld"

#define PRIu8 "u"
#define PRIu16 "u"
#define PRIu32 "lu"
#define PRIu64 "llu"

#ifdef __cplusplus
}
#endif

#endif
