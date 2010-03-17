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
 * File Name:    SKP_Silk_resample_4_3.c                              *
 *                                                                      *
 * Resamples by a factor 4/3                                            *
 *                                                                      *
 * Copyright 2009 (c), Skype Limited                                    *
 * All rights reserved.                                                 *
 *                                                                      *
 * Date: 090407                                                         *
 *                                                                      */

#include "SKP_Silk_SigProc_FIX.h"

#define OUT_SUBFR_LEN        80

/* Resamples by a factor 4/3 */
void SKP_Silk_resample_4_3(
    SKP_int16            *out,       /* O:   Fs_low signal    [inLen * 4/3]           */
    SKP_int32            *S,         /* I/O: State vector    [7+4+4]                  */
    const SKP_int16      *in,        /* I:   Fs_high signal    [inLen]                */
    const SKP_int        inLen       /* I:   input length, must be a multiple of 3    */
) 
{
    SKP_int      outLen, LSubFrameIn, LSubFrameOut;
    SKP_int16    outH[    3 * OUT_SUBFR_LEN ];
    SKP_int16    outHH[   6 * OUT_SUBFR_LEN ];
    SKP_int32    scratch[ 9 * OUT_SUBFR_LEN / 2 ];

    /* Check that input is multiple of 3 */
    SKP_assert( inLen % 3 == 0 );

    outLen = SKP_DIV32_16( SKP_LSHIFT( inLen, 2 ), 3 );
    while( outLen > 0 ) {
        LSubFrameOut = SKP_min_int( OUT_SUBFR_LEN, outLen );
        LSubFrameIn  = SKP_SMULWB( 49152, LSubFrameOut );

        /* Upsample two times by a factor 2 */
        /* Scratch size needs to be: 3 * LSubFrameIn * sizeof( SKP_int32 ) */
        SKP_Silk_resample_2_1_coarse( in,   &S[ 0 ], outH,  scratch,             LSubFrameIn      );
        /* Scratch size needs to be: 6 * LSubFrameIn * sizeof( SKP_int32 ) */
        SKP_Silk_resample_2_1_coarse( outH, &S[ 4 ], outHH, scratch, SKP_LSHIFT( LSubFrameIn, 1 ) );
        
        /* Downsample by a factor 3 */
        SKP_Silk_resample_1_3( out, &S[ 8 ], outHH, SKP_LSHIFT( LSubFrameIn, 2 ) );

        in     += LSubFrameIn;
        out    += LSubFrameOut;
        outLen -= LSubFrameOut;
    }
}
