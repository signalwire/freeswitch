/*
 * SpanDSP - a series of DSP components for telephony
 *
 * testcpuid.c - Check the CPU type, to identify special features, like SSE.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 * Stitched together from bits of testing code found here and there
 *
 * Copyright (C) 2004 Steve Underwood
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

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <inttypes.h>

/* Make this file just disappear if we are not on an x86 machine */
#if defined(__i386__) //  ||  defined(__x86_64__)

enum
{
    X86_EFLAGS_CF   = 0x00000001,   /* Carry Flag */
    X86_EFLAGS_PF   = 0x00000004,   /* Parity Flag */
    X86_EFLAGS_AF   = 0x00000010,   /* Auxillary carry Flag */
    X86_EFLAGS_ZF   = 0x00000040,   /* Zero Flag */
    X86_EFLAGS_SF   = 0x00000080,   /* Sign Flag */
    X86_EFLAGS_TF   = 0x00000100,   /* Trap Flag */
    X86_EFLAGS_IF   = 0x00000200,   /* Interrupt Flag */
    X86_EFLAGS_DF   = 0x00000400,   /* Direction Flag */
    X86_EFLAGS_OF   = 0x00000800,   /* Overflow Flag */
    X86_EFLAGS_IOPL = 0x00003000,   /* IOPL mask */
    X86_EFLAGS_NT   = 0x00004000,   /* Nested Task */
    X86_EFLAGS_RF   = 0x00010000,   /* Resume Flag */
    X86_EFLAGS_VM   = 0x00020000,   /* Virtual Mode */
    X86_EFLAGS_AC   = 0x00040000,   /* Alignment Check */
    X86_EFLAGS_VIF  = 0x00080000,   /* Virtual Interrupt Flag */
    X86_EFLAGS_VIP  = 0x00100000,   /* Virtual Interrupt Pending */
    X86_EFLAGS_ID   = 0x00200000    /* CPUID detection flag */
};

/* Standard macro to see if a specific flag is changeable */
static __inline__ int flag_is_changeable_p(uint32_t flag)
{
    uint32_t f1;
    uint32_t f2;

    __asm__ __volatile__(
        " pushfl\n"
        " pushfl\n"
        " popl %0\n"
        " movl %0,%1\n"
        " xorl %2,%0\n"
        " pushl %0\n"
        " popfl\n"
        " pushfl\n"
        " popl %0\n"
        " popfl\n"
        : "=&r" (f1), "=&r" (f2)
        : "ir" (flag));
    return ((f1^f2) & flag) != 0;
}
/*- End of function --------------------------------------------------------*/

/* Probe for the CPUID instruction */
static int have_cpuid_p(void)
{
    return flag_is_changeable_p(X86_EFLAGS_ID);
}
/*- End of function --------------------------------------------------------*/

int has_MMX(void)
{
    int result;

    if (!have_cpuid_p())
        return 0;
    /*endif*/
    __asm__ __volatile__(
        " push  %%ebx;\n"
        " mov   $1,%%eax;\n"
        " cpuid;\n"
        " xor   %%eax,%%eax;\n"
        " test  $0x800000,%%edx;\n"
        " jz    1f;\n"                  /* no MMX support */
        " inc   %%eax;\n"               /* MMX support */
        "1:\n"
        " pop   %%ebx;\n"
        : "=a" (result)
        :
        : "ecx", "edx");
    return result;
}
/*- End of function --------------------------------------------------------*/

int has_SIMD(void)
{
    int result;

    if (!have_cpuid_p())
        return 0;
    /*endif*/
    __asm__ __volatile__(
        " push  %%ebx;\n"
        " mov   $1,%%eax;\n"
        " cpuid;\n"
        " xor   %%eax,%%eax;\n"
        " test  $0x02000000,%%edx;\n"
        " jz    1f;\n"                  /* no SIMD support */
        " inc   %%eax;\n"               /* SIMD support */
        "1:\n"
        " pop   %%ebx;\n"
        : "=a" (result)
        :
        : "ecx", "edx");
    return result;
}
/*- End of function --------------------------------------------------------*/

int has_SIMD2(void)
{
    int result;

    if (!have_cpuid_p())
        return 0;
    /*endif*/
    __asm__ __volatile__(
        " push  %%ebx;\n"
        " mov   $1,%%eax;\n"
        " cpuid;\n"
        " xor   %%eax,%%eax;\n"
        " test  $0x04000000,%%edx;\n"
        " jz    1f;\n"                  /* no SIMD2 support */
        " inc   %%eax;\n"               /* SIMD2 support */
        "1:\n"
        " pop   %%ebx;\n"
        : "=a" (result)
        :
        : "ecx", "edx");
    return result;
}
/*- End of function --------------------------------------------------------*/

int has_3DNow(void)
{
    int result;

    if (!have_cpuid_p())
        return 0;
    /*endif*/
    __asm__ __volatile__(
        " push  %%ebx;\n"
        " mov   $0x80000000,%%eax;\n"
        " cpuid;\n"
        " xor   %%ecx,%%ecx;\n"
        " cmp   $0x80000000,%%eax;\n"
        " jbe   1f;\n"                  /* no extended MSR(1), so no 3DNow! */
        " mov   $0x80000001,%%eax;\n"
        " cpuid;\n"
        " xor   %%ecx,%%ecx;\n"
        " test  $0x80000000,%%edx;\n"
        " jz    1f;\n"                  /* no 3DNow! support */
        " inc   %%ecx;\n"               /* 3DNow! support */
        "1:\n"
        " pop   %%ebx;\n"
        : "=c" (result)
        :
        : "eax", "edx");
    return result;
}
/*- End of function --------------------------------------------------------*/

#if defined(TESTBED)
int main(int argc, char *argv[])
{
    int result;

    result = has_MMX();
    printf("MMX is %x\n", result);
    result = has_SIMD();
    printf("SIMD is %x\n", result);
    result = has_SIMD2();
    printf("SIMD2 is %x\n", result);
    result = has_3DNow();
    printf("3DNow is %x\n", result);
    return 0;
}
/*- End of function --------------------------------------------------------*/
#endif

#endif
/*- End of file ------------------------------------------------------------*/
