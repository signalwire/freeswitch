/*
 * g722_1 - a library for the G.722.1 and Annex C codecs
 *
 * basop32.c
 *
 * Adapted by Steve Underwood <steveu@coppice.org> from the reference
 * code supplied with ITU G.722.1
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * $Id: basop32.c,v 1.5 2008/09/22 13:08:31 steveu Exp $
 */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include <config.h>
#endif

#if defined(G722_1_USE_FIXED_POINT)

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>

#include "basop32.h"

int16_t shl(int16_t var1, int16_t var2)
{
    int32_t result;

    if (var2 < 0)
    {
        if (var2 < -16)
            var2 = -16;
        return shr(var1, (int16_t) -var2);
    }
    result = (int32_t) var1*((int32_t) 1 << var2);
    if ((var2 > 15  &&  var1 != 0)  ||  (result != (int32_t) ((int16_t) result)))
        return (var1 > 0)  ?  INT16_MAX  :  INT16_MIN;
    return (int16_t) result;
}
/*- End of function --------------------------------------------------------*/

int16_t shr(int16_t var1, int16_t var2)
{
    if (var2 < 0)
    {
        if (var2 < -16)
            var2 = -16;
        return shl(var1, (int16_t) -var2);
    }
    if (var2 >= 15)
        return (var1 < 0)  ?  -1  :  0;
    if (var1 < 0)
        return ~((~var1) >> var2);
    return var1 >> var2;
}
/*- End of function --------------------------------------------------------*/

int32_t L_add(int32_t L_var1, int32_t L_var2)
{
    int32_t L_var_out;

    L_var_out = L_var1 + L_var2;
    if (((L_var1 ^ L_var2) & INT32_MIN) == 0)
    {
        if ((L_var_out ^ L_var1) & INT32_MIN)
            return (L_var1 < 0)  ?  INT32_MIN  :  INT32_MAX;
    }
    return L_var_out;
}
/*- End of function --------------------------------------------------------*/

int32_t L_sub(int32_t L_var1, int32_t L_var2)
{
    int32_t L_var_out;

    L_var_out = L_var1 - L_var2;
    if (((L_var1 ^ L_var2) & INT32_MIN) != 0)
    {
        if ((L_var_out ^ L_var1) & INT32_MIN)
            return (L_var1 < 0L)  ?  INT32_MIN  :  INT32_MAX;
    }
    return L_var_out;
}
/*- End of function --------------------------------------------------------*/

int32_t L_shl(int32_t L_var1, int16_t var2)
{
    if (var2 <= 0)
    {
        if (var2 < -32)
            var2 = -32;
        return L_shr(L_var1, -var2);
    }
    for (  ;  var2 > 0;  var2--)
    {
        if (L_var1 > (int32_t) 0X3fffffffL)
            return INT32_MAX;
        if (L_var1 < (int32_t) 0xc0000000L)
            return INT32_MIN;
        L_var1 *= 2;
    }
    return L_var1;
}
/*- End of function --------------------------------------------------------*/

int32_t L_shr(int32_t L_var1, int16_t var2)
{
    if (var2 < 0)
    {
        if (var2 < -32)
            var2 = -32;
        return L_shl(L_var1, (int16_t) -var2);
    }
    if (var2 >= 31)
        return (L_var1 < 0L)  ?  -1  :  0;
    if (L_var1 < 0)
        return ~((~L_var1) >> var2);
    return L_var1 >> var2;
}
/*- End of function --------------------------------------------------------*/

/*! \brief Find the bit position of the highest set bit in a word
    \param bits The word to be searched
    \return The bit number of the highest set bit, or -1 if the word is zero. */
static __inline__ int top_bit(unsigned int bits)
{
    int res;

#if defined(__i386__)  ||  defined(__x86_64__)
    __asm__ (" xorl %[res],%[res];\n"
             " decl %[res];\n"
             " bsrl %[bits],%[res]\n"
             : [res] "=&r" (res)
             : [bits] "rm" (bits));
    return res;
#elif defined(__ppc__)  ||   defined(__powerpc__)
    __asm__ ("cntlzw %[res],%[bits];\n"
             : [res] "=&r" (res)
             : [bits] "r" (bits));
    return 31 - res;
#else
    if (bits == 0)
        return -1;
    res = 0;
    if (bits & 0xFFFF0000)
    {
        bits &= 0xFFFF0000;
        res += 16;
    }
    if (bits & 0xFF00FF00)
    {
        bits &= 0xFF00FF00;
        res += 8;
    }
    if (bits & 0xF0F0F0F0)
    {
        bits &= 0xF0F0F0F0;
        res += 4;
    }
    if (bits & 0xCCCCCCCC)
    {
        bits &= 0xCCCCCCCC;
        res += 2;
    }
    if (bits & 0xAAAAAAAA)
    {
        bits &= 0xAAAAAAAA;
        res += 1;
    }
    return res;
#endif
}
/*- End of function --------------------------------------------------------*/

int16_t norm_s(int16_t var1)
{
    if (var1 == 0)
        return 0;
    if (var1 < 0)
        var1 = ~var1;
    return (14 - top_bit(var1));
}
/*- End of function --------------------------------------------------------*/
#endif
/*- End of file ------------------------------------------------------------*/
