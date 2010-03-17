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
 * SKP_Silk_resample_2_1_coarse.c                                     *
 *                                                                      *
 * Upsample by a factor 2, coarser                                      *
 *                                                                      *
 * Copyright 2006 (c), Skype Limited                                    *
 * Date: 060221                                                         *
 *                                                                      */
#include "SKP_Silk_SigProc_FIX.h"

/* Upsample by a factor 2, coarser */
void SKP_Silk_resample_2_1_coarse(
    const SKP_int16      *in,            /* I:   8 kHz signal [len]      */
    SKP_int32            *S,             /* I/O: State vector [4]        */
    SKP_int16            *out,           /* O:   16 kHz signal [2*len]   */
    SKP_int32            *scratch,       /* I:   Scratch memory [3*len]  */
    const SKP_int32      len             /* I:   Number of INPUT samples */
)
{
    SKP_int32 k, idx;
    
    /* Coefficients for coarser 2-fold resampling */
    const SKP_int16 A20c[ 2 ] = { 2119, 16663 };
    const SKP_int16 A21c[ 2 ] = { 8050, 26861 };

    /* Convert Q15 -> Q25 */
    for( k = 0; k < len; k++ ) {
        scratch[ k ] = SKP_LSHIFT( (SKP_int32)in[ k ], 10 );
    }
       
    idx = SKP_LSHIFT( len, 1 );
    
    /* Allpass filters */
    SKP_Silk_allpass_int( scratch,       S,     A20c[ 0 ], scratch + idx, len );
    SKP_Silk_allpass_int( scratch + idx, S + 1, A20c[ 1 ], scratch + len, len );

    SKP_Silk_allpass_int( scratch,       S + 2, A21c[ 0 ], scratch + idx, len );
    SKP_Silk_allpass_int( scratch + idx, S + 3, A21c[ 1 ], scratch,       len );

    /* Interleave two allpass outputs */
    for( k = 0; k < len; k++ ) {
        idx = SKP_LSHIFT( k, 1 );
        out[ idx     ] = (SKP_int16)SKP_SAT16( SKP_RSHIFT_ROUND( scratch[ k + len ], 10 ) );
        out[ idx + 1 ] = (SKP_int16)SKP_SAT16( SKP_RSHIFT_ROUND( scratch[ k ],       10 ) );
    }
}
