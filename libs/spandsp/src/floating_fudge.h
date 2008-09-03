/*
 * SpanDSP - a series of DSP components for telephony
 *
 * floating_fudge.h - A bunch of shims, to use double maths
 *                    functions on platforms which lack the
 *                    float versions with an 'f' at the end,
 *                    and to deal with the vaguaries of lrint().
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2008 Steve Underwood
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
 *
 * $Id: floating_fudge.h,v 1.5 2008/08/03 03:44:00 steveu Exp $
 */

#if !defined(_FLOATING_FUDGE_H_)
#define _FLOATING_FUDGE_H_

#if defined(__cplusplus)
extern "C"
{
#endif

#if !defined(HAVE_SINF)
static __inline__ float sinf(float x)
{
	return (float) sin((double) x);
}
#endif

#if !defined(HAVE_COSF)
static __inline__ float cosf(float x)
{
	return (float) cos((double) x);
}
#endif

#if !defined(HAVE_TANF)
static __inline__ float tanf(float x)
{
	return (float) tan((double) x);
}
#endif

#if !defined(HAVE_ASINF)
static __inline__ float asinf(float x)
{
	return (float) asin((double) x);
}
#endif

#if !defined(HAVE_ACOSF)
static __inline__ float acosf(float x)
{
	return (float) acos((double) x);
}
#endif

#if !defined(HAVE_ATANF)
static __inline__ float atanf(float x)
{
	return (float) atan((double) x);
}

#endif

#if !defined(HAVE_ATAN2F)
static __inline__ float atan2f(float y, float x)
{
	return (float) atan2((double) y, (double) x);
}

#endif

#if !defined(HAVE_CEILF)
static __inline__ float ceilf(float x)
{
	return (float) ceil((double) x);
}
#endif

#if !defined(HAVE_FLOORF)
static __inline__ float floorf(float x)
{
	return (float) floor((double) x);
}

#endif

#if !defined(HAVE_POWF)
static __inline__ float powf(float x, float y)
{
    return (float) pow((double) x, (double) y);
}
#endif

#if !defined(HAVE_EXPF)
static __inline__ float expf(float x)
{
    return (float) expf((double) x);
}
#endif

#if !defined(HAVE_LOGF)
static __inline__ float logf(float x)
{
	return (float) logf((double) x);
}
#endif

#if !defined(HAVE_LOG10F)
static __inline__ float log10f(float x)
{
    return (float) log10((double) x);
}
#endif

/* The following code, to handle issues with lrint() and lrintf() on various
 * platforms, is adapted from similar code in libsndfile, which is:
 *
 * Copyright (C) 2001-2004 Erik de Castro Lopo <erikd@mega-nerd.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 */

/*
 *    On Intel Pentium processors (especially PIII and probably P4), converting
 *    from float to int is very slow. To meet the C specs, the code produced by
 *    most C compilers targeting Pentium needs to change the FPU rounding mode
 *    before the float to int conversion is performed.
 *
 *    Changing the FPU rounding mode causes the FPU pipeline to be flushed. It
 *    is this flushing of the pipeline which is so slow.
 *
 *    Fortunately the ISO C99 specification defines the functions lrint, lrintf,
 *    llrint and llrintf which fix this problem as a side effect.
 *
 *    On Unix-like systems, the configure process should have detected the
 *    presence of these functions. If they weren't found we have to replace them
 *    here with a standard C cast.
 */

/*
 *    The C99 prototypes for these functions are as follows:
 *
 *        int rintf(float x);
 *        int rint(double x);
 *        long int lrintf(float x);
 *        long int lrint(double x);
 *        long long int llrintf(float x);
 *        long long int llrint(double x);
 *
 *    The presence of the required functions are detected during the configure
 *    process and the values HAVE_LRINT and HAVE_LRINTF are set accordingly in
 *    the config file.
 */

#if defined(__CYGWIN__)
    /*
     *    CYGWIN has lrint and lrintf functions, but they are slow and buggy:
     *        http://sourceware.org/ml/cygwin/2005-06/msg00153.html
     *        http://sourceware.org/ml/cygwin/2005-09/msg00047.html
     *    The latest version of cygwin seems to have made no effort to fix this.
     *    These replacement functions (pulled from the Public Domain MinGW
     *    math.h header) replace the native versions.
     */
    static __inline__ long int lrint(double x)
    {
        long int retval;

        __asm__ __volatile__
        (
            "fistpl %0"
            : "=m" (retval)
            : "t" (x)
            : "st"
        );

        return retval;
    }

    static __inline__ long int lrintf(float x)
    {
        long int retval;

        __asm__ __volatile__
        (
            "fistpl %0"
            : "=m" (retval)
            : "t" (x)
            : "st"
        );
        return retval;
    }

    static __inline__ long int lfastrint(double x)
    {
        long int retval;

        __asm__ __volatile__
        (
            "fistpl %0"
            : "=m" (retval)
            : "t" (x)
            : "st"
        );

        return retval;
    }

    static __inline__ long int lfastrintf(float x)
    {
        long int retval;

        __asm__ __volatile__
        (
            "fistpl %0"
            : "=m" (retval)
            : "t" (x)
            : "st"
        );
        return retval;
    }
#elif defined(HAVE_LRINT)  &&  defined(HAVE_LRINTF)

#if defined(__i386__)
    /* These routines are guaranteed fast on an i386 machine. Using the built in
       lrint() and lrintf() should be similar, but they may not always be enabled.
       Sometimes, especially with "-O0", you might get slow calls to routines. */
    static __inline__ long int lfastrint(double x)
    {
        long int retval;

        __asm__ __volatile__
        (
            "fistpl %0"
            : "=m" (retval)
            : "t" (x)
            : "st"
        );

        return retval;
    }

    static __inline__ long int lfastrintf(float x)
    {
        long int retval;

        __asm__ __volatile__
        (
            "fistpl %0"
            : "=m" (retval)
            : "t" (x)
            : "st"
        );
        return retval;
    }
#elif defined(__x86_64__)
    /* On an x86_64 machine, the fastest thing seems to be a pure assignment from a
       double or float to an int. It looks like the design on the x86_64 took account
       of the default behaviour specified for C. */
    static __inline__ long int lfastrint(double x)
    {
        return (long int) (x);
    }

    static __inline__ long int lfastrintf(float x)
    {
        return (long int) (x);
    }
#elif defined(__ppc__)  ||   defined(__powerpc__)
    static __inline__ long int lfastrint(register double x)
    {
        int res[2];

        __asm__ __volatile__
        (
            "fctiw %1, %1\n\t"
            "stfd %1, %0"
            : "=m" (res)    /* Output */
            : "f" (x)       /* Input */
            : "memory"
        );

        return res[1];
    }

    static __inline__ long int lfastrintf(register float x)
    {
        int res[2];

        __asm__ __volatile__
        (
            "fctiw %1, %1\n\t"
            "stfd %1, %0"
            : "=m" (res)    /* Output */
            : "f" (x)       /* Input */
            : "memory"
        );

        return res[1];
    }
#endif

#elif defined(WIN32)  ||  defined(_WIN32)
    /*
     *    Win32 doesn't seem to have the lrint() and lrintf() functions.
     *    Therefore implement inline versions of these functions here.
     */
    __inline long int lrint(double x)
    {
        long int i;

        _asm
        {
            fld x
            fistp i
        };
        return i;
    }

    __inline long int lrintf(float x)
    {
        long int i;

        _asm
        {
            fld x
            fistp i
        };
        return i;
    }

    __inline long int lfastrint(double x)
    {
        long int i;

        _asm
        {
            fld x
            fistp i
        };
        return i;
    }

    __inline long int lfastrintf(float x)
    {
        long int i;

        _asm
        {
            fld x
            fistp i
        };
        return i;
    }
#elif defined(WIN64)  ||  defined(_WIN64)
    /*
     *    Win64 machines will do best with a simple assignment.
     */
    __inline long int lfastrint(double x)
    {
        return (long int) (x);
    }

    __inline long int lfastrintf(float x)
    {
        return (long int) (x);
    }
#elif defined(__MWERKS__)  &&  defined(macintosh)
    /* This MacOS 9 solution was provided by Stephane Letz */

    long int __inline__ lfastrint(register double x)
    {
        long int res[2];

        asm
        {
            fctiw x, x
            stfd x, res
        }
        return res[1];
    }

    long int __inline__ lfastrintf(register float x)
    {
        long int res[2];

        asm
        {
            fctiw x, x
            stfd x, res
        }
        return res[1];
    }

#elif defined(__MACH__)  &&  defined(__APPLE__)  &&  (defined(__ppc__)  ||  defined(__powerpc__))
    /* For Apple Mac OS/X - do recent versions still need this? */

    static __inline__ long int lfastrint(register double x)
    {
        int res[2];

        __asm__ __volatile__
        (
            "fctiw %1, %1\n\t"
            "stfd %1, %0"
            : "=m" (res)    /* Output */
            : "f" (x)       /* Input */
            : "memory"
        );

        return res[1];
    }

    static __inline__ long int lfastrintf(register float x)
    {
        int res[2];

        __asm__ __volatile__
        (
            "fctiw %1, %1\n\t"
            "stfd %1, %0"
            : "=m" (res)    /* Output */
            : "f" (x)       /* Input */
            : "memory"
        );

        return res[1];
    }
#else
    /* There is nothing else to do, but use a simple casting operation, instead of a real
       rint() type function. Since we are only trying to use rint() to speed up conversions,
       the accuracy issues related to changing the rounding scheme are of little concern
       to us. */

    #if !defined(__sgi)
        #warning "No usable lrint() and lrintf() functions available."
        #warning "Replacing these functions with a simple C cast."
    #endif

    static __inline__ long int lfastrint(double x)
    {
        return (long int) (x);
    }

    static __inline__ long int lfastrintf(float x)
    {
        return (long int) (x);
    }

#endif

#if defined(__cplusplus)
}
#endif

#endif

/*- End of file ------------------------------------------------------------*/
