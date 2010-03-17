/***********************************************************************
Copyright (c) 2006-2010, Skype Limited. All rights reserved. 
Redistribution and use in source and binary forms, with or without 
modification, (subject to the limitations in the disclaimer below) 
are permitted provided that the following conditions are met:
- Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright 
notice, this list of conditions and the following disclaimer in the 
documentation and/or other materials provided with the distribution.
- Neither the name of Skype Limited, nor the names of specific 
contributors, may be used to endorse or promote products derived from 
this software without specific prior written permission.
NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED 
BY THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND 
CONTRIBUTORS ''AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND 
FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE 
COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, 
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF 
USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON 
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
***********************************************************************/

#include "SKP_Silk_SigProc_FIX.h"

/* Apply sine window to signal vector.                                      */
/* Window types:                                                            */
/*    0 -> sine window from 0 to pi                                         */
/*    1 -> sine window from 0 to pi/2                                       */
/*    2 -> sine window from pi/2 to pi                                      */
/* every other sample of window is linearly interpolated, for speed         */
void SKP_Silk_apply_sine_window(
    SKP_int16                        px_win[],            /* O    Pointer to windowed signal                  */
    const SKP_int16                  px[],                /* I    Pointer to input signal                     */
    const SKP_int                    win_type,            /* I    Selects a window type                       */
    const SKP_int                    length               /* I    Window length, multiple of 4                */
)
{
    SKP_int   k;
    SKP_int32 px32, f_Q16, c_Q20, S0_Q16, S1_Q16;

    /* Length must be multiple of 4 */
    SKP_assert( ( length & 3 ) == 0 );

    /* Input pointer must be 4-byte aligned */
    SKP_assert( ( (SKP_int64)px & 3 ) == 0 );

    if( win_type == 0 ) {
        f_Q16 = SKP_DIV32_16( 411775, length + 1 );        // 411775 = 2 * 65536 * pi
    } else {
        f_Q16 = SKP_DIV32_16( 205887, length + 1 );        // 205887 = 65536 * pi
    }

    /* factor used for cosine approximation */
    c_Q20 = -SKP_RSHIFT( SKP_MUL( f_Q16, f_Q16 ), 12 );

    /* c_Q20 becomes too large if length is too small */
    SKP_assert( c_Q20 >= -32768 );

    /* initialize state */
    if( win_type < 2 ) {
        /* start from 0 */
        S0_Q16 = 0;
        /* approximation of sin(f) */
        S1_Q16 = f_Q16;
    } else {
        /* start from 1 */
        S0_Q16 = ( 1 << 16 );
        /* approximation of cos(f) */
        S1_Q16 = ( 1 << 16 ) + SKP_RSHIFT( c_Q20, 5 );
    }

    /* Uses the recursive equation:   sin(n*f) = 2 * cos(f) * sin((n-1)*f) - sin((n-2)*f)    */
    /* 4 samples at a time */
    for( k = 0; k < length; k += 4 ) {
        px32 = *( (SKP_int32 *)&px[ k ] );                        /* load two values at once */
        px_win[ k ]     = (SKP_int16)SKP_SMULWB( SKP_RSHIFT( S0_Q16 + S1_Q16, 1 ), px32 );
        px_win[ k + 1 ] = (SKP_int16)SKP_SMULWT( S1_Q16, px32 );
        S0_Q16 = SKP_RSHIFT( SKP_MUL( c_Q20, S1_Q16 ), 20 ) + SKP_LSHIFT( S1_Q16, 1 ) - S0_Q16 + 1;
        S0_Q16 = SKP_min( S0_Q16, ( 1 << 16 ) );

        px32 = *( (SKP_int32 *)&px[k + 2] );                    /* load two values at once */
        px_win[ k + 2 ] = (SKP_int16)SKP_SMULWB( SKP_RSHIFT( S0_Q16 + S1_Q16, 1 ), px32 );
        px_win[ k + 3 ] = (SKP_int16)SKP_SMULWT( S0_Q16, px32 );
        S1_Q16 = SKP_RSHIFT( SKP_MUL( c_Q20, S0_Q16 ), 20 ) + SKP_LSHIFT( S0_Q16, 1 ) - S1_Q16;
        S1_Q16 = SKP_min( S1_Q16, ( 1 << 16 ) );
    }
}
