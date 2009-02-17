/*
 * SpanDSP - a series of DSP components for telephony
 *
 * tgmath.h - a fudge for MSVC, which lacks this header
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2006 Michael Jerris
 *
 *
 * This file is released in the public domain.
 *
 */

#if !defined(_TGMATH_H_)
#define _TGMATH_H_

#include <math.h>

#if !defined(M_PI)
/* C99 systems may not define M_PI */
#define M_PI 3.14159265358979323846264338327
#endif


#ifdef __cplusplus
extern "C" {
#endif

/* A kindofa rint() for VC++ (only kindofa, because rint should be type generic,
   and this one is purely float to int */
static inline long int lrintf(float a)
{
    long int i;
    
    __asm
    {
        fld   a
        fistp i
    }
    return i;
}

static inline long int lrint(double a)
{
    long int i;
    
    __asm
    {
        fld   a
        fistp i
    }
    return i;
}

static inline int rintf(float a)
{
    int i;
    
    __asm
    {
        fld   a
        fistp i
    }
    return i;
}

static inline int rint(double a)
{
    int i;
    
    __asm
    {
        fld   a
        fistp i
    }
    return i;
}

#ifdef __cplusplus
}
#endif

#endif
