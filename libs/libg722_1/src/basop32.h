/*
 * g722_1 - a library for the G.722.1 and Annex C codecs
 *
 * basops32.h
 *
 * Adapted by Steve Underwood <steveu@coppice.org> from the reference
 * code supplied with ITU G.722.1, which is:
 *
 *   © 2004 Polycom, Inc.
 *   All rights reserved.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * $Id: basop32.h,v 1.3 2008/09/22 13:08:31 steveu Exp $
 */

#if !defined(BASOP32_H_DEFINED)
#define BASOP32_H_DEFINED

int32_t L_add(int32_t L_var1, int32_t L_var2);

static __inline__ int16_t saturate(int32_t amp)
{
    int16_t amp16;

    /* Hopefully this is optimised for the common case - not clipping */
    amp16 = (int16_t) amp;
    if (amp == amp16)
        return amp16;
    if (amp > INT16_MAX)
        return INT16_MAX;
    return INT16_MIN;
}
/*- End of function --------------------------------------------------------*/

static __inline__ int16_t xround(int32_t L_var1)
{
    return (int16_t) (L_add(L_var1, (int32_t) 0x00008000L) >> 16);
}
/*- End of function --------------------------------------------------------*/

static __inline__ int16_t abs_s(int16_t var1)
{
    if (var1 == INT16_MIN)
        return INT16_MAX;
    return abs(var1);
}
/*- End of function --------------------------------------------------------*/

static __inline__ int16_t add(int16_t var1, int16_t var2)
{
    return saturate((int32_t) var1 + var2);
}
/*- End of function --------------------------------------------------------*/

static __inline__ int16_t sub(int16_t var1, int16_t var2)
{
    return saturate((int32_t) var1 - var2);
}
/*- End of function --------------------------------------------------------*/

static __inline__ int16_t mult(int16_t var1, int16_t var2)
{
    return saturate(((int32_t) var1*(int32_t) var2) >> 15);
}
/*- End of function --------------------------------------------------------*/

static __inline__ int32_t L_mult0(int16_t var1, int16_t var2)
{
    return (int32_t) var1*(int32_t) var2;
}
/*- End of function --------------------------------------------------------*/

static __inline__ int32_t L_mac0(int32_t L_var3, int16_t var1, int16_t var2)
{
    return L_add(L_var3, L_mult0(var1, var2));
}
/*- End of function --------------------------------------------------------*/

static __inline__ int32_t L_mult(int16_t var1, int16_t var2)
{
    int32_t L_var_out;

    L_var_out = (int32_t) var1*(int32_t) var2;
    if (L_var_out == (int32_t) 0x40000000L)
        return INT32_MAX;
    return L_var_out << 1;
}
/*- End of function --------------------------------------------------------*/

static __inline__ int16_t negate(int16_t var1)
{
    if (var1 == INT16_MIN)
        return INT16_MAX;
    return -var1;
}
/*- End of function --------------------------------------------------------*/

static __inline__ int32_t L_mac(int32_t L_var3, int16_t var1, int16_t var2)
{
    return L_add(L_var3, L_mult(var1, var2));
}
/*- End of function --------------------------------------------------------*/

int16_t shl(int16_t var1, int16_t var2);    /* Short shift left,    1 */
int16_t shr(int16_t var1, int16_t var2);    /* Short shift right,   1 */
int32_t L_sub(int32_t L_var1, int32_t L_var2);    /* Long sub,        2 */
int32_t L_shl(int32_t L_var1, int16_t var2);      /* Long shift left, 2 */
int32_t L_shr(int32_t L_var1, int16_t var2);      /* Long shift right, 2*/
int16_t norm_s(int16_t var1);               /* Short norm,           15 */
int16_t div_s(int16_t var1, int16_t var2);  /* Short division,       18 */
int16_t norm_l(int32_t L_var1);             /* Long norm,            30 */

#endif

/*- End of file ------------------------------------------------------------*/
