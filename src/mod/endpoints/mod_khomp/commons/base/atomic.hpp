/*
    KHOMP generic endpoint/channel library.

    This code was based on FreeBSD 7.X SVN (sys/i386/include/atomic.h),
    with changes regarding optimizations and generalizations, and a
    remake of the interface to fit use C++ features.

    Code is distributed under original license.
    Original copyright follows:

 * Copyright (c) 1998 Doug Rabson
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.

*/

#ifndef _ATOMIC_HPP_
#define _ATOMIC_HPP_

namespace Atomic
{
    // Macros used to insert compare and exchange instructions easily into functions.

    #define MAKE_CMPXCHG_FUNCTION(INS, PTR, EXP, VAL, TYPE)  \
        PunnedType pexp; pexp.valtype = EXP;                 \
        PunnedType pval; pval.valtype = VAL;                 \
        TYPE vexp = *(pexp.podtype);                         \
        TYPE vval = *(pval.podtype);                         \
        TYPE res;                                            \
        unsigned char chg = 0;                               \
        asm volatile("lock;" INS "sete %1;"                  \
        : "=a" (res),                           /* 0 */      \
        "=q" (chg),                             /* 1 */      \
        "=m" (*(unsigned char **)(PTR))         /* 2 */      \
        : "r" (vval),                           /* 3 */      \
        "a" (vexp),                             /* 4 */      \
        "m" (*(unsigned char **)(PTR))          /* 5 */      \
        : "memory");                                         \
        *(pexp.podtype) = res;                               \
        return (chg != 0 ? true : false);

    #define MAKE_CMPXCHG8B_FUNCTION(PTR,EXP,VAL)           \
        PunnedType pexp; pexp.valtype = EXP;               \
        PunnedType pval; pval.valtype = VAL;               \
        unsigned long long vexp = *(pexp.podtype);         \
        unsigned long long vval = *(pval.podtype);         \
        unsigned long vval32 = (unsigned long)vval;        \
        unsigned char chg = 0;                             \
        asm volatile(                                      \
            "xchgl %%ebx, %4;"                             \
            "lock; cmpxchg8b %2; sete %1;"                 \
            "movl %4, %%ebx;   "                           \
        : "+A" (vexp),                    /* 0 (result) */ \
          "=c" (chg)                      /* 1 */          \
        : "m" (*(unsigned char**)(PTR)),  /* 2 */          \
          "c" ((unsigned long)(vval >> 32)),               \
          "m" (vval32));                                   \
        *(pexp.podtype) = vexp;                            \
        return (chg != 0 ? true : false);

//            "movl %%ecx, %4;"
//
//          "m" (*((unsigned long*)(*(pval.podtype)))),
//          "m" ((unsigned long)(vval >> 32))
//
//          "m" (*((unsigned long*)(&vval))),
//          "m" ((unsigned long)(vval >> 32))
//
//        unsigned long long vval = *(pval.podtype);
//        unsigned long long res = (unsigned long long)exp;
//
    // Types used for making CMPXCHG instructions independent from base type.

    template < typename ValType, typename PodType >
    union PunnedTypeTemplate
    {
        ValType * valtype;
        PodType * podtype;
    };

    template < int SizeOfType, typename ReturnType >
    struct HelperCreateCAS;

    template < typename ValType >
    struct HelperCreateCAS<4, ValType>
    {
        #if !defined(__LP64__) && !defined(__LP64)
            typedef unsigned long BaseType;
        #else
            typedef unsigned int  BaseType;
        #endif

        typedef PunnedTypeTemplate< ValType, BaseType > PunnedType;

        inline static bool apply(volatile void *p, ValType * exp, ValType now)
        {
            #if !defined(__LP64__) && !defined(__LP64)
                MAKE_CMPXCHG_FUNCTION("cmpxchgl %3,%5;", p, exp, &now, BaseType);
            #else
                MAKE_CMPXCHG_FUNCTION("cmpxchgl %k3,%5;", p, exp, &now, BaseType);
            #endif
        }
    };

    template < typename ValType >
    struct HelperCreateCAS<8, ValType>
    {
        #if !defined(__LP64__) && !defined(__LP64)
            typedef unsigned long long BaseType;
        #else
            typedef unsigned long BaseType;
        #endif

        typedef PunnedTypeTemplate< ValType, BaseType > PunnedType;

        inline static volatile ValType apply(volatile void *p, ValType * exp, ValType now)
        {
            #if !defined(__LP64__) && !defined(__LP64)
                MAKE_CMPXCHG8B_FUNCTION(p, exp, &now);
            #else
                MAKE_CMPXCHG_FUNCTION("cmpxchgq %3,%5;", p, exp, &now, BaseType);
            #endif
        }

    };

    // The CAS function itself.

    template < typename ValType >
    inline bool doCAS(volatile ValType * p, ValType * o, ValType n)
    {
        return HelperCreateCAS<sizeof(ValType), ValType>::apply(static_cast<volatile void *>(p), o, n);
    };

    template < typename ValType >
    inline bool doCAS(volatile ValType * p, ValType o, ValType n)
    {
        return HelperCreateCAS<sizeof(ValType), ValType>::apply(static_cast<volatile void *>(p), &o, n);
    };

    #undef MAKE_CMPXCHG_32_FUNCTION
    #undef MAKE_CMPXCHG_64_FUNCTION

    #define MAKE_LOCKED_TEMPLATE(NAME)                                                     \
    template < typename ValType > inline void do##NAME(volatile ValType * p, ValType v);   \
    template < typename ValType > inline void do##NAME(volatile ValType * p);

    #define MAKE_LOCKED_FUNCTION(NAME, TYPE, INS, CONS, VAL)                                                                                    \
    template < > inline void do##NAME < TYPE > (volatile TYPE * p, TYPE v){   asm volatile("lock;" INS : "=m" (*p) : CONS (VAL), "m" (*p)); }   \
    template < > inline void do##NAME < TYPE > (volatile TYPE * p)        {   asm volatile("lock;" INS : "=m" (*p) : CONS (1),   "m" (*p)); }

    #define MAKE_LOCKED_FUNCTIONS(NAME, TYPE, INS, CONS, VAL)       \
        MAKE_LOCKED_FUNCTION(NAME, TYPE, INS, CONS, VAL)            \
        MAKE_LOCKED_FUNCTION(NAME, unsigned TYPE, INS, CONS, VAL)

    MAKE_LOCKED_TEMPLATE(Add);
    MAKE_LOCKED_TEMPLATE(Sub);
    MAKE_LOCKED_TEMPLATE(SetBits);
    MAKE_LOCKED_TEMPLATE(ClearBits);

    MAKE_LOCKED_FUNCTIONS(Add,   int,   "addl %1,%0",  "ir",  v);
    MAKE_LOCKED_FUNCTIONS(Sub,   int,   "subl %1,%0",  "ir",  v);
    MAKE_LOCKED_FUNCTIONS(SetBits,   int,   "orl %1,%0",   "ir",  v);
    MAKE_LOCKED_FUNCTIONS(ClearBits, int,   "andl %1,%0",  "ir", ~v);

    #if !defined(__LP64__) && !defined(__LP64)

    MAKE_LOCKED_FUNCTIONS(Add,   long,  "addl %1,%0",  "ir",  v);
    MAKE_LOCKED_FUNCTIONS(Sub,   long,  "subl %1,%0",  "ir",  v);
    MAKE_LOCKED_FUNCTIONS(SetBits,   long,  "orl %1,%0",   "ir",  v);
    MAKE_LOCKED_FUNCTIONS(ClearBits, long,  "andl %1,%0",  "ir", ~v);

    #else

    MAKE_LOCKED_FUNCTIONS(Add,   long,  "addq %1,%0",  "ir",  v);
    MAKE_LOCKED_FUNCTIONS(Sub,   long,  "subq %1,%0",  "ir",  v);
    MAKE_LOCKED_FUNCTIONS(SetBits,   long,  "orq %1,%0",   "ir",  v);
    MAKE_LOCKED_FUNCTIONS(ClearBits, long,  "andq %1,%0",  "ir", ~v);

    #endif
};

#endif /* _ATOMIC_HPP_ */
