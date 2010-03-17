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

/*                                                                      *
 * SKP_Silk_resample_1_3.c                                            *
 *                                                                      *
 * Downsamples by a factor 3                                            *
 *                                                                      *
 * Copyright 2008 (c), Skype Limited                                    *
 * Date: 081113                                                         *
 *                                                                      */
#include "SKP_Silk_SigProc_FIX.h"

#define OUT_SUBFR_LEN        80

/* Downsamples by a factor 3 */
void SKP_Silk_resample_1_3(
    SKP_int16            *out,       /* O:   Fs_low signal  [inLen/3]                */
    SKP_int32            *S,         /* I/O: State vector   [7]                      */
    const SKP_int16      *in,        /* I:   Fs_high signal [inLen]                  */
    const SKP_int32      inLen       /* I:   Input length, must be a multiple of 3   */
)
{
    SKP_int      k, outLen, LSubFrameIn, LSubFrameOut;
    SKP_int32    out_tmp, limit = 102258000; // (102258000 + 1560) * 21 * 2^(-16) = 32767.5
    SKP_int32    scratch0[ 3 * OUT_SUBFR_LEN ];
    SKP_int32    scratch10[ OUT_SUBFR_LEN ], scratch11[ OUT_SUBFR_LEN ], scratch12[ OUT_SUBFR_LEN ];
    /* coefficients for 3-fold resampling */
    const SKP_int16 A30[ 2 ] = {  1773, 17818 };
    const SKP_int16 A31[ 2 ] = {  4942, 25677 };
    const SKP_int16 A32[ 2 ] = { 11786, 29304 };

    /* Check that input is multiple of 3 */
    SKP_assert( inLen % 3 == 0 );

    outLen = SKP_DIV32_16( inLen, 3 );
    while( outLen > 0 ) {
        LSubFrameOut = SKP_min_int( OUT_SUBFR_LEN, outLen );
        LSubFrameIn  = SKP_SMULBB( 3, LSubFrameOut );

        /* Low-pass filter, Q15 -> Q25 */
        SKP_Silk_lowpass_short( in, S, scratch0, LSubFrameIn );

        /* De-interleave three allpass inputs */
        for( k = 0; k < LSubFrameOut; k++ ) {
            scratch10[ k ] = scratch0[ 3 * k     ];
            scratch11[ k ] = scratch0[ 3 * k + 1 ];
            scratch12[ k ] = scratch0[ 3 * k + 2 ];
        }

        /* Allpass filters */
        SKP_Silk_allpass_int( scratch10, S + 1, A32[ 0 ], scratch0,  LSubFrameOut );
        SKP_Silk_allpass_int( scratch0,  S + 2, A32[ 1 ], scratch10, LSubFrameOut );

        SKP_Silk_allpass_int( scratch11, S + 3, A31[ 0 ], scratch0,  LSubFrameOut );
        SKP_Silk_allpass_int( scratch0,  S + 4, A31[ 1 ], scratch11, LSubFrameOut );

        SKP_Silk_allpass_int( scratch12, S + 5, A30[ 0 ], scratch0,  LSubFrameOut );
        SKP_Silk_allpass_int( scratch0,  S + 6, A30[ 1 ], scratch12, LSubFrameOut );

        /* Add three allpass outputs */
        for( k = 0; k < LSubFrameOut; k++ ) {
            out_tmp = scratch10[ k ] + scratch11[ k ] + scratch12[ k ];
            if( out_tmp - limit > 0 ) {
                out[ k ] = SKP_int16_MAX;
            } else if( out_tmp + limit < 0 ) {
                out[ k ] = SKP_int16_MIN;
            } else {
                out[ k ] = (SKP_int16) SKP_SMULWB( out_tmp + 1560, 21 );
            }
        }

        in     += LSubFrameIn;
        out    += LSubFrameOut;
        outLen -= LSubFrameOut;
    }
}
