/*
 * SpanDSP - a series of DSP components for telephony
 *
 * saturated.h - General saturated arithmetic routines.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2001, 2008 Steve Underwood
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 2.1,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*! \file */

#if !defined(_SPANDSP_SATURATED_H_)
#define _SPANDSP_SATURATED_H_

/*! \page saturated_page Saturated arithmetic

\section saturated_page_sec_1 What does it do?


\section saturated_page_sec_2 How does it work?

*/

#if defined(__cplusplus)
extern "C"
{
#endif

static __inline__ int16_t saturate16(int32_t amp)
{
#if defined(__GNUC__)  &&  (defined(__ARM_ARCH_6__)  ||  defined(__ARM_ARCH_7A__))
    int16_t z;

    __asm__ __volatile__(
        " ssat %[z],#16,%[amp];\n"
        : [z] "=r" (z)
        : [amp] "r" (amp)
    );
    return z;
#else
    int16_t z;

    /* Hopefully this is optimised for the common case - not clipping */
    z = (int16_t) amp;
    if (amp == z)
        return z;
    if (amp > INT16_MAX)
        return INT16_MAX;
    return INT16_MIN;
#endif
}
/*- End of function --------------------------------------------------------*/

/*! Saturate to 15 bits, rather than the usual 16 bits. This is often a useful function. */
static __inline__ int16_t saturate15(int32_t amp)
{
#if defined(__GNUC__)  &&  (defined(__ARM_ARCH_6__)  ||  defined(__ARM_ARCH_7A__))
    int16_t z;

    __asm__ __volatile__(
        " ssat %[z],#15,%[amp];\n"
        : [z] "=r" (z)
        : [amp] "r" (amp)
    );
    return z;
#else
    if (amp > 16383)
        return 16383;
    if (amp < -16384)
        return -16384;
    return (int16_t) amp;
#endif
}
/*- End of function --------------------------------------------------------*/

static __inline__ uint16_t saturateu16(int32_t amp)
{
#if defined(__GNUC__)  &&  (defined(__ARM_ARCH_6__)  ||  defined(__ARM_ARCH_7A__))
    uint16_t z;

    __asm__ __volatile__(
        " usat %[z],#16,%[amp];\n"
        : [z] "=r" (z)
        : [amp] "r" (amp)
    );
    return z;
#else
    uint16_t z;

    /* Hopefully this is optimised for the common case - not clipping */
    z = (uint16_t) amp;
    if (amp == z)
        return z;
    if (amp > UINT16_MAX)
        return UINT16_MAX;
    return 0;
#endif
}
/*- End of function --------------------------------------------------------*/

static __inline__ uint8_t saturateu8(int32_t amp)
{
#if defined(__GNUC__)  &&  (defined(__ARM_ARCH_6__)  ||  defined(__ARM_ARCH_7A__))
    uint8_t z;

    __asm__ __volatile__(
        " usat %[z],#8,%[amp];\n"
        : [z] "=r" (z)
        : [amp] "r" (amp)
    );
    return z;
#else
    uint8_t z;

    /* Hopefully this is optimised for the common case - not clipping */
    z = (uint8_t) amp;
    if (amp == z)
        return z;
    if (amp > UINT8_MAX)
        return UINT8_MAX;
    return 0;
#endif
}
/*- End of function --------------------------------------------------------*/

static __inline__ int16_t fsaturatef(float famp)
{
    if (famp > (float) INT16_MAX)
        return INT16_MAX;
    if (famp < (float) INT16_MIN)
        return INT16_MIN;
    return (int16_t) lrintf(famp);
}
/*- End of function --------------------------------------------------------*/

static __inline__ int16_t fsaturate(double damp)
{
    if (damp > (double) INT16_MAX)
        return INT16_MAX;
    if (damp < (double) INT16_MIN)
        return INT16_MIN;
    return (int16_t) lrint(damp);
}
/*- End of function --------------------------------------------------------*/

/* Saturate to a 16 bit integer, using the fastest float to int conversion */
static __inline__ int16_t ffastsaturatef(float famp)
{
    if (famp > (float) INT16_MAX)
        return INT16_MAX;
    if (famp < (float) INT16_MIN)
        return INT16_MIN;
    return (int16_t) lfastrintf(famp);
}
/*- End of function --------------------------------------------------------*/

/* Saturate to a 16 bit integer, using the fastest double to int conversion */
static __inline__ int16_t ffastsaturate(double damp)
{
    if (damp > (double) INT16_MAX)
        return INT16_MAX;
    if (damp < (double) INT16_MIN)
        return INT16_MIN;
    return (int16_t) lfastrint(damp);
}
/*- End of function --------------------------------------------------------*/

/* Saturate to a 16 bit integer, using the closest float to int conversion */
static __inline__ float ffsaturatef(float famp)
{
    if (famp > (float) INT16_MAX)
        return (float) INT16_MAX;
    if (famp < (float) INT16_MIN)
        return (float) INT16_MIN;
    return famp;
}
/*- End of function --------------------------------------------------------*/

/* Saturate to a 16 bit integer, using the closest double to int conversion */
static __inline__ double ffsaturate(double famp)
{
    if (famp > (double) INT16_MAX)
        return (double) INT16_MAX;
    if (famp < (double) INT16_MIN)
        return (double) INT16_MIN;
    return famp;
}
/*- End of function --------------------------------------------------------*/

static __inline__ int16_t sat_add16(int16_t x, int16_t y)
{
#if defined(__GNUC__)  &&  (defined(__i386__)  ||  defined(__x86_64__))
    __asm__ __volatile__(" addw %[y],%[x];\n"
                         " jno 0f;\n"
                         " movw $0x7FFF,%[x];\n"
                         " adcw $0,%[x];\n"
                         "0:"
                         : [x] "+r" (x)
                         : [y] "ir" (y)
                         : "cc");
    return x;
#elif defined(__GNUC__)  &&  (defined(__ARM_ARCH_6__)  ||  defined(__ARM_ARCH_7A__))
    int16_t z;

    __asm__ __volatile__(
        " qadd16 %[z],%[c],%[y];\n"
        : [z] "=r" (z)
        : [x] "r" (x), [y] "r" (y)
    );
    return z;
//#elif defined(__GNUC__)  &&  defined(__ARM_ARCH_5T__)
#else
    return saturate16((int32_t) x + y);
#endif
}
/*- End of function --------------------------------------------------------*/

static __inline__ int32_t sat_add32(int32_t x, int32_t y)
{
#if defined(__GNUC__)  &&  (defined(__i386__)  ||  defined(__x86_64__))
    __asm__ __volatile__(" addl %[y],%[x];\n"
                         " jno 0f;\n"
                         " movl $0x7FFFFFFF,%[x];\n"
                         " adcl $0,%[x];\n"
                         "0:"
                         : [x] "+r" (x)
                         : [y] "ir" (y)
                         : "cc");
    return x;
#elif defined(__GNUC__)  &&  (defined(__ARM_ARCH_6__)  ||  defined(__ARM_ARCH_7A__))
    int32_t z;

    __asm__ __volatile__(" qadd %[z],%[x],%[y];\n"
                         : [z] "=r" (z)
                         : [x] "r" (x), [y] "r" (y));
    return z;
//#elif defined(__GNUC__)  &&  defined(__ARM_ARCH_5T__)
#else
    int32_t z;

    z = x + y;
    if ((x ^ y) >= 0)
    {
        if ((z ^ x) < 0)
            z = (x < 0)  ?  INT32_MIN  :  INT32_MAX;
    }
    return z;
#endif
}
/*- End of function --------------------------------------------------------*/

static __inline__ int16_t sat_sub16(int16_t x, int16_t y)
{
#if defined(__GNUC__)  &&  (defined(__i386__)  ||  defined(__x86_64__))
    __asm__ __volatile__(" subw %[y],%[x];\n"
                         " jno 0f;\n"
                         " movw $0x8000,%[x];\n"
                         " sbbw $0,%[x];\n"
                         "0:"
                         : [x] "+r" (x)
                         : [y] "ir" (y)
                         : "cc");
    return x;
#elif defined(__GNUC__)  &&  (defined(__ARM_ARCH_6__)  ||  defined(__ARM_ARCH_7A__))
    int16_t z;

    __asm__ __volatile__(" qsub16 %[z],%[x],%[y];\n"
                         : [z] "=r" (z)
                         : [x] "r" (x), [y] "r" (y));
    return z;
//#elif defined(__GNUC__)  &&  defined(__ARM_ARCH_5T__)
#else
    return saturate16((int32_t) x - y);
#endif
}
/*- End of function --------------------------------------------------------*/

static __inline__ int32_t sat_sub32(int32_t x, int32_t y)
{
#if defined(__GNUC__)  &&  (defined(__i386__)  ||  defined(__x86_64__))
    __asm__ __volatile__(" subl %[y],%[x];\n"
                         " jno 0f;\n"
                         " movl $0x80000000,%[x];\n"
                         " sbbl $0,%[x];\n"
                         "0:"
                         : [x] "+r" (x)
                         : [y] "ir" (y)
                         : "cc");
    return x;
#elif defined(__GNUC__)  &&  (defined(__ARM_ARCH_6__)  ||  defined(__ARM_ARCH_7A__))
    int32_t z;

    __asm__ __volatile__(" qsub %[z],%[x],%[y];\n"
                         : [z] "=r" (z)
                         : [x] "r" (x), [y] "r" (y));
    return z;
//#elif defined(__GNUC__)  &&  defined(__ARM_ARCH_5T__)
#else
    int32_t z;

    z = x - y;
    if ((x ^ y) < 0)
    {
        if ((z ^ x) < 0)
            z = (x < 0L)  ?  INT32_MIN  :  INT32_MAX;
    }
    return z;
#endif
}
/*- End of function --------------------------------------------------------*/

static __inline__ int16_t sat_mul16(int16_t x, int16_t y)
{
    int32_t z;

#if defined(__GNUC__)  &&  (defined(__ARM_ARCH_6__)  ||  defined(__ARM_ARCH_7A__))
    __asm__ __volatile__(" smulbb %[z],%[c],%[y];\n"
                         " qadd %[z],%[z],%[z];\n"
                         : [z] "=r" (z)
                         : [x] "r" (x), [y] "r" (y));
    /* The qadd added one to the shift of 15 */
    return (int16_t) (z >> 16);
#else
    z = (int32_t) x*y;
    if (z == 0x40000000)
        return INT16_MAX;
    /*endif*/
    return (int16_t) (z >> 15);
#endif
}
/*- End of function --------------------------------------------------------*/

static __inline__ int32_t sat_mul32_16(int16_t x, int16_t y)
{
    int32_t z;

#if defined(__GNUC__)  &&  (defined(__ARM_ARCH_6__)  ||  defined(__ARM_ARCH_7A__))
    __asm__ __volatile__(" smulbb %[z],%[x],%[y];\n"
                         " qadd %[z],%[z],%[z];\n"
                         : [z] "=r" (z)
                         : [x] "r" (x), [y] "r" (y));
    return z;
#else
    z = (int32_t) x*y;
    if (z == 0x40000000)
        return INT32_MAX;
    return z << 1;
#endif
}
/*- End of function --------------------------------------------------------*/

static __inline__ int32_t sat_mac32_16(int32_t z, int16_t x, int16_t y)
{
#if defined(__GNUC__)  &&  (defined(__ARM_ARCH_6__)  ||  defined(__ARM_ARCH_7A__))
    int32_t product;

    __asm__ __volatile__(" smulbb %[p],%[x],%[y];\n"
                         " qdadd %[z],%[z],%[p];\n"
                         : [z] "+r" (z)
                         : [x] "r" (x), [y] "r" (y), [p] "r" (product));
    return z;
#else
    return sat_add32(z, sat_mul32_16(x, y));
#endif
}
/*- End of function --------------------------------------------------------*/

static __inline__ int32_t sat_msu32_16(int32_t z, int16_t x, int16_t y)
{
#if defined(__GNUC__)  &&  (defined(__ARM_ARCH_6__)  ||  defined(__ARM_ARCH_7A__))
    int32_t product;

    __asm__ __volatile__(" smulbb %[p],%[x],%[y];\n"
                         " qdsub %[z],%[z],%[p];\n"
                         : [z] "+r" (z)
                         : [x] "r" (x), [y] "r" (y), [p] "r" (product));
    return z;
#else
    return sat_sub32(z, sat_mul32_16(x, y));
#endif
}
/*- End of function --------------------------------------------------------*/

static __inline__ int16_t sat_abs16(int16_t x)
{
    if (x == INT16_MIN)
        return INT16_MAX;
    return (int16_t) abs(x);
}
/*- End of function --------------------------------------------------------*/

static __inline__ int32_t sat_abs32(int32_t x)
{
    if (x == INT32_MIN)
        return INT32_MAX;
    return abs(x);
}
/*- End of function --------------------------------------------------------*/

#if defined(__cplusplus)
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/
