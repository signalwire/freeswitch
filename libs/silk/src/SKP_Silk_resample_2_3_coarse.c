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
 * File Name:   SKP_Silk_resample_2_3_coarse.c                        *
 *                                                                      *
 * Description: Linear phase FIR polyphase implementation of resampling *
 *                                                                      *
 * Copyright 2009 (c), Skype Limited                                    *
 * All rights reserved.                                                 *
 *                                                                      *
 * Date: 090423                                                         *
 *                                                                      */

#include "SKP_Silk_SigProc_FIX.h"
#include "SKP_Silk_resample_rom.h"

/* Resamples input data with a factor 2/3 */
void SKP_Silk_resample_2_3_coarse( 
    SKP_int16           *out,           /* O:   Output signal                                                                   */
    SKP_int16           *S,             /* I/O: Resampler state [ SigProc_Resample_2_3_coarse_NUM_FIR_COEFS - 1 ]               */
    const SKP_int16     *in,            /* I:   Input signal                                                                    */
    const SKP_int       frameLenIn,     /* I:   Number of input samples                                                         */
    SKP_int16           *scratch        /* I:   Scratch memory [ frameLenIn + SigProc_Resample_2_3_coarse_NUM_FIR_COEFS - 1 ]   */
)
{
    SKP_int32 n, ind, interpol_ind, tmp, index_Q16;
    SKP_int16 *in_ptr;
    SKP_int   frameLenOut;
    const SKP_int16 *interpol_ptr;

    /* Copy buffered samples to start of scratch */
    SKP_memcpy( scratch, S, ( SigProc_Resample_2_3_coarse_NUM_FIR_COEFS - 1 ) * sizeof( SKP_int16 ) );
    
    /* Then append by the input signal */
    SKP_memcpy( &scratch[ SigProc_Resample_2_3_coarse_NUM_FIR_COEFS - 1 ], in, frameLenIn * sizeof( SKP_int16 ) ); 

    frameLenOut = SKP_DIV32_16( SKP_MUL( 2, frameLenIn ), 3 );
    index_Q16 = 0;

    SKP_assert( frameLenIn == ( ( frameLenOut * 3 ) / 2 ) );
    
    /* Interpolate */
    for( n = frameLenOut; n > 0; n-- ) {

        /* Integer part */
        ind = SKP_RSHIFT( index_Q16, 16 );

        /* Pointer to buffered input */
        in_ptr = scratch + ind;

        /* Fractional part */
        interpol_ind = ( SKP_SMULWB( index_Q16, SigProc_Resample_2_3_coarse_NUM_INTERPOLATORS ) & 
                       ( SigProc_Resample_2_3_coarse_NUM_INTERPOLATORS - 1 ) );

        /* Pointer to FIR taps */
        interpol_ptr = SigProc_Resample_2_3_coarse_INTERPOL[ interpol_ind ];

        /* Interpolate */
        /* Hardcoded for 32 FIR taps */
        SKP_assert( SigProc_Resample_2_3_coarse_NUM_FIR_COEFS == 32 );
        tmp = (SKP_int32)interpol_ptr[  0 ] * in_ptr[  0 ] + (SKP_int32)interpol_ptr[  1 ] * in_ptr[  1 ] +
              (SKP_int32)interpol_ptr[  2 ] * in_ptr[  2 ] + (SKP_int32)interpol_ptr[  3 ] * in_ptr[  3 ] +
              (SKP_int32)interpol_ptr[  4 ] * in_ptr[  4 ] + (SKP_int32)interpol_ptr[  5 ] * in_ptr[  5 ] +
              (SKP_int32)interpol_ptr[  6 ] * in_ptr[  6 ] + (SKP_int32)interpol_ptr[  7 ] * in_ptr[  7 ] +
              (SKP_int32)interpol_ptr[  8 ] * in_ptr[  8 ] + (SKP_int32)interpol_ptr[  9 ] * in_ptr[  9 ] +
              (SKP_int32)interpol_ptr[ 10 ] * in_ptr[ 10 ] + (SKP_int32)interpol_ptr[ 11 ] * in_ptr[ 11 ] +
              (SKP_int32)interpol_ptr[ 12 ] * in_ptr[ 12 ] + (SKP_int32)interpol_ptr[ 13 ] * in_ptr[ 13 ] +
              (SKP_int32)interpol_ptr[ 14 ] * in_ptr[ 14 ] + (SKP_int32)interpol_ptr[ 15 ] * in_ptr[ 15 ] +
              (SKP_int32)interpol_ptr[ 16 ] * in_ptr[ 16 ] + (SKP_int32)interpol_ptr[ 17 ] * in_ptr[ 17 ] +
              (SKP_int32)interpol_ptr[ 18 ] * in_ptr[ 18 ] + (SKP_int32)interpol_ptr[ 19 ] * in_ptr[ 19 ] +
              (SKP_int32)interpol_ptr[ 20 ] * in_ptr[ 20 ] + (SKP_int32)interpol_ptr[ 21 ] * in_ptr[ 21 ] +
              (SKP_int32)interpol_ptr[ 22 ] * in_ptr[ 22 ] + (SKP_int32)interpol_ptr[ 23 ] * in_ptr[ 23 ] +
              (SKP_int32)interpol_ptr[ 24 ] * in_ptr[ 24 ] + (SKP_int32)interpol_ptr[ 25 ] * in_ptr[ 25 ] +
              (SKP_int32)interpol_ptr[ 26 ] * in_ptr[ 26 ] + (SKP_int32)interpol_ptr[ 27 ] * in_ptr[ 27 ] +
              (SKP_int32)interpol_ptr[ 28 ] * in_ptr[ 28 ] + (SKP_int32)interpol_ptr[ 29 ] * in_ptr[ 29 ];

        /* Round, saturate and store to output array */
        *out++ = (SKP_int16)SKP_SAT16( SKP_RSHIFT_ROUND( tmp, 15 ) );

        /* Update index */
        index_Q16 += ( ( 1 << 16 ) + ( 1 << 15 ) ); // (3/2)_Q0;
    }

    /* Move last part of input signal to the sample buffer to prepare for the next call */
    SKP_memcpy( S, &in[ frameLenIn - ( SigProc_Resample_2_3_coarse_NUM_FIR_COEFS - 1 ) ],
                ( SigProc_Resample_2_3_coarse_NUM_FIR_COEFS - 1 ) * sizeof( SKP_int16 ) );
}
