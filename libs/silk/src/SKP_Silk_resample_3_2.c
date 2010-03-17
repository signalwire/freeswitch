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
 * File Name:   SKP_Silk_resample_3_2.c                               *
 *                                                                      *
 * Resamples by a factor 3/2                                            *
 *                                                                      *
 * Copyright 2008 (c), Skype Limited                                    *
 * All rights reserved.                                                 *
 *                                                                      *
 * Date: 081113                                                         *
 *                                                                      */

#include "SKP_Silk_SigProc_FIX.h"

#define IN_SUBFR_LEN_RESAMPLE_3_2       80

/* Resamples by a factor 3/2 */
void SKP_Silk_resample_3_2(
    SKP_int16           *out,       /*   O: Fs_high signal  [inLen*3/2]             */
    SKP_int32           *S,         /* I/O: State vector    [7+4]                   */
    const SKP_int16     *in,        /* I:   Fs_low signal   [inLen]                 */
    SKP_int             inLen       /* I:   Input length, must be a multiple of 2   */
)
{
    SKP_int     LSubFrameIn, LSubFrameOut;
    SKP_int16   outH[      3 * IN_SUBFR_LEN_RESAMPLE_3_2 ];
    SKP_int32   scratch[ ( 9 * IN_SUBFR_LEN_RESAMPLE_3_2 ) / 2 ];

    /* Check that input is multiple of 2 */
    SKP_assert( inLen % 2 == 0 );

    while( inLen > 0 ) {
        LSubFrameIn  = SKP_min_int( IN_SUBFR_LEN_RESAMPLE_3_2, inLen );
        LSubFrameOut = SKP_SMULWB( 98304, LSubFrameIn );

        /* Upsample by a factor 3 */
        SKP_Silk_resample_3_1( outH, &S[ 0 ], in, LSubFrameIn );
        
        /* Downsample by a factor 2 */
        /* Scratch size needs to be: 3 * LSubFrameOut * sizeof( SKP_int32 ) */
        SKP_Silk_resample_1_2_coarse( outH, &S[ 7 ], out, scratch, LSubFrameOut );

        in    += LSubFrameIn;
        out   += LSubFrameOut;
        inLen -= LSubFrameIn;
    }
}
