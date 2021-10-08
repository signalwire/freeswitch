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

#if defined(_MSC_VER) && _MSC_VER >= 1900
#include <stdint.h>
#else

typedef __int8			        __int8_t;
typedef __int16		        __int16_t;
typedef __int32		        __int32_t;
typedef __int64		        __int64_t;

typedef unsigned __int8		uint8_t;
typedef unsigned __int16	uint16_t;
typedef unsigned __int32	uint32_t;
typedef unsigned __int64    uint64_t;
typedef __int8		        int8_t;
typedef __int16		        int16_t;
typedef __int32		        int32_t;
typedef __int64		        int64_t;

#endif
#if !defined(INFINITY)  &&  _MSC_VER < 1800
#define INFINITY 0x7FFFFFFF
#endif

#if !defined(UINT8_MAX)
#define UINT8_MAX   0xFF
#endif
#if !defined(UINT16_MAX)
#define UINT16_MAX  0xFFFF
#endif
#if !defined(UINT32_MAX)
#define UINT32_MAX	0xFFFFFFFF
#endif

#if !defined(INT16_MAX)
#define INT16_MAX   0x7FFF
#endif
#if !defined(INT16_MIN)
#define INT16_MIN   (-INT16_MAX - 1)
#endif

#if !defined(INT32_MAX)
#define INT32_MAX	(2147483647)
#endif
#if !defined(INT32_MIN)
#define INT32_MIN	(-2147483647 - 1)
#endif

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
