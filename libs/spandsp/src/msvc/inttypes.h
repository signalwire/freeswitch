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

typedef __int8		__int8_t;
typedef __int16		__int16_t;
typedef __int32		__int32_t;
typedef __int64		__int64_t;

typedef unsigned __int8		uint8_t;
typedef unsigned __int16	uint16_t;
typedef unsigned __int32	uint32_t;
typedef unsigned __int64    uint64_t;
typedef __int8		int8_t;
typedef __int16		int16_t;
typedef __int32		int32_t;
typedef __int64		int64_t;

#define  INT16_MAX   0x7FFF 
#define  INT16_MIN   (-INT16_MAX - 1) 

#if !defined(INFINITY)
#define INFINITY 0x7FFFFFFF
#endif

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
